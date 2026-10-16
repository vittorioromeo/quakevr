# Round 22: after 1.0.0 (patch 1.0.1 onwards)

Notes for the work after the 1.0.0 release (2026-10-10). Round 21 (`ROUND21.md`) ends at 1.0.0; its older
sections are in `archive/`. Append new sections at the end of this file.

## Crash reports for every crash and fatal error (1.0.1, 2026-10-10)

The author's request after the campaign-switch crash (a bare "Mod_PointInLeaf: bad model" dialog, no stack): every
fatal end of the game now writes a report a player can send (`Quake/vr/vr_crash.cpp`; TESTING.md, "Crash reports").

- **What reports.** An unhandled exception on any thread (the unhandled exception filter; re-set at each map spawn in
  case a DLL replaced it); a stack overflow (a vectored handler, which runs first, on the stack's reserve: 64 KB kept
  for the main thread by `SetThreadStackGuarantee`; only that exception: access violations are caught and handled by
  the GL driver and the OpenXR runtimes themselves, so they wait for the unhandled filter); abort() (std::terminate,
  `za::abort`), a CRT invalid parameter and a pure virtual call (their handlers, now in players' runs too, not only
  test runs); `Sys_Error` (`Sys_ReportError` -> `VR_ErrorReport`; a Host_Error that ends the game is one); a failed
  Zancle assert (a Sys_Error; before the host is up, `VR_FatalReport`). `Sys_Error` off the main thread (a job, the
  save thread) is reported and the process ended there (`VR_FatalError`): the engine's shutdown is the main thread's.
  A non-fatal `Host_Error` prints its caller's stack under its line ("  from fn (file:line) < ...").
- **Where.** `<game folder>/crash/<date>_<time>.txt` and `.dmp` (the game folder at start: `quakevr/crash`, git-ignored),
  full paths. A player's run: a dialog ("Quake VR: Unleashed - Crash") names both; Sys_Error's own dialog gets the same
  lines. A test run (`QVR_NO_ERROR_DIALOG`): no dialog, and `qvr_crash.txt` in the game folder as before (run.sh's
  `ENGINE CRASH`). The report's head also goes to stderr, the debugger's output and `qconsole.log` (-condebug).
- **What is in it.** What failed (exception code, name, address, the access's kind and address; or the message), the
  thread (main or not, its name), the build and the exe's link time, the time and uptime, the map and map package and
  the game folder, the stack, the command line, and every module with its base and size. The stack: StackWalk64 from
  the exception's context (another thread's walk needs no live stack), each frame's function+offset, file and line
  (a return address looked up less one: the call's line), the functions ThinLTO inlined into a frame as their own
  `[inlined]` lines (DbgHelp's inline trace: without it a Sys_Error's own frame and most small helpers vanish), and
  `[module+offset]` on every real frame; a recursion's repeated frame as one line ("the same frame 998 more times").
  A report without an exception starts at the caller of the function asking (its return address), not the
  reporter's own frames. The minidump: every thread's stack and registers, the memory they point at, the modules
  (~0.5 MB).
- **How it can't hang or crash itself.** A thread made at start (1 MB stack) waits for a request; the failing thread
  hands over its context and waits (60 s at most for the files, then as long as the dialog is up, then 5 s for the
  log copy). DbgHelp is loaded and resolved at start; the report is built in fixed buffers and written with Win32
  files (no CRT heap, no FILE locks); each risky part (header, stack walk, module list, dump, log) in its own SEH
  `__try`. One report per process: a second thread failing meanwhile waits for the first to end the process; a
  failure in the shutdown after an error's report (the same thread) is left to Windows. DbgHelp is shared with
  `VR_DescribeCallers` under a lock (the reporter waits 2 s for it, then goes ahead).
- **Symbols.** `ironwail.pdb` already ships beside the exe (package allowlist); make_release.ps1 now also keeps the
  release's exe and .pdb in `out\release\<version>\symbols`, so a report's offsets or a .dmp read again offline.
- **Testing aid.** `vr_crash_test [av | thread | stack | error | threaderror | assert | zassert | abort | purecall |
  hosterror]` (replaces `vr_debug_crash`), Debug > Profiling and Memory > Crashes: one row per kind.

Checked headless, each kind (`vr_crash_test <kind>` after `map start`), every report symbolised with file and line:
`av` (main thread; `crashTestAccessViolation` shown `[inlined]` into `VR_CrashTest_f`), `thread` (a pool worker:
`JobOf<...>::execute` < `za::FixedFunction` < `za::Thread` < `threadStartRoutine`), `stack` (overflow, 1000 frames
collapsed), `error` (#00 `Sys_ReportError`, #01 the caller), `threaderror` (on the worker, exit 1), `assert`
(`reportAssert` < `VR_ReportAssert` < caller), `abort` (`raise` < `abort` < `za::abort` < caller), `purecall`,
`hosterror` (the console line, the game goes on). A normal e1m1 run: exit 0, no report.

**In the headset:** Debug > Profiling and Memory > Crashes > Crash: Access Violation: the dialog names
`quakevr\crash\<date>_<time>.txt` and the `.dmp`, and the .txt's stack names `VR_CrashTest_f`; Crash: Fatal Error: the
"Quake VR: Unleashed - Error" dialog has the same lines under the message.

## The Loading... crash, root cause (1.0.1, 2026-10-10)

1.0.0 shipped with `vr_loading_notice 0` because a campaign switch with the notice on ended in "Mod_PointInLeaf: bad
model" (his headset: Dimension of the Past's teleporter in vrstart; VR Hub from the main menu while in Scourge of
Armagon). e844de35 cleared `cl.worldmodel` after a switch and stopped putting the next load off, but the caller was
never found: headless runs didn't crash. With the crash report (above) it was found in one run.

**The exact caller.** Reproduced headless with e844de35's guard taken out, `vr_loading_notice 2` and **`-Sound`**
(`map vrstart; vr_campaign_select hipnotic`). The report's stack:

```
What: Sys_Error: Mod_PointInLeaf: bad model
  #00 Sys_ReportError+0xd5 (Quake\sys_sdl_win.c:1076) [ironwail.exe+0x1397b5]
  #01 Mod_PointInLeaf (Quake\gl_model.c:152) [inlined]
  #02 qvr::audio::outOfSolid::<lambda_0>::operator() (Quake\vr\vr_audio.cpp:1538) [inlined]
  #03 qvr::audio::outOfSolid+0x27e (Quake\vr\vr_audio.cpp:1541) [ironwail.exe+0x16881e]
  #04 VR_SndListener+0x1bd2 (Quake\vr\vr_audio.cpp:2437) [ironwail.exe+0x16e992]
  #05 S_Update+0x13e (Quake\snd_dma.c:871) [ironwail.exe+0x11df1e]
  #06 _Host_Frame+0x1894 (Quake\host.c:1564) [ironwail.exe+0x64c34]
```

**The sequence**, all in the switch's own frame:
1. `vr_campaign_select <c>` (the hub's teleporter inserts it from `changelevel start`, VR_CanChangeCampaignMap; the main
   menu's VR Hub runs `vr_campaign_hub`) -> `selectCampaign` -> `COM_ReloadVRGame` -> `COM_SwitchGameInternal`:
   `CL_Disconnect` (`S_StopAllSounds` clears every sound channel), `Host_ShutdownServer`, then `Mod_ResetAll` memsets every
   model slot. `cl` is not cleared by a disconnect: `cl.worldmodel` still points at slot 0, now empty (no nodes);
   `sv.worldmodel` too.
2. `selectCampaign` inserts `map <start>`; `Host_Map_f` -> `VR_LoadingDefer` puts it off (`vr_loading_wait`).
3. Later in the same `_Host_Frame`: `S_Update` -> `VR_SndListener` (spatial audio, Steam Audio, on by default in VR). Its
   voices still held the channels they played at the last mix (`L.channelOf`; released only in the next `VR_SndPaint`,
   which `S_Update` runs after the listener's update). For each, the sound's place (the cleared channel's: 0 0 0) goes
   through `outOfSolid`, which checks only `cl.worldmodel != NULL` -> `Mod_PointInLeaf(p, cl.worldmodel)` on the empty
   slot -> Sys_Error.

Without the notice the `map` ran in the same command-buffer pass: by `S_Update` a new world was in (CL_ParseServerInfo),
so no frame saw the empty slot. Headless never crashed because run.sh starts the game with `-nosound` (`S_Update`
returns at once) and the mock only defers with `vr_loading_notice 2`; his headset had both. Any campaign switch with a
sound playing (a level's static sounds: torches, hums; the teleporter's) did it.

**The audit.** Every `Mod_PointInLeaf` caller (44 sites) and every per-frame user of `cl.worldmodel`'s fields was traced
to its per-frame entry and its guard. World drawing (`VR_RenderView`, `VR_HeadlessView`, `V_RenderView` via
`con_forcedup`) needs `ca_connected`, `SIGNONS` and a world; the server's need `sv.active`; the client's parsing needs
`ca_connected`. The only per-frame users reached while disconnected are spatial audio's: `outOfSolid` from
`VR_SndListener` and from `VR_SndPaint`'s occlusion guess (with a hull trace on `cl.worldmodel->hulls` too), both
guarded only by `cl.worldmodel != NULL`. A sound on a channel (the wrist gadget's chime and taps, channel 1) started in
the frames before the next map would reach them even with every voice released: the real headset's way in that the
mock has no hands for. A handful of console commands also check only `cl.worldmodel` (`vr_snd_info`, the portal views'
dumps, `vr_rope` dump).

**The fix, at the source.**
- `CL_ForgetModels` (cl_main.c), from `COM_SwitchGameInternal` right after `Mod_ResetAll`: the client's state as at
  start-up, before any map: what a map's load does first (`CL_ClearState`) without freeing the hunk: `cl` wiped
  (`CL_FreeState`: the world model, the model precaches, the static entities, the view model, the stats), the lights,
  light styles, temp entities and beams cleared, every `cl_entities[].model` cleared. The server's world and models
  too (`sv.worldmodel`, `sv.models`). So after a switch `cl.worldmodel` is NULL, never a cleared slot, and every user's
  `cl.worldmodel` check means what it says. Frames before the next map are the start-up frames every player runs in
  the main menu.
- `VR_SndStopAll` (vr_audio.cpp) from `S_StopAllSounds`: the voices let go of the channels when they are cleared, not
  at the next mix (no voice ever on a cleared channel: the listener's update and the mix see the same channels).
- `VR_LoadingGameChanged` and its "no notice until a new world" are gone: a switch's map, the longest load, gets the
  notice again (`Loading...: "map e5start" put off ...` with `developer 1`).
- Each layer alone stops the headless crash (checked: either one switched off, the test passes; both off, it fails at
  the first switch with the stack above).

**`vr_loading_notice` 1 again** (config 123): only a 1.0.0 config (122) at 1.0.0's 0 takes it; an older config at 0
had it turned off by hand and keeps it (`Misc/quakevr/config123_test.sh`: PASS).

Tests: `Misc/quakevr/loading_switch_test.sh` (sound on, notice 2 held 0.5 s, rockets fired before each switch):
select (hipnotic, rogue, dopa, mg1, mg3, id1, hipnotic by `vr_campaign_select`, then `vr_campaign_hub`): 8/8 put off
and spawned; portals (the hub's teleporter to each of the six campaigns, `vr_activestartpaknameidx` 3, 4, 5, 1, 2, 0 and
`changelevel start`, and back by `changelevel vrstart`): 12/12; PASS. Before the fix: FAIL at the first switch. e1m1 ->
e1m2 with sound and the notice: put off, spawned, exit 0.

**In the headset:** Loading Notice is on again (the Debug page's row says Headset). From vrstart, the Dimension of the
Past teleporter: "Loading...", then e5start, no error. In Scourge of Armagon, main menu > VR Hub: "Loading...", then
vrstart. Any other campaign's teleporter and the way back to the hub the same.
## Patch 1.0.1: the tutorial's arena railing, lesson 7's pillar, the arena halls' lamps (2026-10-10, worktree `tutmap101`)

The author's tutorial notes; `vrtutorial_gen.py`, recompiled and relit with its `final` preset (the shipped build:
qbsp 0.18.1, 2.0's vis and light `-extra4 -dirt -bounce -lit -lux -lightgrid 64`; `_qvr_prelit 1` kept, so the
in-game relight passes it over).
- **The arena's railing** on the north mezzanine (on his right as he comes in) stood at y -520, 8 units out from the
  ledge's edge (-512), in the air: now at -510, its posts 0.5 in from the edge, on the ledge.
- **Lesson 7's board on a pillar** just inside the door (`R7_PILLAR`: x 5152-5280, y 736-768, floor to ceiling,
  structural, the room's wall bands on it; 160 in from the door, the ceiling lamps' row at y 800 clear of it), the board
  on its face to the door at eye height (LOW + 104; it was over the exit door at LOW + 176): he faces it as he steps in.
  256 clear on each side; the alcove's grunt comes at him north of it; the targets (west wall) are in sight from the
  door. The tips stay where they were.
- **Halls 12a and 12b** (the way to the arena) had no light at all (only the arena door's bounce: black): three ceiling
  lamps (200, as the rooms'), written last (`late()`, after the bullet time board): the other entities keep their numbers.
- The lump: the .bsp's entities == the generator's .map's (keys and order); against the 1.0.0 lump only lesson 7's
  board's origin and the three lights appended. bsp_holes 300k rays: 0 holes.
- `vrtutorial_playtest.py`: the fight's walk to the rifle goes round the pillar's east side; `--god` with `--from` past
  the fight now turns god on (it never did: "--from arena --god" ran mortal).
- Checked headless: loads clean; before/after shots (the railing's posts now on the ledge; the pillar's board filling
  the view from the door; halls 12a/12b lit). `--from fight --seed 1`: fight survived (74), the rifle shot the three
  targets, the door opened (the run then stops at room 8's hand loading, as before). `--from arena`, mortal (as every
  earlier "--god" arena run really was): seeds 1-6 cleared 4 (health 100, 100, 102, 55; seeds 2 and 3 died in wave 3);
  the 1.0.0 map the same way: 5 (85, 40, 66, 25 as the notes above, seed 2 stuck alive in the pit; seed 4 cleared, its
  walk out stopped at the pit's rail). With god really on, seeds 2 and 3 end in the pit on both maps (the mock's straight
  walks after a monster): the arena's outcome per seed moves with any edict change (the three lights), not the geometry.
## Patch 1.0.1: the nailgun's and thunderbolt's magazine wells inset, the thunderbolt's sides (2026-10-11, worktree `art101`)

His model notes: the nailgun's receiver under the gun "should have a little bit of an inset to look like there is a gap
where the magazine fits ... like the one on the super nailgun which clearly has an inset and a silvery border"; the same
for the thunderbolt's; and the thunderbolt's right side "has some stretched textures and it doesn't look symmetrical ...
a lot of grooves in the textures that should be inset in the geometry".

- **Wells under the gun** (make_mags.py `magwell`, the non-`sunk` ones: `vr_magwell_on_v_nail/v_lava/v_light/v_plasma`).
  Their mouth was a flat plate the magazine came through. Now a bright steel lip (its face the border, `steel` texels)
  round a pocket `MOUTH_GAP` 0.22 wider than the magazine all round, its floor dark (`spare` texels) `POCKET_DEPTH` 0.45
  up; the collar's walls round the pocket still end `TUCK` inside the lip (the z-fight fix), the floor's edges inside
  the walls, the lip's hole 0.06 inside the walls' faces: no two faces in one plane. Seats, load points, magazines and
  the super nailgun's well byte for byte as before (only those four files changed); normal maps rebaked.
- **Thunderbolt** (refine_light.py, polish_weapons.py's POST for v_light.mdl, refine_laserg.py's machinery). The body's
  sides: the +y (left) upper face was mapped well (anisotropy 1.06); on -y one triangle spanned it corner to corner with
  its texels folded to a line (39 over the face: the streaks), its triangulation another (a crack filled by a sliver);
  both lower faces were folded onto a few texel rows (19: the long dark lines). Now the +y lower face is unwrapped (LSCM)
  into free skin and painted in the body's dark browns (grain along the gun, lit edges); the +y upper face's three dark
  slots between the bolted plates are carved 0.4 in (Blender's exact boolean, headless, on a slab of that face: floors keep
  the slot's paint, walls its darkest texel); the -y side is that side mirrored across the body's middle plane on the same
  texels, its outline the old -y vertices. After: both sides 1.20 (upper, recess walls aside) and 1.05 (lower);
  check_mdl_art.py uv-stretch 12 -> 7, uv-density 10 -> 1, no normal or zfight finding. The old side triangles stay in
  the file, their vertices collapsed onto the two-handed grip's anchor vertex (230, the -y bottom front corner, kept in
  place): the strip order and so every anchor (57, 104, 230, magazine 655) unchanged; polish_weapons.py now checks the
  magazines' anchors too (vr_view.cpp magMounts). v_plasma.mdl (the same mesh) is not touched: adding it to POST is one
  line if he wants the same there. Normal map rebaked.
- blender/render_views.py draws several models joined by `+` (a gun with its magazine and well) for before/after renders.
- Checked: Blender close renders before/after (the wells with and without the magazine, both thunderbolt sides flat-lit
  and lit); mock-headset eye shots of the thunderbolt held side-on, both sides (the slots recessed, the bolts on the
  plates, the cell in its well); `reload/reload_test.sh` 103 PASS, 0 FAIL (self-test 67/67: the magazines out and in on
  the nailgun, super nailgun and thunderbolt); polish_weapons.py reproduces the shipped v_light.mdl byte for byte before
  the POST step and is deterministic after it.
## Load-time speedups without their drawbacks (1.0.1, 2026-10-10, worktree `load101`)

Your request: the optional load-time speedups' drawbacks ("Map and game loading", above) identified and removed, and
Setup's preparation to make the monsters' guns (PERF_DECISIONS.md 13). Each item: its drawback, the fix, the result.
Every change gives the same output (checked as noted). Timings: `vr_startup_times`, `run.sh --exclusive`, the mock,
fast mode; best of two warm runs, one cold run (the game folder's `quakevr/cache` moved aside); base 8744aed94 against
this branch (scripts `scratch/lp101*.sh`).

| load (ms) | before, cold | after, cold | before, warm | after, warm |
|---|---|---|---|---|
| start-up to the first frame (vrcalibration first) | 1902 | 1757 | 1364 | 1302 |
| `map vrcalibration` (every start) | 1251 | 1126 | 724 | 611 |
| `changelevel vrstart` (from the tutorial) | 4910 | 5135 | 540 | 516 |
| `map vrstart` again after e1m1 (a return to the hub) | 307 | 259 | 302 | 250 |
| `map e1m1` (first map of a session) | 1523 | 1426 | 914 | 780 |
| `changelevel e1m2` | 433 | 418 | 262 | 271 |
| `map e4m7` | 553 | 577 | 360 | 334 |
| `map hip1m1` (first map) | 1668 | 1799 | 956 | 865 |
| `map hip2m3` | 840 | 811 | 450 | 441 |

(Cold vrstart is its hull build, the same code; the cold rows move by 100-200 ms from run to run.)

1. **The monsters' guns in Setup's preparation** (PERF_DECISIONS.md 13). Drawback: a step more in Setup. Fix: the
   preparation's last map, the hub, drops the 16 weapons a monster can drop (`vr_pickup_test 4`, new: the grunt's and
   enforcer's guns, both knights' swords, the ogre's chainsaw, the random and ammo-box drops) with `CreateThrownWeapon`
   as a death does; a held gun's triangles are its dropped copy's, so holding one reads the same file. 13 cut in 1.0 s
   (`result guns read=0 cut=13 written=13`); a later session's grunt death reads its gun (0.2 ms, was 35), the 16
   dropped again read all, none cut. Debug > Tests: "Every Enemy Weapon Dropped Ahead".
2. **The skins' normal maps made in Setup's preparation** (was: "Normal maps made after the load", drawback flat
   shading for a moment). Instead of deferring them, the preparation precaches on the hub every model of the game's
   folders (progs/*.mdl: 102 the hub lacks) and every monster's limbs (162, as `vr_limbs_prebuild` makes a map's):
   2.4 s more in Setup, about 42 MB more in `cache/normalmaps` (50 MB). A first session's e1m1, e1m2, e2m2, e4m7 and
   hip2m3 then made no skin's normal map (before: 126 skins, 218 ms over those loads); `vr_normalmap_cache 2` on e1m2:
   139 compared, none differed. Mission packs' own models (another game folder) are still made at their first visit.
   Command `vr_prepare_models`; Debug > Tests: "Every Model and Limb Precached".
3. **The Box3D world mesh kept across map changes** (drawback: keyed by name and counts, an edited map of the same
   counts would keep a stale mesh). Fix: each kept mesh carries what `worldMesh` reads of its map (each face it makes:
   corner count, normal, the corners' vertex numbers and points; 5.3 MB for vrstart) and a map's mesh is reused only
   when those words are the same, compared whole (a hash first; a frame's check compares none after the first). The
   meshes of the two maps played before are kept (`vr_box3d_mesh_keep_maps 2`; 0 only this map's). Reading the words:
   2.8 ms of vrstart's load. A return to the hub: 302 to 250 ms. The same mesh object is used (hash c602335f).
4. **The memory log's GL object count** (drawback of counting less often: the leak check sees fewer loads). Fix: it
   counts at every load as before, but skips the names `glGen*` hands out (a few thousand per kind, one call, deleted at
   once): by the GL spec those are no object, so `glIs*` is asked only about the rest; the same count by
   construction. 13 to 5 ms of every load (programs have no `glGen*`: still asked one by one, 2.5 ms).
   `vr_memstats_glscan 2` compared both at 8 loads (e1m1, e1m2, vrstart, e4m7, hip1m1, r1m1, hip2m3): the same counts.
   `vr_memstats_glscan 0` is the old count. (A first try, stopping at a fresh name when names come in order, failed:
   NVIDIA reuses the lowest free name.) Debug > Profiling: "Memory Log: GL Count".
5. **Model loading in parallel** (drawback: the loaders share the hunk, the cache and GL). Not done whole: it needs each
   loader split into a decode into its own buffers on the pool and a commit in order on the main thread. Two exact
   pieces of the calibration room's 372 ms of alias models (warm) done instead:
   - authored normal maps' heights (the body's and hands' 1024 x 1024 maps: 60 ms on one thread) on the pool, a row a
     task, each texel's sum as before (75 maps compared byte for byte, 0 differed);
   - the image prefetch lists the first load's images that took over 0.2 ms (was 1 ms): 112 smaller skins and maps
     (50 ms decoded on the main thread at every start) decoded ahead; images 120 to 80 ms, VR init 10 ms more.
   Left: texture uploads (30 ms), CPU mipmaps (12), the decoded images' copies into the hunk (40: page faults),
   md5 replacements (32), islands (18), the cache's normal maps read (18).
6. **Shader program binaries** (drawback: driver bugs). Tried with every safeguard (keyed by GL_VENDOR, GL_RENDERER,
   GL_VERSION, GL_SHADING_LANGUAGE_VERSION, each program's sources and the build; LINK_STATUS checked, compiled on a
   refusal; a kill switch) and dropped: on NVIDIA the 106 programs from binaries took 191 ms against 192 compiled (the
   driver's own shader cache already serves the compile), and asking for retrievable binaries made the first start's
   link 25.9 s. Kept as a diff (`scratch/progcache_attempt.diff` in the worktree, not committed). Worth a try on AMD or
   Intel only (BACKLOG).

**In the headset:** a fresh install's Setup (the preparation's two new steps: about 3.5 s more; its log has `step guns`,
`result guns`, `step models`, `result models`); the first session after it: a grunt's, a knight's and an ogre's first
death (no hitch), the first visits to e1m1 and e1m2; a return to the hub from a map; nothing should look or play
differently.

## Patch 1.0.1: stretched skins re-mapped, normal maps rebaked (2026-10-11, worktree `blend101`)

The art pass's "For the author" items (BACKLOG, Art), approved: "Blender work: as suggested". Headless only, every
output from a script (polish_weapons.py, bake_normals.py).

**Stretched skins** (`Misc/quakevr/reuv_weapons.py`, run by polish_weapons.py as POST after the polish; the plasma
gun first through art101's refine_light.py, it has the thunderbolt's mesh). Seeds: faces over 0.5 sq units stretched
over 3:1 (or folded onto a line) over paint that is not one colour; each grows into its flat panel (faces within 20
degrees, stretched over 2:1 too). Panels are cut into charts by angle and unwrapped flat (reuv_shot2.lscm), packed in
free texels (the skin grows by 8 rows when needed). The paint is read from the old one: each new texel shows a surface
point, coloured as the old mapping showed it (the skin before polish_weapons.py's edge wear, bilinear with a
sharpened blend, never the background round an old island), quantized to the ramps the old paint used there with a
4x4 dither, plus fine grain, a few scratches and the convex edges lit and worn. A panel mostly folded onto a line with
its streaks across it (or named: the grappling hook's front cap) takes its old mean colour instead; streaks along a
part (id's shading of barrels and tubes) stay. The density: the model's (median of its well-mapped faces) or the old
one along the sharpest direction where higher, at most 3 times it (a fine pattern resampled coarser aliases).

**Anchors.** vr_anchor.cpp names anchors by their place in QuakeSpasm's old strip order, and strips join faces by
vertex index: a chart's edge across a strip (its corners copied) ended the strip and shifted every later anchor (the
first try moved anchors on all five guns). Charts are now joined along the old strips (taking the rest of each strip
they touch; a joined chart that will not unwrap flat, over 2:1, is left), and each kept only if every anchor names a
vertex at the same place in every pose. polish_weapons.py's check now compares an anchor's place in every pose (not
its vertex index: a copy is at its vertex's place, and vr_anchor.cpp reads only the place), and a magazine anchor past
the source model's vertices (v_nail/v_lava 1497/1499, which made a full run of the script fail) against the shipped
file. Every anchor of every gun held; the 14 guns not re-mapped come out byte for byte as shipped.

| `check_mdl_art.py --stretch` (area, frame 0) | over 3:1 | over 3:1 over varied texels | skin |
|---|---|---|---|
| plasma gun | 32.1% -> 15.6% | 29.6% -> 11.9% | 512x178 -> 512x290 |
| super nailgun | 42.5% -> 12.8% | 29.7% -> 1.2% | 512x146 -> 512x474 |
| rocket launcher | 33.5% -> 25.9% | 9.4% -> 3.2% | 512x146 (same) |
| grappling hook | 23.0% -> 10.7% | 20.8% -> 9.7% | 512x235 (same) |
| Mjolnir | 25.0% -> 0.0% | 25.0% -> 0.0% | 512x128 -> 512x240 |

"Over varied texels" (new in `--stretch`) leaves out faces over one colour (polish_weapons.py's swatches: bands,
bolt heads), which look the same however mapped. Left: the claws' and some plasma/rocket faces whose strips do not
unwrap flat; the plasma gun's coil turns under the muzzle (kept: their streaks are the turns). check_mdl_art.py's
other findings: streaks gone on the hook and Mjolnir, seam-ring bleed fewer in proportion (the new islands carry a
two-texel margin). The lava super nailgun and multi rocket launcher (same meshes, own skins): done later (worktree
`remap2`, below).

**Normal maps.** The baker keeps z >= 0.05 (`normalbake.clamp_z`, after the box filter: a sharp bevel's supersamples
averaged into the surface): the crowbar's 400 pixels with z < 0 -> 0 (630 pixels changed). Rebaked: the crowbar,
`v_shot.mdl` and its pump parts (11 pixels changed: it was nearly current), the super nailgun's and lava super
nailgun's magazine wells; `vr_shell.mdl`'s came out byte for byte (it was current); the nail/lava/thunderbolt/plasma
wells are art101's (rebaked there after its make_mags.py change). The five re-mapped guns' maps rebaked.

**Body skins at the wrist: not done.** The 16 body skins are the author's own repaint (commit 24fa2de6d, 59-98% of
each block differs from make_vrbody.py's painter): a re-map or a bigger skin would only resample his paint. A sharper
sleeve needs him to paint the wrist and bracer at a higher resolution (make_vrbody.py's painter is resolution-free and
could paint 512 x 512, but would replace his repaint).

**In the headset:** the super nailgun's barrels, the rocket launcher's front, the grappling hook's front cap,
Mjolnir's head and the plasma gun's sides up close: the paint as before, finer, no streaks; reload the super nailgun
and plasma gun (magazine anchors) and holster each.
## Decals on the world: a quarter cheaper, the same image (2026-10-10, worktree `decal101`)

BACKLOG "Optimise decals", PERF_DECISIONS.md 15. `play_e1m1_lights` at his eyes (2782, `--exclusive`), GPU ms:

| | world+brush | the marks' share | gpu 3D | grid build (CPU, a frame) |
|---|---|---|---|---|
| before (8744aed94) | 3.02 | 1.10 | 6.51 | 0.06 |
| after | 2.74 | 0.82 | 6.44 (6.26 in a second set) | 0.07 |
| no marks (`vr_decals 0`) | 1.93 | 0 | 5.59 | 0 |

(2 runs each, back to back; a first set: 3.09 -> 2.76.) CPU busy is too noisy in this scenario to show a change.

**Where the time went** (variants of the shader timed in the same scenario; sampled counters in the shader for a test
build: each world pixel walked 12 marks of its bucket, 8.6 passed the facing test, 1.6 lay inside a mark's rectangle,
1.4 drew one):
- the loop over a bucket (index, then the 80-byte record, then the tests) about 0.4 ms; the work past the early-outs
  the rest. Texture reads and their filtering cost almost nothing while the retro path's code was there (0.24 ms
  without it); at most one mark walked a pixel: 0.04 ms.
- the decals' retro textures' path (RetroBegin, RetroSample, the palette), skipped by a uniform branch when off, still
  cost 0.23 ms by its presence (the compiler's registers round the loop). Splitting the loop in two under that branch
  did not help; leaving the code out did.
- Tried, no gain: the loop unrolled by 2 (none) or 4 (0.5 ms worse: registers), the record's fields read lazily, the
  decals found before the world's texture reads (less kept live round the loop), the world's retro set saved only round
  a retro mark (kept: the code it leaves in the retro programs is smaller).

**What changed** (all the same image):
- `vr_decals.cpp` worldBuckets: a mark still counts in every cell of its box (and the bucket keeps the newest 64 of
  what it counted), but is listed only in the buckets of the cells reaching its rectangle at its full size (projected
  along its normal; the parallax's 12-unit reach is not needed there: the shader looks the cell up at the moved point
  itself). The count pass packs counted and listed in each bucket's header and flags the counted memberships for the
  fill. Fight: 10.5k grid entries of 18k counted; `vr_decal_count` prints both.
- `vr_glsl.h` DecalsAt: a pixel outside a mark's rectangle at its full size (it only spreads to it; with room for
  rounding) skips the age, spread and filtering. The world's retro set is saved and put back round a retro mark only.
- `gl_shaders.c`: the solid world's and its pdo programs are made with `NO_DECAL_RETRO 1` (no decal retro code);
  `GL_WorldDecalRetroProgram` makes the ones with it the first time the decals have a retro set (`vr_retro`), so the
  start compiles as many programs as before (233 ms warm; a first start after an update compiles everything anyway).
- `vr_decal_stress [count] [size] [splatter|pool|wall|run]` (Debug > Test Effects > Decal Stress ...): spreading pools,
  splatters and runs on the wall ahead, `rand` seeded so that they are laid the same way each time.

**Checked the same**: in one session, paused (`showpause 0; pause`), each view shot with the old path (every cell
listed, no early-out, the program with the retro code) and the new, twice each: start.bsp with 626 stressed marks
(splatters, pools, wall marks, runs; 31 buckets capped) and the rocket fight's 768 marks (26 capped): 0 pixels
different in the static views (max 0); where something still moved (a gib, a flame) old/new differed no more than
old/old. With retro textures on decals (`vr_retro 1`): the same (old and new use the retro program). HEAD's shader
against the new across sessions (retro on): the marks' pixels the same; only the fireplace's light differs, as it does
between two sessions of one build.

**Options left, each a visible change** (his call; PERF_DECISIONS.md 15): capping a bucket on what reaches its cells
(old marks show again where they pile up: 1.43 -> 1.67 marks drawn a pixel in the fight) with 16-unit cells, about 0.1
ms more but a 1.2 MB grid at each build (0.3 now); one atlas read a mark (up to 0.24 ms; blurrier or shimmering at
grazing angles); a lower `vr_decal_max`.

**In the headset**: a rocket fight's blood and scorch marks look as before (pools spreading, runs down walls, marks on
slopes and stairs); with Retro Textures on decals the first time (a short pause while its programs are made).

## AMD start-up crash: bindless off on AMD, GL safe mode, breadcrumbs (1.0.1, 2026-10-10, worktree `amdgl`)

The first player report on 1.0.0. The setup: an RX 7900 XTX on Windows 11, a Quest 3 over Steam Link, Adrenalin 26.8.1. The game crashed at start-up, in VR and on the monitor alike. The fault was an access violation in `atio6axx.dll` (AMD's GL driver), on one of the driver's own threads, before any map. The player's `-condebug` log stopped right after the extension list, so it died during `GL_CreateShaders`. With `-nobindless` the game started. Plain Ironwail's bindless runs on AMD, so the fault is on Quake VR's side of that path. That means the world shader's bindless additions:
- the normal and specular maps' handles (`in_nmsampler`);
- the samplers built from them, passed through functions into pixel-dependent loops: the parallax walk, the decals loop, and the retro textures and bump-map code in `vr_glsl.h` and `vr_retro.h`.

We have no AMD GPU, so the exact construct is not pinned down.

**Changes**
- **Vendor workarounds** (`vr_gl_workarounds`, default 1, archived, read before the window opens). On an AMD/ATI vendor or a Radeon renderer, bindless textures are off, as with `-nobindless`. The layered shadow casters stay on: they compiled on the player's GPU. Setting 0 gives every feature the driver offers, for testing a fix.
- **GL safe mode** (`vr_glsafe 1`, or `-glsafe`). It turns these off, each one logged:
  - persistent mapped buffers;
  - bindless textures;
  - multi-bind;
  - clip control (reversed Z, float depth);
  - layered shadow casters;
  - GL debug output;
  - MSAA;
  - the 8192 shadow atlas (4096 instead).

  Safe mode also turns on by itself after a start whose `gl_startup.log` has no end line, that is, a start that died before its first map was drawn. It says why, naming the last step. Once a start in automatic safe mode ends well, release builds stay in it; dev builds try the full renderer again next time. `vr_glsafe_retry` turns it off for the next start. The automatic mode is never used in test runs unless `-glsafeauto` is passed, and `-noglsafe` disables it. The existing per-feature switches still work for bisecting: `-nobufferstorage`, `-nobindless`, `-nomultibind`, `-noclipcontrol`, `-noviewportlayer`, `vid_fsaa 0`, `vr_shadow_atlas 4096`.
- **Breadcrumbs** (`vr_glsafe.cpp`, `VR_GLStep`). Each start-up GL step is named before its calls run:
  - the window and context, the vendor, the extensions, every shader compile and link (in `gl_shaders.c`, `vr_gfx_gl.cpp` and `vr_upscale.cpp`, including decal101's lazily built "world decal retro" programs);
  - the framebuffers and frame resources;
  - the `VR_TimeMark` stages;
  - the first three frames and the first map.

  They go to `quakevr/crash/gl_startup.log`, unbuffered, until 30 frames after the first map. They also go to `qconsole.log` with `-condebug`, and the last 24 are kept for the crash report.
- **Crash report** (`vr_crash.cpp`):
  - a `GPU:` line giving the vendor, renderer and GL version (which includes AMD's driver version), and the safe mode;
  - "Last GL steps", the last 12 breadcrumbs;
  - when another thread crashed (a driver's), the main thread's stack at that moment. Its context and stack are copied the moment the crash is caught (suspended only for the copy), so the report shows which GL call the game was in.
- **Debug > Crashes > Startup (GL safe mode)**: the two toggles, `vr_glsafe_status` (each feature, and the last steps) and `vr_glsafe_retry`. For tests, `-glfakevendor <vendor>` applies another vendor's workarounds.
- **Shader portability**: "alias depth", "alias depth layered" and "wound paint" now define `MODE 0`. They used `#if MODE == ...` with `MODE` undefined, which the GLSL spec makes an error; NVIDIA reads it as 0. The shadow atlas is also clamped to `GL_MAX_TEXTURE_SIZE`.

**Verified on an RTX 4090**
- A normal start logs about 400 steps and ends with "the first map drawn".
- `-glsafe` logs each feature off, and e1m1 renders.
- `-glfakevendor ATI` logs bindless off, and e1m1 renders.
- `vr_crash_test thread` before a map, with `-glsafeauto`: the report has the GPU line, the worker's stack, the main thread's stack (`Sys_Sleep < VR_CrashTest_f < Cbuf_Execute`) and the last steps. The next start is in automatic safe mode, names the last step, and ends well. The start after that (a dev build) is back to full mode.
- The menu path check reports 0 missing.
- On a cold driver shader cache, the BINDLESS 0 programs took about 26 s to compile on the 4090, once. An AMD player's first start after 1.0.1 may sit on a black window for a while; that is not a hang.

**For the player (1.0.0)**: start with `-nobindless`. In Steam: right-click > Properties > Launch Options. For `QuakeVR.bat`: add it after `ironwail.exe`. With 1.0.1 nothing is needed: bindless is off on AMD.

**What to ask for**
- The whole `quakevr\crash` folder: `gl_startup.log`, and any `<date>_<time>.txt` with its `.dmp`.
- `qconsole.log` from a start with `-condebug` (beside `ironwail.exe`).
- Whether shadows look right in a map with lights. The layered casters stay on for AMD.
- Optionally, to pin down the culprit for a real fix: one start with `+vr_gl_workarounds 0` (bindless back on) on 1.0.1, whose crash report then names the last program compiled and the main thread's GL call. Also tell them to reset Adrenalin's shader cache (Graphics > Advanced) if anything still crashes.

## VDXR swapchain crash: runtime crashes caught, every request logged (1.0.1, 2026-10-10, worktree `vdxr`)

**The report.** A player (Quest 3, Virtual Desktop, RTX 4060 Ti, driver 617.14) crashed at 1.0.0's first VR start: an
access violation reading 0x0 in `virtualdesktop-openxr.dll+0x9a668`, under the game's `createSwapchain` (line 768).
Later, with `+vr_xr_runtime 2`, SteamVR 2.17.10 stopped at the same point in his log. His log had `d3d11.dll` and
`dxgi.dll` loaded before OpenXR started; the author's has neither.

**What the crash was, from the binaries.** The author's VDXR is the player's exact build: 1.1.0, VD Streamer 1.34.23,
linked 2026-09-07. In it, `+0x5689a` (the frame above) comes right after VDXR's virtual `xrEnumerateSwapchainImages`
call. Read with dumpbin:
- The loader frame is `xrEnumerateSwapchainImages`, not `xrCreateSwapchain`: `+0x86f9a` is in that function once the
  export's jump thunk is followed. Line 768 is the return address after the game's second call, the one that fills the
  image list.
- VDXR's OpenGL image function (`+0x9a310`) assumes a swapchain is set up whenever its list of D3D11 images is not
  empty, and copies `glImages[i]` from it. The D3D11 images are fetched, and added to that list one by one, before the
  image structs' type is checked. So a call that failed the type check (`XR_ERROR_VALIDATION_FAILURE`), or that threw
  part way, leaves the D3D11 list full and the GL list empty. The game's own fill call then reads element 0 of the empty
  GL list: the null read.
- The game's own calls can't cause this. Per swapchain it makes one count call, which returns before any fetch, and one
  fill call. Another caller must have enumerated first: most likely an implicit API layer or a hook that enumerates the
  game's swapchain as D3D11 or Vulkan images. That would also explain SteamVR failing. Not proven: the next logs list
  the layers and DLLs.

**Changes** (`vr_backend_openxr.cpp`, `vr_xr_runtime.cpp`, `vr_crash.cpp`):
- **Crashes inside the runtime are caught** (`vr_xr_guard 1`, Debug > Crashes > Runtime Crash Guard).
  - Guarded calls, each under a structured exception handler: `xrCreateSession`, `xrCreateSwapchain`, both
    `xrEnumerateSwapchainImages` calls, and the destroy calls.
  - On a crash: it is logged with its module and offset, the runtime is marked as crashed, and that VR start fails. Auto
    then tries the next runtime, else the game plays flat, and the console says what to change.
  - The eye swapchains get one more try in plain `GL_RGBA8`. This is logged, with a warning that the colours look paler.
  - VDXR's lock is a scoped lock in a frame that can throw, so it is released when the stack unwinds. If the same thread
    locked it again it would not hang (MSVC's `_Mtx_lock`).
- **Every request is logged before its call**, as a breadcrumb (`VR_GLStep`: the crash report's "Last GL steps", and
  `gl_startup.log`) and in `qvr_openxr.txt`:
  - `xrCreateInstance`, `xrGetSystem`, `xrCreateSession` (with hDC and hGLRC), `xrEnumerateSwapchainFormats`;
  - each swapchain's request (left eye, right eye, panel): size, format, usage, samples, faces, array, mips;
  - both enumerate calls.
- **Validation**, each failure clean and named:
  - the format must be one the runtime offered, and the size within the system's largest;
  - the enumerate results are checked: 1 to 64 images, none of them texture 0.
- **The GL context** (the coordinator's lead), checked before `xrCreateSession` and before each `xrCreateSwapchain` and
  fill call:
  - the context and DC bound to the session must be the ones current on this thread, the DC's window must still exist,
    and SDL must agree;
  - both handles and the thread are logged;
  - if the context is not current it is made current again (logged); if that fails, the call is not made.
  - Test: `vr_xr_test_drop_context 1`, also in Debug > Crashes.
- **The crash report has a `VR:` line**: the runtime and its version, the attempt, the implicit API layers (on or off),
  the known overlays' DLLs, the headset, and the last OpenXR step.
- **`vr_xr_runtime_explain` and each start's log** list:
  - every implicit API layer, from both HKLM and HKCU `...\ApiLayers\Implicit`, with its library and its state
    (registry, `disable_environment`, `enable_environment`);
  - every other program's DLL in the process, naming the known ones: RTSS, Steam, Discord, OBS, ReShade, NVIDIA,
    Overwolf, VD's injector, and proxies of system DLLs next to the exe;
  - the GL interop entry points: `GL_EXT_memory_object_win32` (which VDXR's code uses) and `WGL_NV_DX_interop2`.
- **`vr_xr_api_layers 0`** (Debug > Crashes > OpenXR API Layers): from the next VR start, every implicit layer is turned
  off by its own `disable_environment` variable. Verified on the author's PC: OBS's mirror layer DLL is then no longer
  loaded.
- **When SteamVR doesn't find the headset** within `vr_xr_steamvr_wait`, the log now says to start SteamVR first or
  raise the wait. A SteamVR that the game's start launched can take longer than 5 s; Auto then went on to VDXR, which
  may be why the player's first try of SteamVR gave "the same result".

**Tests**: `Misc/quakevr/xr_runtime_test.sh` part 7, 69 checks, all pass. The fake runtime gets two switches,
`FAKEXR_FAULT_IMAGES` and `FAKEXR_FAULT_SRGB`, that make its image enumeration read a null pointer as VDXR's does.
- Only the sRGB swapchains crash: caught, the GL_RGBA8 retry works, and VR runs.
- Every swapchain crashes: VDXR's fake is destroyed and SteamVR's fake starts. A later `vr_crash_test av` report has the
  `VR:` line.
- No context current: it is made current again, and the runtime always sees it current.

Part 4's check of the timing line's order was flaky on a fast machine (the fake headset's frames aren't paced). It is
now a check of its own.

**What to ask the player**
- On 1.0.1 or a test build: `quakevr\qvr_openxr.txt` and the whole `quakevr\crash` folder (`gl_startup.log` and the
  report). They show the layers (`implicit OpenXR API layers`), the other DLLs, the context lines before each swapchain,
  and the step that died.
- His VD version (Streamer > About) and SteamVR version.
- Whether OpenXR Toolkit, OBS's OpenXR mirror, fpsVR, OpenKneeboard, ReShade or another XR overlay is installed.
- How he starts the game (Steam, with its overlay?), and whether RTSS/Afterburner, Discord or the NVIDIA overlay runs.
- **Workarounds**, to try in order:
  1. `+vr_xr_api_layers 0`.
  2. Close the overlays, or turn off Steam's in-game overlay.
  3. Start SteamVR from VD's menu, then the game with `+vr_xr_runtime 2`: SteamVR only, no fallback. (`vr_xr_runtime`:
     0 the system's active runtime, 1 VDXR, 2 SteamVR, 3 a manifest, 4 Auto.)

## Diagnostics mode (1.0.1, 2026-10-10, worktree `vdxr`)

The author asked for a single switch that collects everything a crash report needs. It is `-diagnostics`, also
`vr_diagnostics 1` (from the next start; Debug > Crashes > Diagnostics Mode), and the installer's new Start menu
shortcut **Quake VR Unleashed (diagnostics)**. Everything goes into `quakevr/diagnostics/<date>_<time>/`, which is
printed at the start and opened in Explorer at a quit (`Quake/vr/vr_diagnostics.cpp`):
- `console.log`: the console. With `-diagnostics` it starts at the first line after the filesystem
  (`LOG_Reopen`); with the cvar it starts at the window's creation.
- `gl_debug.log`: GL debug output.
  - The GL context is created with `SDL_GL_CONTEXT_DEBUG_FLAG`, and debug output is synchronous, as with
    `-gldebug`. GL safe mode still turns it off.
  - Each message is logged with its source, type, id and severity: an id's first 10, then every 100th, and a
    summary by id at the end.
  - The game sends one message of its own, so the file always shows that the output works. The driver's
    notifications go only to this file, not to stdout.
- `openxr_debug.log`: the `XR_EXT_debug_utils` messenger, with all severities. It holds both the loader's messages
  (`OpenXR-Loader ... Completed loader trampoline`) and the runtime's, plus the backend's steps and the API layers
  the loader lists.
  - `vr_diagnostics_xr 2` also chains the messenger to `xrCreateInstance`, to get the loader's messages from the
    instance's creation. This is not the default: with a runtime that doesn't offer the extension, loader 1.1.63
    fails that `xrCreateInstance` (-1). The next one then gets `XR_ERROR_LIMIT_REACHED`, so VR is off for that run.
  - Mode 1 (the default) creates the messenger after the instance exists, so a runtime without the extension costs
    nothing.
  - The message-type bit `XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT` is left out.
- `debug_output.log`: everything sent to a debugger with OutputDebugString. A vectored handler catches the exception
  that OutputDebugString raises, so this works without a debugger.
  - The game's own `Sys_Printf` lines are left out, and so is the narrow copy that Windows makes of each wide line.
  - On the author's PC this catches the OBS mirror layer's log ("XR_APILAYER_NOVENDOR_OBSMirror layer ... is
    active"): API layers and runtimes often log there.
  - `XR_LOADER_DEBUG=all` is set, but this loader reads it when it loads with the game, which is before that. Its
    messages come through the messenger instead.
- `crash/`: the crash report and dump (`VR_SetCrashDir`).
- `diagnostics.txt`: the build and the command line, then at the end how the run ended and which logs were copied.
  At a quit and in the crash handler it also lists these copies:
  - `gl_startup.log`;
  - `qvr_openxr.txt`;
  - SteamVR's `vrserver.txt`, `vrcompositor.txt`, `vrmonitor.txt`, `vrclient_ironwail.txt` and `vrstartup.txt`,
    found through the registry's `SteamPath`;
  - Virtual Desktop's `%ProgramData%\Virtual Desktop\OpenXR.log` (VDXR's, found by the file name inside its DLL and on
    the author's PC) and `StreamerLog.txt`.

  Only logs written during the run are copied. Meta's are not looked for.

**Off** (the default): no folder is made and nothing runs. The only cost is one branch in the GL debug callback,
which is installed only with `-gldebug`. Checked: a run without the flag adds no folder.

**Khronos' core validation layer** now ships (below, "OpenXR core validation layer").

**Tests**: `xr_runtime_test.sh` part 8 (75 checks in all, all pass). The fake runtime offers `XR_EXT_debug_utils` with
`FAKEXR_DEBUG_UTILS`; its messenger sends 3 messages. The installer's self-tests pass 37/37: the shortcut's arguments
and its place, and the shortcut counts after install, update and uninstall. The installer's screenshot harness was not
run, because it opens a window.

## Scoping: hide the armbands, seamless forearm-to-wrist (worktree `armscope`, 2026-10-11; no code)

Backlog: "Option to hide the armbands, with the forearm and the wrist/hand joining seamlessly without them." Read from
the generators and the engine; nothing was run or changed.

**What the "armbands" are.** There are two candidates, and the first is almost certainly the one meant:
1. **The bracers**: the dark leather cuff on each forearm of the body (`vrbody*.md5mesh`, `Misc/quakevr/make_vrbody.py`
   loft `"bracer"`). Five rings: 0.13, 0.19, 0.225 m from the elbow, the wrist (0.26 m), and a 7 mm flared lip over the
   base of the hand. Part of the one body mesh (`numMeshes 1`, one skin per build and damage state, the bracer's skin
   block `(0.75, 0.5)-(1, 1)`: buckled straps and rims, hand-repainted by the author in 24fa2de6d). Not a separate model.
2. **The wrist gadget's straps** (`vrgadget_strap.mdl`, `vr_view.cpp` ~6400): two olive bands drawn 1.5 mm outside the
   bracer, fitted to its ring (`avatar::forearmGirth` mirrors the bracer's ring numbers). They go with the gadget,
   not with the body option.

**How it is drawn and attached.** `avatar::pose` (`vr_avatar.cpp`) skins the body; the forearm has `foretwist1..4`
helper joints at the rings (twist spread by `vr_body_forearm_twist`), `wrist_<side>` (all the roll, half the bend,
mitre-stretched) and `hand_<side>` (the whole bend; carries only the bracer's lip). The hand is a **separate model**
(`hand_rig.mdl` with `vr_hand_rig`, else `hand_base.mdl` plus fingers), its wrist end deliberately buried inside the
cuff: `taper_hand.py` shrinks `hand_base`'s end to 0.55; `make_hand_rig.py`'s first palm ring is at x -4.40 "inside
the arm's cuff". So the bracer's lip is what hides the hand/arm joint, and its job is explicit in the generator's
comments: so "a bent wrist neither thins nor opens a gap".

**What hiding the bracer exposes** (the answer to "is the band hiding a seam": yes, and more than a seam):
- The forearm **skin loft stops at 0.15 m of 0.26 m** ("ends inside the bracer, 2 cm into it"). Remove only the bracer
  and the lower 11 cm of the forearm is a hole: you see into the body's inside and the hand's buried end floating.
- The skin loft is thicker at its end (0.046 x 0.040 * fm) than the hand's wrist; the wrist ring (0.036 x 0.027) is
  sized to hold the hand with a few mm to spare, and the hand's end swings out as the wrist bends.
- Different skins: the body's 256 x 256 skin (the author's paint; the wrist's texel density is about a sixth of the
  hands') against the hand's own skin. A visible colour or detail step at the join is likely even if geometry is perfect.
- Different joint set: the body's wrist joint (half the bend) vs the hand rig's own bone: where the hand rolls/bends the
  seam opens a crack unless one overlaps the other.
- Other things measured from the bracer: `vr_selfcollide.cpp` (arm capsules use the bracer's ring means), the gadget
  strap fit (`forearmGirth`), `vr_bodyblood`/the blood paint block "bracer", `MODELS_IN_BLENDER.md` notes. Only the
  gadget strap is a hard dependency: the straps must either hide too or keep a ring to sit on (a bare forearm of the
  same radius is fine, the code only needs `forearmGirth`).

**Options.**
- **(a) Toggle only the bracer off (hide its triangles).** Needs the bracer's triangles separable: they are in the same
  mesh, so either a second md5 mesh/index range, or two body files. Cheapest in code, but as above it shows a hole and
  the buried hand end. Unusable on its own. Effort 0.5 day. Not recommended.
- **(b) A "bare forearm" body variant: extend the skin loft to the wrist and give it its own wrist ring, hide the
  bracer** (recommended). In `make_vrbody.py`: when the bracer is off, continue the `skin` loft from 0.15 m through
  0.19 / 0.225 / the wrist rings (reuse the bracer's ring sizes and weights, so the twist/wrist joints behave the same)
  ending at the wrist ring; texture it from the `skin` block (so it is the arm colour, tiling matches) with a small
  lip of skin over the hand's end, or let the hand's tapered end sit 2 cm inside the loft exactly as it sits inside the
  bracer now. Ship as **one extra mesh in each build's md5mesh** (both versions: bracer mesh, bare tube) and have the
  avatar draw one of them by a setting (`vr_body_bracers`, default 1, Body page row), or
  generate `vrbody*_bare.md5mesh` (6 more files: 3 builds, `usable()`/name logic in `vr_view.cpp` ~6547 and ~832).
  Engine work is small if the md5 loader can drop a mesh (verify: it reads `numMeshes 1` only today, so a second mesh
  needs a loader change or the two-file route). The hand needs no change: it already ends inside the cuff region.
  Risks: the hand and the forearm skins differ, so an obvious colour step at the wrist unless the hands' skin tone is
  matched (the author paints both: ask him which colour wins) and the author's hand-painted body skins keep working
  (the bare tube uses the existing `skin` block: no repaint needed, but the 24fa2de6d paint only exists in the 16 TGAs,
  so no regeneration of the skins: `make_vrbody.py` is genguard-protected, the loft change must not rewrite the TGAs).
  Effort **1.5-2 days** (generator + loader + setting/menu/calibration check + mock screenshots of both builds, bent
  wrist, rolled wrist, gadget on).
- **(c) A hand model with a built-in forearm** (hand rig extended 10-15 cm up the arm in `make_hand_rig.py`, skin-matched
  to it, bracer and the body forearm cut short). Gives the most control of the join (one mesh, one skin, one set of
  bones) but duplicates the forearm, breaks the twist helper joints' purpose, and touches hands in every mode (Only
  Hands has no forearm at all today), the gadget strap fit, the blood paint, self-collision. 5+ days, high risk.

**Interaction with the wrist gadget.** It is placed on the forearm by the frame at `along`; it only reads
`forearmGirth`, so a bare tube with the same ring sizes keeps it exact. The straps still sit 1.5 mm off the surface;
with the bracer hidden they would float over bare skin as straps do over a sleeve (fine). A toggle that hides the
straps too ("armbands" in the broad sense) is a one-line `visible = false` in the strap loop.

**Recommendation: (b)**, with the strap hide folded into the same setting (`vr_body_bracers` 0 hides both); default
unchanged (bracers on). About 2 days; the one human decision is the wrist's colour match between the body's bare arm
skin and the hand's skin. For VR testing: roll and bend the wrist fully with the setting off, look for a gap at the
wrist and the colour step, and check it for all three builds.

## Bare forearms: the bracers off, seamless wrists (feature, worktree `armbands`, 2026-10-11; not for 1.0.1)

The scoping note's option (b), approved. `vr_body_bracers` (default 1, as before; Body page: Bracers) 0 draws bare
forearms and hides the wrist gadget's straps.

**Models.** `make_vrbody.py` writes three more bodies, `vrbody_bare`, `vrbody_lean_bare`, `vrbody_brawny_bare` (.md5mesh,
.md5anim, placeholder .mdl), the same skeleton and skins. The bracer bodies come out byte-identical; the 16 hand-painted
TGAs are not written (`--keep-edited`). In the bare ones the arm's skin loft goes on from 0.15 m: rings at 0.19 and
0.225 m (the twist joints') easing (smoothstep) from the 0.15 m ring to the hand's wrist, then a wrist ring (the wrist
joint: all the roll, half the bend, the mitre) and a lip 1.1 hand units (1.4 cm) up the heel of the hand (the hand bone,
rigid with it), both **fitted round the jointed hand**: `hand_rig.md5mesh` (read only) sliced at those places along the
ring's 12 rays, 0.6 mm off its skin, pushed out wherever the skin would bulge past a side of the 12-gon between two rays.
The lip's length clears a 67 degree bend (shorter, the mitred wrist ring overtook it on the outer side and the tube folded
into a flap). The end is open (the hand fills it), so its last ring's normals are the skin's, not a cap's. The texture
is the arm's own block down to the wrist (the author painted it there under the bracer). The 1.0.1 `hand_rig` was edited
in Blender: after editing the hand's wrist, run `make_vrbody.py --keep-edited` again (MODELS_IN_BLENDER.md).

**Engine.** `vr_view.cpp` picks `progs/vrbody*_bare.mdl` when the setting is off and both hands are the jointed hand
(`vr_hand_rig 0`'s old hand keeps the bracers: the ends are fitted to the jointed one), else the bracer body as before.
- The fit follows the hand as drawn: the hand's size against the body changes with the height calibration (0.0102 m a
  hand unit at 1.95 m, 0.01286 at the default, 0.0137 at 1.45 m), so `avatar::solveArm` scales the hand bone and the
  wrist ring across by the drawn hand's size over `handUnit` (0.01286, make_vrbody.py's HAND_UNIT).
- **Colour.** The arm's skin at the wrist (vrbody_00_00.tga) is 1.4x lighter than the hand's (hand_rig_00_00.lmp) round
  its wrist. The alias shader tints the arms' skin block's rows from 0.19 m to the wrist ring towards the hand's
  (`SkinTint`, `SkinTintRange` in the instance data; times 0.753 0.692 0.653, make_vrbody.py prints it as "wrist tint";
  multiplied, so blood and wounds keep their colour). Painted blood and the dynamic wounds paint the bare forearm (one
  mask a side, as before).
- **Light.** Each model's light, its direction and the light round it (vr_ambient's cube) are sampled at its own place,
  the body's round the torso and a hand's round the hand: after the tint the seam still stepped 21% in some poses (the
  ambient cube; with `vr_model_ambient_dir 0` it matched). With bare forearms the jointed hands are drawn in the body's
  light (`view::bareLight`, `view::bareShade`; the body's kept each frame): lip 80 vs hand 82 at the seam where it was
  80 vs 97. With the bracers the hands are lit as before.
- **The hand's end inside the arm** (behind the wrist: the palm's first ring and the thumb's base) came out through the
  closely fitted forearm in a side bend (a slit of the hand's skin). For the jointed hand on a bare forearm the alias
  vertex shader folds its bind positions behind the wrist (0.2 hand units back) onto the wrist's middle line
  (`QVR_ALIAS_VS_HANDFOLD`): a cone inside the arm.
- The gadget rests on the skin: lowered by how much thinner than the bracer the arm is there (`avatar::forearmGirth`'s
  `bare` rings, make_vrbody.py's bare_rings and "bare wrist" numbers); the straps are hidden.
- `vr_body_wrist_info` (Debug > Hands and Weapons > Wrists): per hand the body drawn, the hand's size against it, the
  arm's wrist off the hand's, the wrist's place from the controller, the forearm's direction, the wrist's bend; the lights.

**Checks** (headless mock, `vr_gunangle 0`, `vr_body_wrist_limits 0` so the wrist really bends, `r_drawworld 0`): each
wrist from its outer side, from above and from below, both hands, three builds, in 10 poses (neutral, roll +-90, bends
measured against the forearm 37, 61, 75, 89, 91 degrees, sideways 34 and 36): 180 shots, 0 holes (background pixels
enclosed by skin; the check finds a 12-pixel slit painted in as a test). Taller and shorter calibrations (1.95, 1.45 m):
48 shots, 0 holes. Z-fighting: the camera moved 0.3 mm, the frames' worst 16-pixel blocks differ by 8.6 (bracers: 16.3),
all at silhouette edges, none on the seam. Straps: 2 with the bracers, 0 bare, 2 in Only Hands (no body, unchanged);
`vr_hand_rig 0` draws the bracer body. Blood (`vr_gore_spatter_test arm`) paints the bare forearms to the wrist.

**To test in VR:** Body > Bracers off; look at both wrists close up, roll them fully, bend them up, down and sideways,
grip and let go (the thumb's base is near the lip); the wrist gadget on its arm; the three builds; a lamp near one hand
(the hands are now lit as the body when bare). Decide whether the hand's colour should win (now) or the arm's.
Known: past about 70 degrees of bend the wrist pinches (as with the bracers); the self-collision capsules still use the
bracer's girth (a few mm fuller); the hands' copies in a mirror keep their own light.
## Patch 1.0.1: the lava super nailgun and multi rocket launcher re-mapped (2026-10-10, worktree `remap2`)

The `blend101` section's "not done", approved: `v_lava2.mdl` and `v_multi2.mdl` (the super nailgun's and rocket
launcher's meshes, their own skins) added to `reuv_weapons.py`'s SPECS with no overrides (`{}`, as their siblings),
then `polish_weapons.py v_lava2.mdl v_multi2.mdl` and `bake_normals.py v_lava2.mdl v_multi2.mdl` (Python with numpy;
no Blender step: reuv_weapons.py and the baker run in Python). Checks: polish_weapons.py's anchor check held (lava
super nailgun 28, 148, 254 (its magazine), 652; multi rocket launcher 3, 12, 17: each at the same place in every pose);
`v_nail2.mdl` and `v_rock2.mdl` polished again into a scratch folder came out byte for byte as shipped, and their normal
maps rebaked unchanged (the pipeline is reproducible).

| `check_mdl_art.py --stretch` (area, frame 0) | over 3:1 | over 3:1 over varied texels | skin |
|---|---|---|---|
| lava super nailgun | 40.8% -> 13.3% | 27.0% -> 1.6% | 512x146 -> 512x498 |
| multi rocket launcher | 33.6% -> 8.4% | 15.4% -> 3.3% | 512x146 -> 512x162 |

(Their siblings, for comparison: super nailgun 12.8% / 1.2%, rocket launcher 25.9% / 3.2%.) As there, the faces whose
strips do not unwrap flat are left (5 charts, 75 faces on the lava gun; 1 chart, 12 faces on the multi launcher).
check_mdl_art.py's other findings: the multi launcher's triangles over 4:1 22 -> 2; mirrored-overlap texels fewer on
both; the lava gun's normal map has 9 steep seam-ring texels (the super nailgun's has 12).

**Smoke** (mock, e1m1): the lava super nailgun in the off hand on lava nails (`vr_reload_test 16`), its magazine out by
B/Y (a spent lava nail magazine, smoking), a new one from the pouch seated (36), the gun back to the main hand, dropped
(lying: its magazine's seat 0.00 units off the gun's), taken again: `v_lava2.mdl`, its magazine and well drawn with it
throughout. The rocket launcher in the off hand switched to multi-rockets by its ammo button (`v_multi2.mdl` drawn),
emptied, a multi-rocket from the pouch loaded at the muzzle (1 of 4), dropped and taken again.

**In the headset:** both guns up close (the lava gun's barrels and body, the multi launcher's front): the paint as
before, finer, no streaks; reload the lava super nailgun (magazine anchor) and holster both.
## OpenXR core validation layer in diagnostics mode (1.0.1, 2026-10-10, worktree `xrval`)

Diagnostics mode now loads Khronos' core validation API layer (`XR_APILAYER_LUNARG_core_validation`). The layer checks
every OpenXR call the game makes against the specification: handles, structure types, `next` chains, enum values,
and call order. It writes what it finds to `openxr_validation.log` in the diagnostics folder.

**The files** are in `quakevr/xr_layers/`: `XrApiLayer_core_validation.dll`, its manifest `XrApiLayer_core_validation.json`,
`LICENSE.txt` (Apache-2.0) and `NOTICE.txt` (where the source came from, the build options and the copyright lines).
Git tracks them, so `package-quakevr.ps1` ships them through its game-folder allowlist with no change to the script,
and the zip check in `make_release.ps1` compares the zip against the same list. Credited in `CREDITS.md`.
- **Source**: OpenXR-SDK-Source tag `release-1.1.63`, the version of the shipped loader (`openxr_loader.dll` 1.1.63.0
  and `openxr.h` agree). Built unchanged. The SDK source was downloaded into `scratch/` and is not committed.
- **Build**: VS 2022 CMake, MSVC 19.44, x64 Release. Only the layer target is built (no loader, tests or samples).
  It uses the static CRT, so its only import is `KERNEL32.dll`. It is built with `-O1 -Gy` and `-OPT:REF -OPT:ICF`:
  4.77 MB, against 6.39 MB at CMake's default `-O2`. The exact command is in `NOTICE.txt`.
- **Size**: the folder is 4.78 MB on disk and about 1.06 MB in the zip.

**How it is loaded** (`vr_diagnostics.cpp`, `useValidationLayer`, called from `VR_DiagnosticsBegin`):
- **Finding it**: the first game folder in the search path (not a pak) that has
  `xr_layers/XrApiLayer_core_validation.json`.
- **XR_API_LAYER_PATH**: that folder goes first in `XR_API_LAYER_PATH`, and a value the player had already set is
  kept after it. The variable is set through `setEnv`, in all three environments. Loader 1.1.63 reads it with
  `GetEnvironmentVariableW` each time it enumerates layers and at `xrCreateInstance`
  (`ApiLayerManifestFile::FindManifestFiles`). It is not read when the loader DLL loads, so setting it at diagnostics'
  start, before any VR start, is early enough.
  - While the override is set, the loader doesn't read the registry's explicit layers. Implicit layers are not
    affected.
- **The layer's own variables**: `XR_CORE_VALIDATION_EXPORT_TYPE=text` and
  `XR_CORE_VALIDATION_FILE_NAME=<folder>/openxr_validation.log`. The layer reads these at `xrCreateApiLayerInstance`
  and appends each message to the file, opening and closing it every time, so the file survives a crash.
- **The log's header**: the game writes two header lines first. One of them names the layer's known false positive
  (below).
- **Enabling it**: `vr_backend_openxr.cpp` enables the layer by name in `enabledApiLayerNames`, only when the loader
  lists it and `vr_diagnostics_xr_validation` is 1. If `xrCreateInstance` fails with the layer, the existing fallback
  retries without the layer and the messenger.
- **Outside diagnostics mode** no variable is set and no layer is requested, so the loader never loads the DLL.

**New cvar**: `vr_diagnostics_xr_validation` (default 1, not archived). At 0 the layer is not loaded in a diagnostics
run, from the next VR start. It is for a runtime that behaves differently with the layer: the layer refuses calls it
finds invalid instead of passing them down. It is in Debug > Crashes > Diagnostics as "OpenXR Validation Layer".
`openxr_debug.log`'s layers line says ON, "not found" or "off (vr_diagnostics_xr_validation 0)".

**Known false positive** (a bug in layer 1.1.63, not in the game): at each `xrDestroyInstance` the layer logs
`VALID_ERROR VUID-xrDestroyDebugUtilsMessengerEXT-messenger-parameter` for the debug messenger.
- What happens: the game's own `xrDestroyDebugUtilsMessengerEXT` passes the layer correctly. But the layer erases the
  handle's record (`GenValidUsageNextXrDestroyDebugUtilsMessengerEXT`) before it looks that record up to drop the
  messenger from its instance's list (`CoreValidationXrDestroyDebugUtilsMessengerEXT`). The messenger stays on the
  list, and the layer destroys it a second time at `xrDestroyInstance`.
- The second destroy is refused by the layer itself and never reaches the runtime.

**Tests** (fake runtime with a headset, `FAKEXR_HEADSET=fakexr_steam`, `-diagnostics`; `scratch/xrval_test.sh`):
- **Diagnostics run**:
  - `openxr_debug.log` says "core validation: ON".
  - `diagnostics.txt` has the `XR_API_LAYER_PATH` line.
  - The crash report's module list has `quakevr\xr_layers\XrApiLayer_core_validation.dll`.
  - `openxr_validation.log` has the header and "Core Validation Layer is initialized".
  - VR started and ran with the layer.
  - A `vr_restart` and a quit give only the false positive above, once for each instance.
- **Diagnostics run with `vr_diagnostics_xr_validation 0`**: "off", and the DLL is not in the module list.
- **Normal run**: the crash report's module list has `openxr_loader.dll` and the fake runtime, but not the layer.
- `xr_runtime_test.sh` part 8 now checks the layer too (4 checks; 79 in all, all pass; TESTING.md).

**To test with a headset**: start with the Diagnostics shortcut, play a minute, and quit. Then open
`quakevr\diagnostics\<date>_<time>\openxr_validation.log`. Anything other than the header, the initialize/destroy
lines and the known false positive is a real finding about the game's OpenXR calls on that runtime.
## Patch 1.0.1: the arena's mezzanine ladder tops out through the railing (2026-10-10, worktree `ladder`)

The north mezzanine's ladder (x 704-752, its rungs on the ledge's face at y -519) came up under the railing that
`tutmap101` moved onto the ledge (y -510): the climb's top step met the rails, and he had to climb over them. The author
approved a gap. `vrtutorial_gen.py`: the railing is two runs, x 384-688 (posts every 76: whole units) and 768-1408
(every 64, as before), each with end posts; the gap (`MEZZ_N_GAP`, 688-768) is the ladder plus 16 each side. The rest
of the railing (fall protection along the whole ledge) is unchanged.
- Compiled with `--compile --preset final` (the shipped build, as `tutmap101`); `--check 300000`: 0 holes. The .map's
  diff is the railing's brushes only (one more brush; 2655); the .bsp's entity lump is identical to HEAD's (351
  entities, same keys and order); texts, music and banners untouched.
- **Climb test** (headless, `vr_fixed_frames 1; vr_climb 1`, `setpos 728 -541 -120 0 90 0`, a hand-over-hand play with a
  20-unit stroke (hands at y 1.835, forward -0.733, pulled down 0.667; the top grab at 1.78, -0.87): new map: five strokes
  up, then "mantle ... up to 56.0, onto (724.4 -490.6 56.0)": on the mezzanine. The same play on HEAD's map: "no room to
  mantle" at every stroke up to z 4, then he lets go and falls. (`climb_plays.py ladder`, tuned to vrclimb, finds no
  hold in this harness: its hands sit 6 units over the rungs.)
- **Arena** (`vrtutorial_playtest.py --from arena`, mortal): seeds 1-3 cleared and alive (101, 25, 25); seed 2's first
  run stopped at the arena's south door (health 62) and cleared on two reruns: the runs are not repeatable to the frame.
  HEAD's map the same way (45, 62, 23, all cleared). With `--god`, both maps end some seeds in the pit (the mock's
  walks), as `tutmap101` found.
- Loads clean; before/after shot from over the arena floor: the rails stop either side of the ladder's top.

For VR: climb the north mezzanine's ladder (right of the arena door) and step off its top onto the ledge; the railing
should still stop a walk off the edge everywhere else.
## Setup's preparation: the newer campaigns' models too (1.0.1, 2026-10-10, worktree `expnorm`)

The preparation's models step (item 2 of the load notes above) runs on the hub with `-game quakevr`. On that search
path the mission packs' folders are mounted whenever they are installed, so Scourge of Armagon's and Dissolution of
Eternity's models were already covered. After the preparation, first visits to hip1m1 and r1m1 made 0 skins' normal
maps (129 and 68 were read from the cache). The newer campaigns' folders (dopa, mg1, mg3) are searched only while
that campaign is active, so their own models still made their normal maps at their first visit.

**Change** (`Quake/vr/vr_prepare.cpp`). After the hub's models step, the preparation goes through each installed
campaign that is playable and has a folder of its own (`VR_CampaignUnavailable` false; the others are skipped without
a line). For each one it:
- makes that campaign the active one the selector's way: `selectCampaign` with no map started, new
  `VR_SelectCampaignNoMap`. The game folders are rebuilt by `COM_ReloadVRGame`, which also runs `CL_ForgetModels` and, through
  `CL_Disconnect`, `S_StopAllSounds`;
- lists that folder's models (`progs/**.mdl` in its paks and loose files);
- if there are none, writes `campaign <folder> none` and loads no map. This is Dimension of the Past: it has no model
  of its own;
- otherwise loads its start map, waits 1 s, then runs `vr_prepare_models <folder>`: that folder's models and the limbs
  of its rigged models.

At the end, Quake's campaign is made the active one again, so the relight's batch sees the same folders as before.
The monsters' guns are not dropped again in a campaign. On mg1's and mg3's start maps that step cut 0 pieces (the
dropped guns are the same models) and cost 1 s each.

New progress lines (documented in `vr_prepare.hpp`): `step campaigns <i> <n> <folder>`,
`campaign <folder> ok|none|failed <seconds> [models=<n> limbs=<n>]` and `result campaigns ok= none= failed=`.

**Setup** (`GamePreparation.cs`) used to ignore the guns', models' and campaigns' lines. It now reads all of them:
- a status for each step ("the monsters' weapons", "the models' normal maps", "Dawn of the Machine (3 of 3)");
- a log line for each result (counts and seconds); a campaign that failed is logged as a warning;
- counts in `PreparationResult`: `CampaignsOk`, `CampaignsNone` and `CampaignsFailed`.

The bar's share for the maps is split: the first three maps take 60% of it, the guns and the models up to 70%, and
the campaigns the rest.

**Debug > Tests**: "Every Model of This Campaign Precached" (`vr_prepare_models campaign`).

**Measured** on this PC, with Steam's rerelease providing dopa, mg1 and mg3 and an empty `quakevr/cache`:

| | before | after |
|---|---|---|
| time added by the campaigns, cold | — | 6.5 s (dopa 0.3, mg1 3.3, mg3 2.9) |
| time added by the campaigns, warm | — | 5.8 s |
| models precached | — | mg1 7, mg3 51 (no limbs) |
| `cache/normalmaps` | 50 MB | +7.5 MB (40 files) |
| skins' normal maps made at the first visit | mg1 hub 6, mg3 hub 1, map1 6, map2 19 (294 ms) | 0 |

- "Before" is the same build with `-nosteam -nogog -noepic`, so the preparation found no newer campaign. That hub
  also lacks the campaign displays, so it made 132 limbs, not 162, and hip1m1 made 3 skins there.
- The first visits after the preparation (hip1m1, r1m1, e5start, e5m1, mg1 start, hub and mge1m1, mg3 start, hub,
  map1 and map2, then e1m1) made 0 skins' normal maps.
- `vr_normalmap_cache 2` on the same loads compared 482 maps, and 0 differed.
- A run with no newer campaign installed writes no campaign line, as before.
- The installer's self-tests pass 37/37. The preparation test now covers the new lines, the fractions and the
  engine's format strings.
- The relight that follows (`-preparerelight`) was not run here, because the shared relit maps are read-only. It
  starts the same way as before (`disconnect`, then the batch) with Quake's campaign active.

## Menu on any controller (1.0.1, 2026-10-10, worktree `menubind`)

A player's report: a Pico Neo 3 Link over DisplayPort (SteamVR) plays, but no controller button opens the menu (only
Escape on the keyboard). The menu is the OpenXR `menu` action; its suggested bindings are the left `menu/click` on
Touch, Vive wands and WMR, both on `khr/simple_controller`, the left B on Index. A runtime picks one of the suggested
profiles for the controller in hand. SteamVR keeps the Touch menu button for its dashboard and gives Quest controllers
the Index profile (SETTINGS.md), and it most likely does the same, or something like it, to a Pico. Which one it was
we can't know without the player's `vr_debug_buttons 1` line (below). Two fixes, the second for any controller:

**Vendor profiles** (`vr_xr_vendor_profiles 1`, the default, read at the VR start; `vr_backend_openxr.cpp`
`suggestBindings`). When the runtime offers their extension, the game enables it and suggests bindings for Pico Neo 3
and Pico 4 (`XR_BD_controller_interaction`), Vive Cosmos and Focus 3 (`XR_HTC_*_controller_interaction`), HP Reverb
G2 (`XR_EXT_hp_mixed_reality_controller`) and Samsung Odyssey (`XR_EXT_samsung_odyssey_controller`). Each gets its left
menu button as the menu; the G2 gets both. The G2 also gets its A/B/X/Y: under the WMR profile it has none (trackpad
clicks). If a runtime turns a path down, the whole profile is lost. So the game then tries the profile's basic
bindings: no menu button, no finger sensors (`qvr_openxr.txt`: `... turned down its bindings; its basic ones:
taken`). Pico, Cosmos and Focus 3 get Touch's legacy grip (`vr_controller_legacy_pose`), the nearest shape the shipped
offsets know; HP and Odyssey stay raw, as WMR does. The component paths come from the extension specs as I remember
them. Only the fake runtime has checked them, not a real Pico, HTC or HP runtime.

**Menu gestures** (`vr_menu_button`, VR Settings > Controls > *Menu Button*, also HUD and Menus > Menu; `vr_input.cpp`).
Each one acts as a press of the menu button: it opens the menu, and closes it from any page (`backToGame`). It goes
through the same path as the button, posing mode included, so VR Calibration's pause and `waitsOnMenu` behave as they
do for the button. The modes:
- 0 *Menu Button Only*: the gestures are off.
- 1 *Or Both Sticks* (the default): click both sticks within 0.3 s of each other, or in the same frame. The second
  click is the menu's, press and release. The first click reaches the game as usual (+speed, or +reloadmain, or bullet
  time's stick). Two clicks 0.3 s or more apart are two game clicks, so holding run and clicking reload stays safe.
- 2 *Hold Y*, or 3 *Hold B*: hold it for 0.8 s in the game. The press reaches the game first (a weapon switch), so a tap
  is never delayed. Holding it in a menu does nothing, because there it goes back a page.
- 4 *Y*: Y is the menu button. It is taken whole, before the posing mode, the toolgun and voice notes.

Every mode except 0 keeps the stick chord, so choosing another mode never leaves a player without a way to the menu.
The gestures are off while a take plays back. `vr_debug_buttons 1` now prints every menu press:
`VR buttons: main hand's both sticks clicked: opens the menu`. The calibration boards, the body calibration's board
and the sight alignment's board say `menu button or both sticks: stop` (`input::menuButtonName`). The tutorial has a
new tip, `t1_menu` in lesson 1's hall: "The menu: the menu button, or click both sticks together.". It went in through
an entity-lump edit (`bsp_set_entities.py --from-file`), with no recompile. It is the lump's last entity (the
generator writes it last, in `build_extras`' `late()` block), so a 1.0.0 save made mid-tutorial keeps its edict
numbers. The lump matches `vrtutorial.map`, which was regenerated: the new entity is the only change. The docs are updated: SETTINGS.md (the
controls table and a *Menu Button* paragraph), FEATURES.md, PLAYTEST.md and TESTING.md.

**Tests.** The mock run (`vr_debug_buttons 1`) checks each path:
- Both sticks 2 frames apart opens the menu; both in the same frame closes it.
- Clicks 90 frames apart are two game clicks.
- Mode 0: no menu.
- The menu button opens and closes.
- Mode 2: Y held 150 frames opens the menu; a Y tap does not.
- Mode 3: B held opens the menu.
- Mode 4: Y opens and closes the menu.

`xr_runtime_test.sh` part 9, run against the fake runtime with the new `FAKEXR_EXTENSIONS` and `FAKEXR_REJECT_PATH`,
passes 6/6: the extensions enabled, every profile suggested with the expected binding counts, none with
`vr_xr_vendor_profiles 0`, and the basic fallback. One older check fails in this worktree, part 2c's "a relative
library_path resolved" (the console wraps its long path). It is not touched here. `vr_menu_path_check`:
vrcalibration 4 found and vrtutorial 1 found, 0 missing in each.

**In the headset** (the author):
- Quest under SteamVR: `vr_debug_buttons 1` should still name the Index profile. A vendor profile picked instead would
  move the hands.
- Click both sticks in game and in a menu.
- VR Calibration: the chord pauses it.
- Try the other *Menu Button* modes.
- The tutorial's new tip, before LESSON 1's board.

**Reply to the player, for 1.0.0:**
> Thanks for the report! Under SteamVR the Pico's menu button is kept by SteamVR itself. Two things that work in
> 1.0.0:
> 1. Try the **left Y** button. SteamVR often hands controllers to the game as Valve Index controllers, and then the
>    left upper button opens the menu.
> 2. If Y doesn't do it, open the console with the **~** key on the keyboard and type `bind YBUTTON togglemenu`, then
>    Enter. Y then opens the menu in the game. It no longer switches to the previous weapon; B still cycles weapons.
>    In the menu, Y still goes back a page, and *Back to Game* is at the menu's top left. To undo it:
>    `bind YBUTTON "impulse 12"`. (`bind LTHUMB togglemenu` puts it on the left stick click instead, losing run/walk.)
>
> It would help a lot if you typed `vr_debug_buttons 1` in the console and then pressed the Pico's menu button and
> Y. The console prints which controller type SteamVR gave the game. Please send me those lines and
> `quakevr/qvr_openxr.txt` (it lists what SteamVR offers).

**Reply for 1.0.1:**
> 1.0.1 fixes it for every controller: **click both sticks together** to open the menu, and again to close it. VR
> Settings > Controls > *Menu Button* has other choices: hold Y, hold B, or make Y the menu button. If SteamVR
> offers Pico's own controller profile, the game now uses it, with its menu button. If your aim feels different
> after the update, redo the pitch/yaw alignment, or type `vr_xr_vendor_profiles 0` to go back to how it was. If you
> bound Y to the menu in 1.0.0, you can give it back its weapon switch with `bind YBUTTON "impulse 12"`.

## "Guns only grab as props": wrong game code, the folders searched (1.0.1, 2026-10-11, worktree `gungrab`)

A player who had built a pre-release from source installed 1.0.0 (installer, fresh folder): every gun taken off the
floor was held like a prop, even gripped at the handle. Later he cleaned his Quake folder down to id1, rogue and
hipnotic and used the zip: the same.

**What a lying gun's grab depends on.** The client's updateGroundSpots (vr_view.cpp): the hand's point within
`vr_weapon_grab_anywhere_min` (12 cm) of the gun's origin (its handle) takes it by the handle; farther, the fist on its
drawn shape takes it "anywhere" (held as a prop: no firing). The QC takes it (weapongrabspot). Without Quake VR's QC
(id's progs.dat, "compatibility mode") no gun is ever a weapon in the hand.

**Ranked causes, with evidence (headless, scratch folders, the release package made by package-quakevr.ps1):**

1. *Another quakevr's game code, or none.* Nothing checked that progs.dat belongs to the engine (no handshake: the VR
   fields and functions are bound by name, missing ones silently off).
   - Launched without the shortcut's `-basedir`s (QuakeVR.bat or ironwail.exe from the installer's folder, a launcher's
     entry): Ironwail finds Quake itself (the current folder, Steam, GOG, Epic) and the install's folder is no base dir,
     so the quakevr searched is the Quake folder's (a source build's link or copy). Reproduced: the fake Quake folder
     with 2026-10-01's quakevr (git archive + fteqcc), the 1.0.x exe started as QuakeVR.bat does: `path` lists only
     that old quakevr. Its progs.dat lacks 51 of the engine's 169 VR fields.
   - A quakevr folder with a config but no progs.dat in a later base dir (Ironwail's Saved Games user dir, added last
     by the store branch; a pre-release before 800023659 saved its config and saves there when launched without
     `-basedir`): "the last base dir that has quakevr" was that one, so progs.dat came from rogue/hipnotic/id1 (id's):
     `VR: progs without Quake VR's gameplay: compatibility mode`. Reproduced with `-basedir <pkg> -basedir <userdir with
     quakevr/ironwail.cfg only>`.
   - The installer's shortcut (`-basedir <Quake> -basedir <install>`): the install's quakevr only (reproduced with an
     old quakevr in the Quake folder). The player's case is not this one unless he starts it another way.
2. *Files in the Quake folder's id1, which every launch still reads:* `id1/autoexec.cfg` runs at every start (the kit's
   own tests rely on it); `id1/config.cfg` runs at a first start; an `id1/pak2.pak`+ (a model pack) replaces
   `progs/v_*.mdl` (quakevr ships no id weapon models). He kept id1: worth asking what is in it.
3. *Hardware / runtime.* Hand poses are moved to the old raw pose only for Touch and Index profiles
   (vr_backend_openxr.cpp toLegacyPose); every other profile (Vive wands, WMR/Reverb, Pico, Cosmos...) uses the grip
   pose as is (~10-13 cm and 15-20 degrees off what the weapon offsets were tuned on). That shifts hand and guns
   together, so it should not by itself turn handle grabs into "anywhere" ones, but it is the one per-controller path.
   The profile is printed with `vr_debug_buttons 1`.
4. *Data missing from the package: ruled out.* The package is every tracked quakevr file plus progs.dat; weapon
   offsets, hotspots and props are compiled in (vr_defaults.cfg ships). A zip-style install (package in a scratch Quake
   folder with only id1/rogue/hipnotic, started as QuakeVR.bat does) with the tests' config takes the lying shotgun by
   its handle (6.0 units off). No grab code changed since v1.0.0 (git log). A fresh config's state after its
   migration differs from the tests' baseline in 20 cvars, none a grab or weapon setting (`writeconfig` dumps diffed).
   (The mock grab test from a fresh config misses only because the mock's walk ends elsewhere and its standing,
   uncalibrated player cannot reach the floor there: with `cl_alwaysrun 1` and the baseline's height, or its arm
   calibration, it takes the gun by the handle. A harness limit, not the player's.)

**Fix (1.0.1):**
- `COM_AddExeBaseDir` (common.c): without `-basedir`, the exe's own folder, when its quakevr has progs.dat and it is no
  base dir yet, is added as the last base dir: QuakeVR.bat or ironwail.exe from the install use its quakevr and save
  there. Reproduced fixed: the same fake Quake folder with the old quakevr, `VR: no -basedir: the exe's own folder
  added...`, the game code this build's.
- VR_GameDirectoryRoot: quakevr from the last base dir whose quakevr has the game code (progs.dat or pak0.pak), else the
  last that has the folder (a config-only quakevr in the user dir no longer hides it).
- Game code check (vr_progs.cpp checkGameCode, each progs load, said once per source): the console says
  `VR: game code <path>: Quake VR's, this build's` (or older, or not Quake VR's); a warning and a centre print when it is
  not Quake VR's while quakevr is the game, or is missing any of the engine's VR fields (another build's).
- `vr_gamecode` (Debug > Diagnostics, "Game Code and Folders"): the base dirs (which one's quakevr is searched), the game
  dirs, the check again, and `path`.
- README-QuakeVR.txt: installer users start from the shortcut; the steps are for the zip.

**What to ask the player** (console: `~` or the menu's console): `version`, then `path`, `vr_cfg_version`,
`vr_wofs_version`, `vr_weapon_grab_anywhere`, `vr_weapon_grab_anywhere_min`, `vr_weapon_grab_hotspots`,
`vr_weapon_grab_handle_leniency`, `vr_gunangle`, `vr_handcal_x`, `vr_handcal_y`, `vr_handcal_z`,
`vr_controller_legacy_pose`, `vr_body_mode`, `vr_height_calibration`; then `developer 1; vr_debug_buttons 1;
vr_debug_carry 2; vr_debug_2h_grip 2`, grab a lying gun at its handle twice, and `condump`
(quakevr/condump.txt: it has the game code line, the interaction profile, and each grab's "anywhere: ... from its
handle" and "weapon: ... taken" lines). Also: headset, controllers, the OpenXR runtime (SteamVR, Oculus/Meta,
Virtual Desktop/VDXR, WMR, Pico), how he starts the game (QuakeVR.bat double-clicked, a shortcut, a launcher), and the
list of files in `id1` (any .cfg, any pak beyond pak1.pak) and whether `%USERPROFILE%\Saved Games\Ironwail` or
`Saved Games\Nightdive Studios\Quake` has a quakevr folder. Workaround meanwhile: start with
`ironwail.exe -basedir "<Quake folder>" -game quakevr` from the folder Quake VR is in, move `id1\autoexec.cfg` aside,
and move any quakevr folder out of those Saved Games folders.
## Exit from the runtime's menu quits the game (1.0.1, 2026-10-10, worktree `xrexit`)

Player report (Pico Neo 3 through SteamVR): "using exit in SteamVR kills mod but game still work in flatscreen". When
the player quits an app from SteamVR's dashboard, the runtime ends the session as OpenXR says: FOCUSED > VISIBLE >
SYNCHRONIZED > STOPPING. The app calls `xrEndSession`, and the runtime then sends IDLE > EXITING. EXITING means "end
the XR experience, don't restart it". 1.0.0 treated EXITING like a lost session: it stopped VR and kept playing flat on
the desktop. Virtual Desktop's and Meta's "quit" go through the same EXITING.

What the game does now (`pollEvents` in `vr_backend_openxr.cpp`, `sessionEnded` in `vr_main.cpp`):
- **STOPPING**: `xrEndSession` (only if the session was begun; it is now checked and logged), then wait. What comes
  next tells the cases apart. `qvr_openxr.txt` says `session ended (xrEndSession): waiting for the runtime`.
- **READY** after it (the headset back on, the runtime restarting the app's session): the session begins again and
  VR goes on. This was already the case.
- **EXITING**: with `vr_xr_exit_quits 1` (new, archived, default 1) the game quits as closing the window does
  (`CL_Disconnect`, `Sys_Quit`; `Host_Shutdown` saves the config). It first destroys the session and the instance
  (`stopBackend`). With `0` it plays on flat, the 1.0.0 behaviour (`vr_restart` starts VR again). Menu: VR Settings >
  Headset > *Exit From Runtime Menu* (advanced): Quit the Game / Play Flat.
- **LOSS_PENDING**, and `XrEventDataInstanceLossPending` (the runtime going away: SteamVR shut down or updating): flat,
  as before. The message now says why and how to get back: `VR: openxr session lost: playing flat (the headset or the
  runtime went away; vr_restart, VR Settings > Restart VR, starts VR again)`. There is no automatic reconnect. A
  runtime that went away needs `xrCreateInstance` again, and polling for it each frame would stall the game. Restart VR
  does it on demand.
- Every end prints `VR: the session's last states: ...`, the last 12 states as the runtime gave them, to the console
  and to `qvr_openxr.txt`. Example: `IDLE > READY > SYNCHRONIZED > VISIBLE > FOCUSED > VISIBLE > SYNCHRONIZED >
  STOPPING > IDLE > EXITING`. The OpenXR log also gets `the runtime asks the game to exit (states: ...)`, `session loss
  pending` or `instance loss pending`.

Fake runtime (`Misc/quakevr/fakexr`): `FAKEXR_END=n:kind` ends the session at `xrWaitFrame` n the way runtimes do:
- `exit`: VISIBLE, SYNCHRONIZED, STOPPING, then IDLE and EXITING after `xrEndSession`.
- `restart`: the same, then READY.
- `loss`: LOSS_PENDING.
- `instanceloss`: the instance-loss event.

Its `xrEndSession` now fails with `XR_ERROR_SESSION_NOT_STOPPING` outside STOPPING, as the spec says, and logs each call.

`xr_runtime_test.sh` part 10 (13 checks):
- exit: the game quits at once with exit code 0, before the script's own `echo` and `quit`; the fake logs
  `xrEndSession`, then `xrDestroySession` and `xrDestroyInstance`; `qvr_openxr.txt` has the states in order.
- `vr_xr_exit_quits 0`: flat, and the game runs on.
- restart: READY > SYNCHRONIZED > FOCUSED again, the game still in VR, no end reported.
- loss and instance loss: flat, with their states.
- `xrEndSession` is never called outside STOPPING.

**Part 2c's failing check was not a regression.** "A relative library_path resolved" matched the line `OpenXR:
other_openxr.json's library is the one loaded: <path>` in the console log. The hidden test window is 960 wide, so the
console wraps at about 120 columns, and that line is 55 characters plus the DLL's path. The path's length depends on the
worktree's name: `xrexit` gives 119 characters and passes, while a name two letters longer (`menubind`) gives 121 and
wraps. Neither the xrval nor the menubind commit touched `vr_xr_runtime.cpp` or part 2c; they only added test parts. The check now reads the same
line from `qvr_openxr.txt`, which is not wrapped (copied after the run as `cmdline_openxr.txt`). The new part 10 also
checks the long state lines in `qvr_openxr.txt` for the same reason.

Full `xr_runtime_test.sh xrexit`: before the change 0 failed (this worktree's short name hid 2c's wrap); after:
98 checks, 0 failed.

**In the headset** (the author, or the Pico player):
- SteamVR: in game, open the dashboard and use its exit/quit for Quake VR. The game should close within a second, and
  `quakevr/qvr_openxr.txt` should end with `the runtime asks the game to exit` and the states. Check that a setting
  changed just before is still there at the next start (the config is saved).
- Virtual Desktop (VDXR) and Meta Link: quit from their menus. The game should close the same way. If it stays open,
  the last lines of `qvr_openxr.txt` show which states the runtime sent.
- Take the headset off for a minute and put it back on. The game should go on in VR, not quit. Runtimes that stop the
  session when the headset sleeps send STOPPING and then READY, not EXITING.
- Close SteamVR itself while the game runs. Expected: flat, with the `session lost` message, or, if SteamVR sends
  EXITING, a quit. Either is fine, but note which one happens.

**Reply to the player** (draft):
> Thanks! In 1.0.1, exiting Quake VR from SteamVR's menu closes the game properly (your settings are saved) instead of
> leaving it running flat on the desktop. If you'd rather have the old behaviour, VR Settings > Headset > *Exit From
> Runtime Menu* > Play Flat.
## Continue, level-start autosaves, the hub remembers the campaign (1.0.1, 2026-10-10, worktree `contgame`)

A player's report: "the game doesn't seem to save its progress; after restarting, the hub shows the first campaign as
default; no 'continue previous game'".

**What there was in 1.0.0.** Quake's own saves (the Save and Load menus' s0..s19, `save <name>`), and Ironwail's
autosave, on by default: `sv_autosave 1`, a save into `autosave/<map>.sav` after about `sv_autosave_interval` (30)
seconds of quiet play (not hurt, not firing, standing still; sooner after a secret, health or a teleport;
`Host_CheckAutosave`). Nothing showed those autosaves except Ironwail's skill menu (Play Custom Map > a map > *Resume*).
They were made in the hub and the tutorial too. The hub's campaign choice is `vr_activestartpaknameidx`, archived. But
every way back to the hub (`vr_campaign_hub`, `map vrstart` from another campaign, the hub's portal from a campaign)
went through `selectCampaign(0)`, and that set the choice to Quake. Choosing Dimension of the Past, of the Machine or
Dawn of the Machine set it to 0 as well ("the legacy selector only describes three campaigns"). So after a restart, or
any return to the hub, the stones showed QUAKE selected.

**What 1.0.1 adds** (`Quake/vr/vr_continue.cpp`, new):
- **An autosave as each level starts**: a map or a changelevel, not a load. It is made 1.5 s in (`qcvm->time` 2.5),
  once the player is in, with no score needed (`vr_autosave_level_start 1`, archived). It goes through Ironwail's
  gates: `sv_autosave` on, single player, alive, not in an intermission, no god/noclip/notarget. It is queued with
  `Cbuf_InsertText`, so it runs next, before any queued command. Ironwail's interval autosaves are unchanged and still
  go to the same file.
- **No autosaves on Quake VR's own maps** (`vr*`: the hub, the tutorial, the calibration room, the firing range):
  `VR_AutosaveAllowed`.
- **One autosave file per campaign's map**: `autosave/<map>.sav` for Quake's (Ironwail's name, so 1.0.0's still
  count), `autosave/<folder>/<map>.sav` for hipnotic, rogue, dopa, mg1 and mg3. Their `start` and `end` maps share
  names. `VR_AutosaveName` is also used by the skill menu's *Resume* and by `load` completion (`SaveList_Init`). The
  player's own saves are never written.
- **Continue**: the newest save of all, by file time: `*.sav`, `autosave/*.sav` and `autosave/<folder>/*.sav`. It must
  load here: version 5, a `// qvr_save` line of a format this build reads, the campaign keys with this mission-pack
  installation, and a campaign that can start now (`VR_SaveCampaign`, a quiet copy of `VR_CanLoadCampaignSave`'s
  checks). It must be of a campaign's map, not a `vr*` one. Every campaign saves into the one `quakevr` folder, so the
  newest save belongs to the campaign played last. Loading it switches its campaign first, through the existing
  `VR_CanLoadCampaignSave`. From a multiplayer server it first puts back `maxplayers 1; deathmatch 0; coop 0`.
  - **Main menu**: a *Continue* row at the top, on its own above the VR rows. It shows only when there is a save. On
    its right, in small text: the save's map title and name, then its campaign and how long ago ("The Gateway (start)
    / Dimension of the Machine, 5 minutes ago"). The cursor starts on it at the menu's first opening in a session.
    During a campaign's game it asks first ("The game in progress ends: you go to ..."). In the hub it loads at once.
  - **Hub**: a stone lectern with a CONTINUE button, left of the quick-start stand (tutorial, calibration), in line
    with its front. A screen above the button shows the save's map, campaign and time. QC's `func_button` removes the
    button when there is no save (`vr_continue_available`, set as a Quake VR map spawns). The lectern then stands
    empty. The button is the generator's last entity, so a 1.0.0 save's edicts and brush models keep their numbers.
    This needed a recompile (`vrstart_gen.py --compile`, the final preset, about 14 minutes). The unchanged map
    recompiled first looked the same as the shipped one in a screenshot of the arrival. The lightmaps are not
    byte-identical, which is the light tool's usual run-to-run difference.
  - `vr_continue` (the command), and `vr_continue_info` (Debug > Tests > Saves > *What Continue Would Load*: the save,
    and the newer ones passed over).
- **The hub remembers the campaign**: `selectCampaign` syncs the hub's choice only when a campaign's map is chosen. It
  does not sync for Quake VR's own maps or for Setup's preparation (`VR_SelectCampaignNoMap`). It syncs every campaign,
  3-5 too. A campaign map's spawn also makes the choice that campaign, which covers a save loaded in the campaign
  already active. The hub's stones then show the campaign last played as SELECTED, across restarts (the cvar was
  archived already).

**Tests** (mock, `sv_autosave 1`; the kit's autoexec turns autosaves off):
- `map e1m1`, `changelevel e1m2`: `autosave/e1m1.sav`, then `autosave/e1m2.sav`. `vr_continue_info`: "Castle of the
  Damned (e1m2), Quake, just now".
- A new game process: `vr_continue` loads `autosave/e1m2`, map e1m2.
- `vr_campaign_select mg1`: `autosave/mg1/start.sav`. Then `vr_campaign_hub`: `vr_activestartpaknameidx` stays 4 and
  `vr_continue_available` is 1.
- A new process (Quake's campaign active): `vr_continue` loads `autosave/mg1/start`. `vr_campaign` is 4, the map is
  start, and the written config has `vr_activestartpaknameidx "4"`.
- The main menu screenshot shows the Continue row and its two lines.
- The hub: see below.

**Decisions for the author:**
- How often: a save at every level start, plus Ironwail's interval saves (about every 30 s of quiet play, as in 1.0.0).
  One file per map, never pruned. A long campaign leaves a few dozen 200-400 KB files in `autosave/`.
  `vr_autosave_level_start 0` turns the level-start ones off; `sv_autosave 0` turns all of them off. Neither has a menu
  row yet.
- Continue loads the newest save, whichever kind. A manual save made after the last autosave wins, and so does an
  autosave made after it.
- When there is no save, the hub's lectern stands empty, because the button is removed. The other choice is a dimmed
  "no saved game yet" button.

**In the headset:** finish a level, quit, start again. The main menu's Continue should show the next level. Open the
hub's Continue screen, press it, and check that you land there. Play a level of Dimension of the Machine, quit,
restart: the hub's DIMENSION OF THE MACHINE stone should be SELECTED, and Continue should switch campaigns.

## One save folder; every save knows its campaign and map package (1.0.1, 2026-10-11, worktree `saves1`)

The author's design, replacing `contgame`'s per-campaign autosave folders (`Quake/vr/vr_continue.cpp`):
- **One folder**: `quakevr/` and `quakevr/autosave/`. Autosave names are unique across campaigns and map packages:
  `autosave/<map>.sav` for Quake's campaign (Ironwail's name: its Play Custom Map *Resume*, 1.0.0's autosaves),
  `autosave/<campaign folder>~<map>.sav` for the others (`mg1~start.sav`), `autosave/<package>~<map>.sav` for a Map
  Library package's map. `<package>` is the first 16 hex digits of its sha256, the name of its `qvr_addons` folder
  (a title is unknown until the map index arrives and can change upstream; the hex is stable). The map part is
  sanitised (letters, digits, `_ - .`). The name comes from what is running: the active campaign and the mounted
  package (`VR_AutosaveName`), never from the map's name. The skill menu's *Resume* predicts it for a map about to start
  (`VR_AutosaveNameForLaunch`: the `map` command's own campaign check and the package the map is in).
- **Migration**: at start-up, `autosave/<folder>/<map>.sav` (and its `.addon`) is moved to
  `autosave/<folder>~<map>.sav`, logged (`Saves: autosave/mg1/start.sav moved to autosave/mg1~start.sav`); the emptied
  folder goes. A file whose new name is taken stays where it is, logged.
- **Every save says what it was played as**: comment lines after the model list (`SaveData_WriteHeader`; COM_Parse
  skips them, so 1.0.0 still reads these saves; no format change): `// qvr_campaign <n>`, `// qvr_package <sha256>`
  (nothing after the key: none), `// qvr_package_title <title>`, `// qvr_map_title <the level's full title>`. An older
  save: the campaign from its `vr_save_campaign` global, the package from the `<save>.addon` beside it (still written,
  so 1.0.0 builds keep finding it).
- **Loading any save** (Load menu, `load`, Continue, Ironwail's autoload; all are `Host_Loadgame_f`): its package must
  be installed (`VR_SavePackageReady`: else a console line and a centre message, nothing changed), then its campaign is
  made the active one (`VR_CanLoadCampaignSave`, as before; a campaign that can't start now also gets a centre
  message), its package mounted (`VR_AddonForSave`, the key first), and its map run in the save's campaign
  (`VR_CanLoadSaveMap`, new: the save's key, not the map's name; the spawn check `VR_CheckSpawnCampaignMap` takes it
  for that one spawn). The campaign switch is the existing path (selectCampaign, then the package's reload).
- **Continue** scans `*.sav` and `autosave/*.sav` only (Doom's saves, in `quakevr/doom/`, are never seen). A save
  whose package is not installed is passed over, said once in the console. Its second line names the package for a
  package's save ("Base To Hell (basetohell) / Base To Hell, just now").
- **Load and Save menus**: each slot shows "<campaign or package> - <map title>" (`VR_SaveMenuLabel`, 39 characters;
  the package's title once when it is the map's title). The kill count of Quake's comment is no longer shown.
- `vr_save_info [save]`: one save's or every save's campaign, package, map title and menu line (Debug > Tests > Saves >
  *List the Saves*).
- Unchanged: no autosaves on `vr*` maps, in multiplayer, or under `-prepare` (`e03a275d0`).

**Tests** (mock, `sv_autosave 1`; the author's index cache and packages linked in by `bench_maps.ps1`):
- `vr_campaign_select id1`, `mg3`, `mg1`, then `maps_play` Base To Hell: `autosave/start.sav`, `autosave/mg3~start.sav`,
  `autosave/mg1~start.sav` and `autosave/4108605842870e05~basetohell.sav` side by side. The package's save:
  `qvr_campaign 4` (the custom map ran in Dimension of the Machine, the campaign active when it was played), its sha
  and "Base To Hell".
- A new process, the hub (Quake's campaign): `vr_continue` loads that autosave, mounts Base To Hell's folder;
  `vr_campaign` 4, map basetohell.
- A Dissolution of Eternity save (`s3`, map start) loaded with Quake active from the hub: `vr_campaign` 2,
  `vr_activestartpaknameidx` 2, map start. The Load menu shows "Dissolution of Eternity - Introduction" in slot 3.
- 1.0.0 saves (the author's `s1.sav`, map vrstart, and `autosave/e1m2.sav`, build 1.0.0-dev 2026-10-09) loaded from
  Dimension of the Machine: Quake's campaign, maps vrstart and e1m2 (the usual "saved by build" warning).
- Base To Hell taken out of the installed list (the registry copy): Continue passes its newer autosave over with the
  message and offers `autosave/e1m2`; `load` of it is refused ("which is not installed: not loaded"), e1m2 stays.
- Migration: `autosave/mg1/start.sav` (+ `.addon`) moved to `mg1~start.sav`; `autosave/mg3/start.sav` left (taken).
- `map e1m1`, `save s2`, `load s2`: fine; `s2` says campaign 0, package none.

**Decisions for the author:**
- A Map Library map runs in whatever campaign is active when it starts (as before: here Dimension of the Machine's), and
  its saves remember that. Starting packages always in Quake's campaign would be a separate change.
- The package part of the name is the sha's first 16 hex digits, not the title (stable, matches `qvr_addons`).
- A save whose package is gone is refused rather than loaded into another package that has a map of the same name.

**In the headset:** play a Map Library map right after a Dimension of the Machine level, quit, restart, press
Continue: you should be back in the custom map. Open Load: each slot names its campaign or package and its level.

## Pre-release verification of 1.0.1: every test, a synthetic installation (2026-10-11, worktree `test101`)

Commit under test `abfc18bcb` (origin/vr-ironwail). Everything ran headless. Nothing was published or pushed. The
packages were built with `make_release.ps1 -Local` in local clones under `scratch/`: `rel101` for 1.0.1 (VERSION bumped
in the clone only) and `rel100` for tag v1.0.0. That way the kit's build was never relinked while the suite ran. Logs
are in `scratch/suite1`, `suite2`, `extra` and `synth/logs`.

**Regression suite** (`run_test_suite.py test101`, 42 tests, 60 min): 35 passed, 7 failed. The failures were
collectfx, dead_gear, fist_alert, pickup_mag, stats_levelchange, swim_sound and weapon_catch, each only on its "no error"
check. **This is a test-tool issue, not the game.** These tests grep the console for `rror`. The 1.0.1 GL breadcrumbs
(`VR_GLStep`, vr_glsafe.cpp, from the AMD start-up crash fix) are copied into qconsole.log until the first map is drawn.
The test maps load in that window, so the kit's output includes lines like `1471.4 ms  compile vr mirror vertex shader`,
and "mi**rror**" matches. I reran the same 7 tests with a copy of the kit's run.ps1 that filters those lines out
(`|\s+[0-9]+\.[0-9] ms  ` added to its log filter, QVR_KIT pointing at the copy): 7 of 7 passed. **The kit's run.ps1
needs that change at line 133.** Without it, a release's `-RunTests` fails on these 7 tests. Other results: xr_runtime
98 checks, 0 failed; reload self-test 67 passed, 0 failed; teleporters_chase passed on its first try.

**Outside the suite:**

| Test | Result | Numbers |
|---|---|---|
| config116_test.sh | pass (after a fix) | every value was right. It expected `vr_cfg_version` 116 and got 123; that check has been stale since config 117, before 1.0.0. It now accepts 116 or later |
| config123_test.sh | pass | NEW 1, KEPT 0, ON 1 |
| loading_switch_test.sh (sound on) | pass | select: hipnotic, rogue, dopa, mg1, mg3, id1, hipnotic, hub; portals: 12 changes, each put off and spawned in order |
| ragdoll_test.sh (16 cases) | no errors | prints numbers only. Its `cap` case once left 8 ragdolls awake and unmoved at 9 s. A rerun, and the packaged 1.0.0 and 1.0.1 with the same script, all settled (0 awake): flaky |
| vr_menu_path_check | pass | vrcalibration 4 found, 0 missing; vrtutorial 1/0; vrstart 4/0 |
| Campaign start maps (kit, sound) | pass | vrstart, start x2, e5start, start x3, vrcalibration, vrtutorial, vrstart: no error; game code this build's |
| Installer self-tests | pass | 37/37 (both packages) |
| Installer screenshot harness | pass | off-screen (the packaged Setup's `--screenshots`, run by make_release): 17 pages, install and verify |

**Synthetic installation** (the 1.0.0 table again). The setup:
- A fake `steamapps/common/Quake` with id1's pak0 and pak1, hipnotic and rogue.
- The dev-config trap: a `quakevr` folder in it holding the author's kind of `ironwail.cfg` (Menu Detail Developer,
  voice notes on, no first start, an echoed marker), an `autoexec.cfg` with a marker, and 1.0.0's `progs.dat` (an old
  quakevr).
- A fake `Saved Games\Ironwail\quakevr` with only a config.
- The newer campaigns came from this PC's Steam rerelease.

The game ran with the shortcut's command line, the mock headset and a hidden window, driven by `vr_mock_play` command
lines. Timed commands were needed because the first start's map loads only after the whole autoexec has run.

| Step | Result | Numbers |
|---|---|---|
| `make_release.ps1 -Local` (1.0.1 and 1.0.0) | pass | zip = allowlist (1,802 / 1,798 files); self-tests 37/37; latest.json read back; the packaged Setup's harness install and verify; smoke launch |
| `install --package --sandbox --quake <fake> --relight --prepare` | **bug found, fixed** | 91 s, of which prepare 88 s (73 maps relit, now the expansions' too; dopa no models, mg1 7, mg3 51). The unfixed package left `autosave/mg1/start.sav` and `autosave/mg3/start.sav` (see below) |
| `install --feed http://127.0.0.1:8517/latest.json`, then verify | pass | 5.7 s; all files intact |
| First start | pass | no trap marker; the search path doesn't include the Quake folder's quakevr; Menu Detail 0, voice notes 0, `vr_setup_pending` 1, config 123, Loading notice 1; `VR: game code ...sbf/QuakeVR/quakevr/progs.dat: Quake VR's, this build's`; no relight line; Continue none; process start to first frame 1.58 s |
| Calibration | pass | welcome at 0.74 s, intro 7.74, height 15.75 ("eyes at 1.70 m"), body skipped at 28 s: "done. The glowing doorway behind you leads to the tutorial."; stickman shown 5,054 frames, hidden 0; board paths 4 found, 0 missing |
| Tutorial | pass | changelevel 300 ms; track 3; welcome "view message: 8.0 s at 1.29 (in view)"; tips held from 1.4 to 9.3 s; vrtutorial.bsp identical to the worktree's (railing gap; the lesson 7 pillar, shot from the door) |
| Tutorial playthrough (`vrtutorial_playtest.py`, seed 1) | test tool | in the sandbox it stops at room 2's button (the mock's off hand is 2 m away). Fresh 1.0.0 and 1.0.1 configs fail the same way. In the kit 14 gates pass up to "heal", then the known keycard flake |
| Arena, then the hub | pass | arena music track 148 (Scourge's track04); the waves came, player alive (70); Kill All, as in the 1.0.0 test; cleared, teleporter; "Loading...: changelevel vrstart put off" (`vr_loading_notice 2`); hub load 632 ms |
| Hub, e1m1, e1m2 (Normal) | pass | the second start lands in the hub; `vr_continue_available` 0 (lectern empty); "Loading..." before e1m1 and e1m2; loads 448 / 257 ms; `autosave/e1m1.sav`, then `e1m2.sav` |
| Restart, Continue | pass | "Continue: Castle of the Damned (e1m2), Quake, just now"; `vr_continue` loaded `autosave/e1m2` (241 ms) |
| Each campaign and back, sound on | pass | hipnotic start (track 8), rogue start (3), dopa e5start (4), mg1 start (5), mg3 start (9), id1 start (4), hub: 8 spawns, 7 put off, no error |
| Gungrab scenarios | pass | (1) the shortcut plus a config-only `Saved Games\Ironwail` base dir: its quakevr is not searched, no marker, this build's game code. (2) ironwail.exe started without `-basedir` from the Quake folder: "no -basedir: the exe's own folder added", this build's game code. That start wrote `history.txt` into the Quake folder, which Ironwail already did in 1.0.0 |
| 1.0.0 install, played (s0 saved on e1m1, config 122, Loading notice 0), quakevr.cfg edited | pass | 1.0.0's prepare 84.5 s |
| `update` to 1.0.1 `--prepare` | pass | 28 s (prepare 26.6); 48 files copied, 768 player files kept; the edited quakevr.cfg backed up and the shipped one restored; config 122 -> 123, Loading notice 0 -> 1; Continue: s0 |
| `reinstall --clean --prepare` | pass | 24 s; 12 files set aside (5 settings, 1 save, 6 personal); the 73 relit maps kept; the next start is a first start |
| Diagnostics mode (the Diagnostics shortcut, fake runtime fakexr_steam) | pass | the folder has console, gl_debug, openxr_debug ("core validation: ON"), openxr_validation.log ("Core Validation Layer is initialized" and only the known false positive), debug_output, qvr_openxr.txt and diagnostics.txt; OpenXR started |

**Fixed: Setup's preparation made autosaves.** The preparation's campaign step (`expnorm`) loads mg1's and mg3's start
maps for a second or more. The level-start autosave (`contgame`) fires 1.5 s into a map. So every fresh install, and
every update's preparation, left `autosave/mg1/start.sav` and `autosave/mg3/start.sav` behind. Continue (the main
menu's row and the hub's lectern) then offered "The Thirtieth Passing (start), Dawn of the Machine, just now" to a
player who had never played it. After an update, that save was newer than the player's own game, so the 1.0.0 player's
save `s0` lost to it. I checked both cases in a sandbox updated with the unfixed package. Fix: `Host_CheckAutosave`
(host.c) returns early under `-prepare`. Checked with the rebuilt package: a fresh install and an update leave no
autosave; Continue shows none and s0 respectively.

**For the coordinator:** the kit's run.ps1 breadcrumb filter (above).

**In the headset:** nothing new beyond the 1.0.1 items. After installing, the main menu has no Continue row until a
level has started.
- 1.0.1 Installer Statement: two claims instead of three ("does *not* mean a low-quality result", "... a lack of care, passion, or expertise"); CLI text, self-tests, screenshot harness, README and INSTALLER.md updated; Statement page fits at default and minimum size.
## Doom on the wrist gadget, and the gadget's touchscreen (feature branch, 2026-10-11, worktree `wristdoom`)

Not for 1.0.1: branch `agent/wristdoom`, shipped when Vittorio says so. Design, controls, the touch layout, the
touchscreen's interface for other apps and the decisions: [DOOM.md](DOOM.md).

- **Doom** is `qvr_doom.dll` (doomgeneric `dcb7a8db`, GPL-2, vendored in `Quake/vr/external/doomgeneric` with small
  `QVR_DOOM` patches: `README.QVR.md`), built by the solution (`qvr_doom.vcxproj`) beside the exe and loaded when
  first opened (`vr_doom`): only `qvr_doom_*` exported, Doom's globals its own; its memory a private heap and its C
  runtime static, so a shutdown frees all and a restart loads it afresh; `I_Error`/`I_Quit` return to the engine.
  Its clock is the engine's frames (a tic run when one is due); it runs in real time beside Quake, which never pauses
  for it; Quake's pause pauses it. Cost: 0.02 ms a frame on average, 0.18 ms at most (a tic run and drawn), the screen
  0.005 ms of CPU; nothing before it is opened.
- **WADs**: Freedoom 0.13.0 (Phase 1 and 2, BSD-3; the zip's SHA-256 checked) in `quakevr/doom/`; the player's own
  found on Steam (every library), GOG (the registry), `quakevr/doom/wads/` (the drop-in folder, its README tracked so
  the package makes it) and `vr_doom_wad_dir`, official ones known by MD5; the automatic pick prefers them; a WAD
  that fails falls back to the next.
- **The touchscreen** (`vr_touch.cpp`, `vr_touch_*`): the other hand's index fingertip on the gadget's screen (or the
  big panel) as touches (from in front only, hysteresis, debounce, a hard press through the face, a buzz), and an
  immediate-mode widget kit; while an app takes the gadget's screen the bullet-time tap is off.
- **Doom's controls**: the touch overlay (a drag stick, FIRE, USE, MENU, MAP, W-/W+, RUN, LOCK, X; Doom's menu by tapping
  its items, or UP/DOWN/OK/BACK; YES/NO), and while Doom has the focus (looked at 0.5 s, or touched; lost 0.5 s out of
  view) the gadget hand's stick and buttons (Quake sees them let go; they change hands only once let go); `vr_doom_controls` 0 touch only, 1 both, 2 the
  controller only. Sounds: the engine's sounds at the gadget (`VR_SndMemory` makes Doom's DMX lumps WAVs), quieter.
  Music: not done (the silent music module; Nuked-OPL3 and Chocolate Doom's OPL player would do it).
- Menu: VR Settings > *Doom on the Wrist Gadget* (also Wrist Gadget > *Doom*; `menu_vr 171`); Debug > Tests: Doom Info,
  Show Touches, Debug, Restart. `vr_gadget_screen_dump` writes Doom's screen while it shows. Mock:
  `vr_mock_hand_to <hand> touch <x> <y> [<cm>]`.

**Verified headless (the mock):** Freedoom started in 11-13 ms; the picture on the raised gadget (a screenshot), the
overlay hovered and in Doom's menu (`vr_gadget_screen_dump`); by touch: MENU, a tap on New Game (menu tap: Enter), OK,
OK: E1M1; the MOVE stick walked Doom's player from x -416 to 119; FIRE held: ammo 50 to 48, its sounds played through
the engine (one channel playing, `-Sound`); the off stick unfocused moved Quake's player (y -352 to -68) and not
Doom's; focused (the look) Doom's (x -416 to 76) and not Quake's; Quake's menu froze both clocks (186 tics), closing it
resumed them; looking away returned the controls and Doom kept running (201 to 246 tics); the off stick held as the focus came kept walking
Quake's player (y -352 to 115) and not Doom's, until let go and pushed again (Doom's x -416 to -236); touch only: the stick stayed
Quake's; `vr_doom_args "-warp 9 9"`: Phase 1's init error reported, Phase 2 tried and started; a restart reloaded the
DLL with fresh globals; the DLL exports only its eleven functions; `vr_menu_path_check`: 0 missing.

**In the headset:** open Doom (VR Settings > Doom on the Wrist Gadget), raise the wrist and look: does the stick take
Doom over quickly enough, and give it back as you lower the arm, without surprises in Quake? Touch the screen: is the
touch distance right (Touch Distance, Release Distance), are the buttons big enough, does the cursor help, does the
hard press fire when you mean it? Play a level by touch only and by the stick; try Doom's menu by tapping items, save
and load. Is the picture readable on the gadget (CRT Look, 4:3 Picture), is the big panel comfortable? Are Doom's
sounds from the wrist at a good level against Quake's (Volume)? Drop your own DOOM.WAD into quakevr/doom/wads and pick
it on the page.

## Controls as if another controller: binding audit, per-path fallback, profile logging, the bindings guard (1.0.1, 2026-10-11, worktree `bindfix`)

**The report** (1.0.0, fresh GOG Quake, Quest 3 through Virtual Desktop, VDXR 1.0.10): the grips grab nothing but the
flashlight, A does not jump but the *left trigger* does, no climbing. His `qvr_openxr.txt`: VDXR 1.0.10 (older than the
author's 1.1.0), 31 extensions, `XR_META_touch_controller_plus` not offered, session FOCUSED; 1.0.0 logged nothing about
bindings or profiles.

**What the symptoms match: Ironwail's own gamepad bindings, not another controller profile.** Ironwail's default
config (`Quake/default_cfg.h`, `Misc/pak/default.cfg`, which ships in `ironwail.pak`) binds `LTRIGGER +jump`,
`RTRIGGER +attack`, `LSHOULDER "impulse 12"`, `RSHOULDER "impulse 10"`, and nothing on `ABUTTON`. The VR controllers are
those same gamepad keys (vr_input.cpp: trigger LTRIGGER/RTRIGGER, grip LSHOULDER/RSHOULDER, A/X ABUTTON/XBUTTON...):
with those bindings the left trigger jumps, the right one fires, the grips cycle weapons (no `+grabmain`/`+graboff`, so
no grabbing and no climbing), A does nothing; only the flashlight still takes a grip because flashlight.cpp reads the
grip before the key (vr_input.cpp's `flashlight::Button::Grip`). That is his report exactly. A wrong *profile* would
look different: VDXR plays any profile on Touch controllers and remaps (the simple controller's `select` on the
trigger, its menu on the menu button), so A would do nothing, but the left trigger would be the off hand's trigger, not
a jump.

**The OpenXR audit (VDXR read from its source, `mbucchia/VirtualDesktop-OpenXR`, `action.cpp`/`mappings.cpp`, current and
as of 2025-12-06, which 1.0.10 is: it offers `XR_EXT_active_action_set_priority`, added then).** VDXR's
`xrSuggestInteractionProfileBindings` turns a whole suggestion down (`XR_ERROR_PATH_UNSUPPORTED`) for an unknown profile
or one path its table lacks; it then picks Touch if suggested, else WMR, Index, Vive, simple, in that order ("Emulate
Index controllers": Index first). Every path the game suggests for Touch is in VDXR's Touch table (x/y/a/b click and
touch, left menu, squeeze/value, trigger value and touch, thumbstick, its click and touch, grip pose, haptic: no
thumbrest, no proximity, no extension path), so the Touch suggestion is taken by VDXR 1.0.10 and 1.1.0 alike. Against
the OpenXR 1.0/1.1 specification every profile's paths are valid (khr/simple, oculus/touch, valve/index, htc/vive,
microsoft/motion, and the vendor ones: bytedance pico_neo3/pico4, htc vive_cosmos/vive_focus3, hp/mixed_reality,
samsung/odyssey). One real bug: Quest 3's profile was asked as `/interaction_profiles/meta/touch_controller_plus`; its
name is `/interaction_profiles/meta/touch_plus_controller` (XR_META_touch_controller_plus, OpenXR 1.1 core). The wrong
name was always turned down, harmlessly: every runtime played Quest 3 controllers as Touch ones. *Decision:* it is no
longer suggested at all (the 1.0.0 behaviour, the only one ever tested); `updateControllers` knows the right name.

**Changes.**
- `suggest` (vr_backend_openxr.cpp): a turned-down suggestion is rebuilt one binding at a time (the grip pose first),
  each binding the runtime turns down dropped and named (`WARNING: OpenXR: <profile>: <path> turned down by the
  runtime (<result>): not bound`, console and `qvr_openxr.txt`), the rest suggested; none taken means the runtime lacks
  the profile (`no bindings for <profile>: the runtime does not have it`, the log only). Every profile goes through it;
  the vendor profiles' hand-written "basic" fallback lists are gone (a turned-down menu button now costs only the menu
  button, 28 of 29 bindings, not 9). Every suggestion is logged: `OpenXR: bindings suggested for <profile> (n)` or
  `(k of n: m turned down)`.
- Controller profiles (`updateControllers`): looked up at the session's start (`controller profiles at the session's
  start: left ..., right ...`, usually "none yet"), at each `XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED`, and every
  2 s while a hand has none (a missed event); each change logged (`OpenXR: the left hand's controller profile: ...`,
  console and log), named in the crash report's VR line (`; controllers oculus/touch_controller /
  oculus/touch_controller`), and printed by `vr_debug_buttons` as before. A profile without the game's buttons (the
  simple controller, or one the game has no bindings for) gets a warning once a start: console, log, and a centre print
  once a game is on ("Your controllers are played as ...: controls may be limited").
- `qvr_openxr.txt` at the session's start: `OpenXR: the controller keys: LTRIGGER "+offhandattack", RTRIGGER ...` (the
  eight button keys' bindings), so a report shows gamepad bindings at once.
- The bindings guard, `vr_bindings_guard` (default 1; archived; 0 never): after every start-up config (`vr_startgame`,
  after `autoexec.cfg`), no grab bound to any controller key (trigger, grip, A/B/X/Y, stick click keys) means another
  config's gamepad bindings: a console warning naming LTRIGGER/LSHOULDER/RSHOULDER/ABUTTON's bindings and the loose
  `config.cfg`/`autoexec.cfg`/`ironwail.cfg` files on the search path (where they may have come from), then
  `exec vr_bindings.cfg`. A grip rebound to another grab (any binding containing "grab", an alias so named) is left
  alone. `vr_bindings_check` runs it by hand; `vr_bindings_reset` sets `vr_bindings.cfg` whatever the version (Debug >
  Logging > *Restore Controller Bindings*).
- Fake runtime (Misc/quakevr/fakexr): `FAKEXR_REJECT_PATH` takes several entries (`;`), each `profile=path` for one
  profile only; accepted suggestions are kept (each replaces the profile's last), and `xrAttachSessionActionSets` picks
  the profile as VDXR does (or `FAKEXR_PROFILE`), logs it with its bound paths, answers
  `xrGetCurrentInteractionProfile` and sends `XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED`.
- `xr_runtime_test.sh`: part 9's turned-down menu button now drops only that path (28 of 29); 9b: Touch with its
  finger sensors turned down (`oculus/touch=thumbstick/touch;oculus/touch=trigger/touch`): 25 of 29 kept, the runtime
  holds trigger, squeeze, X/A/B, the hands are Touch, the controller keys logged; every full profile turned down: the
  simple controller and its warning; 9c: `bind LTRIGGER +jump; unbind LSHOULDER; unbind RSHOULDER; vr_bindings_check`:
  the warning, then LSHOULDER `+graboff`, RSHOULDER `+grabmain`, LTRIGGER `+offhandattack`.

**Where the gamepad bindings came from: not found.** At a first start the order is `default.cfg` (Quake VR's: the VR
bindings), `config.cfg` (the game folder's `ironwail.cfg`; at a first start any `config.cfg` on the search path, e.g.
`id1/config.cfg` of the Quake folder, written by another source port with `unbindall` and Ironwail/Quakespasm's gamepad
bindings), `quakevr.cfg` (`vr_checkbindings`: at `vr_bindings_version` 0, a first start, `vr_bindings.cfg` again over
whatever came before), `autoexec.cfg` (any on the search path, every start: the Quake folder's `id1/autoexec.cfg`
too). So a foreign `config.cfg` alone is repaired at a first start; candidates are an `autoexec.cfg` that binds or
`unbindall`s, a saved `ironwail.cfg` already at bindings version 3 with gamepad bindings (an earlier install or run),
or the bindings changed in the game (Ironwail's key menus). 1.0.1's guard repairs all of these at the next start and
names the loose config files it finds; the player's answers below should tell which.

**What to ask the player:**
1. Open the menu, the corner's *Console*, and type `bind LTRIGGER`, `bind LSHOULDER`, `bind ABUTTON`: send what they
   say (expected with the bug: `"+jump"`, `"impulse 12"`, not bound).
2. Send `quakevr\qvr_openxr.txt` and `quakevr\ironwail.cfg` from the Quake VR install folder (the folder the
   installer installed to, with `quakevr` in it).
3. Any `config.cfg` or `autoexec.cfg` in the GOG Quake folder's `id1` (and `rerelease\id1`), and in
   `%USERPROFILE%\Saved Games\Nightdive Studios\Quake` and `%USERPROFILE%\Saved Games\Ironwail`? Did he run another
   source port (Ironwail, vkQuake, Quakespasm) in that GOG folder before?
4. How he starts the game (the installer's shortcut, Virtual Desktop's game list, Steam, his own shortcut), and whether he
   used *Reset to Defaults* or Ironwail's key-binding menus.
5. Virtual Desktop: the streamer's OpenXR runtime setting (VDXR or SteamVR), and whether "Emulate Index controllers"
   (or similar controller-emulation options) is on.

**Workaround on 1.0.0:** menu, corner *Console*, type `exec vr_bindings.cfg` (the VR bindings; saved at quit). If they
come back wrong at the next start, something runs after Quake VR's configs: an `autoexec.cfg` (question 3); renaming it
fixes it. Switching Virtual Desktop's OpenXR runtime to SteamVR (or `+vr_xr_runtime 2`) would not help if this is the
bindings, which is what the symptoms say; it is the thing to try only if `bind LTRIGGER` already says
`+offhandattack`.

**In the headset:** with VDXR and with SteamVR: the first lines of `qvr_openxr.txt` after a start (each profile
`bindings suggested ... (n)`, no `turned down`), `the left hand's controller profile: /interaction_profiles/oculus/
touch_controller` (with VD's "Emulate Index controllers": valve/index_controller), `the controller keys:` with the VR
bindings; triggers, grips, A/B/X/Y as before. Then in the console `bind LSHOULDER "impulse 12"; bind RSHOULDER
"impulse 10"; bind LTRIGGER +jump`, quit and start again: the console says the bindings were set again, the grips grab.
Doom opens from the wrist gadget's side button too: 6 presses within 0.4 s each (`vr_doom_light_presses`, `vr_doom_light_gap`), counted before the button's cooldown, the light put back as it was; one press then closes Doom without toggling the light; `vr_gadget_button_press` (Debug > Tests); DOOM.md.

## Flashlight: Always on Head (1.0.1, 2026-10-11, worktree `headlight`)

The author's request: a third flashlight mode, simpler than the belt torch. `vr_flashlight` keeps its meaning (0 off,
1 Immersive: the belt torch as before) and gains **2, Always on Head**: the torch is the round-21 head torch
(`Mode::OnHead`, its pose, light, beam and zone code unchanged), worn at the temple on `vr_flashlight_side`'s side, and
nothing else: no torch on the belt (it is never in `Mounted`), no cord, no taking it with the grip, no B/Y unclipping,
no clipping on a gun, no late grip; an open hand's trigger at it switches it (the existing `button` path, `reachesLamp`
on the head's zone). The rows *Flashlight* (VR Settings) and *Chest Flashlight* (Body and Display) are now cycles: Off,
Immersive, Always on Head (names unchanged: the calibration boards' `{menu:...}` lines still match).

- `vr_flashlight.cpp`: `headMode()`; `wearOnHead()` each view in mode 2, also while a restore waits (never a frame on
  the belt): from anywhere else straight to the head, at once (the stale grip swallow of a restored held torch
  cleared). A `vr_flashlight` callback (`onModeChanged`): set to 2, the light on; from 2 to anything else, the torch
  home on the belt at once (as `vr_flashlight_home`), on or off as it was. `button`: in mode 2 only the trigger at it.
  `wantsSecondary` false in mode 2 (B/Y at the head is the game's).
- On by default: when the mode is chosen; at a fresh start (`reset`: `on = headMode()`); a save made in mode 2
  restores it as saved (on or off; also across changelevels); a save made in another mode (on the belt, in a hand, on
  a gun) loaded in mode 2: worn, on. One torch, one state: no duplicate. Not handled: a mode-2 save loaded in mode 1
  keeps the torch clipped on the head (a valid Immersive state: B/Y or the grip at it takes it off).
- Coop: client-side as before (each client its own torch and mode); the saved state is single-player only, as before.
- Weapon Offsets > Flashlight's *Clip the Torch on It* still sets `vr_flashlight 1` (it already did from 0): from
  mode 2 it switches to Immersive. Left as is.

**Tested (mock, `vr_flashlight_probe`, `vr_gear_status`):** mode 1 on e1m1: mounted, off, lamp at z 107; `vr_flashlight
2`: onhead, on, lamp at z 120, cord 0, one flashlight entity. Off hand at the left temple (`vr_mock_hand off -0.08 1.72
-0.06`): lit; trigger presses 30 frames apart: off, on, off; grip and Y there: still on the head, unchanged. Saved off,
toggled on, loaded: off; changelevel e1m2: kept; `map e1m2` (fresh): on. Back to 1: mounted, light as it was.
`vr_flashlight_give left` in mode 1, saved (held), switched to 2: on the head; loaded that save in mode 2: onhead, on.

**In the headset:** Chest Flashlight > Always on Head: the torch at your left temple, lit; open hand at it, trigger:
off and on; grip and Y there do nothing to it; nothing on the belt; Flashlight Side Right moves it to the right
temple. Back to Immersive: the belt torch is back.
Targeted 1.0.1 check of saves and controller bindings (worktree `svctest`, headless): autosave names, Continue across campaigns and packages, missing package, 1.0.0 saves, migration, no autosave on vr*/multiplayer/-prepare, Doom's saves unseen, xr_runtime_test 103/0, guard, menu modes, Doom's off-hand routing all pass; fixed Continue naming a package "map package <sha>" when first worked out before the map index arrived; the guard leaves `bind LSHOULDER "impulse 12"; bind LTRIGGER +jump` alone (RSHOULDER still grabs: by design).

## Only Quake VR's own autoexec.cfg and config.cfg at the start (1.0.1, 2026-10-11, worktree `owncfg`)

Player bug (confirmed): a fresh install next to a GOG Quake had `bind LTRIGGER` = `+jump`: Ironwail's gamepad
bindings won over vr_bindings.cfg. Suspected: another source port's `autoexec.cfg` / `config.cfg` in the Quake
folder's `id1` (or a re-release folder), which every start read from anywhere on the search path.

- **Cmd_Exec_f** (Quake/cmd.c `Cmd_ForeignStartupConfig`): `exec autoexec.cfg` and the bare `exec config.cfg` (the
  legacy config, only reached with no saved `ironwail.cfg`) run only when the file is in `com_gamedir` (the install's
  quakevr; a pak in that folder counts too). Found only elsewhere (id1, rerelease, another base dir, a user dir): not
  run, one console line per file and start: `Quake VR: skipped <path>: not in Quake VR's own folder (<quakevr>),
  another game's or source port's (vr_foreign_configs 1 runs it)`. A skipped config.cfg still gets
  `vr_migrate_config new` (the first start's settings), so the first start is unchanged: VR Calibration, Standard
  menus.
- **`vr_foreign_configs`** (archived, default 0; Debug > Logging > *Other Ports' Startup Configs*): 1 = as before.
  **`-foreignconfigs`** on the command line does the same for one run: a saved setting can't reach a fresh install's
  config.cfg, and the test kits write their script to `id1/autoexec.cfg` (qvrprof.sh passes it now; the kit's
  run.ps1/runq2.ps1 need it too, or every run's script is skipped and times out).
- **The bindings guard** already ran at `vr_startgame`, after autoexec.cfg and the command line; it missed a partial
  clobber: it only fired with no grab on *any* controller key, and the reported file's `bind LSHOULDER "impulse 12"`
  left RSHOULDER `+grabmain`. Now each controller needs a grab on one of its keys; its warning names the controller
  (`no grab on the left controller's keys (...)`). Its "where from" list names only the configs that ran.
- **Decisions:** explicit `exec <file>` typed or bound runs from any folder (unchanged); only the two names
  quake.rc and the dedicated server exec at the start are restricted, by name, so a typed `exec autoexec.cfg`
  follows the same rule (simpler than telling quake.rc's exec from a typed one; any mod's own quake.rc is covered).
  Quake's own id1/config.cfg binds are no longer imported at a first start (the task's rule; 1 restores it).

**Tests** (headless mock, kit base `bases/owncfg/qbase`): hostile `id1/autoexec.cfg` (`bind LTRIGGER +jump; bind
LSHOULDER impulse 12`) → skipped line, LTRIGGER `+offhandattack`, LSHOULDER `+graboff`, no guard warning. Same file in
`quakevr/autoexec.cfg` → runs, then the guard at vr_startgame: `no grab on the left controller's keys`, lists
quakevr/autoexec.cfg, `execing vr_bindings.cfg`. `vr_foreign_configs 1` in the saved config → the id1 one runs, the
guard restores (lists id1/autoexec.cfg). A harness script with `-foreignconfigs` runs and quits (exit 0). Fresh
install (no ironwail.cfg) with a hostile `id1/config.cfg` (`vr_menu_level 2` in it) → skipped, LTRIGGER
`+offhandattack`, `vr_menu_level` 0, "a first start: VR Calibration".

**In the headset:** with an `autoexec.cfg` holding `bind LTRIGGER +jump` in the Quake folder's id1: the console's
skipped line at the start, the left trigger fires. Put it in quakevr instead: the left trigger fires again after the
start (the guard), with its warning in the console.

## 1.0.1: the author's judo throw values are the defaults

His voice note: "I've tweaked the judo throw values a little bit to make them feel even better. Please make those the
new defaults." His config of 2026-10-11 against the shipped defaults (Combat > Holding Enemies), config 124 moves a
config still at the old value:

| cvar | was | now |
|---|---|---|
| `vr_foegrab_throw_push` (Throw Push) | 190 | 240 units/s |
| `vr_foegrab_throw_lift` (Throw Lift) | 170 | 120 units/s |
| `vr_foegrab_throw_topple` (Topple) | 300 | 200 deg/s |
| `vr_foegrab_throw_feet_speed` (Feet Speed) | 60 | 30 units/s |
| `vr_foegrab_throw_grunt` (Throw Grunt) | 0.6 | 0.75 |

The Throw Grunt slider steps 0.05 and shows two decimals (0.75 read "0.8"). Unchanged: `_twist` 140, `_speed` 2.5,
`_spin` 0.55 (his are the defaults). **Not promoted:** his `vr_knockdown_push` 1.3 (Knockdowns > Launch): the judo
throw's launch is times it (QC VR_Knockdown_Start), so in his game the throw leaves at 312/156 units/s, but it is every
knockdown's launch (shoves too) and was left out on purpose last round (the shove redesign). If the throw feels weaker
on a fresh config, that is why: either promote 1.3 or fold it into Push/Lift (312, 156).

**Test:** `vr_cfg_version 123`, the five at the old values (the grunt at a hand-set 0.66), `vr_migrate_config` →
240, 120, 200, 30 at default, the grunt kept at 0.66, `vr_cfg_version` 124.

## 1.0.1: a clone of the body while tweaking

His voice note: "It's really hard to tweak the flashlight head zone because you can't really see it from the first
person perspective. When I'm tweaking the flashlight zones, the holsters, the ammo pouch, and anything attached to the
player's body, a clone of the body should appear in front of the player while the tweaking menu is open."

- **What:** while a VR page tweaks something worn on the body, the body as drawn this frame (with its head), the hands,
  the holsters and what is in them, the pouches, the gadget and the torch are drawn again, 1.4 m from the eyes, facing
  you, **as a mirror** (reflected about a vertical plane half way: your right hand moves the image's hand on your
  right, as in a bathroom mirror; chosen over a turned copy, whose left and right read swapped). The zones drawn on the
  body for the page are drawn on the image at the same places, live with the sliders: body::queueDebug's (the holster
  pair whose calibration row is chosen, Show Hip/Chest/Back Holsters, Show Grenade Pouch's pouches, the virtual stock)
  and the flashlight's head zones (temples, forehead) and the held torch's middle, which from your own view are skipped
  (round your eyes). The head zones show on the image with Show Flashlight Zones, or while a `vr_flashlight_head*` row
  is chosen.
- **When** (menu::bodyClonePage): the menu open on a VR page, and either the page is *Flashlight - On a Gun or Head* or
  *Weapon Offsets - Holstered*, or a row of a body setting has been chosen on the page shown (then kept until you leave
  the page: VR Settings' holster calibration and ammo pouch rows, the hip/shoulder/upper holster and grenade pouch
  rows, Flashlight's belt Forward/Up/Out/Lean Out and Head Clip Range, Wrist Gadget's Size and mount, the virtual
  stock's, the pauldrons'). Off as the menu closes.
- **Where** (vr_bodyclone.cpp): beside the menu panel (it is drawn over everything, so an image behind it would be
  hidden): the panel's half width, the image's (0.45 m) and 4 degrees off its middle, on the side away from the main
  hand (its laser works the menu). The mirror stays put while you stand within 0.6 m of where it was placed and the
  panel keeps its yaw: lean in and the image comes closer, as a mirror's.
- **How** (vr_view.cpp addClones): each worn or held view entity (not the guns lying about, the review's ghosts or the
  enemies' flashes) gets a copy in a parallel array (`clones`, one per Entities slot) with its place reflected and its
  turn the reflected one (a turn and a flip across the model's Y, so `mirrored` toggles, as the off hand's models).
  view::find() gives the copy's own ViewEntity; view::cloneSource() maps it back for the hooks keyed on the entity:
  VR_AliasBonePoses (the source's bones: the body's with its head, its shadow-map skin) and avatar::modelScale. Not a
  shadow caster (collectAliases skips it), not solid. The window's spectator view sees it too.
- **Settings** (archived; Debug > Views): `vr_menu_body_clone` 1 (*Body Clone While Tweaking*: 0 off, 2 every VR page,
  for tests), `vr_menu_body_clone_distance` 1.4 m (0.5 to 5), `vr_menu_body_clone_side` 0 (away from the main hand; 1
  left, 2 right, 3 straight ahead, behind the panel). `vr_menu_body_clone_info` prints whether it is shown, its yaw,
  where its eyes are and how many entities it drew.
- **Cost:** menu closed, one test of `key_dest` a frame (bodyclone::mirror's first call); open, about 19 more alias
  models (the body's skinning shared).

**Tests** (headless mock, e1m1): VR Settings on *Height* → off; *Hip Holsters Forward* → on, yaw 155 (view 90: 65
degrees left of the panel, the main hand on the right), its eyes 1.40 m off, 19 entities, the body among them; back
on *Height* → still on (kept on the page); *Body* page → off; *Ammo Pouch Forward* → on; `vr_menu_body_clone 0` → off;
the menu closed → off. *Flashlight - On a Gun or Head*, *Wrist Gadget > Along the Arm*, *Weapon Offsets - Holstered*,
*Flashlight > Forward* → on; the Debug > Views row cycles it to 2. Eye shots (`vr_eyeshot 1`, `vr_mock_look 15 65`):
the image with its head, hands and gadget, the head zone's rings round its head; with *Hip Holsters Forward*
(`vr_mock_look 30 65`): the hip pair's spheres at its hips.

**In the headset:** open Flashlight > On a Gun or Head (or VR Settings and pick a holster or ammo pouch row): the
image beside the menu, left of it if right-handed; raise your right hand: the image's hand on your right rises. Move
Head Zone Up: the balls round the image's head move. Check the image is not inside a wall in tight rooms (it is
depth-tested: a wall nearer hides it; Body Clone Side or Distance then).
## Pools piled up: no square holes (1.0.1, 2026-10-11, worktree `decalstack`)

His playtest: gibs piled on the floor and shot apart, each new pool made those under it vanish in square holes. The
cause is the world decal grid's cap, not decal101: each 32-unit cell's bucket listed only its newest 64 marks, counting
every mark whose box (plus the parallax's 12-unit reach) touched the cell, so where a pile passed 64 an older pool
was dropped in that cell and kept in the next: a hole with the cell's edges. The build before decal101 (its three
commits reverted) shows the same holes pixel for pixel (`decal_stack_test.sh`: 10223 pixels lighter there, 10222 in
1.0.1); the cap is as old as the marks in the world's shader (2026-10-04), so 1.0.0 had it too.

**Fix** (`vr_decals.cpp` worldBuckets/buildWorld, `vr_glsl.h` DecalsAt):
- a mark's memberships are only the buckets of the cells reaching its rectangle (decal101's listing), and only those
  count towards a bucket's cap;
- the cap is 255 (the header's 8-bit length; the shader no longer stops at 64);
- newest first, a mark is listed only if each of its buckets has room, and then in all of them: never a hole. Past 255
  marks on one cell the oldest go whole, as the ring's oldest do. `vr_decal_count` prints the buckets over 255, the
  marks left out, the most marks in one bucket and the longest list.

**Checked**: `Misc/quakevr/decal_stack_test.sh <agent>` (new; vrfiringrange, 3 pools stacked, then 128 pools and
splatters clustered round them: `vr_decal_stress ... stack|cluster`, Debug > Test Effects > Decal Stress: Stacked
Pools). `decal_stack_check.py` counts pixels lighter after the new marks than before (the marks multiply the texture:
more only darken; pixels the control shot saw move are left out): before decal101 10223, 1.0.1 10222, now 7 to 19 in three runs (the
threshold is 50). In the rocket fight (`play_e1m1_lights`) 1.0.1 had 40 buckets capped; now the longest list is 73,
none left out, 9.15k grid entries (9.2k before).

**Cost** (`play_e1m1_lights`, 2782 eyes, `--exclusive`, world+brush GPU ms, 4 runs each, interleaved): before
decal101 3.07, 1.0.1 2.77, now 2.87: two thirds of decal101's 0.30 ms kept. The rest is the marks the cap dropped,
drawn again (the shader's old 64 bound with the new grid: 2.83, within the spread). The grid build walks a mark's (now fewer)
memberships twice, to check for room and to count.

`Misc/quakevr/decal_grid_test.py` (the grid word for word against the 2026-10-08 reference) no longer applies: the
grid now lists marks the reference did not.

**In the headset**: pile gibs, shoot them apart one by one: every pool stays under the newer ones, no square gaps.
## Respawn loaded the map several times (1.0.1, 2026-10-11, worktree `respawnloop`)

Playtest: dead in the start map's lava, A to respawn, the map loaded five or six times in a row. Reproduced headless
(e1m1's slime, `vr_loading_notice 2` so the mock headset defers like a real one, A held): 4 spawns; with the button
held 10 frames while the command buffer waited, 10 autoloads in a row.

**Root cause**, three pieces together:
- `PlayerDeathThink` calls `respawn()` (→ `localcmd("restart")`) on every server frame the button is down until the
  restart actually runs. Harmless when it runs the next frame; but "Loading..." (vr_loading.cpp) puts the level
  change off for frames, and each of them queued another `restart`.
- Ironwail's `Host_AutoLoad` appends `load <last save>` and runs the command buffer: it runs every restart queued
  ahead of its load first, each starting its own autoload (nested), so each queued restart became one more load.
- With "Loading..." on, the first autoload's load is put off; `Host_Restart_f` then puts itself off too, but
  `VR_LoadingDefer` read the command line after Host_AutoLoad's nested buffer run had re-tokenized it: it recorded
  `wait`, not `restart`. The player's own respawn was lost, and it was the extra restarts queued meanwhile that
  loaded (and then reloaded fresh) the map.

**Fix:** QC `respawn()` asks for the single-player restart once (`nosave float vr_restart_queued`, a new server starts
it at 0; MG1 Horde's `MGH_restart_queued` already did so). `Host_Restart_f` ignores a restart while an autoload is in
progress (`sv.autoloading`: any progs). `Host_AutoLoad` re-tokenizes its caller's command line after running the
buffer (restart and changelevel's own deferral read it again), and no longer prints "Autoload failed!" when the load
was only put off (`VR_LoadingWaiting`).

**Tests** (headless, A held from the death on; one SpawnServer in each): e1m1 slime, start lava, MG1 start lava, each
with the level-start autosave (one `Loading game from autosave/...`) and e1m1/start without (`sv_autosave 0`: one
`restart`), all with `vr_loading_notice` 0 and 2. Engine guard alone: `restart;restart;restart` typed while dead, and
the press held 10 frames behind a waiting buffer: one load.

**In the headset:** die in the start map's lava, hold A: "Loading...", one load of the level-start autosave, then
play. Same with a quick tap, and on a map with no autosave yet (a plain restart).

## Anchors are points on the models' surfaces (worktree `anchors`, 2026-10-10; a feature branch, not for 1.0.1)

(Superseded in part by "Anchors without vertex indices" below: no legacy indices, hints or conversion tools any more.)

The author approved deprecating vertex-index anchors so the weapon models can be edited freely. Before, every anchor
(the weapon settings' `hand_av`, `2h_hand_av`, `muzzle_av`, `wpnbtn_av`, `wpntxt_av`; vr_view.cpp's magazine mounts;
vr_shells.cpp's ejection ports) was an index into QuakeSpasm's old BuildTris strip order, rebuilt from the file by
vr_anchor.cpp: deleting or re-ordering triangles renumbered it under the anchors (the Blender export refused such edits).

**The format** (Quake/vr/vr_anchor.hpp): an anchor is text. `"x y z"`: a point on the model's surface in its own
coordinates as frame 0 has them (scale and origin applied: what `vr_anchor_info` prints and Blender shows);
`"x y z t"`: the same with a triangle hint, taken among triangles as near as the nearest; one number: a legacy index,
still read as before (old takes' headers, an `exec`'d old config). At first use per model and text the point is bound to
the nearest triangle of the rest pose (frame 0; a group's first pose): barycentric weights over its three vertices,
snapped to a corner it is within 0.001 of. Among triangles within 0.001 of the nearest: the hint if it is one; else
those with the point on a corner (not a triangle of another part passing that close), the first. Each frame the same
weights over the posed vertices (as drawn: the renderer's lerp, then the zero blend towards frame 0) give its place:
it follows the animation, and a renumbering changes nothing. The bindings are a `mem::Cache` ("anchors", per model and
text), emptied on a game change and a model reload, so `vr_model_reload` rebinds an edited model's anchors.

**Conversion.** The engine converted the shipped anchors itself (`vr_anchor_from_index <model> <index>`: the vertex's
rest place, written with 4 decimals; a hint only where another binding the point could take goes elsewhere in some pose
of the file): vr_weapons.inc's 144 defaults of the slots with models (the placeholders' "0" stay legacy), the six
magazine mounts, the two shell ports (`Misc/quakevr/anchors.py convert-commands` / `apply`). Three need a hint: the
rocket launcher's, the proximity gun's and the multi-rocket launcher's muzzles (and the proximity gun's button) sit on
the collapsed muzzle flash, whose six vertices are in one place at rest and fly apart (up to 20 units) when it fires.
**vr_wofs_version 41** converts a config's legacy indices once (`convertAnchors`, after the other migrations; a slot
still at a legacy default keeps it): from the baseline config (version 21), 99 converted in 87 ms on the first slot
lookup, and all 160 anchor cvars end exactly at the new defaults' text; a player's own index (`muzzle_av_02 "5"`)
became its vertex's point. Motion take headers write an anchor's spaces as `_` (the anchor parser reads `_` as a space).

**Verification.** For every converted anchor, the legacy index's vertex against the point's binding in every pose of
the model (`vr_anchor_compare`; and the same independently in Python, `anchors.py table`): 144 anchors, worst
**0.00000** model units (weights snap to exactly 1, so the engine's sums are bit-identical; blending is linear in the
weights, so in-between frames match too).

| Model | Anchors | Poses | Engine max | Script max |
|---|---|---|---|---|
| v_axe, v_shot, v_shot2, v_nail, v_nail2, v_light, v_lava, v_lava2, v_plasma, v_rock, v_multi | 5-6 each | 9 | 0 | 0 |
| v_rock2, v_multi2, v_laserg, v_prox | 5 each | 7 | 0 | 0 |
| v_hammer | 5 | 5 | 0 | 0 |
| hand | 5 | 18 | 0 | 0 |
| v_grpple, v_gruntgun, v_enfrifle, v_crowbar, v_ksword, v_hksword | 5 each | 9 | 0 | 0 |
| v_chainsaw | 5 | 11 | 0 | 0 |
| v_toolgun | 5 | 1 | 0 | 0 |
| owned/mg3 v_hammer, v_hammer_glow | 5, 6 | 5 | 0 | - (not in the repo) |

A/B of the builds (the base commit with the same probe, against this one): `vr_fixed_frames 1`, seeds, e1m1, fifteen
weapons put in the main hand (axe to the crowbar: impulses 153-167) fired, five in the off hand (mirrored), each
`vr_anchor_where` five times through its firing animation (muzzle, ammo button and screen, magazine and well, in the
world and in the gun's frame), plus `vr_hotspots_check` (every slot's hand, muzzle and two-handed grip anchors, both
hands): 548 lines, **0 differences**. (Without fixed frames two runs of one build differ by up to 4.7 units in the
firing frames: the animation's timing, not the anchors.) `anchors.py selftest`: every anchor bound on its model with
the vertices and triangles reversed and every triangle's corners turned stays put in every pose, but the four hinted
ones (a hint is a triangle number).

**Tools.** `vr_anchor_info <model> <anchor>` (binding, weights, distance off the surface, ties),
`vr_anchor_nearest <model> x y z` (now prints the nearest surface point's anchor, to paste), `vr_anchor_from_index`,
`vr_anchor_compare`, `vr_anchor_where` (Debug > Reports > Weapon Anchors). `Misc/quakevr/anchors.py`: the binding in
Python (the engine's rules), `list`, `check <model> [old]`, `selftest`, the conversion's commands and table.
polish_weapons.py checks "each anchor within 0.1 units of the surface and of where it was, in every pose" (bound on
the old model and on the new) instead of "the anchor vertex's bytes unchanged"; its run reproduces all 19 shipped
models byte for byte (reuv_weapons.py and refine_light.py take the anchors' vertices from their points:
`anchors.strip_indices` / `anchor_vertices`). The Blender add-on (mdl.py) binds the anchors the same way and reports
each one's move in every frame (MOVED past 0.01 units; RENAMED and the export refusal only for legacy indices); it
also reads the magazine mounts and the current shell table now. MODELS_IN_BLENDER.md "Anchors" rewritten.

**For the author.** Nothing to see: everything is where it was. In VR: fire each gun (the flash, tracer and shot at the
muzzle), the ammo screen and button on the guns that have them, the nailguns' and thunderbolt's magazines in Immersive
reloading (kick with the gun), the shotguns' shells. Open question: the three launchers' muzzle anchors on the
collapsed flash jump with it in the firing frames (as before); `vr_anchor_nearest` on the barrel's tip would give a
steady one. Merging: a branch that changes a weapon model or an `*_av` default before this lands needs its anchors
converted again (`anchors.py convert-commands <rev>`, run them in the game, `apply <log> <rev>`, then
`compare-commands`/`table`).


## Anchors without vertex indices (worktree `anchors2`, 2026-10-11; the same feature branch, not for 1.0.1)

The author: "Full replacement for vertex attachments, also simplify code around those areas." The section above is
the first step; this one takes the vertex indices out (its legacy paths, conversion tools and the hint are gone).

**The format** is `"x y z"` only (vr_anchor.hpp): a point on the model's surface, bound to the nearest rest-pose
triangle (snapped to a corner it is on; among triangles as near, those with the point on a corner, the first in the
file). No triangle hint: a fourth number is ignored. Text that is not three numbers binds to the model's first vertex
(vr_anchor_info says it is not an anchor). The engine no longer reads a lone number as a strip-order index.

**The migration** (vr_wofs_version 41) gives each of a config's anchor values that is not a point its slot's default:
those are the old defaults' vertices' points, the same in every pose (below), so a config at the old defaults ends
exactly where the shipped one is. It no longer translates indices through QuakeSpasm's strip order of today's files:
once the models are edited that order says nothing about 1.0's vertices. A player's own index (no menu sets one; only
a hand-edited config has it) gets the default too. Checked: a version-40 config, `muzzle_av_06 5`, `hand_av_02 472`:
"160 anchor indices replaced by their points", each cvar at its default after the first slot lookup.

**No anchor needs a hint.** The grenade launcher's (slot 5, v_rock.mdl), proximity gun's (10) and multi-grenade
launcher's (13, v_multi.mdl; the notes above call them the rocket launchers) muzzles were on the collapsed muzzle flash (six vertices in one place at rest, the triangles 0..7) and followed it in
the firing frame: 8.6 model units from the barrel there. Re-picked on the barrel's end face (the nearest point of the
barrel to the old one, 0.42 units behind it: about 1.3 mm drawn), unique (no other triangle as near):
`30.3762 -0.0556 5.9837`, `30.3752 -0.0558 5.9597`, `30.4389 -0.0494 6.0182`. The proximity gun's hidden ammo button
(its WpnButtonMode is 0) was on the flash too: the same point as its muzzle now. The placeholders' anchors are `0 0 0`.

**The monsters' guns** (vr_weaponfx.cpp: the soldier's and the enforcer's muzzle and rear, the middle of 14 + 6 and 7 + 5
vertices) are anchors now, the points of those vertices (each once, with how many of the vertices it stands for: a UV
seam's twins share a point; review fix, below); the model's vertex count still says it is the model they were picked on.

**Code.** vr_anchor.cpp 1175 -> 702 lines: QuakeSpasm's BuildTris strip search, the legacy and hint paths, the
every-pose model read, `legacyToText`, `vr_anchor_from_index` and `vr_anchor_compare` gone; the tie count is worked
out only by `vr_anchor_info` / `vr_anchor_nearest`, and only triangles that move elsewhere in some pose count (a UV
seam's twins don't). The API: `anchor::posedAnchor(entity, text, zeroBlend)` (model byte space) and
`view::anchorPosition` (the world: a view entity, or any alias entity drawn mirrored or not; `entityAnchorPosition` is
an overload of it now), `anchor::isAnchor` for the migration. `weapons::Key::*AnchorVertex` are `*Anchor` (the cvars
keep their `*_av` names: configs). Kept, not attachments: `anchor::posedVertex` / `view::vertexPosition` (the melee
HUD's embers off a random vertex), `posedVertices` (the whole mesh: magazines' boxes, the firing motion), and
vr_monstermods.cpp's knownSwords vertex lists (which triangles to hide, by vertex counts).

**Tools.** anchors.py: `list`, `selftest`, `check`, `point <model> <vertex>` (a vertex's anchor, for the generators);
the conversion commands gone, `vertex_text` the generators' printer. The Blender add-on (mdl.py, mdl_blender.py,
__init__.py): no strip order, no RENAMED, no Remap Anchors option; the report covers the monster guns' anchors too
(soldier.mdl, enforcer.mdl, after their checks.py lines); the gadget's points compare each old vertex near them with
the new model's nearest vertex (not by index). The generators print points: improve_weapons.py (the double shotgun's
new hand anchor), improve_weapons_alt.py (the alternates' anchors: the matched vertices' points), make_chainsaw.py,
make_crowbar.py, make_enemyguns.py (its `pin_anchors`, which ordered the triangles to keep two strip indices, is gone),
make_toolgun.py; their old index checks (`check_anchors`, strip-prefix asserts) are gone and the hands' old anchors
are the frozen sources' vertices (`hand_vertex`, the strip index in a comment). improve_weapons.py and
improve_weapons2.py ran again (they had broken on the converted defaults). polish_weapons.py finds the shotgun's
fore-end by its two-handed anchor's point; reuv_weapons.py lost `keep_anchors` (it never dropped a chart in the shipped
run).

**Decision: reuv_weapons.py keeps its strip groups** (`strip_groups`, `merge_by_strips`: charts joined along
QuakeSpasm's old strips). No anchor depends on them, but the shipped skins (plasma gun, super nailgun, rocket
launchers, grappling hook, Mjolnir) were mapped so: without them a rerun of polish_weapons.py re-maps other faces and
the skins change. Dropping them is one call (`charts = merge_by_strips(...)`) for when the skins are redone anyway.

**Verification.**
- Every anchor in every pose against vr-ironwail (bc3163ed4): a probe command dumping an anchor's place in every pose
  of its model (model units, `%.5f`), the base build with the indices (strip order; the monster guns by file vertex),
  this build with the points: 165 anchors (125 weapon slots' with models in the repository, 6 magazine mounts, 2 shell
  ports, 32 monster-gun points), 4848 anchor-poses: **0.00000** for all but the four re-picked (0.42 at rest, 8.62 at
  most in the firing pose). The probe isn't committed (scratch/probe_new.txt, probe_base.txt, gen_dump.py).
- Scripted fire/reload/two-hand A/B, the same probe (vr_anchor_where ported to the base with its indices): `vr_fixed_frames
  1`, seeds 7, e1m1, `vr_weapon_grip_mode 1`, grips held; 15 weapons in the main hand fired (grappling hook to toolgun:
  impulses 151-169 but the launchers), 5 in the off hand, `vr_anchor_where` six times through each firing (muzzle,
  button, screen, magazine and well, world and gun frame), then `vr_hotspots_check`, then the launchers last: 909
  lines, **0 differences** before the launchers; in their blocks only their muzzles (and the proximity gun's hidden
  button) in the gun's frame, and everything by the same 25.7 units in the world once the grenade launcher's grenade
  (from the moved muzzle) knocked the player elsewhere.
- anchors.py selftest: 165 anchors bound on their models renumbered, 0 moved. The add-on's report on every shipped
  model against itself: 165 anchors ok. The engine's dump against anchors.py's and the add-on's bindings: 0.00000 on
  every weapon model.
- polish_weapons.py reproduces all 19 shipped models byte for byte (as before); improve_weapons.py, improve_weapons2.py
  (now runs), improve_weapons3.py, make_chainsaw.py, make_crowbar.py: their models byte for byte as the old scripts
  wrote them; make_enemyguns.py: the triangles in another order (no pin_anchors), its anchors where they were in every
  pose (`anchors.check` against the old output: 0.0000) and the printed points are the shipped defaults.

**Found:** the toolgun's muzzle (slot 26) is `15.0245 -0.6162 5.3987`, the conversion of index 614, which was
make_toolgun.py's printed *file* vertex number taken as a strip index: 0.86 units from the lens's front centre the
script meant (`15.4 0.0064 5.7941`, which it prints now). Kept as shipped (behaviour unchanged); the author may want the
script's point. (Taken later: "Review fixes" below, weapon settings 42.)

**For the author.** In VR: fire the grenade launcher, the proximity gun and the multi-grenade launcher: the flash,
smoke and shot now leave from the barrel's end face in the firing frame too (before, the point jumped with the flash's
folded vertices). Everything else is where it was.

## Smoother animations: `r_lerpmodels 3` (feature branch, 2026-10-11, worktree `animsmooth`)

Quake's frame lerp (`r_lerpmodels 1`, FitzQuake's) slides every vertex in a straight line from the last pose to the new
one over the frame's time: a limb swinging through a big angle shortens mid-swing (its chord), and the speed jumps at
every frame. `r_lerpmodels 3` draws the same frames, at the same times, along curves; the server, its frames, their
times and everything the game tests against a model are untouched. Default stays 1 until Vittorio decides.

**A, the timing** (every alias .mdl/MD3 but your weapons): each vertex goes along a cubic through four poses, the
lerp's two, the pose before them (the lerp that ended where this one starts: `entity_t.animpose0/animfrom/animspan0`,
kept by `R_AnimHistoryPush` in r_alias.c; none after a reset, a lerp cut short or a held pose: then it starts from rest)
and the one expected after (the next frame of its animation by the frames' names; models whose names say nothing, the
shambler's "From ASE" and Quake VR's numbered grunt and knight, by the frame seen after it most, three votes; deaths end
where the rig's seed table says). Its tangents are the non-uniform Catmull-Rom ones by the frames' times (LERP_FINISH's
spans; the one before clamped to 0.5-2 times this one's), scaled by `1 - vr_anim_smooth_tension` (default 0: the speed
carried through each frame; 1 eases to rest at each). It is a weighted sum of four poses (`vr_rigpose.hpp`'s `spline`),
so the shader just fetches two more poses: InstanceData's `Smooth` (the extra poses, the rig rows) and `SmoothW` (the
weights), in `alias_vertex_shader` (standard, depth, layered shadow, wound paint: all alias draws).

**B, the limbs** (models with a ragdoll seed table, vr_ragdoll.cpp: checked on the grunt, knight, ogre, shambler,
enforcer, death knight, rottweiler, scrag, zombie, fiend, vore and the player model): each bone's fitted rigid motion
(`Rig::poseRot/posePos`) is turned along the same spline in rotation (a Hermite in the rotation vectors, world frame),
its joint carried by its parent as posed (plus the fits' joint gap along the spline, so parent and child meet exactly at
the pivot), less the chord the same weights draw: one 3x4 "swing" per bone and instance (`SmoothSwing`, SSBO 9), added to
each vertex from its rest place (`SmoothRest`, SSBO 8, per VBO vertex, uploaded once a frame per model). A vertex's own
residual from the fit is blended along the spline; a vertex whose rms residual is over `vr_anim_smooth_rig_error` (1.5
units) swings less, none at twice it; a bone fitted worse than that in any of the four poses, folded away (the grunt's
dropped gun) or turning over 150 degrees between the two isn't swung. `vr_anim_smooth_rig 0`: A alone.

**Exact frames**: at a pose itself (blend 0, 1, a held frame) nothing of this is drawn: the instance goes down Quake's
path, so its vertices are the frame's bytes. The spline's weights there are exactly (0,1,0,0) and (0,0,1,0)
(`vr_anim_smooth_check`). Pixel check (`vr_anim_smooth_pin` holding an ogre at swing9-swing10, eyeshots, fixed frames,
paused): r_lerpmodels 1 against 3 at blend 0 and 1: the only differing pixels are an animated sign's that a control pair
(two shots, nothing changed) shows too (480/601 pixels, threshold 20); at 0.5: 28.7k pixels, all on the ogre. Drawn
0.001 off a key, the shape is within 0.03-0.08 units of it (the 12 rigged models checked): no pop entering or leaving a frame.

**Limbs**: `vr_anim_smooth_check`, frame pairs that follow by name, limbs (bones) turning 30-150 degrees, the spread of
their vertices about their middle at mid-frame against the frames': Quake's blend shrinks them 4.9-10.8% on average
(worst 16-64%), A 3-8%, A+B 0.0-1.2% (worst 1-36%: the death knight's and the shambler's folds). Mid-frame the drawn
shape is up to 3-20 units off Quake's blend (the ogre's chainsaw swing 13.8, the fiend's 16.8).

**Speed**: `vr_anim_smooth_trace 400 <model>` (a live monster fighting you, vr_fixed_frames 1, the same seeds each run):
the vertices' mean change of speed a frame, Quake's blend against A+B: ogre p95 2.79 -> 1.34, most 8.0 -> 4.5; shambler
p95 3.18 -> 1.66 (its most, 39.7, is a frame cut short, the same in all); knight p95 1.02 -> 0.66, most 3.0 -> 1.8.
Quake's median is 0 (all the change at the frames' ends).

**Cost** (`bench.sh combat_48`, 48 mixed monsters, 3 runs each, exclusive): CPU 2.60-3.00 ms (1), 2.60-2.67 (A),
2.65-2.66 (A+B): within the spread. GPU alias pass p50 0.34-0.37 ms (1), 0.35 (A), 0.38 (A+B). An entity's spline and
swings are worked out once a frame (`Memo`, by entity: the other eye and every shadow light copy them; 2.2-3.4 us each
on the CPU, `vr_anim_smooth_check`).

**Not changed**: your weapons (their flashes, mounts and shells follow the gun's vertices by Quake's blend), IQM and the
ragdolls' skinned models, MOD_NOLERP models (flames), reverse demo playback. The CPU's copies of a model's shape
(vr_hitmodel.cpp, vr_modelcollide.cpp, anchors, the ragdoll's first frame) keep Quake's blend: a few units off the
drawn shape mid-swing. Frame groups are handled (the next pose of the cycle) but none are smoothed in the id maps
(NOLERP).

**Debug** (Debug > Reports): Smooth Animation Check (`vr_anim_smooth_check [model ...]`), Smooth Animation Models
(`vr_anim_smooth_info [model [frames]]`: the predictions, the rig, per bone the poses too poorly fitted, the splines
drawn), Pin Nearest Monster Mid-Swing (`vr_anim_smooth_pin <f1> <f2> <blend> [<f0> <f3> [<entity> | <model>]]`, `off`),
Limb Fit Limit. `vr_anim_smooth_trace <frames> [<entity> | <model>]`. Settings: Graphics > Models and Effects > Model
Animation (Stepped / Quake's Blend / Smooth), Animation Ease, Swing Limbs.

**Shared with `ragcore`**: `vr_rigpose.hpp` (a bone's key transform, the spline's weights, rotation vectors, the
rotation spline, the rig posed along a spline with joints carried by parents). The rig code itself is unchanged.

**Comparison strips** (key f1 | Quake's blend 0.5 | A 0.5 | A+B 0.5 | key f2; the agent's scratch folder): ogre
swing9-10 (the chainsaw keeps its reach), shambler swingl (the arm stays out at full length; Quake's folds it into the
chest), knight 47-48 (the sword), grunt death 9-10.

**In the headset**: Graphics > Models and Effects > Model Animation: Smooth. Watch ogres swing, shamblers claw, knights
slash and dogs run, near: limbs should arc, not fold, and no frame should pop. Try Animation Ease 0.3 if a swing seems
to overshoot, and Swing Limbs off to see A alone.

## The vore shoves you (feature, 2026-10-11, worktree `voreshove`)

Backlog (2026-10-02): "I would like the vore to also have a shove attack when the player is close", "shoves the
player harder than other enemies". Built on the grunts' and enforcers' shove (QC `vr_enemyshove.qc`, ROUND21.md,
"Grunts and enforcers shove you"): same checks (in reach for `vr_enemy_shove_delay`, facing you, not firing, not in
pain, not knocked away; `vr_enemy_shove_cooldown` from one shove to its next; the nearest player, and only its own
enemy: coop as before), same blow (through the parry, the slide, letting go of a ledge, the haptics, `vr/shove.wav`).

- **The swing:** the vore's own throw without the ball (`orig_mon_shalrath.qc` `shal_shove1..7`: `attack5..attack11`,
  arms drawn back and up, swung forward and down; the blow on `attack9`, 0.4 s in: a little more warning than a
  grunt's 0.3 s), with its attack hiss at the start. Picked by the model's vertices: `attack9` is where its arms reach
  furthest forward (`scratch/vore_frames.py`, not committed). Honey's trigger-spawned vores get it too.
- **Harder:** `vr_enemy_shove_vore_damage` 2 and `vr_enemy_shove_vore_distance` 2 (times Damage and Push Distance:
  10 damage, 256 units at the defaults; the enforcer's is 1.5x, 192). `vr_enemy_shove_vore` 1 turns it off alone.
  Combat > Enemy Shoves: "Vores Shove", "Vore Damage", "Vore Push".
- **Reach:** the range is now measured for a grunt's width: a wider monster reaches `maxs_x - 16` further
  (`VR_EnemyShove_Reach`; a vore +16, 68 units at the default 52; grunts and enforcers unchanged). A vore
  touching you (48 between the middles) was already in reach at 52: the +16 is margin, and keeps it in reach when Too
  Close is set below 48 (the menu goes down to 36).
- **Comfort:** the shove is a slide (not scaled by `vr_push`, as before). The comfort vignette now counts an enemy's
  shove sliding you as moving (`vr_input.cpp shoveSlide`: its speed against a run's 320 units/s; modes 1 and 2), for
  every shoving enemy; the listen server's own player only (a coop guest's slide is on the host).
- Death by it: "was swatted by a Vore".

**Tests** (headless, `vrtesthall`, god, `vr_stealth_meter 0`, `impulse 241` in front, developer 1):
grunt 40 units away: pushed 128, slid 127.9; enforcer: 192, slid 191.8; vore 64 away: 10 damage, pushed 256, slid
200 (the hall's wall; at `vr_enemy_shove_vore_distance 1.5`: 192, slid 191.8). Vore 200 away: no shove (its balls).
`vr_enemy_shove_vore 0`: no shove. Cooldown (push 0, no knockback or pain knock so you stay): shoves at 2.87 and
7.57 (3 s cooldown; it was staggered by its own ball's blast between). Vignette (`vr_comfort_vignette 1`): rises to
0.96 during the slide (temporary print).

**In the headset:** walk up to a vore and stand there: after about half a second it hisses, swings its arms down and
shoves you far back (further than an enforcer). Parry it (a blocked swing pushes you a quarter as far). With Comfort
Vignette on, the view's edges darken during the slide. Too far or too often: Combat > Enemy Shoves > Vore Push,
Cooldown.

## Climbing props: crates, barrels, stacks (feature branch, 2026-10-11, worktree `propclimb`)

Approved design: the hands also take hold of the edges of the solid physics props and climb them, as they lie.
`vr_climb.cpp` has the rule in full at the top ("Holds on props").

**Ledges from the prop's shape.** `box3d::climbShape(num)`: a loose solid prop's convex pieces (Box3D's hulls: the
crate as drawn, its bevels too) as faces, corners and the face across each edge, in its body's frame, made once for a
body and kept (`mem::Cache` "box3d climb shapes", by edict, made again when the body is). `ledges::propLedges` turns
them by the body's pose now and keeps the edges of the walkable tops (normal z at least `minTopNormal`) whose face beyond
falls away, each with one sample: the drop's start (half a unit out, or as far as the prop's own side flares out under
the lip over `minDrop`; past `edgeReach` no ledge) and the top's depth from the lip's middle, walked across the
walkable faces (a bevel's strip, then the top: the first version measured the bevel alone, 0.9 units, and the mantle's
spot search failed). Only props whose box is round the hand are looked at (the area query the brush models already
used); `vr_climb_probe` lists them too, and Debug: Show Ledges draws them orange.

**Taking a hold.** The same search and checks as the level's (placeOf, takenBefore, freePoint, the leniency); the
checks also see the props' shapes (`box3d::climbRay`: another crate on the top covers the hold, a crate under the lip
is no drop: a small crate on a large one is a 32-unit step, no ledge). From the ground a prop's ledge must be
`vr_climb_prop_min_height` (36) over the feet, else the grip is the QC's (it picks the prop up: a small crate, 32, is
carried; a large one, 48, is climbed; one lying on its side, 40, too); hanging or in the air `vr_climb_min_height`.

**Holding.** The grip keeps the ledge (one edge and its sample) in world space and follows the prop's body each frame
(`followProps`: moved and turned as the body moved; the way out kept level, square to the lip), so props on lifts,
tipped ones, stacks all work the same. Heavy (`vr_climb_prop_mass` 60 kg or more, or pinned): like the level, nothing
pushes it, its motion carries the body. Light: the hands' pull goes into it (`pullProps` -> `box3d::climbPull`, a
force through each piece of the step as the standing players' weight is): the body's weight
(`vr_box3d_player_mass` 80) shared by the holding hands, at most `vr_climb_prop_pull` (40 kg) a hand, from the hold
towards the body (a body over the hold presses down), in Box3D's gravity (Quake's: about 2.6 g in metres; a first
version in newtons at 9.81 m/s^2 never tipped a 65 kg crate). A light prop's motion doesn't carry the body. Let go:
the top turned `vr_climb_prop_tilt` (10 degrees) from how it lay, a light one's hold moved `vr_climb_prop_slip` (6
units), the hold's speed changed by `vr_climb_prop_letgo_speed` (200 units/s) in a frame, or the prop gone (broken,
taken into a hand, no longer solid). The climbing hand's own Box3D bodies (open hand, fist) are off while it holds a
prop (`climb::heldProp`; they would shove it).

**The risks, as handled.** Feedback (pulling a crate onto yourself): the pull is capped, the light prop never carries
the body (so the body can't chase a crate it pulls), and the release thresholds stop it within 6 units. The player's
capsule was left solid to the held prop (not made non-solid as the design allowed): with it, a crate tipping towards
you leans on you and the hand lets go; without it, a crate tipped 10 degrees into the capsule would be pushed out of it
hard on release. Sliding/falling props: the slip and speed thresholds. Mass threshold feel: a setting on the Debug
menu.

**Settings** (all archived): `vr_climb_props` 1 (Climbing page: Climb Props), `vr_climb_prop_min_height` 36 (Climbing
page: Lowest Prop Ledge), `vr_climb_prop_mass` 60, `vr_climb_prop_pull` 40, `vr_climb_prop_tilt` 10,
`vr_climb_prop_slip` 6, `vr_climb_prop_letgo_speed` 200 (Debug > Tests > Climbing, with three test crates). Test
crates: `vr_test_spawn 120..124` (impulse 241): a large crate square to you, two stacked, one on its side, a small one on
a large one, a small one.

**Tests** (`Misc/quakevr/climb/prop_climb_test.sh <agent>`, vrclimb's open floor, 8 PASS): heavy crate mantled onto
(standing at z 72); two large crates stacked (96 high) mantled onto (z 120); a large crate on its side mantled onto
(z 64); a large crate made light (`vr_climb_prop_mass 70`) tips towards the player, both hands let go ("pulled away",
7 units), the player not moved (within 2 units), standing back and against it; a held crate flung at 300 units/s lets
go ("knocked away"); held 6 s, no let-go and no drift; `vr_climb_props 0` no hold. World climbing:
`slopes_test.sh` 8 PASS. The hold search with 8 two-crate stacks round the player: median 0.002 ms, worst 0.035 ms,
3 traces (no props: 0.0002 ms).

**In the headset:** climb a large crate in vrstart's range and stand on it; climb a stack; pull at a small crate on
top of a stack (it should tip towards you and your hand let go, without dragging you). Check that picking a large
crate up by its top edges still feels right: from the ground, a grip at its top edge now climbs it
(`vr_climb_prop_min_height` 50 would leave large crates to carrying).

## The toolgun's own panel (feature branch, next release, 2026-10-11, worktree `toolgun2`)

**The request** (the author's, through the coordinator): make the toolgun intuitive, as Garry's Mod's and S&Box's.
1. Its menu was the VR Settings' pages on the gun (54 x 30 cm at its 8 units high): too much space. 2. Its laser was
the menus' (too thick, its spot too large). 3. Switching tools took a drop-down. 4. The spawn choice was a list: wanted
a grid of thumbnails. 5. Joints likewise: the panel shows only the tool's own choices. 6. A row of tool tiles. 7. The
physgun should turn what it holds by the wrist (and with the body), the sticks still there; snapping if easy.

**What changed.**
- **A panel of its own** (`vr_toolgun_panel.cpp`): 256 x 192 virtual pixels in a target of its own (three texels each,
  mipmapped), `vr_toolgun_panel_size` 6 units wide (23 x 17 cm; was 54 x 30), placed over the gun as before
  (`vr_toolgun_menu_up`, `_forward`, `_tilt`: the author's placement kept), opened by the gun's B/Y. It is drawn with
  the VR menus' own widgets and colours (`menuui::paintButton`, `paintSwitch`, `paintSlider`, the menus' box colours,
  Quake's font), its laser is the menus' laser drawn thinner (`menuui::drawLaser`, `size` `vr_toolgun_laser_size` 0.3:
  width 0.09 units, spot 0.48; the menus' stay 0.3 and 1.6). The game goes on while it is open and you can walk (the
  key_game input: no menu state at all); a menu opened meanwhile hides it. One layout function runs twice a frame, an
  immediate mode: in the host frame for the laser (what it lights, a press's click, a slider's drag; a tick in the hand
  on each widget it comes onto) and at the 2D pass's end for the image. The other hand's trigger clicks only where its
  laser meets the panel; elsewhere it is its own (fires its weapon, freezes the physgun's prop).
- **Top row:** Spawn, Remove, Physgun, Scale, Joint tiles with drawn icons (a plus, a cross, a ring, a box, two balls),
  and More (the cheats and the panel's size). Under it the tool's own page only: Spawn's tabs and a 4 x 2 grid of
  thumbnails with names under them, pages `<` `>`, Pick, Reset, Clear; the joints as seven icon tiles with a line on
  the chosen one; the physgun's switch and choices (Turn With the Wrist, Snap Off/15/45/90, Sticks Turn); Scale's
  Proportional and Reset; Remove a word and Clear Props. More: twenty cheats (Debug > Cheats and Recording's own rows
  found by their labels, `vr_menu_toolgun.inc`; god mode, noclip, notarget, fly and the switches lit while on) and
  Panel Size. *Decision:* joints have no parameters to slide (the engine has none), so their page is the tiles.
- **Thumbnails** (`vr_toolgun_icons.cpp`): each spawn entry's .mdl (its first frame, its skin: the three armours) or
  .bsp (version 29, its first model's textured faces: the item boxes, the explosive boxes, whatever the map has loaded)
  read again from the game's files and drawn by a small CPU rasteriser (orthographic, z-buffered, 2x2 supersampled,
  flat light from over the shoulder, fullbrights unlit), from ahead and to the right, guns from the side; 96 x 96
  pictures for the 2D pass (`gfx::Picture`), two a frame while the panel is open, kept for the session (made again
  after a game directory change). 46 entries: 1.0 ms each on average (39 in 37 ms). *Decision:* no disk cache: making
  them is cheaper than reading a file.
- **Tools by a button:** the gun's stick press is the next tool, with X/A held the one before (`vr_toolgun_next`), a
  pulse in the gun's hand; the gun's own screen already says the tool (and the spawn choice, the joint). The stick
  press was the stick's own key otherwise (RTHUMB `+reloadmain`, LTHUMB `+speed`). With the toolgun in the off hand
  that is the move stick's press, so the run/walk switch is lost while the toolgun is held (walking with the panel open
  included). It is swallowed while the trigger holds something (the stick pushed hard while adjusting a held prop). It
  is left to bullet time when `vr_bullettime_trigger` names that stick (`bullettime::ownsStick`). The menu gesture
  (both stick presses) still opens the menu (it is read first; the first press also turns the tool).
- **The physgun's wrist** (`vr_toolgun_physgun_wrist` 1): what it holds keeps the turn it had to the gun's hand when
  taken: its axes and its offset from the beam's point turn by the hand's turn since (held::axesFromAngles of the
  hand's angles, which carry the body's turn), the sticks' turn on top (kept as a turn about the world's axes). X/A
  held, its angles snap to `vr_toolgun_snap` (15) degrees. A monster or a pickup turns only about the up axis, by the
  hand's yaw since (tilted, a monster would walk on so). 0: the old way (the sticks alone).
- **Gone:** the Toolgun, Toolgun - Spawn and Toolgun - Cheats pages, their Back, `menu::openToolgun`/`closeToolgun`/
  `toolgunPageShown`, the panel's gun quad in `vr_panel.cpp`, the mock laser's hand switch and the game running under
  them (`VR_MenuRunsGame`), `vr_toolgun_menu_height`. Every console command stays (`vr_toolgun_menu` toggles the panel).
- **Found on the way:** the spawn list's Brick named `progs/vr_brick0.mdl`, which doesn't exist (`vr_brick1.mdl`).
- Debug > Tools > Toolgun: Open or Close Its Panel, Next Tool, Panel Widgets (`vr_toolgun_panel_dump`), Thumbnails
  (`vr_toolgun_icons`). Tests: `vr_toolgun_mock_laser <x> <y> | <widget name> | off`.

**Tests** (headless, vrfiringrange). `Misc/quakevr/toolgun_panel_test.sh toolgun2 [--shots]`: PASS. The Props tab and
the Barrel tile hit by their pixels (96 53, 160 89: the tiles at the new size) choose the barrel; five stick presses go
Remove, Physgun, Scale, Joint, Spawn, the gun's screen REMOVE, PHYSGUN, SCALE, Weld, Barrel; X/A + press back to
Joint; the Hinge tile sets `vr_toolgun_joint` 2; God Mode toggles; the Panel Size slider dragged from 3 to 12; a crate
held turns 45.0 degrees with the hand's 45 degree roll and 30.0 with its 30 degree turn, snapped to 0 0 30 with X/A; a
grunt held keeps its pitch and roll (0, 0) while its yaw follows. `toolgun_joint_menu_test.sh` (now clicking the joint
tiles): PASS in either hand (70 picks, 6 kinds joined, unjoined). `vr_menu_path_check maps/vrcalibration.map`: 0
missing. e1m1: the panel hidden under the menu and back after it.

**In the headset:** hold the toolgun, press B (or Y): the panel over the gun, about a hand's width; point with the other
hand (a thin laser, a small spot) and pull its trigger on Physgun, on a tab and a thumbnail; walk with it open. Press
the gun's stick to step through the tools (the gun's screen names them; X/A + stick goes back). With the physgun hold a
crate and twist the wrist, turn yourself: it turns with you; hold X/A: it clicks to 15 degree steps. Tell us whether
the panel's size (Panel Size on More) and place suit, and whether the laser is now thin enough.

## Active ragdolls: living hit reactions (feature branch, 2026-10-11, worktree `raghit`)

Stage 6 of the active-ragdoll programme. His goal: "Realistic hit feedback while still maintaining active AI (e.g. shoot
grunt in the leg, it stumbles a bit realistically but continues to move/fire and execute its AI)." The agreed scope (low
risk): no physical ragdoll on a living monster, but an additive kick layered on its animation, plus a short slow-down of
its steps. Its AI, damage, hitboxes (hit tests stay on Quake's blend) and frames are untouched. `vr_hit_react 1`
(default), `vr_hit_react_strength 1`, `vr_hit_react_time 1`, `vr_hit_react_slow 1`; Gibs and Corpses > Ragdoll Settings
> Hit Reactions (Living Monsters).

### The blow (server: QC `vr_hitreact.qc`, `VR_HitReact_Hit`; builtin `hitreact`, `QVR_SVC_HITREACT` 34)

T_DamageDeal, after the blow and before the pain (a living monster: `FL_MONSTER`, not knocked down, staggering or
getting up (`vr_knockdown`: a ragdoll then), not a bleed, a burn's tick or a liquid): the point struck (PositionalDamage's
point; a shotgun blast's pellets' centroid, gathered in TraceAttack by `VR_HitReact_Pellet`; a blast's: the nearest point
of the box, pushed away from it; else `vr_hitpos`, a missile where it touched, the middle), the push (the shot's
direction, the pellets' summed, a missile's flight), the part (0 body, 1 head, 2 arm, 3 leg; the pellets' majority), the
damage and its share of the monster's `max_health`. One unreliable message to every client (18 bytes): multiplayer
clients draw it as the host does. The server's `vr_hit_react 0` sends nothing and slows nothing; each client draws by
its own setting.

### The kick (client: `vr_hitreact.cpp`)

Size = clamp(1.4 sqrt(share), 0.15, 1.2) x strength: a grunt's shotgun blast near 1, a nail a third, a shambler's rocket
about a half. Resolved at its first draw, in the pose then:
- **Rigged models** (a ragdoll rig: grunts, knights, ogres, ...): the bone nearest the point among the part's bones
  (by the seed tables' names: head, upperarm/forearm/arm, thigh/shin/leg), else any. That bone and its parents up to
  (not) the root turn about their joints, half as much each further up: bent the push's way as much as the push is
  across the bone (0.45 rad at size 1), twisted by a push off its middle (a third as much, 8 units off). A pelvis hit
  folds the torso over it. A leg: the knee buckles (the shin bent back about its hinge, 0.5 rad; the sign picked so the
  foot goes back) and the torso leans over that leg (0.2 rad). The head turns against the torso's turn (0.3: it lags).
  Each turn is kept in its bone's own frame, so it follows the animation.
- **Drawn:** each frame each bone's turn about its joint as posed, carried by its parent's (a loose piece, the grunt's
  shotgun, goes with the bone nearest it), turned into a 3x4 delta a bone (the kicked place less the place, affine in
  the rest position) appended to r_lerpmodels 3's swing rows; the alias shader adds it whole (`RigKick`, `Smooth.w` 2 +
  its first row; the swings keep `Smooth.z` and their fit share). Not smoothed (r_lerpmodels 0 or 1, a frame held), the
  instance is drawn on Quake's blend's own weights (0, 1 - blend, blend, 0) with the kick: the kick works whatever Model
  Animation is (a decision: the brief said r_lerpmodels 1 falls back to the tilt; the bones look better and cost the same).
- **Other models, or `vr_hit_react 2`:** the whole model tilted about its feet by the push's torque (0.12 rad at size 1;
  its entity matrix, `VR_AliasPreTransform`).
- **Over time:** a damped spring's answer to a blow (damping 0.6, 15.7 rad/s): peak at 74 ms, a tenth back the other way
  at ~0.3 s, nothing after 0.6 s (times vr_hit_react_time); kicks add (4 an entity at most, the oldest replaced). A
  monster that becomes a ragdoll (a stagger, a knockdown) drops its kicks.

### The slow-down (server: `VR_HitReact_Dist`, ai_run and ai_walk)

The steps' distance scaled, easing back to 1: a leg hit 1 - 0.6 s' for 0.5 s plus a hitch (a walkmove of up to 4 units
towards the struck leg's side of the shot's line, 2 along it, at its next step), the body or head 1 - 0.35 s' for 0.3 s,
an arm 1 - 0.15 s' for 0.25 s (s' = min(1, 1.4 sqrt(share)) x vr_hit_react_slow). No state, no decision: only how far
ai_run/ai_walk move. A monster in its pain frames doesn't step, so after a blow that pains it the slow-down has usually
run out (Quake's pain is its stumble then); it shows on hits without pain (nightmare's pain rest, ogres', shamblers').

### Measured (`Misc/quakevr/ragdoll/hitreact_test.sh raghit`)

- trace: a grunt's calm leg hit (12 damage, share 0.4): shin 25.3, thigh 19.3, chest 12.1, head 3.6 deg at 70 ms; 1.96
  at 0.19 s, the swing back 2.4 at 0.29 s, 0.2 at 0.55 s, 0 after.
- eyes (the kit's scratch `hitreact_sheet.png`; body, head, arm, leg, on and off, the same frames): pixels differing
  between on and off at ~0.04/0.07/0.1/0.16/0.3/0.6 s: body 2751/6368/2010/2486/383/115, head 3246/4921/945/1456/217/157,
  arm 1427/4629/1061/1307/232/154, leg 3059/5970/2755/3886/(2110/2332: the blood pool's random shape). The torso leans
  back, the head snaps back, the knee gives; no tearing. r_lerpmodels 1: the same (leg 1634/6135/2179/2364). The whole-
  model tilt (vr_hit_react 2): 4.6 deg at peak.
- ai: the runs aren't bit-exact (fast mode): five runs of one setting (reactions off) came out three ways (the third
  hit landing 222, 247 or 223 units away). Against the off runs, reactions on without the slow-down kept all 84 of the
  84 frame changes of its twin; on with the slow-down 62 and 79 of 84 against the off run of its class (a 0.015 s
  timing spread). In those runs the pains covered every slow-down window: 0 steps slowed. Nothing seen differs beyond
  the runs' own spread.
- slow: a running knight's calm leg hit (share 0.27): steps x 0.62, 0.71, 0.80, 0.88, 0.97 (16+20+14+6+16 = 72 units
  became 56: 22% less ground in 0.5 s) and a 3.2-unit hitch.
- cost (12 grunts struck at once, --exclusive): the module 0.006 ms a frame with none reacting (the lookup on every
  alias model drawn), 0.012 ms with 12 (Limbs), 0.009 (Whole Body); the posing for the rows is skipped for monsters not
  reacting.
- After the merge of next (ragstagger): a leg kick under way when a shove staggers him is dropped once he is a ragdoll;
  hits during the stagger send nothing; after his recovery an arm hit reacts again.

**In the headset:** shoot grunts and knights in the legs, arms, head and body at a few metres (Debug: Hit the Nearest
Monster, or real shots). Tell us whether the flinch reads (Reaction Strength: 0.45 rad a bone at size 1 may want more),
whether the knee buckle and the torso lean look right, whether a monster with no rig (a scrag, a fish) tilts sensibly,
and whether Hits Slow Them is felt on knights and ogres.

## Active ragdolls: environment (feature branch, 2026-10-11, worktree `ragenv`)

Stage 4 of the active-ragdoll programme, on top of the core below (`ragcore`). His goal: "basic interaction with
surrounding environment or props, trying to hold onto a wall while falling/dying". The design is the research report's
section 8 (the kit's `scratch/ACTIVE_RAGDOLLS.md`): sense, a goal per hand, act (Overgrowth's and Euphoria's recipe).
Behind `vr_ragdoll_active_env` (default 1 on this branch) and its sub-toggles `_brace`, `_lean`, `_ledge` (all 1); only
for active deaths (the core's monster set: grunt, enforcer, knight, death knight, ogre, zombie), never a headshot or a
beheading (a collapse). (This section sits above the core's so the stages' notes merge without touching each other.)

### What it does (`vr_ragactive.cpp`, "Environment"; `vr_ragactive.hpp`, "What is round him")

- **Sense** (every 3 frames, from his first driven frame): a ray from his chest along his fall (the pelvis's and chest's
  mass-weighted velocity, length speed x 0.4 s + half an arm, at most 8 arms), and one of six level rays all round him
  in turn (1.5 arms; from behind him: a dying torso is often twisted 40-90 degrees from where he faced, so no ray
  trusts his chest's facing alone). Both start 0.3 arms inside his body: thrown into a wall, the ragdoll is often made
  with the chest's middle past the wall's face. They see the world, movers and props (`Context::envMask`), not his own
  entity, not a loose thing under 8 kg or 0.4 m tall (his dropped gun, a backpack). While he falls, the world's ledge
  map (`vr_ledges`, `Context::ledges`) near his hands.
- **Catch** (`_brace`): a surface along his fall, facing it, ahead of or beside him (a floor only when falling face
  first: `dot(chest front, fall) > 0.3`; a wall from -0.15): the hands on its side (a floor: both) reach (two-bone
  `Reach`, strength 0.9) to the surface below their shoulders led by 0.12 s of his motion. Landed (within 0.12 arms of
  it, or 0.8 s on) the push gives: the arms' strength 0.9 -> 0.1 over 0.35 s (the motors' cap is the "capped push"),
  then the hand goes back to the death's own pose. Never held. One catch at a time, 0.6 s apart.
- **Lean** (`_lean`): a wall (normal within 30 degrees of level) found by the ring or his fall within his first second,
  while he is on his feet (chest over 0.9 arms above the floor, pelvis under 1.6): the hand whose shoulder is nearer the
  wall (not the one on his wound; 20 % of deaths none: "sometimes no hand") reaches for the wall at shoulder height, 0.3
  arms to its side; after 0.3 s, still off, it goes straight to the wall from where it is (an arm can't reach far behind
  him). Within 0.2 arms of the wall: a **hold**, a Box3D motor joint from the wall's body (the world's, or a mover's:
  it rides it) to the hand's end, at most Grip / 3 body weights (`vr_ragdoll_active_env_grip` 1.2; heavier monsters
  less: x sqrt(90 kg / mass), 0.6..1.2), 15 Hz (a light hand on a soft spring barely pulls), colliding with the wall;
  its anchor slides down the wall, faster as he weakens (to 0.45 m/s over 1.2 s). The knees give slowly (the legs'
  strength 0.5 -> 0 over 1.6 s), the pelvis drawn to the wall (0.25 body weights fading over 1.5 s). The hand lets go
  on its timer (1.1-2 s), at the floor (0.3 arms above it), its load at the cap for 0.25 s, pulled 0.6 arms off it, hit,
  taken by a player's hand, cut, a blast (`weaken`), or once he crawls.
- **Brace** on a prop (a crate: the same lean, ring or fall ray meeting a loose body): the hand pressed on it (0.8 ->
  0.2 over 1.2 s), no joint, no pelvis pull (no grabbing props).
- **Ledge** (`_ledge`): falling (down faster than 0.8 m/s) past a lip near a hand: the hand at most half an arm over
  the lip, his shoulder at most 0.8 arms over it and 0.6 under, the lip within 1.15 arms of the shoulder (stretched: the
  hold pulls the rest), him out over its drop. The nearest hand reaches for the hold (`holdInset`/`holdLift`, checked by
  a short ray down onto the top: the world's or a mover's body under it), the other for the same lip a shoulder's width
  along when it reaches. Grabbed: a **ball joint** (a motor joint stretched 13 units and let go: a hanging body needs no
  give), the hand passing into the lip rather than fighting it. He hangs `vr_ragdoll_active_env_hang` (1.8 s) x
  0.45..1.65, then drops; earlier on overload (Grip body weights a hand, past it 0.25 s: a one-handed knight swinging
  slips off), a hit or the rest above. Once a death.
- **Curl** (`_brace`): tumbling (chest spin over 5 rad/s, over 2 m/s, off the floor by 0.8 arms, after 0.3 s), nothing
  caught or held: the hands to before his face, the knees up (weight eased to 0.8).
- **Hit**: a shot's push on a holding hand or its forearm (`box3d::push` -> `ragactive::struck`) lets it go. Saved
  games: nothing to save (a loaded ragdoll is made limp as before; the joints are the world's, made again).
- The reaches go over the clip's pose and the death's own (a wound's hand: a catch takes it over, a lean picks the
  other hand); a limb the environment let go of is given back (its reach dropped unless it is the wound's or a step's).

### Debug and tests

- `vr_ragdoll_active_debug 1` (Debug > Logs: Active Ragdoll Targets) now also draws the probes (cyan, orange where they
  met something), each hand's goal and hold (magenta, thick when held), the wall leaned on (its normal, blue).
- `vr_debug_ragdoll 1`: each catch, lean, ledge reach, hold made (where, its cap, its time) and let go (why, held how
  long, load, slid); 2: each probe; 3: each ledge candidate. `vr_ragdoll_active_list`: each one's hands' modes, holds,
  catches. `vr_ragdoll_active_trace n`: a `ragenv` line beside each `ragtrace` (each hand's mode, held, distance to its
  goal, load, slide; the curl).
- `vr_ragdoll_env_hit [speed]` (Debug > World and Physics: Strike Holding Hands): each holding hand struck as a shot.
- Ragdoll Settings rows: Use What Is Round Him, Catch the Fall, Lean on Walls, Grab Ledges, Grip, Hang Time.
- `Misc/quakevr/ragdoll/env_test.sh <agent> [wall along ledge thrown hit crate front open off cost]` on vrtesthall
  (`env_trace.py` sums each death's `ragenv` lines; `active_trace.py` the stability bounds; `EYES=1`: one contact sheet,
  the kit's `scratch/env_sheet.png`, via `env_sheet.py`).

### Checked (headless, `env_test.sh`, vrtesthall, `sv_random_seed 7`, grunt and knight)

- **Wall** (shot towards the north wall, 24 units off: thrown into it, the ragdoll made against it): the grunt's left
  hand held the wall 1.1 s, slid 8 units, let go at the floor; the knight's held 1.5 s, slid 13 units (0.42 m), let go
  on its timer; both upright against the wall about a second, then slumped to sitting (pelvis 25-29 -> 6-8 units).
  At 60 units off: the grunt leaned (held 1.5 s, slid 0.43 m); the knight rolled "no hand" (leaned, slumped).
- **Along** (shot along the north wall): the grunt's and the knight's wall-side hands held 0.7 and 1.3 s, slid down,
  let go at the floor.
- **Ledge** (killed at the platform's lip, no kill push: he topples over it): the grunt grabbed with both hands and hung
  2.1 s (load up to 2.6 kN against 2.6 kN caps), then dropped; the knight one-handed, slipped off on its load after
  0.6 s in one run, hung 2.1 s in another. **Hit**: struck 0.5-0.6 s into the hang, both let go ("hit").
- **Thrown** (the kill push on: thrown 50-130 units out, past the lip's reach): both hands caught the floor; the knight
  caught the 72-unit block's lip on the way down (slipped off at once on the landing's 16 kN).
- **Crate** (the big crate behind him): braced on it 1.4 s, no joint; the crate pushed a little.
- **Front** (shot from behind in the open): the knight caught the floor with both hands; the grunt's death animation
  falls him back first (his chest's front points up): no catch, as wanted. **Open**: no events. **Off**: no `ragenv`.
- **Stability**: every run (the suite's 18 and the reruns) under `active_trace.py`'s bounds: no NaN, no crash, energy 0 J and spin under 2 rad/s
  once the drive let go; the most energy 12 kJ (the 192-unit fall), under the 20 kJ explosion bound.
- **Cost** (8 grunts along the north wall killed at once, `--exclusive`): the drive 0.041 ms a frame for 8 (0.0054 ms
  each) against 0.021 with `vr_ragdoll_active_env 0`: the probes and holds 0.0026 ms a ragdoll. The physics step over
  the 60 frames after: 0.33 ms against 0.34; 300 frames: 0.22 against 0.22. (The world's ledge map is made the first
  time a driven ragdoll asks for it if Climbing hadn't: once a map.)
- Contact sheet (`EYES=1`, the grunt; the kit's `scratch/env_sheet.png`): against the wall he stays up on a hand, then
  slides to sitting; he hangs from the platform's lip by both hands, arms up, then drops; he falls onto the crate and
  slumps against it.
- `active_test.sh deaths head` (vrfiringrange, nothing near): unchanged.

### Not done / notes

- Ledges on movers (their own ledge maps in model space) are not looked up; a lean or a catch on a mover works (the
  hold rides its body).
- The "catch" reads his chest's front for falling face first; a grunt's twisted torso may make a forward fall read as
  sideways (no catch) and vice versa. Rays don't depend on it.
- Living knockdowns (`ragstagger`) could use the same: `envFrame` only runs for deaths now (`Env::on` needs `death.on`).
- No pull-up from a hang; no grabbing an ally ragdoll or a prop (by design: props are braced, never held).

**In the headset**: kill grunts and knights standing near a wall (shot towards it and along it): a hand should go to the
wall and he should slump down it. Kill one at the edge of a drop (vrtesthall's platform: stand on it and shoot one at
its west lip with a weak weapon): he should grab the lip and hang a moment; shoot the hand to make him let go. Kill one
in front of a crate: he braces on it. Turn Ragdoll Settings > Use What Is Round Him off to compare; Debug > Logs: Active
Ragdoll Targets shows the probes and holds.

## Active ragdolls: dismemberment (feature branch, 2026-10-11, worktree `ragdismember`)

Stage 5 of the active-ragdoll programme, on stage 1 ("Active ragdolls: core and deaths", below). His goals: "reacting to
dismemberment (e.g. holding wound with hand, trying to keep balance when one leg is dismembered)" and "minimum effect on
headshots and decapitation (almost pure collapse)" (stage 1's, kept). Behind `vr_ragdoll_active_dismember` (default 1 on
this branch; Gibs and Corpses > Ragdoll Settings > React to Lost Limbs). Same monsters as stage 1.

### What he does (`vr_ragactive.cpp` "Dismemberment": `cutReaction`, `dismemberFrame`)

- **Where it starts**: Limb gore cuts only the dead (a killing blow, a dying body, a corpse). `ragdollCutLimb` calls
  `ragactive::cutReaction` after each cut on a driven death. Killed standing by the cut (QC's `VR_Limb_Killed` runs
  before his death code: his ragdoll is made at once from the live frame, which stage 1 left limp), `createRagdoll`
  now starts an active death (`startCutDeath`): one of his death animations (rolled) from its start, the live frame kept
  for the balance beat. A cut that makes the ragdoll of one already dying (before his first frame not solid) starts the
  stage-1 death from the frame he is in. A cut on a ragdoll no longer driven (a corpse) does nothing new.
- **The head** (a beheading mid-death): a collapse as a headshot (strength x `vr_ragdoll_active_headshot`, no pin, hand,
  step or crawl). A beheading killing blow stays limp at once, as before (no drive started).
- **A hand to the stump**: an arm lost, the other hand to its stump; else a leg's, the hand on its side (else the
  other). The point: the vertex of the part it was cut from nearest the joint, leaning towards the holding shoulder and
  his front, within 0.75 of that arm's length of its shoulder (the joint itself is inside the part; an ogre's arms don't
  reach across his chest), 2 units out. Strength 1 (a wound's 0.8), in over 0.25 s from 0.05 s, held until the writhe
  ends, let go over 0.8 s. The stump goes before a wound: a wound hand that is now the stump's leaves the wound; a lost
  hand never holds anything (stage 1's reach stops on a cut limb).
- **The writhe**: once down (0.8 D, D his death animation's length; after the beat, from its break) for
  `vr_ragdoll_active_dismember_time` (3 s): the torso's and legs' strength at least 0.35 (over his death's), each joint's
  target swung about a fixed axis (a hinge's own) by 0.12 rad (torso) or 0.3 rad (legs) at 0.7-1.5 Hz, phases rolled;
  its turn fed forward to the joint's motor (`Dismember::spin`, added to the clip's in `drive`: the motor at speed 0
  held the joint still, the first try stirred at most 0.8 rad/s). Eased in over 0.5 s, out over 1 s.
- **One leg lost, killed standing**: the balance beat. The pose he stood in held, every strength full; a pelvis pin of 2
  body weights at 12 Hz (at 6 Hz his pelvis rolled over the standing hip under his torso's weight, the pin's turn spring
  being set by the pelvis's own inertia: the beat broke at 0.36-0.49 s; at 12 Hz 1-1.2 s); the leg left braced (its reach, strength 2.5) where
  its foot stands (a hop under him lifted what held him up: he fell at once); the pin's anchor drifting to the side lost
  (0.3 m/s/s) and sagging 0.06 m, his torso bending to it (0.35 rad over its joints); his arms flung out and flailing
  (1.6 Hz, nearly mirrored: out of phase they rocked his chest, 130 J), strength 0.5. It breaks when his foot-to-pelvis
  line leans `vr_ragdoll_active_balance_lean` (15 deg) from its start or after `vr_ragdoll_active_balance_time` (1.2 s):
  the pin lets go over 0.12 s, his death animation plays from then (its strengths shifted to the break), he falls; then
  the hand to the hip or knee stump, the writhe.
- **Both legs lost**: the stage-1 crawl (forced for the crawling kinds, both arms whole), no hold, no writhe. Killed
  standing, he pitches forward (2.5 rad/s, at his first drive frame, after Limb gore's `vr_limbs_body_speed` scaling)
  with no clip, landing on his front: no roll (stage 1's roll threw a legless ogre 85 units at 3 kJ: it is turned off for
  the legless, `rollFrom` set long past).

### Debug and tests

- `vr_test_cut <n>` (QC `VR_TestCut_Frame`, vr_limbs_test.qc; Debug, World and Physics: Cut the Nearest Monster's Leg /
  Arm / Both Legs Off): a sword's cut at the nearest live monster's left arm (1), right arm (2), left leg (3), right leg
  (4), both legs (5), left forearm (6), left shin (7), head (8), killing it (armed as Limb gore's slash); n + 10 the
  nearest dead one's. Joints by bone name: QC `ragdolllimbs(e, 6, limb)` (`ragactive::limbJointOf`).
- `ragdis` trace line (with `vr_ragdoll_active_trace`): the hand from the stump, the lean, the beat, the hold's and
  writhe's weights, the torso's and legs' stir. `vr_debug_ragdoll 1`: what each cut does, the beat's break; 2 each
  balancing frame (pelvis, pin, foot) and each holding frame (the stump in arm's lengths).
- `Misc/quakevr/ragdoll/dismember_test.sh <agent> [arm forearm leg shin legs head dying early off cost]`
  (`dismember_trace.py`; `EYES=1` a contact sheet, `CAM`, `BOX`, `DEBUGDRAW`).

### Checked (headless, vrfiringrange, `sv_random_seed 7`; grunt, knight, ogre)

- **Arm** (shoulder): the other hand on the stump from 0.5-1 s, 4-6 units off held (least 3.8-4.1); **forearm** (elbow)
  2-5 (grunt, knight), the ogre's 10-16 (his elbow stump lies under him).
- **Leg** (hip): the beat lasted 1.06 / 1.13 / 1.08 s (broken at 15 deg), the pelvis 2 units lower by then; on the
  floor the hand at the hip 4 / 4 / 8 units off. **Shin** (knee): 0.97 / 1.08 / 1.2 s, the hand 4-9 off.
- **Both legs**: pitched forward and crawled 32 / 30 / 29 units.
- **Dying** (a body shot, 0.4 s on the left arm then the leg cut): the hand to the arm's stump, 4.5 / 4.5 / 18 units
  (the ogre lying on it); **early** (the arm cut 0.1 s after the shot, the cut making the ragdoll): 3-8.
- **Writhe**: the torso and legs stir 0.4-1.3 rad/s on average while it lasts.
- **Head**, **off** (`vr_ragdoll_active_dismember 0`): limp at once, as before (no drive).
- Every case at rest after (0 J, 0 rad/s, 1 s after the drive let go; driven 4.5-5.8 s), no NaN, no crash.
  `active_test.sh deaths head wound` (grunt, knight): stage 1's numbers unchanged.
- **Cost** (8 grunts killed by a leg cut, `--exclusive`): the drive 0.023-0.029 ms a frame for 8 (0.003 ms each); the
  physics step over the next 200 frames 0.30 ms against 0.15 with `vr_ragdoll_active_dismember 0` (writhing bodies stay
  awake 4-5 s; limp, they sleep).
- Contact sheet (the kit's `scratch/dismember_grunt_leg.png`): he stands on one leg about a second, tips to the side
  lost, falls, lies with a hand at the hip.

### Not done

- The hand holds the stump's surface near the joint, not the cut face; no blood on the hand.
- No hop or step to keep the balance (the leg left only braces); no reaction on corpses no longer driven.
- The ogre's elbow stump and a stump lying under him are often out of his reach (8-18 units).

**In the headset**: cut a grunt's or knight's leg off with the sword (he should stand on the other about a second, arms
out, tip over to the side lost and fall, then clutch the hip and writhe), an arm (the other hand to the shoulder), both
legs (he falls on his face and drags himself on his arms), and behead one already falling (a collapse). Debug, World and
Physics: the Cut ... Off rows do it to the nearest monster.

## Active ragdolls: core and deaths (feature branch, 2026-10-11, worktree `ragcore`)

Stage 1 of the active-ragdoll programme (research: the kit's `scratch/ACTIVE_RAGDOLLS.md`, options 1-3 and section 8).
His goals: "death animations with some stumbling, realistic collapse holding own wounds, crawling away on the ground
before dying"; "minimum effect on headshots and decapitation (almost pure collapse)". Behind `vr_ragdoll_active`
(default 1 on this branch; 0: today's limp ragdolls, limp at Go Limp At).

### The core (`vr_ragactive.hpp/.cpp`, `vr_ragpose.hpp`)

- **Joint ids kept** (`createRagdoll` no longer throws them away) and handed to `ragactive::begin` with the limp joint's
  friction and stiffness. `vr_ragpose.hpp` (header only) holds the shared rig-pose helpers: `jointFrame` (the frame
  `createRagdoll` gives a joint: now used there too, so the drive's frames are the joints' own), `bonePoseLerp`, a
  joint's turn as Box3D's target (`toJoint`/`fromJoint`), `twistZ`, `clampSwingTwist`, `rotationVector`, `between`,
  `bendFor` (a two-bone limb's bend for a reach). The rig (`vr_ragdoll.cpp`) is unchanged.
- **Drive per joint, each frame before the step** (`drive`): a ball's spring to its target rotation, a hinge's to its
  target angle (clamped inside the joint's cone/twist or range: no fighting the limits), at `vr_ragdoll_active_hertz`
  (10) times the square root of its strength (the pull goes with the strength; at most 20 Hz), damping
  `vr_ragdoll_active_damping` (1); its motor turning at the pose's own speed (frame to frame, fed forward), at most
  joint friction + strength x `vr_ragdoll_active_torque` (2) x the weight of what hangs on the joint x its lever. Strength
  0: exactly today's joint (friction motor at speed 0, `vr_ragdoll_joint_stiffness` spring to the rest pose). No Box3D
  change: the spring is not capped (the optional `maxSpringTorque` patch was not needed: the motor is the capped part
  and the springs fade).
- **Targets**: a `Clip` of the model's frames (the rig's per-pose bone transforms, blended between frames at the clip's
  frame rate, held at its last frame or looped), each joint's child turn in its parent's rest space; `Reach`es (two-bone
  IK per limb: the lower bone a hinge bent by the distance, `bendFor`; the upper turned the least way from where it is
  to point at the goal) blended over it by weight, with a strength floor while they lead.
- **Strength**: per channel (legs, torso, arms, head: by the bone's name) a `Fade` (from, to, t0, t1; smoothstep), times
  `base` (a headshot: `vr_ragdoll_active_headshot`, 0.05), times `vr_ragdoll_active_strength`, times each part's own:
  **a limb a hand holds (or a throw's pinned feet) has none, with its whole limb up to the torso** (the pelvis: all),
  easing back over 0.5 s once let go of (PuppetMaster's rule). `weaken(keep)` lowers all (a blast: x0.35).
- **Pelvis pin**: a kinematic anchor following the clip's pelvis (plus an offset: a stumble's carried motion), a Box3D
  motor joint (the hand grabs' pattern) pulling the pelvis at most `force` body weights (`vr_ragdoll_active_pin` 0.6),
  fading over `vr_ragdoll_active_pin_time` (0.3 s), then destroyed.
- The drive stops (`on` false) once nothing is driven: the ragdoll is today's limp one. `release` makes every joint limp
  at once (`vr_ragdoll_active 0` while one is driven). `end` (the ragdoll destroyed) drops the pin's anchor.

### Deaths (`beginDeath`, `deathFrame`; vr_box3d.cpp `startActiveDeath`, `activeClasses`)

Grunt, enforcer, knight, death knight, ogre (and the marksman ogre: the ogre's model), zombie. Others keep limp ragdolls.
- **Handoff**: the ragdoll is made at the first frame his death leaves him not solid (`wantsRagdoll`: an active death
  is limp at once), from the frame drawn, driven by his own death animation at 10 fps from there. Not a loaded game's
  (its frames not seen changing: limp at Go Limp At, as before).
- **Fades** (D: the rest of his death animation, times `vr_ragdoll_active_fade`): legs 1 -> 0 over 0..0.5 D (the knees
  give first), torso 0.25..0.9 D, arms 0.3..1.1 D, head 0.3..1.2 D.
- **The killing blow** (QC `VR_Ragdoll_NoteDeathHit`, combat.qc, before `Killed`): `.vr_death_hit` (PositionalRegion's
  1 head, 2 limb, 3 leg, 0 body; a shotgun's pellets: head if half of them; -1 unknown) and `.vr_death_hitpos` (the
  point, from his origin as he died: his body is thrown after, `vr_kill_push`). A headshot, or a beheading
  (`.vr_headless`): **collapse**, base 0.05, no pin, stumble, wound or crawl. A beheaded monster's ragdoll made at once
  (from whatever frame) stays limp as before (no death animation to drive): a pure collapse.
- **Stumble**: his own motion as he died (vr_box3d.cpp `deathMotion`: `ownMotion` of his last frame alive, within
  0.6 s) at least `vr_ragdoll_active_stumble` (90 u/s): the pin stronger (0.8) and longer, its anchor carried on along
  his motion (dying away over 0.4 s); the leg behind steps ahead (a reach: its foot lifted along a 0.32 s arc to a
  stride ahead of the pelvis, on the floor below); the legs fade 0.32 s later.
- **Holding the wound**: a body, limb or leg blow (`vr_ragdoll_active_wound` 0.7 chance): the part hit (the nearest of
  its rig's vertices to the point, within 16 units; not the head), the hand on the wound's side unless that arm is the
  one hit (then the other), its reach to the wound (2 units off the surface, outward) from 0.1 s, strength 0.8,
  until 1.6 D, gone by 2.2 D.
- **Crawling** (`vr_ragdoll_active_crawl` 0.25 chance, the humanoids): after 1.1 D, low and nearly still, he crawls.
  Face up or on his side, he first rolls onto his front (the whole body turned about the edge on the floor he rolls
  over, at 4 rad/s, at most 1.6 s). Then `vr_ragdoll_active_crawl_time` (3 s) of strokes, the arms in turn (1.1 s: a
  reach ahead to the floor, the hand lifted on the way; then the hand held where it was put down while the torso is
  pulled along: friction plus the rest to 0.7 m/s, at most `vr_ragdoll_active_crawl_force` 1.3 body weights), his
  head's way; arms 0.9, torso 0.45, head 0.5, legs 0.1 (dragged); then all fade over 1 s: he goes limp.
- Rolls (wound, crawl) are hashed from his entity, the frame and `vr_ragdoll_active_seed`.

### Debug and tests

- `vr_ragdoll_active_debug 1` (Debug > Logs: Active Ragdoll Targets): each driven joint's actual bone (white) and its
  target (green as strong as it is, grey limp), the pin (yellow), the reaches' goals (red). Drawn with or without
  Show Physics Shapes.
- `vr_ragdoll_active_list` (Debug, World and Physics: Active Ragdolls): each one's strengths per channel, pin, parts held, what its
  death does; the drive's cost since the last look. `vr_ragdoll_list` prints the same line under each active one.
  `vr_debug_ragdoll 1`: each active death (its clip, collapse, stumble, wound hand, crawl, where it was struck), a crawl's
  end (how far); 2 a roll's progress; 3 each crawl pull.
- `vr_ragdoll_active_trace n`: every n-th frame a `ragtrace` line per ragdoll (driven or not): pelvis, the joints' error
  from the death animation per channel (deg), their strength, the energy (J), the fastest spin, the pin, the
  step/wound/crawl flags, how far crawled.
- `vr_test_kill n` (Debug, World and Physics: Kill the Nearest Monster; QC `VR_TestKill_Frame`): the live monster nearest you
  (not the training dummy) killed by a 20-point shot of yours struck on its body (1), head (2), an arm (3), a leg (4).
- `Misc/quakevr/ragdoll/active_test.sh <agent> [deaths head wound crawl stumble decap off cost]` (`active_trace.py`
  sums each death up and fails on NaN, a crash, energy over 5 kJ, or energy over 50 J / spin over 25 rad/s once the
  drive let go; `EYES=1`: contact sheets via `active_sheet.py`).

### Checked (headless, `active_test.sh`, vrfiringrange and e1m1, `sv_random_seed 7`)

Each death of the grunt, enforcer, knight, death knight and ogre summed up by `active_trace.py` (the joints' error from
the death animation per channel while strong, deg; when the pelvis reached the floor; how long driven; energy and spin
once the drive let go):
- **Body shot**: limp at 10-25 % of the death (the first frame not solid; before: 31-36 %), errors legs 7-24, torso 3-19,
  arms 26-64 (a hand on the wound leads them off the animation), head 6-17; driven 1.5-2.4 s; at rest after (0-11 J,
  spin under 1 rad/s; the limp ones the same, 0-10 J). No NaN, no crash, no explosion in any of the 45 runs.
- **Headshot**: strength 0.05 from the start (no channel over 0.5): a collapse; still energy 0-3 J after.
- **Wound** (chance 1): body, arm and leg shots: a hand held the wound in all 15 (first 11: the test's made-up point,
  off the side of the box, lay over 12 units from the nearest vertex for an arm or the enforcer; the search is 16 units
  now. A real shot's point is on the mesh: precise hits).
- **Crawl** (chance 1): the grunt, knight, death knight and ogre rolled over (they die on their backs: the shot throws
  them back) and crawled 29-38 units in 3 s; the enforcer in his first death animation lies too high to crawl (he
  dies where he lies, as asked).
- **Stumble** (e1m1, coming at you from 450 units, killed after 1.7 s): grunt 133 u/s, enforcer 180, knight 160:
  stumbled, a leg stepped; the death knight (78 u/s) and the ogre (19) were too slow (`vr_ragdoll_active_stumble` 90).
- **Beheaded** (a sword's slash, `vr_decap_test 1`): limp at once, as before (no drive).
- **Off** (`vr_ragdoll_active 0`): limp at Go Limp At's frame (31-36 %), no `ragtrace` drive: today's ragdolls.
- **A limb held while driven** (the off hand takes a shin 0.4 s into the death, `vr_ragdoll_active_fade 4`): its thigh
  and shin get no strength while held (`2 parts held`), the rest stays driven; back after the hand lets go.
- `ragdoll_test.sh` flat stairs blast shot gib cap save grab pile: unchanged results (a loaded game's ragdoll made
  limp at frame 28; the grab, the throw, the gibbing as before).

**Cost** (8 grunts killed in 8 frames on vrfiringrange, `--exclusive`): the drive 0.020 ms a frame for 8 driven
(0.0027 ms each); the physics step over the first 60 frames 0.43 ms against 0.35 with `vr_ragdoll_active 0` (the
springs, the pins, bodies awake longer), and while some crawl (5 s on) 0.23 against 0.11 ms (a crawler stays awake 6 s).
`ragdoll_test.sh cap` (e1m1, not exclusive): 0.88 / 0.72 ms against 0.70 / 0.51 over the first 50 / 120 frames.

Contact sheets (`EYES=1`, the kit's `scratch/active_<monster>.png` and `_limp.png`; `KP=0`: no kill push) and the debug
overlay looked at for the grunt and the knight: the body follows the death animation's fall and lands as limp ones do;
the crawl rolls him over and drags him on his arms.

### API for the later stages

- Stagger and get-up (option 2): `begin` on a knockdown's ragdoll (it is made by the same `createRagdoll`), a `Clip` of
  the pain or first get-up frame (`loop` for a writhe), Fades for the channels, a `Pin` with a breaking rule (watch the
  torso lean; drop the pin: `dropPin` is internal, set `pin.force` 0), then the existing get-up blend. `held` already
  covers the judo pins.
- Liquids: lower `base` or the Fades in water (`ragdollsInLiquids` knows each part's depth); the pin off when floating.
- Holds on the world (catch fall, ledge hang, wall slide; section 8): `Reach` per limb is the effector (`goal`, `weight`,
  `strength`); an attach joint to the world or a brush body is the next piece (not here); probes via a `Context`
  callback like `floorBelow`.
- Dismemberment: cut parts are skipped (`drivable`: the part and its parent uncut, its joint valid); a reach whose limb
  is cut stops. A limb cut could also `weaken`.
- Living hit reactions: the drive is independent of death (`begin` + a `Clip` of the current frame + a strong `Pin`); the
  overlay and traces work for any driven ragdoll.

### Not done (deferred)

- No brace / catch-fall, protect-head, writhe or get-up preparation yet (they are Clip/Reach users for stage 2).
- No Box3D spring torque cap (not needed so far).
- Saved games: an active death is made again limp (as before), its drive state is not saved.
- The zombie is in the class list but only ever has a ragdoll beheaded: always the collapse.
- Determinism: the drive's rolls are seeded, but the QC's own (which death animation, the AI) still vary between runs
  even with `sv_random_seed`: the test prints which frame each went limp in.

**In the headset**: shoot grunts and knights in the body (they fall following their death animation, legs giving
first, often a hand going to the wound), in the head (they drop), kill a charging knight (he stumbles on a step), and
watch for one rolling over and crawling. Grab an arm of one still dying: it should go slack in the hand. Turn
Gibs and Corpses > Ragdoll Settings > Active Deaths off to compare with the old limp ragdolls.

## Box3D updated to upstream 5d83df83 (feature branch, 2026-10-11, worktree `box3dup`)

Vendored Box3D (`Quake/vr/external/box3d`) from `5643cd81` (2026-09-25) to upstream main `5d83df83` (2026-10-07), the 8
commits since: AVX2 with run-time CPU detection (the contact solver's and the hull-hull manifolds' wide kernels compiled
twice from `*_wide.inl`, 4-wide SSE2 in `wide4.c` and 8-wide AVX2 in `wide8.c`; the SIMD layer split into
`simd_sse2/avx2/neon/scalar.h`), dynamic tree perf, shape casts for box movers (we use none), compound
recording/serialization fixes (`recording_replay.c` renamed `replay.c`). No API change touched our code (it built
unchanged; `B3_ALIGNMENT` went to 32, which our allocator passes through). The `QVR` change in `src/mesh_contact.c`
(pre-solve for mesh contact points: props through teleporters) re-applied at the same place, after the accepted
manifolds are chosen. README rewritten: commit, SIMD paths, and the threads (the engine's job pool through Box3D's task
callbacks, `vr_box3d_workers` while `vr_box3d_threads_bodies` are awake; the old "single-threaded" paragraph was stale).

**Build.** The exe must keep running without AVX2: only `wide8.c` is compiled for AVX2, and Box3D calls it only when
CPUID says AVX2 and the OS saves the YMM registers (`src/simd.c`). Under clang/gcc upstream does it with a target
pragma round the kernels (`B3_AVX2_BEGIN`: `target("avx2")`, no FMA), under MSVC's cl with `/arch:AVX2` on that file
(as upstream's CMake; `vr.cmake` does the same). Our clang-cl 19 refused `wide8.c`: `verstable.h` includes `intrin.h`
inside the pragma, and clang's `intrin.h` redeclares `_xend` (already defined `target("rtm")` by `immintrin.h`)
-> "attribute 'target' multiversioning cannot be combined with attribute 'always_inline'". Fixed in the build, not the
sources: `wide8.c` gets `/FIintrin.h` (`quakevr.props`, `vr.cmake`; worth reporting upstream). Checked: `wide8.obj` has
6951 ymm instructions and no `vfmadd`; the rest stays at the x64 baseline; FMA contraction still off.

**`vr_box3d_avx2`** (Debug > Profiling, "Physics AVX2", default **0**): 1 uses the AVX2 kernels where the CPU has
them (`b3World_EnableSSE2Fallback` before each step). The console says at the first map which path runs:
`box3d: Box3D 0.2.0, SSE2 kernels (4 lanes; vr_box3d_avx2 0)` (`b3GetVersion` says 0.2.0, the release after the
0.1.0 tag). AVX2 and SSE2 give the same world to the bit (`vr_physics_mtbench 200 300`: hash `d33bc06e429210a0`
both, every worker count). Default 0 because in the game AVX2 measured no faster (below).

**Performance** (i9-13900K, `--exclusive`, ms a frame, mean of 3 runs; `SV_Physics` holds the Box3D step):

| scenario | before | SSE2 | AVX2 |
|---|---|---|---|
| `play_props_fire` SV_Physics | 0.320 | 0.316 | 0.336-0.340 (2 sets) |
| `play_e1m1_lights` SV_Physics | 0.511 | 0.504 | 0.513-0.526 |
| `props_500_active` SV_Physics | 1.129 | 1.095 (1 noisy set 1.186) | 1.090-1.097 (3 sets) |
| `ragdolls_32_active` SV_Physics | 0.895 (254 awake) | 0.842 (230 awake; 1 set with an outlier run) | 0.824-0.853 |
| `vr_physics_mtbench 500 600`, 1 worker, ms a step | 0.326 | 0.320 | 0.262 |
| same, 4 workers / 8 workers | 0.210 / 0.200 | 0.213 / 0.203 | 0.188 / 0.182 |

Box3D's own solver phase in play_props_fire's scene (`vr_physics_frametime`, 900 paced frames, 3 runs each way,
alternating): SSE2 0.075-0.090, AVX2 0.088-0.104 mean. 300 props tossed (`box3dmt/physbench.sh`): median step
SSE2 0.394-0.423, AVX2 0.410-0.427. So AVX2 wins only in mtbench's world (boxes on a box floor, no world mesh); in
the game (props on the map's mesh, small hulls, ragdolls) it is even or a little slower. The update itself: about
even in light scenes, 3% less in props_500, 6% less in ragdolls_32 (fewer bodies awake on average: 230 vs 254).

**Behaviour** (old and new build, the same tests): ragdolls (ragdoll_test.sh, all 17 cases) end the same way: every
ragdoll at rest, pelvis heights within a couple of units, the stairs, pile, walk, blows and burn cases alike; teleporters
(teleporters_test.sh throw, teleporter_edges_test.sh, teleporter_cross_test.sh): props, ragdolls and corpses through
the gates, the controls stopped at the wall; throws (throw_spin, throw_grace, throw_trigger, gib_2h, throw/flick
slowmo) within a few percent; weapon_catch, held_over_props, slopes (8/8), the reload suite (8 scripts, 0 FAIL;
self-test 67/0), decap, selfchecks: PASS. No difference seen beyond run-to-run noise but ragdolls_32's lower awake
count (bodies fall asleep a little sooner).

Pre-existing (the old build too): `teleporter_edges_test.sh` push/held print an empty or 0 place for some cases (the
harness looks for an entity number the run did not get), and `walltorch_grab_test.sh` takes 0 of 120.

**In the headset:** a few props piled and knocked over, a ragdoll or two, a crate through a teleporter: nothing should
look different. Debug > Profiling > Physics AVX2 on, same again; the frame timing should not get worse.

## Runtime brushes: the toolgun's Brush tool, stage 1 (feature branch, next release, 2026-10-11, worktree `brushtool`)

**The request** (the author's): "Add the brush tool to the toolgun. The created brushes should have the same
functionality as normal brushes (within reason): climbable, react to light and collisions, etc. Don't worry too much
about persistence now but design the system so saving/loading from files would be easy. If reasonable, design so it
supports baking." The design is `BRUSH_TOOL.md`; this is its stage 1 (boxes), on `feature/toolgun2`.

**What changed.**
- **The brush** (`vr_brushes.cpp`): planes (outward normal, distance, world space, doubles) with each face's texture
  and Valve 220 alignment; its text is a .map brush (`toText`/`fromText`, Valve 220 written, Quake's format read too),
  the one form for saves, copies, the network and a bake later. Polygons are cut from the planes (convex sets in
  general, boxes from the tool).
- **Its entity and model:** `func_qvr_brush` (SOLID_BSP, MOVETYPE_PUSH, made in C: no QC spawn function), model
  `*qvrbrush/<id>` made in memory at `Mod_LoadModel` (`VR_RuntimeBrushModel`): own planes, hull 0 a chain of nodes (one
  a face, the last behind them solid) with real surfaces/edges/vertices/texinfo of the map's textures, hulls 1 and 2
  chains of the planes pushed out by the world's boxes (axial bevels for non-boxes). The name is per id, an id reused
  after its brush goes and its model made again in place (same `qmodel_t`, `brushes::revision`): edits never use up
  precache slots. A model whose brush is gone (a save's) is empty, never a load error.
- **Drawn by Ironwail's own brush path:** `R_NewMap` reserves `vr_brush_max` (256) slots at the end of the brush
  vertex, index and indirect-command buffers (64 corners, 192 indices, 8 commands a slot) and a strip of the lightmap
  (16 texels a slot). A brush's model points `firstcmd`/`texofs`/`vbo_firstvert` into its slot, so the world shader
  (depth pre-pass, lightmap, dynamic lights and their shadows, detail, normal maps, fog, the toolgun's glow) draws it
  as a door; its instances also take the world's decals (`retro[3]`). Light: one texel a face, the map's light where
  it is (`R_LightPoint` 4 units out), shaded by its way (`vr_brush_shade`). Shadow casting: the dlights' and the
  map lights' "moving things" casters read `vbo_firstvert` (free); the map-light casters now take runtime brushes too.
- **Hooks:** Box3D movers made again when the model's revision changes (old hulls retired till the world goes);
  `vr_hull` (narrow player box) meets a brush's planes directly (Quake 2's box against convex planes), never caching
  it; ledges for runtime brushes (`fromThisMap`), dropped on change (`ledges::forget`: holds let go); decals laid on
  brush faces (`clipToWorld`, `hitWorld`), removed with the brush (`decals::removeIn`); `vr_audiosim` skips them (its
  `atoi(name + 1)` made sub 0); `modelcollide::forget`; the load-time brush buffers skip them.
- **The toolgun's Brush tool** (`vr_toolgun_brush.cpp`; the panel's sixth tile, a box icon): Create (trigger on a
  surface starts the base on the grid, held it follows the aim on that plane, let go it rises with the aim, trigger
  makes it), Move (held, grid steps; X/A + stick nearer/further), Resize (a face's handle held), Paint (the panel's
  texture; Paint: Whole Box / One Face), Copy (a ghost stamp), Delete; Pick takes a texture from a shot surface. Ghosts
  are lines and see-through faces with the size over them; a tick in the hand at each grid step. No box over the
  player or a monster (refused). The panel: mode tiles, Grid 1..64, the map's textures as thumbnails (12 a page).
  The physgun leaves boxes to it. `vr_toolgun_tool` 5; `vr_toolgun_brush_mode` 0, `_grid` 8, `_paint_all` 1.
- **Saves:** not saved yet (stage 3). A save with boxes loads without them (said once: their entities removed).
- **Commands:** `vr_brush_box|here|set|move|texture|texture_set|delete|duplicate|list|text|textures|selftest`, tests'
  `vr_brush_spawn <class> <x y z> [yaw] [drop]`, `vr_brush_ent <n|last>`, `vr_brush_trace`, `vr_toolgun_brush_status`,
  `vr_toolgun_brush_mode_set`. Debug > Tools > Boxes (Brush Tool).

**Tests** (headless, vrfiringrange): `python Misc/quakevr/brush_test.py brushtool [--shots] [--perf]`: PASS. Self-test
(a box, a wedge and a prism through their text and back, hull 0/1/2 probes: 35 checks); the player put over a box
stands on it (ground: the box) at z 89 and falls to 41 when it's removed; a crate let fall rests on it (97.1) and falls
to the floor (33.2) when it goes; climb: the mantle play on a 48-high box mantles onto it (ground: the box); a grunt on
a platform in the air walks 189 units along it to the player, on it all the way; a grunt behind a box wall has no
enemy, the wall removed it takes the player and comes; a rocket's scorch lands on a box (1 mark), none after it goes;
the tool makes a box by drag and raise (176 -616 17 .. 200 -552 81), moves it 40, pulls its top to 104, paints,
copies and deletes; a save with a box loads without it (said), its trace falls through, a new box stops it.
`toolgun_panel_test.sh` (six tools now) and `toolgun_joint_menu_test.sh`: PASS. eval.sh: skipped (no takes).
Shots: scratch/brush_shots.png (a dynamic light on a pillar and its shadow, the Create ghost, a scorch, the panel).
**Cost** (`--perf --exclusive`, 50 pillars in view): making them 1 ms, their slots and light the next frame 67 ms
(once: about 1.3 ms a box, mostly its six light samples and uploads); the frame's GPU render 0.90 -> 0.96 ms, world 0.25 -> 0.37 ms; CPU render 0.90 -> 0.93.

**In the headset:** take the toolgun, B/Y, the box tile (sixth). Create: trigger on the floor, drag, let go, raise
your hand, trigger. Stand on it, climb it (both hands on its lip), shoot it, put a light (rocket) by it. Move, Resize,
Paint (pick a texture tile), Copy, Delete. Tell us: the grid (8) and the tick at each step, whether drag-then-raise
is natural, the ghost's look, and how the boxes' flat light sits in the room.

## Active ragdolls: liquids (feature branch, 2026-10-11, worktree `ragliquid2`)

Stage 3, his goal: "Swimming/flailing when in water/drowning or dying in lava." Built on stage 1's drive (Clip, Reach,
Fades; the API unchanged: additions only). Behind `vr_ragdoll_active_liquids` (default 1 on this branch; Gibs and
Corpses > Ragdoll Settings > Swim and Flail in Liquids, Paddle Time, Flail Time). Same monster set (grunt, enforcer,
knight, death knight, ogres; the zombie only ever dies beheaded: a collapse, no swim).

### What it does (`vr_ragactive.cpp`, "Liquids": `swimFrame`, `startSwim`, `stopSwim`; `Swim` in `State`)

- **Detection, per body part**: each frame the pelvis, chest and head centres are looked up (`Context::liquidAt`, new:
  vr_box3d.cpp `activeLiquidAt`, the same `liquidAt` as the ragdolls' buoyancy, so currents are water). A swim begins
  when the pelvis or chest is in lava or slime, or in water with water still 10 units over it (not lying in a puddle),
  within 6 s of the death (`liquidWindow`): dying in it, drowned in it, or falling in during or just after the death
  animation. A body thrown in later is a corpse and stays limp. Never twice per death. A headshot or beheading (a
  collapse) never swims. A limp death's drive is woken for it (`liquidPending`: vr_box3d.cpp `driveActive` still calls
  `drive` for one within the window).
- **On entry** the death's own pieces stop: the pin dropped, the step, the hand on the wound and the crawl given up,
  the death clip off (the strokes lead); the channels' Fades replaced by the swim's.
- **Water: paddling** (`vr_ragdoll_active_paddle` 4.5 s, each death 0.8-1.2 times it, seeded): a dog paddle (each hand
  a reach circling ahead of the chest, pulling down and back, the arms in turn), a bicycle kick (each foot a reach
  drawn up and pushed down and back), the torso held straight, the head tipped back; stroke rate 1.3 Hz slowing to
  0.6 Hz and the circles shrinking as it weakens. Strengths: arms 0.9 (fading from 0.35 T to T), legs 0.7 (0.3-0.9 T),
  torso 0.45 (0.25-0.8 T), head 0.8 (0.35-0.75 T). **Head kept up**: while the head is under, 0.3 body weights lift,
  half on the head and half on the chest, until 0.4 T, gone by 0.7 T: the head breaks the surface and bobs there, then
  goes under and the body settles face down (it floats, as before) or, armoured, sinks to the bottom.
- **Lava and slime: flailing** (`vr_ragdoll_active_flail` 1.5 s; slime 1.4 times; each 0.8-1.2 times it): each hand
  and foot thrown to a new random place (about its own side: arms up and out, legs down) every 0.13-0.18 s, blended
  smoothly; the torso arching, bending and twisting (2, 1.6, 1.3 Hz), the head thrashing (2.7 Hz); arms and legs at
  1, torso and head 0.8, all fading from 0.3-0.4 T to T; stroke rate 3.8 Hz slowing to 2.2. The burning is the QC's,
  unchanged (vr_burning.qc: lit in lava, burnt through after `vr_burn_lava_gib`).
- **Out of the liquid** (pelvis, chest and head all out for 0.5 s) or the cvar turned off: its strengths fade from
  where they are over 0.4 s, then limp.
- No new joint code: the strokes are Reaches (weight 1, no strength floor: the channels' Fades lead) and torso/head
  targets written over the drive's after `deathFrame`; the drive ends by itself when all fade to 0.

### Debug and tests

- `vr_debug_ragdoll 1`: `ragdoll: N in water 0.01 s after its death: paddles for 5.0 s`, `... out of the water ...:
  stops`. `vr_ragdoll_active_trace n` adds a `ragswim` line while it swims: the liquid, its vigour, head under (and its
  z), the head lift (N), its hands' and feet's speed against the part their limb hangs from (m/s).
- `Misc/quakevr/ragdoll/liquid_active_test.sh <agent> [water knight ogre enforcer hknight drown fallin head lava slime
  off land cost]` (live monsters dropped from noclip into vrtesthall's pool, e1m7's lava, e3m1's slime, then killed or
  left to die there), summed up by `liquid_trace.py` (fails on NaN, a crash, an explosion, energy over 50 J or spin over
  25 rad/s after the drive).

### Checked (headless, `sv_random_seed 7`)

| case | what | swum | limbs early / late (m/s) | head under 1st / 2nd half | pelvis z start, low, last | after: ke, spin |
|---|---|---|---|---|---|---|
| grunt, body shot on the pool's bottom | paddles | 4.9 s | 1.10 / 0.20 | 54 % / 69 % | -98, -98, -17 (floats) | 0 J, 0.1 |
| knight | paddles | 4.2 s | 1.26 / 0.09 | 88 % / 94 % | -102, -121, -121 (sinks) | 22 J, 4.7 |
| ogre | paddles | 4.2 s | 1.54 / 0.24 | 62 % / 70 % | -101, -101, -29 | 2 J, 1.1 |
| enforcer | paddles | 4.2 s | 1.10 / 0.33 | 78 % / 90 % | -96, -98, -42 | 1 J, 1.6 |
| death knight | paddles | 4.2 s | 0.92 / 0.27 | 86 % / 100 % | -89, -116, -116 (sinks) | 35 J, 4.9 |
| grunt drowned (vr_liquids.qc, ~17 s on the bottom) | paddles | 5.1 s | 1.48 / 0.20 | 49 % / 67 % | -98, -98, -16 | 0 J, 0.1 |
| grunt killed in the air over the pool | in 0.76 s after death, paddles | 4.0 s | 2.18 / 0.40 | 64 % / 70 % | -16, -59, -17 | 1 J, 0.3 |
| grunt, headshot in the pool | no swim (collapse) | - | - | - | - | 18 J, 1.8 |
| grunt falls into lava, dies of it | flails | 1.2 s | 2.02 / 1.70 | - | -42, -44, -36 | 0 J, 0.5; gibbed |
| grunt falls into slime, dies of it | flails | 2.1 s | 1.53 / 0.88 | - | -330, -330, -195 | 33 J, 2.0 |
| `vr_ragdoll_active_liquids 0` | no swim (death animation, 1.0 s) | - | - | - | - | 21 J, 2.9 |
| on land (vrfiringrange) | no swim; stage 1 unchanged | - | - | - | - | 0 J, 0 |

The grunt on the bottom (head 70 units under) rises to the surface in about 1.2 s paddling, his head out and bobbing
for about 1.5 s, then under for good at about 2.7 s, face down; the strokes slow from about 1.1 m/s to 0.1. No NaN,
crash or explosion; most energy while swimming 3.6 kJ (the death knight), 12.9 kJ for the body falling in (its fall).
`active_test.sh deaths` (grunt, knight) unchanged. **Cost** (4 grunts killed together in the pool, `--exclusive`): the
drive 0.013 ms a ragdoll-frame against 0.008 with liquids off (3 point lookups a frame each); the physics step 0.14 ms
against 0.19 (the noise of which bodies are awake). Contact sheet (the worktree's `scratch/liquid_sheet.png`): a grunt
in vrtesthall's pool (the water's surface hides most of him headless: the numbers above are the check) and one in e1m7's lava: arms and legs thrown
about, burning, then limp spread-eagled on the surface.

### Not done

- A living knockdown in water does not paddle (knockdowns are stage 2's; `liquidPending` needs an active death).
- The strokes give no thrust (the liquid's drag is the same in every direction): the paddle is seen, it does not swim
  anywhere. The head lift is what brings him up.
- A standing monster drowning is not changed (it is an animated monster until it dies; its death then paddles).

**In the headset**: shoot a grunt or knight standing in water (vrtesthall's pool, e1m1's) or knock one in and kill it:
he should paddle up, head out for a moment, then go under and float face down (a knight sinks). Kill one on a ledge
over water: he paddles once he lands in it. Push one into lava (e1m7) or slime: a short violent flail as he burns, then
limp. Compare with Swim and Flail in Liquids off.

## Active ragdolls: stagger, judo, get-up (feature branch, 2026-10-11, worktree `ragstagger`)

Stage 2 of the programme (the kit's `scratch/ACTIVE_RAGDOLLS.md`, 4(b1), 4(c), section 8). His goals: "Shoving with
some stumbling (might end up falling or recovering), judo throw more realistic, more realistic 'getting up' animation
when on the ground." The same classes as stage 1 (grunt, enforcer, knight, death knight, ogre, zombie), with
`vr_ragdoll_active` on. Two toggles (default 1 on this branch; Gibs and Corpses > Ragdoll Settings): Active Shoves and
Throws (`vr_ragdoll_active_stagger`), Active Get-Ups (`vr_ragdoll_active_getup`). KNOCKDOWNS.md has the rules.

### Staggers (`ragactive::beginStagger`, `staggerFrame`; vr_box3d.cpp `ragdollStagger`, `ragdollStaggered`; QC `VR_Knockdown_Try`, `_Think`)

- **Which shoves**: every shove that could knock it down (its chance above 0; not over a ledge, the sure knockdown as
  before; not wading). The QC makes it a knockdown at once (touchable, not solid, `.vr_kd_stagger` 1) and the engine
  drives its ragdoll; the QC asks every 0.05 s how it goes (`ragdollstaggered`: 1 going, 0 fell, 2 recovered).
- **Its balance** = 0.5 + 2.5 x (the roll - the knockdown's chance), 0..1 (`VR_KD_STAGGER_SLOPE`): below the chance it is
  likely to go over, the push decides at the edge. Everything holding it up goes with it: its weight held up by its
  muscles (a force up on each part, 0.3..0.95 g), its pelvis pin (0.1..1 x `vr_ragdoll_active_stagger_pin` 2 body
  weights), its upright (below), its torso's strength (0.5..1); its legs at full strength, its arms 0.6, head 0.8.
- **Its pose**: its stand frame (frame 0 of each class: stand1), turned as its pelvis is (a pain frame twists the ogre
  60 degrees); the body is made in its pain's first frame (the flinch) and goes back to it. Not the pain clip: the
  grunt's painb is a fall.
- **The push**: its pelvis pinned where it is and carried a share of the standing slide (`vr_ragdoll_active_stagger_travel`
  0.2 of `VR_Shove_Reach`, at most 1.25 m, easing out over 0.35 + 0.45 x travel s); its parts' motion along the shove
  (the pin's first speed by height, its top `vr_ragdoll_active_stagger_push` 2 m/s x the shove's push more).
- **Upright** (`turnWhole`): the pin's own turn is the pelvis's alone, mass-normalised: far too weak for the body on it
  (the shove pitched the grunt's pelvis 50 degrees and it stayed there). The whole body's spin and its motion about the
  pelvis are eased towards turning back to its stance (8 rad/s per radian x its balance, at most 4 rad/s).
- **Steps** (capture steps): a foot farther than a fifth of its leg from its stance under its middle's capture point
  (its middle plus its level motion x sqrt(height / g)) lifts along an arc there (0.22 s, a leg's reach), one at a
  time, at most 4; feet not stepping are left to their joints and the floor (a reach to where they stood straightened the
  ogre's knees and lifted it off its pin). Step targets keep the foot's end as high over the floor as it was (the ogre's
  ends lie 8 units under its soles).
- **Falls** when its middle leans past `vr_ragdoll_active_stagger_lean` (30 degrees) over its base (its feet, the
  stepping one where it lands) for 0.12 s while not stepping, or 1.5 x that at once, or its pelvis sinks below 55 % of
  its height over the floor. Then a Fall (below); the QC: down from then (`vr_knockdown_time_min`..`_max`).
- **Recovers** after its travel and 0.2 s, not stepping, leaning under 12 degrees and its pelvis under 0.4 m/s (or still
  up after 2 s): handed back (`handBack`: its stand frame fitted where it stands, room within 24 units, a 0.2 s blend,
  `vr_ragdoll_active_stagger_blend`); no room for 0.5 s: it falls. QC `VR_Knockdown_Afoot`: solid, `th_run` (the training
  dummy, which has none: `th_stand`; the first version's NULL think was a Host_Error in the cost test).
- Hit again while staggering (a QC knock): its strengths halve. Killed: limp at once. `vr_ragdoll_active 0`: falls.

### Falls and judo (`beginFall`, `fallFrame`; vr_box3d.cpp `startFall`, `ragdollThrown`; vr_foegrab.cpp `throwDown`)

- A stagger gone over, a shove's knockdown (`ragdollShove`, also over a ledge) and a two-hand throw: its legs give
  (0.05 over 0.25 s), its torso keeps 0.5 (a throw 0.7: a curl) and its head 0.6 of the pose it went down in, its arms
  brace: each hand to the floor ahead of its shoulder the way it goes (its top's motion), or to a wall within an arm's
  reach that way (`Context::rayHit`), held where it touches. Landed (its chest low, or after 1.4 s): a flinch (torso and
  head 0.7 fading over 0.9-1 s), the hands let go, then limp (and the knockdown's struggle).
- **Judo**: the parts nearest the holding palms (or, in `vr_foegrab_throw_test`, two points on its upper arms) are
  slack for 0.6 s with their limbs (as held), then ease back; its pinned feet are slack (held) while the topple holds
  them. The throw's own motion (the author's topple, sweep, launch) is unchanged: the throw trace's summary is the same
  within noise (tilt at 0.5 s 102 / 103 degrees, turned 347 / 333, feet 18 / 19 units), plus the arms and the curl.

### Get-ups (`beginRise`, `riseFrame`; vr_box3d.cpp `ragdollGetUp`, `getupEnd`, `handBack`)

- `ragdollgetup` fits the get-up frame and finds room as before; with Active Get-Ups (not in a liquid, not `anywhere`) it
  starts the body's rise and returns -1 (the QC asks every 0.05 s), then 3 or 4 once risen.
- **The frames**: Quake VR's models' frames have no names, so the end is found by the pose: from the chosen frame on, the
  first whose pelvis is 7/8 of the way up to the stand frame's (at most 12 frames, never into a death); a death frame
  (the death knight's): backwards to that death's first. Grunt 52-59, enforcer 89-99, knight 39-40, death knight 62-54,
  ogre 85-96, zombie 173-162 (its rig counts its paine as a death: it rises up its fall backwards).
- **Rolled over first** when its chest faces the other way from the first frame's (`rollOver`, the crawl's roll, now
  shared), then pulled in over 0.35 + 0.4 x (its pelvis's turn / pi) + 0.3 x (the place's distance / 48 units) s
  (strengths 0.25 -> 1, muscles 0 -> 0.8 g, the pin 0 -> `vr_ragdoll_active_getup_pin` 2.5 body weights, 5 Hz, its turn
  12 Hz and as strong as its pull), then the frames played at 10 fps x `vr_knockdown_getup_speed` x
  `vr_ragdoll_active_getup_pace` (0.8), the whole body turned towards the frames' pelvis (`turnWhole`, 9 rad/s per
  radian), held 0.12 s at the last frame, then handed back there (`vr_ragdoll_active_getup_blend` 0.2 s), `th_run`.
- The QC not asking for 0.35 s (hit: its time down extended), or its pelvis a metre off once pulled in: it slumps (a
  Fall) and tries again later. Its contacts while rising or staggering are not falls (no fall damage).
- **Stealth fix** (also for 1.0.1's knockdowns): `VR_Stealth_Applies` was true for a knocked-down monster (touchable,
  not SOLID_NOT); an alert (the rising body's noise) set its think to its stand and it lay there for good. Knocked-down,
  staggering and getting-up monsters are now outside stealth.

### Checked (headless; vrfiringrange, `sv_random_seed 7`; scripts in the worktree's `scratch/`)

**Stagger outcomes** (`st_rates.sh`/`st_all.sh`: 8 fresh monsters each, shoved by `vr_knockdown_test 21` one hand or 22
two hands; push = `vr_shove_push_onehand` (0.7 default) or two hands at 1; the old rule's fall rate is the chance):

| Monster | chance 1 hand / 2 | 1 hand, push 0.4 | 0.7 | 1.2 | 2 hands |
|---|---|---|---|---|---|
| grunt | .49 / 1.09 | 3 of 8 fell | 4 | 5 | 8 |
| enforcer | .23 / .47 | 1 | 2 | 1 | 3 |
| knight | .20 / .40 | 2 | 0 | 0 | 3 |
| death knight | .08 / .15 | 0 | 0 | 0 | 0 |
| ogre | .10 / .19 | 1 | 1 | 3 | 3 |
| zombie | .21 / .37 | 0 | 1 | 0 | 4 |

Steady (chance 0.01: balance ~1) and sure (chance 100: balance 0), one hand: every class recovers 6 of 6 steady and falls
6 of 6 sure, but the ogre (sure: 2 of 6 fell; it barely moves, 1 unit of travel). No QC error, NaN or crash in any run.

**Get-ups** (`gu1.sh`: `vr_knockdown_test 0` then 1; the hand-back's gap: each vertex of the ragdoll against the
animated frame it is handed to, units, most / mean):

| Monster | rise (pelvis off at the end, turned off) | gap, risen | gap, before (blended from the floor) |
|---|---|---|---|
| grunt | 2.6 units, 8 deg | 16.0 / 4.6 | 35.0 / 16.0 |
| enforcer | 2.5, 4 | 33.3 / 14.1 (an arm 109 deg off) | 46.4 / 21.5 |
| knight | 3.7, 6 | 18.4 / 6.9 | 47.5 / 25.9 |
| death knight | 7.5, 1 | 18.8 / 8.2 | 45.8 / 12.9 |
| ogre | 1.0, 1 | 13.6 / 3.8 | 28.3 / 9.9 |
| zombie | 1.1, 4 | 5.5 / 2.4 | 18.1 / 4.3 |

The worst parts are the hands that hold a welded gun. One zombie run of about 15 ended 1644 units off (not reproduced
in 9 more; the rise now lets go a metre off). The grunt lies on his back after `vr_knockdown_test 0`: he rolls over
(107 deg), is pulled in 0.6 s and rises in 1.1 s.

**Stability**: get-ups (zombie, 3 runs) at most 90 J and 24 rad/s while rising; staggers (6 each) grunt at most 464 J
(the shove itself) and 24 rad/s, ogre 1239 J and 60 rad/s in the first frame (its parts' launch), after 1.5 s 12 / 161 J.
Killed or hit while staggering: no error (killed: limp; hit: recovered after 2 s). Stage 1's `active_test.sh deaths
stumble` (grunt, knight): as before (limp at 10-22 %, driven 1.0-2.4 s, late energy 0).

**Cost** (`st_cost.sh`, 6 grunts shoved two-handed in 6 frames, `--exclusive`): the drive 0.029 ms a frame for 6 (0.005
each); the physics step over the next 10 / 60 / 200 frames 0.170 / 0.207 / 0.173 ms against 0.125 / 0.171 / 0.143 with
`vr_ragdoll_active_stagger 0`.

Contact sheets (the worktree's `scratch/sheet_<grunt|knight>_<stagger_recover|stagger_fall|getup>.png`): the grunt
sways back on a step and stands; tips back and falls; rises from a kneel and runs.

### Not done, open

- The rates are physical at the edge: a strong one-hand push (1.2) fells a steady grunt half the time, a weak one (0.4)
  rarely; the ogre shrugs off even a sure one-hand shove. Tuning the balance slope or the push is his call.
- Recovery hands back to `th_run` (the pain's frames skipped); `attack_finished` from the shove still holds it a moment.
- Saved games: a stagger or rise is not saved (loaded: a plain knockdown).
- No hit reactions on standing monsters (option 4(b2)); no head protection, ledge hangs or catching on others.

**In the headset**: shove grunts and knights with one hand (they stagger, step back and recover, or go over) and two
(grunts go over, bracing with an arm); throw one with both hands (the arm you held flops, the rest curls); wait for one
to get up (it rolls over if on its back, pushes itself up and stands before running). Gibs and Corpses > Ragdoll
Settings > Active Shoves and Throws / Active Get-Ups off to compare.

## INTEGRATION: the feature branches merged for testing (worktree `integ`, 2026-10-11; pushed as `origin/next`)

**Merged** onto vr-ironwail `bc3163ed4`, in this order, each a real merge (`--no-ff`, its commits kept: revert one
feature by reverting its merge commit, `git revert -m 1 <merge>`): `feature/anchors` (10516525e), `feature/animsmooth`
(e0cb20afc), `feature/vore-shove` (178b6c556), `feature/propclimb` (4c3b93046), `feature/toolgun2` (3513e018a),
`feature/armbands` (45b6a27b6). Not merged: `feature/surface-anchors` (superseded by anchors), `feature/arcane`.

**Conflicts.** This file: every branch appended its section at the end; all kept, in merge order (armbands' section
merged by itself at "Bare forearms", after "Scoping"). `Quake/gl_shaders.h` and `Quake/vr/vr_api_render.h` (animsmooth
and armbands): both added fields at the end of the alias instance block; both kept, in the same order in the C struct
and the GLSL `InstanceData` (std430, all 16-byte: `Smooth`, `SmoothW`, `SkinTint`, `SkinTintRange`). Everything else
merged by itself: vr_cvars.inc, vr_menu.cpp, vr_menu_pages.inc, vr_view.cpp, vr_box3d.cpp (no two branches touched the
same lines). No branch bumped `configVersion` (125): no migrations to renumber.

**Checks** (each merge: build, `map e1m1` smoke; at the end, once each):
- anchors: `py -3.13 Misc/quakevr/anchors.py selftest` (needs numpy: the plain `python` lacks it): 165 anchors bound,
  0 moved.
- animsmooth: `vr_anim_smooth_check`: as on its branch (soldier: linear 6.5%, A+B 0.1% shrink; dog: 0.001 off a key
  at most 0.036 units). Eye shot, bare forearms with `r_lerpmodels 3`: wrists whole, ends tinted to the skin.
- vore shove (no script; vrtesthall, god, `vr_test_spawn 10` 64 ahead, `impulse 241`): starts at 2.45 s 54 units away,
  10 damage, pushed 256, slid 200 (the wall); `vr_enemy_shove_vore 0`: no shove.
- propclimb: `Misc/quakevr/climb/prop_climb_test.sh`: 8/8 PASS (heavy, stack, side, light, close, knock, hang, off).
- toolgun2: `toolgun_panel_test.sh` PASS; `toolgun_joint_menu_test.sh` PASS (each hand: 70 picks, 0 wrong, 6/6 joined).
- armbands: `vr_body_bracers 0` and 1, `map e1m1`, `vr_body_wrist_info`: progs/vrbody_bare.mdl (bare) / vrbody.mdl,
  no errors.
- `Misc/quakevr/climb/slopes_test.sh`: 8/8 PASS. Save/load (`save` on e1m1, `map start`, `load`): fine.
  `vr_menu_path_check maps/vrcalibration.map`: 0 missing.

**Then `feature/ragcore`** (ef165c6aa, active ragdolls stage 1), merged on top (`--no-ff`). Conflict: this file only
(its section kept, before this one). vr_box3d.cpp (with propclimb), vr_menu.cpp, vr_cvars.inc, vr_view.cpp merged by
themselves. animsmooth's `vr_rigpose.hpp/.cpp` (`qvr::rigpose`) and ragcore's `vr_ragpose.hpp` (`qvr::ragpose`) are
both rig-pose helpers over `ragdoll::Rig`; separate namespaces, no clash, both kept as they are (a later merge of the two
is for their owners). Checks: build, `map e1m1` smoke; `Misc/quakevr/ragdoll/active_test.sh integ deaths head` (the
quick subset): 10/10 settle, no NaN or crash, late energy 0-4 J, spin under 5 rad/s (hknight's headshot the most:
4.0 J, 4.7); `ragdoll_test.sh integ flat grab`: limp at frame 10, at rest after 4.3 s; a shin taken, held, thrown at
1086 u/s, at rest; `vr_anim_smooth_check` again: unchanged (soldier A+B 0.1%, dog 0.001 off a key at most 0.036).

**Then `feature/box3dup`** (bdff4df2f, Box3D upstream 5d83df83, `vr_box3d_avx2` 0), merged on top (`--no-ff`).
Conflict: this file only. quakevr.props, vr.cmake, vr_box3d.cpp, vr_cvars.inc, vr_menu.cpp merged by themselves (no
project file-list conflicts). Built with 0 warnings; the new library and every Box3D user (vr_box3d, vr_climb,
vr_ragactive, vr_toolgun) recompiled against the new headers with no change needed. Checks: smoke; prop_climb_test.sh
8/8 PASS; active_test.sh deaths head: 10/10 settle, no NaN or crash (one run left the ogre's headshot at 17 J, spin 2.4,
under the test's 50 J / 25 rad/s; that case run again: 0.0 J, spin 0.5); ragdoll_test.sh flat grab: limp at frame 10,
at rest after 4.3 s, the shin thrown at 1095 u/s; toolgun_joint_menu_test.sh PASS (each hand: 70 picks, 0 wrong, 6/6
joined); selfchecks_test.sh: PASS hull cache, PASS physics workers (the same world hash with every worker count).

**Then `feature/brushtool`** (d5482a9a3, runtime brushes and the toolgun's Brush tool, stage 1; built on toolgun2: only
its own 6 commits came in), merged on top (`--no-ff`). Conflicts: `vr_ledges.cpp` (both includes kept: vr_box3d.hpp
for propclimb's prop ledges, vr_brushes.hpp for runtime brushes); `vr_ledges.hpp` (propclimb's `propLedges` and
`heldBytes` kept; `of()`'s comment brushtool's, which covers runtime brushes and `forget()`); this file. r_brush.c,
vr_box3d.cpp (with propclimb, ragcore and box3dup), vr_hull, decals, menus, cvars merged by themselves and build against
the updated Box3D with 0 warnings. Checks: smoke; `python Misc/quakevr/brush_test.py integ`: PASS, all 16 checks
(selftest, stand, crate, save/load, climb: mantled onto a box, monster on a platform, sight through a wall, rocket
scorch, the tool's create/move/resize/paint/copy/delete); toolgun_panel_test.sh PASS (six tools); prop_climb_test.sh
8/8 PASS again.

**Then `feature/ragliquid`** (7e0a0811f, active ragdolls stage 3: liquids), merged on top (`--no-ff`). Conflict: this
file only (vr_ragactive.*, vr_box3d.cpp's driveActive hooks, the menu rows merged by themselves). Built with 0 warnings.
Checks: smoke; `liquid_active_test.sh integ` (all 12 cases): no NaN, crash, explosion or failure, numbers as on the
branch (water: grunt, ogre, enforcer paddle 4.4 s and float, 0-2 J; knight and death knight paddle 5.0 s and sink,
20 J / 29 J; drown, fall-in paddle; headshot: no swim; lava flails 1.6 s, gibbed; slime flails 2.4 s, 33 J; off and
on land: no swim); active_test.sh deaths head: 10/10 settle, late energy 0-1 J, spin under 2.5; prop_climb_test.sh
8/8 PASS. (Stage 2, `ragstagger`, not merged yet: expect mild conflicts with this in the State and the menu.)

**Then `feature/ragenv`** (d402b1720, active ragdolls stage 4: what is round him), merged on top (`--no-ff`).
Conflict: this file only (its section kept where its branch has it, right above "Active ragdolls: core and deaths").
vr_ragactive.* (with ragliquid's swim), vr_box3d.cpp, the menu and cvars merged by themselves; 0 warnings. ragenv
was made without ragliquid: `drive()` now runs `swimFrame` then `envFrame`, and envFrame doesn't know about swims (a
later pass for their owners: whether env should stand down while a body swims). Measured, not changed: the liquid cases
after this merge match ragliquid's own table (knight, death knight: paddle 4.4 s, limbs 1.27 / 0.09, head under
88 % / 94 %; the branch: 4.2 s, 1.26 / 0.09, 88 % / 94 %); slime's late flailing varies as much with
`vr_ragdoll_active_env 0` as with 1 (limbs late 0.28-0.78 against 0.48-0.75 m/s, two runs each): run-to-run, not env.
Checks: smoke; `env_test.sh integ wall ledge crate off cost`: lean on the wall and slide down (grunt: left hand, held
1.14 s, slid 8 units; knight: right hand), the ledge grabbed and hung from (grunt both hands, 2.1-2.2 s, then curls),
braced on the crate (no hold), off: no env lines; cost, 8 grunts: the drive 0.0425 ms a frame with env, 0.0236 without;
no NaN or crash. liquid_active_test.sh (water, knight, hknight, drown, fallin, slime): as above; active_test.sh deaths
head: 10/10 settle (late energy 0-1 J); brush_test.py PASS (16/16); prop_climb_test.sh 8/8 PASS.

**Integration fix (the coordinator's call): no environment while swimming.** `drive()` (vr_ragactive.cpp) skips
`envFrame` while `s.swim.liquid` is set and calls `envSwimming` instead (the hands' holds let go and their joints
destroyed, no lean or curl, the limbs left to the swim's reaches); envFrame takes up again once the swim stops.
Checked: the fall-in grunt's catch (both hands, made in the air) lets go as he enters the water ("swimming"), then 95
`ragenv` frames of the swim with no hand mode; the liquid cases (water, knight, drown, fallin, slime) as before (paddle
5.0 / 4.4 / 5.3 / 4.1 s, slime flails 1.9 s; no NaN or crash); env_test.sh wall ledge unchanged on land; smoke.

**Then `origin/vr-ironwail`** (7aea8dfa8, the stealth guard for knockdowns) **and `feature/ragstagger`** (33c13567a,
active ragdolls stage 2: staggers, braced falls, judo slack and curl, get-ups), each merged on top (`--no-ff`).
Conflicts: `QC/vr_stealth.qc`: one guard (`m.vr_knockdown`: both sides had it), the branch's comment (knocked down,
staggering or getting up). `vr_ragactive.hpp`: Swim and Stagger/Fall/Rise all kept, `Context` has `liquidAt` and
`rayHit`. `vr_ragactive.cpp`: the Liquids section and the Staggers/Falls/Get-ups sections both kept (spliced from each
side's file: git had interleaved them line by line); the debug list keeps the stagger line and the env line; two helpers
both named `level()` (ragenv's a level unit direction, ragstagger's the level part alone): ragenv's renamed `levelDir`.
`vr_box3d.cpp`: `activeLiquidAt` and `activeRayHit` both set on the Context. Menu: the liquids rows, then the stagger
and get-up rows. **drive()'s precedence: swim > stagger / fall / get-up > env.** swimFrame runs first (it begins
swims); staggerFrame, fallFrame and riseFrame are skipped while a body swims; envFrame stands down (`envStandDown`,
was `envSwimming`: holds let go, joints destroyed, no lean or curl) while it swims, staggers, falls or gets up.
Checks: build 0 warnings, smoke; `stagger_test.sh integ rates getup judo` (N=4): no QC error, NaN or crash, rates
within the branch's spread (grunt one hand 1 of 4 fell, two hands 4 of 4; the others 0-1 of 4); get-up hand-back gaps
as on the branch (grunt 16.1 / 4.4 units, knight 15.3 / 6.0, ogre 21.7 / 5.8, zombie 5.7 / 2.2; from the floor
35-47); judo: grunt, enforcer, death knight thrown, curled, landed 0.25-0.43 s. The knight (monster 5) thrown at 60
units lands at ~910 u/s ("a 518 unit drop": 147 damage), the same with `vr_ragdoll_active_stagger 0` and with
`vr_ragdoll_active 0`: older than stages 2-4 (the throw on vrfiringrange), not this merge. env_test.sh wall ledge, run
raw: the grunt leans (left hand, 1.15 s) and hangs from the ledge (2.13 s), no stand-down; liquid_active_test.sh water
slime fallin as before; active_test.sh deaths head 10/10 settle (late 0-5 J); prop_climb_test.sh 8/8; brush_test.py
PASS (16/16).

**Then `feature/ragdismember`** (1453f82d3, active ragdolls stage 5: dismemberment; made on ragcore alone), merged on
top (`--no-ff`). Conflict: this file only (its section sits above the core section, as on its branch). Integration:
`chestOf` was defined by both ragstagger and ragdismember, the same body: one kept. **drive()'s precedence: swim >
dismemberment / stagger / fall / get-up > env.** dismemberFrame (and its spin) is skipped while he swims (startSwim
drops the balance beat's pin); envFrame stands down for the whole dismemberment (the stump hold and the crawl lead his
arms; `envStandDown` "dismembered"); and the guard: a dismemberment is a death's (cutReaction needs one), a stagger, fall
or get-up a living monster's, so a body cut while still in one of those ends them (only the balance beat pins the
pelvis). Checks: build 0 warnings, smoke; `dismember_test.sh` (grunt): arm: the right hand to the stump, least 3.8
units, at rest; legs: crawled 32 units, at rest; dying: the hand to the stump, least 2.9, at rest; leg: the balance
beat with all three (`MONS="0 5 1"`) 1.06 / 1.11 / 1.08 s (the branch: 1.06 / 1.13 / 1.08), at rest. The grunt's beat
alone varies from run to run, 0.25-1.09 s over six runs (always broken by its lean, 15-17 deg; always at rest after):
for the branch's owner (its notes give one run). Then the earlier set again: stagger_test.sh rates getup (N=4): no
errors, get-up gaps as before (grunt 16.5 / 4.6, zombie 5.8 / 2.1); env_test.sh wall ledge (leans, hangs, then the
floor); liquid_active_test.sh water slime fallin (paddle 5.0 s, flail 2.4 s, fall-in 4.1 s); active_test.sh deaths head
10/10 settle (late 0 J); prop_climb_test.sh 8/8; brush_test.py PASS (16/16).

## 1.0.1: the author's values after his 1.0.1 test are the defaults (worktree `defaults101`)

"I tweaked some values, please make them the new defaults." His config of 2026-10-11 15:29 (after his VR test of 1.0.1,
build f701f0a26) against the shipped defaults (`resetcfg; writeconfig` on that build: compiled defaults, vr_defaults.cfg
and the migrations' end state): 72 archived settings differ. Config 126 moves a config still at an old value; a value
set by hand is kept.

| cvar (menu) | was | now | where |
|---|---|---|---|
| `vr_flashlight_head_zone_forward` (Head Zone Forward) | 0.05 | 0.07 m | vr_defaults.cfg |
| `vr_flashlight_head_zone_up` (Head Zone Up) | 0.19 | 0.15 m | vr_defaults.cfg (menu help said 0.040: now 0.150) |
| `vr_flashlight_head_zone_radius` (Head Zone Radius) | 0.105 | 0.125 m | vr_defaults.cfg |
| `vr_flashlight_forward` (belt clip Forward) | 0.02 | 0.025 m | vr_defaults.cfg |
| `vr_flashlight_flick_speed` (Flick Strength) | 800 | 750 deg/s | vr_cvars.inc |
| `vr_doom_crt` (Doom > CRT Look) | 1 | 2x | vr_cvars.inc |
| `vr_comfort_vignette_strength` (Vignette Strength) | 0.6 | 0.5 | vr_cvars.inc (the vignette itself stays off) |

**Not promoted** (differ, on purpose left alone):
- Arms: `vr_body_elbow_back` 0.35 (0.25), `_hand` 0.5 (0.4), `_lift` 8 (4): per-player settings (`personal()` in
  vr_cvars.cpp, "the player's to tweak", his decision of 2026-09-28; armsweep.py records them as his).
- Body and calibration: `vr_bodycal_*`, `vr_height_calibration`, `vr_bodycal_undo`.
- `vr_wofs_hs3_{x,y,z,pitch,yaw,roll}_04`: the nailgun's third hotspot, but its `hs3_type_04` is 0 (none): no effect.
- `vr_melee_phase_speed` 3.996 (4), `vr_melee_phase_time` 0.34965 (0.35), `vr_relight_strength` 1.1988 (1.2): each the
  default times 0.999 (an artefact, not a choice; melee is not tuned on his archived takes either).
- Window and recording: `vr_spectator_fov` 120, `vr_spectator_scale` 1.5, `vr_window_view` 0, `vr_mirror_hide_hud_text` 1,
  `vr_motion_*`.
- Hardware, video and flat screen: `vr_foveated` 2, `vid_*`, `fov`, `gamma`, `contrast`, `scr_*`, `sensitivity`,
  `cl_alwaysrun`, `volume`, `ui_live_preview`.
- Dev tools and bookkeeping: `vr_toolgun_*`, `vr_relight_reload` 0, `vr_menu_level` 2, `vr_notes` 1, `vr_menu_positions`,
  `vr_prop_id_*`, `vr_*_version`, `vr_tutorial_started`.

**Test:** a v125 config with the seven at the old values (Doom's CRT at a hand-set 1.5) → the six new values at default,
`vr_doom_crt` kept at 1.5, `vr_cfg_version` 126; a start with no config → all seven at the new values.

**In the headset:** the torch clipped on and off at the head (zones further forward, lower, larger), its belt clip,
the wrist flick; Doom's picture on the gadget (stronger CRT).

## A judo throw killed a knight by fall damage (1.0.1, 2026-10-11, worktree `judofall`)

Integration test and his playtest: a knight thrown with both hands on flat ground "landed at ~900 u/s, a 518 unit
drop", 147 damage of its 75 health (same with `vr_ragdoll_active 0`: older than the active ragdolls).

**Root cause:** a knocked-down monster's fall (vr_box3d.cpp `touches`, then QC `VR_Monster_Fall`: drop = v^2 / 2g,
0.4 damage a unit past 150) was the hardest hit of any single part of its ragdoll on the level, along the vertical.
A light part at the end of a turning body whips into the floor far faster than the body falls: the knight's sword
(its "loose" bone, welded in his hand while down) at 858-896 u/s while the body's middle came down at 20-50. Not
knight-only, the knight's long sword is the worst: on vrfiringrange's floor (`vr_foegrab_throw_test 0`, 60 units,
full health) the zombie's loose piece hit at 614 (34 damage), the mummy's at 573 (22), the fiend's tail at 560 (18),
grunts' guns ~315, an ogre's saw 344; the body's middle never came down faster than ~190.

**Fix:** before each piece of the step, each ragdoll's middle's speed down (its parts' by mass, a loose weapon not
counted, plus the piece's gravity: `RagdollBodies::fallDown`, `noteFallSpeeds`); a landing counts the part's hit no
faster than that. Flat-ground throws: every kind lands at 188 u/s or less, no fall damage (8 of the 11 kinds took
none before either). Real drops unchanged: a knight thrown 400 units up came down at 816 u/s, 107 damage, dead; lift
900: 584 u/s, 25 damage. No cap and no new cvar; standing monsters' and the player's fall damage untouched.
`vr_knockdown_debug 2` prints each hard hit: the part's speed, the body's, what it landed at. A slam that hurts on
flat ground would be its own setting (none asked for).

Merge note: origin/next adds a stagger/rise `continue` just above the changed lines in `touches`: keep both.

**In the headset:** judo-throw a knight (and a zombie, a fiend) on flat ground: it gets up, no fall damage. Throw
one off a high ledge: it still takes the fall (dies from high enough).

## A judo-thrown mummy (and zombie) shook itself apart (2026-10-11, worktree `judofall`, branch `agent/mummyrag`)

On next a mummy judo-thrown on flat ground (`vr_test_spawn 14`, 60 units) went unstable once down: parts into the
floor at 4000-12000 u/s, the body's middle up to 3000, fall damage 57-1049 though the fix above counts the body's speed.

**Root cause:** not the active ragdolls nor the Box3D update. The zombie's and the mummy's flesh (their "loose" bone)
is a 1.2-1.4 unit speck inside the chest in the rest pose ($stand1), and a ragdoll part is its rest pose's hull. Dead,
they are ragdolls only beheaded, and then it's hidden; knocked down alive it got a body: `looseMass` 3 kg in that speck
(166 times the body's density, inertia 1/600 of the chest's), welded in the chest (`vr_knockdown_weld`). The weld can't
hold it: it whirls (300 rad/s) and drags the body about. Random by the frame timing: 6 of 29 mummy runs on next blew up (2 more
had one part over 2000), 5 of 10 zombie runs (49-233 damage); `vr_ragdoll_active 0` 2 of 3; vr-ironwail too (parts 2335 and 11985 in 2 of 8, its
body under 180 there by luck). `vr_knockdown_weld 0`: 12 of 12 clean. The weapons welded the same way (grunt, enforcer,
knight, death knight, ogre) are 26-34 units and 0.1-0.45 times the body's density: fine.

**Fix** (vr_ragdoll.cpp `derive`, `looseSpeck` 4 units): a loose piece smaller than that in the rest pose is hidden in
every ragdoll (`Rig::hideLoose`, as the player's weapons). Only the zombie's and the mummy's are. A density cap alone
(tried: 20 times the body's) did not do: the light speck still whirled (parts 11000-15000). **Guard** (vr_box3d.cpp
`noteFallSpeeds`, `unstableSpeed` 5000 u/s): a ragdoll part that fast prints once a ragdoll "ragdoll: N class's part at
X u/s (spin, kg): unstable?" (with `vr_knockdown_debug` or `vr_debug_ragdoll`, else `developer`); nothing is changed.
`vr_knockdown_debug 2` also says what a loose piece was welded to and its density against the body's.

**Measured** (judo throw at 60 units, vrfiringrange): mummy 14 of 14 clean (8 active, 6 `vr_ragdoll_active 0`): parts
at most 401 u/s, the body 122, no damage; zombie 6 of 6 (403, 105, none). The 11 kinds once: every part under 910
(the knight's sword), bodies under 190, no damage. With the fix off the guard fired on both blow-ups seen (the speck at
5016 and 5074 u/s, 291-306 rad/s).

**In the headset:** judo-throw a mummy (Dissolution of Eternity) and a zombie a few times on flat ground: they land and
lie or get up, no fall damage, no thrashing. Nothing in their hands changes (the flesh was never seen down).
## Hide holstered weapons per pair (2026-10-11, worktree `holsterhide`)

The author: "Is there an option to hide weapons stored in the shoulder (highest) holders? Sometimes the holstered model
interferes with the view." He chose a simple toggle per pair; all but the shoulder pair drawn by default; one row in
VR Settings, all three under Advanced.

His "shoulder (highest)" pair is the code's **shoulder** pair (body::LeftShoulder/RightShoulder, QC hotspots 3 and 4:
on the back, reached over the shoulders; `vr_shoulder_offset_z` 16 + `vr_shoulder_holster_offset_z` -0.25, above the
chest's "upper" pair at 8.5). VR Settings > Holster Calibration calls them "Back Holsters".

- `vr_holster_weapons_draw_hip` 1, `vr_holster_weapons_draw_upper` 1, `vr_holster_weapons_draw_shoulder` 0 (archived):
  0 hides that pair's guns in `setupHolsters` (vr_view.cpp) as death does (`vr_dead_hide_gear`): the gun's view entity
  has no model, so nothing of it is drawn anywhere (the eyes, the desktop window and spectator camera, the body clone,
  shadows), nor its button, ammo screen, magazine, well, open super shotgun parts, lava glow or blood. The holster
  sleeve (`legholster.mdl`, hips and chest only) stays: the holster is there, only the gun is hidden. The Weapon Offsets
  Holstered preview and a holster posing session still show the gun (editing tools).
- Taking and holstering are untouched (server hotspots, reach, haptics, sounds). The draw blend: a gun drawn from a
  hidden holster appears in the hand at once, and one put in vanishes at once (no ease from or to an unseen pose).
  `vr_collect_fx` (a box or key shrinking into a holster) is unchanged: it is what is put away, not the holstered gun.
- Config migration: `vr_cfg_version` 127 sets `vr_holster_weapons_draw_shoulder` 0 in a config that has it 1.
- Menus: VR Settings > Holster Calibration: **Shoulder Holster Weapons**; Advanced VR Options > Body and Display
  (its Setup group; after Holster Models): **Hip / Chest / Shoulder Holster Weapons**.
- `vr_gear_status` adds a line: `gear: holstered guns drawn: hips N of M, chest N of M, shoulders N of M` (drawn of
  held).

Test (`scratch/holsterhide_test.sh`): both shoulder and both chest holsters filled (`impulse 154`, `vr_test_weaponinst
4`, then `1` into hotspots 3, 4, 8, 9, sticky grip): defaults `hips 2 of 2, chest 2 of 2, shoulders 0 of 2`; shoulders
on `2 of 2` each; all off `0 of 2` each; the mock main hand at the hidden right shoulder holster, grip: the gun (#8) in
the hand; let go there: back in hotspot 4. Migration: a 126 config with the shoulders 1 loads as 0, version 127.

**In the headset:** look over your shoulders and down at your chest with guns in every holster: the back pair's guns
gone, the hips' and chest's there. Reach over a shoulder and take the hidden gun, put it back (buzz and sound as ever).
Toggle Shoulder Holster Weapons in VR Settings and the three on Body and Display.

### Holstered guns read another holster's flags and clip (fixed)

The drawn holsters go hips, upper, shoulders; their stats (`STAT_QVR_HOLSTERWEAPON*0`) go in vr_body's order,
shoulders, hips, upper. The open super shotgun's parts (its flags, its shells) and the magazines (the no-magazine flag)
of a holstered gun indexed the stats by the drawn order: a hip's read a shoulder's, an upper's a hip's, a shoulder's an
upper's (a nailgun with its magazine out in a shoulder holster showed one, if the left upper's had one). Now
`holsterStat` (vr_view.cpp) reads them in the body's order, as the model always was. `vr_gear_status` lists each
holster as the view reads it: `gear: holster <name>: <model> clip N flags N; drawn, magazine, open parts`.

Test (`scratch/holsterstat_test.sh`): a nailgun with its magazine out in the left shoulder holster, full ones in the
right shoulder and the left upper: `lshoulder: v_nail clip 0 flags 16; magazine 0`, `rshoulder: v_nail clip 24;
magazine 1`, `lupper: v_nail clip 24; magazine 1`, hips `v_axe clip 0`, `v_shot clip 8`: every holster as
`vr_test_weaponinst 0` lists its hotspot.

**In the headset:** break the super shotgun open and holster it on a hip: it stays open there (and shut ones shut);
drop a nailgun's magazine and holster it: no magazine drawn in it.


## Review fixes: anchors, animsmooth, vore-shove (2026-10-11, worktree `fixanim`)

From the review of the three merges (group g1).

**Anchors.**
- Motion takes played back with the weapon anchors broken: the `' '` to `'_'` swap was on the melee settings line
  (no anchors there), not the weapon settings line, so playback kept the first number ("30.3762": not an anchor, bound
  to vertex 0: muzzle, ammo button and screen there for the replay). Now `weaponSettings` writes `_`, and playback
  turns it back into spaces; a value that is not three numbers (a take from before the anchors: an old strip-order
  index) gets the setting's default, as migration 41 does for configs, with one console warning per playback.
  `vr_motion_header_check` (in `selfchecks_test.sh`) round-trips the line and checks an index and a broken value.
  Checked by hand: a recorded take plays with its anchors' exact text and restores the player's after; the same take
  with indices (`614`, `5`) warns "2 weapon anchors ... their defaults instead" and plays at the defaults.
- Text that is not an anchor (an old config `exec`ed, a hand-typed index) still binds to the first vertex, now with a
  console warning once per model and text.
- `weapons::anchor(slot, key)` reads a slot's anchor text ("" for a bad slot or key: no crash); the 24 call sites that
  read `weapons::cvar(slot, *Anchor)->string` use it.
- The soldier's gun table listed seam twins' points twice to keep the vertex means; each point is listed once with
  its vertex count now (14 + 6 posedAnchor calls a frame -> 6 + 4; the same means). anchors.py selftest: 155 anchors
  (was 165: the 10 repeats), 0 moved. The enforcer's had no repeats.
- Not committed: the "0.00000 in every pose" probe. It compared against the build before the anchors (strip-order
  indices), which this tree no longer has; `anchors.py selftest` and `vr_anchor_where` A/B runs remain the checks.

**Smoother animations.**
- `vr_anim_smooth_pin` and `_trace` are off after a map change (they named an entity by number: the next map's entity
  with it froze). `vr_anim_smooth_info` prints both.
- Each model's rest data is uploaded once to a GL buffer of its own (again when `vr_anim_smooth_rig_error` remakes it),
  not every frame. A/B screenshot of a grunt pinned mid-swing (seeds, fixed frames): identical pixels.
- The per-entity memos are also freed on a model reload (they held swing rows of the old rig).
- Debug > Reports > Trace Nearest Monster's Motion (`vr_anim_smooth_trace 180`); vr_animsmooth.hpp says Debug > Reports.
- `Misc/quakevr/animsmooth_test.sh <agent>` (in the release suite): the check's keys exact, 0.001 off a key within 0.1
  units (0.045 worst), a grunt drawn smoothed with limbs swung, the trace, the pin off after a map change.

**Vore shove.** The reach note above corrected (a touching vore was already in reach at 52); `shoveSlide` lost a
check `fieldFloatOr` already makes.

**Left for the author:** Ironwail's own Options toggle (menu.c) writes 0 or 1 over `r_lerpmodels 3`; `r_lerpmodels 2`
shows as "Quake's Blend" in the cycle (nearest); `modelData` derives the rig even with `vr_anim_smooth_rig 0`; no
committed vore-shove test.
## Review fixes: active ragdolls (2026-10-11, worktree `fixrag`; review g4/g5)

- **Get-up onto a taken spot** (ragstagger): a finished rise was handed back at the spot chosen 1-2.5 s before, solid
  inside a player or monster now standing there. `ragdollGetUp` checks the spot again (`standSpot`, 24 units): it
  stands beside it, or holds 0.5 s and slumps. `getup: N rises through ... (at x y z)` now prints the spot.
- **Get-up slump loop** (ragstagger): a rise that lost its pose (`riseLost`) was pulled in again 0.05 s later, mid-slump,
  for ever. A lapse while still asked for now returns 0 once (the QC waits `vr_knockdown_retry`); after 2 lapses the
  body gets up the old way (blended from the floor), as the notes above claimed ("tries again later").
- **Balance beat on the run** (ragdismember): `startBalance` kept the death's stumble, whose carry pulled the pin up to a
  metre off the planted foot. It is kept as far as it got (no jump) and ends. A knight cut at 117 u/s balances 1.15 s
  (0.15 before); at 184 u/s his own momentum still tips him in 0.08 s.
- **Stale hitch** (raghit): a leg hit's sidestep not taken in its 0.5 s (pain frames) waited for a later hit; dropped
  now on any new hit and when its window ends.
- **Grabbed crawler** (ragcore): a hand on the pelvis or chest stops the crawl's pull and the roll-over.
- **Split steps** (ragcore): a drive's forces (crawl pull, swim head lift, lean pull, living support) went into a slow
  frame's first step piece only; kept per ragdoll (`driveForce`) and given again with the lift. A grunt's 3 s crawl:
  31.4 units at 72 Hz, 28.6 at 30 Hz.
- **Liquids** (ragliquid): `vr_ragdoll_active 0` mid-death no longer lets the body's later fall into water wake a
  swim; the lava/slime flail's hops now follow the stroke rate (every 0.13-0.18 s at full vigour, slower to 2.2/3.8 of
  it as it weakens; the torso and head keep their fixed rates).
- **Environment** (ragenv): a hand whose hold target was destroyed before it grabbed lets go ("its hold gone"; it
  reached for ever); a hold's anchor is on the goal's plane (1.5 units off the wall or lip, not 3); an env-driven arm
  is solved once a frame (drive() no longer re-applies last frame's env reach first); the ledge map is looked up only
  for a death whose env is on.
- Cleanups: one `chestOf` (three copies), `blastRagdoll` uses its own ragdoll, describe() drops a stray "(none)".
- Tests: `stagger_test.sh` prints PASS/FAIL per case and a FAIL count, with new `taken` and `lost` cases;
  `dismember_test.sh run` (e1m1, a running knight's leg cut: beat at least 0.5 s); `liquid_active_test.sh actoff`.
- Checked: smoke; stagger_test (grunt, knight) 0 FAIL; env_test wall/along over seeds 1-4, grunt and knight, against
  origin/next: lean holds 10 of 16 both, "out of reach" 2 (0 before), slides alike; ledge, hit, crate, front as
  before; active_test deaths/wound/crawl/stumble, liquid water/fallin/lava/slime/off/land, dismember leg/legs/dying,
  hitreact ai/slow: no error, numbers in their usual ranges.

Not changed (his call): vr_hitreact.cpp's own `chestOf`/`regionOf` (they read the rig, not the parts), fallFrame's
arm brace beside stage 4's Catch, the env "hit" test through a real shot.
## Review fixes: climbing props, the toolgun's panel, runtime brushes (2026-10-11, worktree `fixtools`)

The review of `next` (groups g2 and g3) found these; each is its own commit on `agent/fixtools`.

**Climbing props.** A hand's light-prop flag was set only on a prop's hold, so after holding a light prop the same
hand's next hold elsewhere kept it: a hold the QC moves (or a heavy prop's) no longer carried the body. It is reset on
every hold; the hold's debug line says `, light` after its height on a hold not on a prop (a stale flag), which
`prop_climb_test.sh`'s new `lift` case looks for: the light case, then vrclimb's lift held as it rises (FAIL without
the fix: `holds at 51.2, light`; PASS with it, carried to z 74). Movers Quake pushes carry the body either way
(`VR_ClimbCarried`), so the old bug showed only with holds the QC moves. The `light` and `close` cases now need the
slip ("pulled away"), not any let-go. Show Ledges' prop ledges use a scratch set (`ledges draw`), no allocation a
frame. 9 PASS. Not covered: the small crates (spawns 123, 124): `vr_climb_try` finds no hold at a small crate's lip,
alone or on a large one, where a large crate's is found ("0 holds seen", the hand on the spot the large crate's hold
has); left for a look (the claim of a hold on a small crate on a stack is untested).

**The toolgun.**
- The gun's stick press while the trigger holds something (the physgun's prop, Scale's gizmo, a box being made or
  moved) is swallowed: pushed hard while adjusting it changed the tool and dropped the prop.
- More's page closes when the stick press changes the tool.
- A held monster turns with the hand's yaw only as the hand points within 60 degrees of level, none past 80, a share
  between: near straight up or down the Euler yaw swings with a small tilt (by a half turn over the top).
- The thumbnails' .mdl and .bsp readers bound every count and offset from the file before adding or multiplying.
- The panel's widget names hold 48 characters ("Clear Props Made Since the Map Loaded", 37, was cut).
- Leftovers: one `toolgun::haptic`, `interact` uses `hovering`, the joint hints checked against the joint kinds
  (`static_assert`), `mockLaserHand` inlined. ROUND22's stick press note: a left-hand toolgun takes the move stick's
  `+speed`.
- **The muzzle on the lens's middle** (the author: "Please fix the off-center toolgun muzzle"): slot 26's
  `MuzzleAnchor` is `15.4 0.0064 5.7941`, the lens's front face's middle vertex (`anchors.py check`: 0.0000 off the
  surface; the old point was a corner of the lens's back, 0.86 model units off: 0.33 Quake units, about 1 cm). Weapon
  settings 42 move a config still at the old point (his has it, as `vr_wofs_muzzle_av_27`). In the game the aim's start
  moved (-0.2, -0.3, 0.2) units at a level aim (up onto the barrel's axis). `brush_test.py`'s tool corners moved with it
  (the far edge a grid step: -560 and -520, checked with the old point set by hand: -552 and -512).
- `toolgun_panel_test.sh` covers the stick press while holding (still the physgun, still holding), More closed by the
  stick press, the monster as the controller goes over its top (yaw unchanged: the old way turned it a half turn),
  Remove's long button by name. PASS; thumbnails 15 made, 0 failed.

**Runtime brushes.**
- A paint (only textures changed) keeps the box's revision: no new Box3D hulls, ledge map or held weapons' triangles,
  its marks kept. A move, resize or removal drops the marks laid on the box (each mark now carries its brush: 0 none,
  -1 several, those by the old box), not the world's beside it (`vr_decal_count` prints the marks laid and those on
  brushes; `vr_brush_list`'s count is the brush's own).
- A box's edit drops only its own ledge map: holds on the world's or other boxes' keep their depth and slide data
  (`ledges::forgets`, `ledges::alive` by the map's serial; the global generation is for a new world only).
- Box3D frees a box's old hulls once no mover body of that model and revision is left (they were kept to the map's
  end: one set per Move, Resize or Paint); `vr_debug_box3d` says so.
- The world's usable textures and the default are found once a world, a cache emptied at a map or game change (no
  rescans every panel frame; no stale default when a later world lands in the same model slot).
- `brush_test.py`: the rocket's scorch kept when painted, gone when moved, the hulls made again for the two moves only
  and freed; six marks on the floor under a box (a unit under it) kept through a paint, a move and a resize; a box made
  and moved while the hands hold vrclimb's long ledge, then the mantle. PASS. The last doesn't tell the old code from
  the new (no readout of a grip's map; the slide came out the same with the old check), so the ledge
  change is checked by reading only.

**Box3D update.** CREDITS.md's row: 0.2.0 (main `5d83df83`), run on the engine's job pool.

**Not changed (his decision):** the toolgun's snapping on X/A (F2). Also left: the carry divided by the light-prop
grips too (g2's second finding: a design choice), the panel redrawn every frame (F8: the cheats' lit states and the
info line come from game state, so a dirty check isn't simple), the brushes' frozen animated light (g3's third).

**Tests:** `prop_climb_test.sh` 9 PASS, `slopes_test.sh` 8 PASS, `toolgun_panel_test.sh` PASS, `brush_test.py` PASS,
e1m1 smoke clean.

**In the headset:** the toolgun's beam leaves the lens's middle; push the gun's stick hard while turning a held prop
(X/A): it stays held; with More open, the stick press shows the new tool's page; lift a grunt with the physgun and
point the gun steeply up and down: it doesn't spin. Make a box against a scorched wall, paint and move it: the wall's
mark stays.

## Runtime brushes, stages 2-5: shapes, rotation, baked light, saving, the full bake (2026-10-11, worktree `brush2`)

**The request** (the author's): "expand the brush tool to also support other shapes, baked lighting, saving, and full
bake. Keep multiplayer-specific features as a TODO for now." Design and what was built: `BRUSH_TOOL.md` section 8.
Four commits on `agent/brush2`.

**What changed.**
- **Shapes, rotation:** the panel's Shape row (Box, Wedge, Cylinder, Pyramid) and Sides (3..12); Create makes the shape
  in the box dragged (a wedge rising away from the gun). New **Rotate** mode (after Resize on the panel; mode value 6):
  the trigger held on a brush turns it with the hand, in `vr_toolgun_snap` steps (15), Tex Lock on the panel
  (`vr_toolgun_brush_texlock` 1). Resize stretches any shape by its bounds. **Undo** on the panel (`vr_brush_undo`, 64
  deep). Hulls 1/2 and the narrow player box use the faces plus axial and edge bevels (no snag off a turned corner).
  Commands: `vr_brush_shape`, `vr_brush_rotate`, `vr_brush_align`, `vr_brush_undo`. New cvars
  `vr_toolgun_brush_shape` 0, `_sides` 8, `_texlock` 1.
- **Baked light** (`vr_brushlight.cpp`): each brush has a 64 x 32 lightmap rectangle (real luxels, 16 units, up to 4
  lightstyles a face); flat light at once, then a bake from the map's lights (id/ericw formulas, spotlights, sun,
  minlight, shadows through the world and other brushes, plus the map's unexplained light: bounce, sky, glows), 3 ms
  a frame; neighbours rebaked after an edit. `vr_brush_bake` 1, `_ms` 3, `vr_brush_luxel` 16, `vr_brush_bake_fill` 1,
  `_reach` 512, `_report` 0; `vr_brush_light`, `vr_brush_lights`, `vr_brush_rebake`.
- **His open question** (a box made in a flickering room kept one frame's brightness): the bake puts each light in
  its lightstyle's map, so the box flickers with the room (e1m1's corridor: the face towards the light has style 10);
  the flat light before the bake reads every style at its average.
- **Saving:** saved games keep their brushes (`// qvrb` comment lines in the `.sav`, made again on their entities at
  the load); brush files `<game>/brushes/<map>.brushes` (`vr_brush_save`, `vr_brush_load [name|@map] [add]`,
  `vr_brush_autoload` 0). An older build loads such a save without the brushes.
- **Full bake** (`vr_brushbake.cpp`): `vr_brush_bake_map` (panel: Bake Map) relights the map with the brushes in it as
  submodels (light.exe lights them, their shadows land on the world), then reloads where you are; the brushes take
  light.exe's light from the relit world while unchanged ("relit"). `vr_relight_revert` (or a plain relight) takes it
  out. `vr_brush_bake_write` writes the input without light (tests).
- Debug > Boxes: Undo, Save/Load This Map's Brushes, Load Brushes with the Map, Bake the Brushes' Light, Bake Every
  Brush Again, Bake the Map with Its Brushes, The Lights the Bake Sees.
- **Coop:** TODO, written up in BRUSH_TOOL.md section 8.

**Tests** (headless): `python Misc/quakevr/brush_test.py brush2`: PASS, 32 checks. New: the self-test's tool shapes, a
45-degree box (hull 1 empty 17 off its corner, solid at 15: the bevel) and a box turned 90 degrees (axial again);
the player stands on a wedge's slope (his height the slope's at his box's edge); a box trace stops 8 off a turned
box's corner (vr_hull_width 16); undo of a move, a turn, a removal; a wedge made and turned 45 by the mock hand,
undone; a save with a box and a turned, painted wedge loads with both as they were (traces stop on them; the next
id 3); brush files round trip and autoload; baked light: sunlit side 154 vs shaded 86, bottom 0, a pillar beside it
89..153 (the box's shadow), even (within 8) once the box goes, the same values after a save's load; e1m1's flicker
style; the written bake maps (BSP2 vrfiringrange, BSP29 e1m1) solid at 81 (hull 0) and 105 (hull 1), the brushes
"relit" from them (a turned wedge too). **Not run:** light.exe itself (none in the kit: "ericw-tools' light not
found"), so `vr_brush_bake_map` end to end is untested; its input and read-back are.

**In the headset:** toolgun, Brush tile. Pick Wedge / Cylinder (Sides 6) / Pyramid and drag one out; Rotate: hold
the trigger on it and twist your wrist (15-degree ticks); Undo. Watch a new box: flat light at once, then (a moment
later) shadows and the light's fall-off; build next to a flickering light (e1m1's corridor) and see it flicker; put a
box between a light and another box. Save, quit, load: the brushes are there. With ericw-tools downloaded: Bake Map
(a minute or more), then the box's shadow on the floor. Tell us: Rotate's feel (wrist vs. stick), shape defaults, and
how the bake's light matches the room.
## Shoves: the backpedal, and the active ragdolls' known gaps (2026-10-11, worktree `ragmove`)

His words: "When humanoid enemies are shoved, they seem to cover much less distance than before, and they do not
backpedal trying to keep balance. I would like them to backpedal a few steps before falling down (or managing to
recover)." and "Please address the known gaps".

### `stagger_test.sh` on `next` (3-4 FAIL there, 0 on `agent/fixrag`): neither a merge regression nor flakiness

`agent/fixrag`'s 0 FAIL was `stagger_test (grunt, knight)` (`MONS="0 5"`); `next` runs the default six. The ogre and the
death knight failed on their own, the same on both branches (QC and the engine's stagger code are identical):
- **judo**: `vr_foegrab_throw_test` on a fresh ogre or death knight prints "not hurt enough" (they are the
  `vr_foegrab_throw_when_hurt` kinds, thrown only below `vr_foegrab_throw_hurt` 0.4 of their health): no throw, no
  "landed". The test now hurts each to 0.3 first (`vr_foegrab_hurt 0.3`).
- **steady** (the ogre): ragstagger's own notes already said "the ogre shrugs off even a sure one-hand shove": its falls
  and recoveries didn't follow its balance (4 of 8 steady ones fell, 3 of 8 sure ones stood). Fixed with the backpedal
  below (the outcome follows the balance); the ogre still goes over on its own about 1 steady shove in 10 (top-heavy:
  its middle 20 units past its feet), which the test spares.

### The backpedal (`vr_ragactive.cpp` "Staggers"; `vr_box3d.cpp` `ragdollStagger`)

Measured first (`scratch/dist.sh`, one-hand shoves on vrfiringrange, `vr_knockdown_test 21`): the old standing slide
(`vr_ragdoll_active_stagger 0`, "shove slide: went", 1.0.1's) against the stagger's travel:

| | grunt | enforcer | knight | death knight | ogre | zombie |
|---|---|---|---|---|---|---|
| old slide (units) | 84-116 | 73-124 | 86-135 | 66-112 | 44-100 | 90-127 |
| stagger before (travel 0.2, at most 1.25 m) | 18 | | 19 | | 7-11 | |
| backpedal now: steady (recovers) | 107, 6 steps | 94, 6-9 | 110, 6 | 75, 6 | 54-58, 6-9 | 117, 6 |
| beaten (gives way, then falls on) | 52-55 at 0.47 s, 3 steps | 51 | 59 | 57 | 40-45 | 64 |

- **Distance**: `vr_ragdoll_active_stagger_travel` 0.2 -> **1.2** (`vr_cfg_version` 128 moves a config still at 0.2:
  his has it), the cap 1.25 -> 4 m; its time 0.4 + 0.3 x travel (at most 1.6 s; a grunt's 3.4 m over 1.42 s).
- **Steps**: quicker the faster it goes and the farther it leans (0.22 s down to 0.13 s), each landing within 0.75 of a
  leg of where its pelvis will be (the leg's reach), lower to trigger while leaning; at most
  `vr_ragdoll_active_stagger_steps` (new, 6; Ragdoll Settings > Backpedal Steps), a steady one 3 more while it still
  leans (catch steps).
- **The outcome follows the balance** (the knockdown's roll against its chance: under 0.5 is the old rule's knockdown):
  a **beaten** one backpedals 3 or 4 steps (4 nearer the edge) and **gives way** (a Fall, its arms bracing); a **steady**
  one takes its steps and **recovers**. Held up enough to backpedal either way (its muscles' hold and pin 0.7 + 0.3 x
  balance: at 0.5 + 0.5 x a beaten grunt's knees gave on his 2nd step). Physics can still fell either: twice
  `vr_ragdoll_active_stagger_lean` for 0.1 s, or (steady) past it for 0.36 s, or the pelvis sagging. So shoves knock
  down at the knockdown's chance again (steady 6 of 6 recover, sure 6 of 6 fall, every class; the ogre 18 of 20
  steady).
- **Its arms** thrown out ahead (against the shove) and wheeling at 2 Hz, the more the shakier it is; the upright's
  strength 0.5 + 0.5 x balance.
- Debug: the fell / recovered lines say how far it moved and where its middle was (`vr_knockdown_debug 1`).
- Ragdoll Settings: Backpedal Steps and Backpedal Distance rows; Stagger Falls Past's help says the new rule.

### The known gaps

- **Living knockdowns swim** (`vr_box3d.cpp` `livingSwim`; `ragactive::beginLivingSwim`, `swimGoal`, `swimDies`): a
  knocked-down monster of the active classes lying in water deep enough paddles at full strength while it lives (its
  head tipped back and held up for good), its strokes pushing it (0.2 body weights at the pull) towards where it can
  stand (`standSpot`'s liquid search, looked for again every second), turned to face it; in lava or slime it flails.
  Killed meanwhile, its strokes weaken as a death's from then. The knockdown's struggle waits while it swims.
  vrtesthall's pool: a grunt knocked down there paddles 2.5 s, head out after 0.5 s, and gets up 80 units from where it
  lay (128 before: the get-up searched from farther out); killed after 1.4 s, its vigour falls to 0.01 by 3.5 s more.
- **Strokes give thrust**: a dying paddle's strokes push it ahead (its chest's way, 0.05 body weights at the pull,
  weakening with it).
- **Lava/slime flail**: the torso's and head's thrash rates follow the strokes' (slowing from 3.8 to 2.2 Hz with them),
  not fixed (the fix worker's open question).
- **Saved games: staggers** (`ragdollstaggered` 3, QC `.vr_kd_balance`, `.vr_kd_dir`): a loaded game's stagger is begun
  again where it stands (its ragdoll made again in the frame it was drawn in), no push, from its saved balance: steady
  it recovers, beaten it gives way. A stagger still "on" 8 s after it began goes down (`VR_KD_STAGGER_MOST`: its
  ragdoll never made again). Rises were already begun again from how it lies (a fresh ragdoll asks for a new one).
- **One-legged balance hops** (`dismemberFrame`): leaning 7 degrees from his line, his foot hops under his pelvis (0.2
  s, lifted 0.1 of the leg, at most 0.4 of a leg, a little to the side lost), his line taken again; at most 3; the pin
  holds him up meanwhile (his lean isn't judged mid-hop). The grunt's leg cut: one hop at 0.52 s, the beat lasting its
  full 1.2 s (1.06-1.13 s before, broken at 15 degrees).
- **The hand on the cut face, bloodied**: the stump's hold goes to the joint's cut face (facing along the lost bone)
  when it is within 0.75 of the holding arm's length of its shoulder, else the surface beside it as before. Each time
  the hand is within 4 units of it (every 0.7 s, at most 4) a light melee wound is sent where the hand is
  (`progs::sendWound`, QC's `woundevent` from the engine): the palm bloodied.
- **A stump out of reach** (the ogre's): his torso curls towards it (the holding shoulder turned towards the stump about
  his pelvis, at most 0.6 rad shared by the torso's joints, strength 0.6) while it is past 0.85 of his arm.

### Decisions (the fix worker's open questions)

- A knight cut at full sprint tipping over in 0.08 s: **no brake**. At 184 u/s his own momentum carries him over the
  one foot left; an invisible brake would look like the hand of God. Slower (117 u/s) he balances 1.15 s, now with hops.
- Lava flail torso/head rates: they now slow with the limbs (above).

### Not done (his call, or a feature of its own)

- **A standing monster drowning** is still an animated monster until it dies (then its death paddles): turning a live
  one into a ragdoll fighting for air is a new behaviour (when, and does it get out?).
- **Pull-up from a ledge hang**: hangs are dying men's (env runs for deaths only); a living monster's hang and pull-up
  needs the knockdowns to use the env first.
- **Ledges on movers**: the movers' ledge maps (model space) are still not looked up.
- **Head protection, catching on others**: what should they be: hands to the head when struck while down? grabbing an
  ally as it falls?
- **Recovery skips the pain frames**: the backpedal is the pain's flinch now; handed back to `th_run` as before.
- A death's drive is not saved (a loaded dying body lies limp).

### Checked

Headless, `sv_random_seed 7`: `stagger_test.sh ragmove steady backpedal saved judo getup` (N=6, all six classes) 0
FAIL (steady 6 of 6 recover and sure 6 of 6 fall for every class; the ogre 18 of 20 steady in a longer run);
`liquid_active_test.sh kdswim kddie water lava land` PASS / as before (a dying grunt paddles 4.2 s, head under 62 % /
80 %, at rest after); `dismember_test.sh leg arm forearm`: the beat its full 1.2 s for grunt, knight and ogre (the
pelvis 3-7 units lower by then: 2 before), the hand on the stump 2.7-5.1 units (grunt, knight; 2-6 before), the ogre's
forearm 8.0 (10-16 before), its leg 3.5-15 (8 before: it varies); e1m1 smoke. Build 0 warnings.

**In the headset**: shove grunts and knights one-handed: they should reel back several quick steps, arms thrown out,
about as far as a shove slid them in 1.0.1, then stand and fight, or (as often as Knockdowns' chance says) give way
after 3 or 4 steps and go down bracing. Ragdoll Settings > Backpedal Steps / Distance to taste. Knock one into
vrtesthall's pool: it should paddle with its head up towards the edge and climb out; kill it there: it weakens and
sinks. Cut a grunt's leg standing: he should hop once or twice before falling, and hold the stump's end with a
bloodied hand. Save mid-stagger and load: it staggers on (recovers or goes down).
## Dead ogres standing in Dawn of the Machine (1.0.1 report, 2026-10-11, worktree `deadpose`)

A player ("real", Discord, 1.0.1): in MG3 "dead ogres is posing like this" (upright, arms out, legs apart, on a blood
pool), "most of them", and he "can't do nothing with them": no grabbing, carrying or cutting dead bodies.

**Cause.** Those are not killed ogres: they are the map's own corpses, `misc_corpse` (QC vr_mg_corpses.qc, the
rerelease's code: 310 in MG3's maps, 7 ogres in `start` and 7 in `map2b`; MG1 has them too). Its pose table gave the
ogre frames 116 and 126 (`$death5`, `$bdeath1`: upright) and the shambler 87 (`$death5`); every other monster's entry is
a death's last frame. Quake VR's ogre keeps id's frame order (checked against the rerelease's ogre.mdl: the same
names), so they stood. And a `misc_corpse` was scenery (SOLID_NOT, no FL_MONSTER): no physics body, no ragdoll, no
damage, so no hand, blade or blast did anything. Killed monsters were fine in every check: MG3 `vr_test_spawn` ogres
(id's, rocket, marksman) and `impulse 205` on map1/map2/map5 (no cap: 52 and 104 ragdolls, 0 not lying), after an
id1 map then the campaign switch, knocked down then killed, saved and loaded.

**Fix.**
- The pose table: the ogre's 125 and 135, the shambler's 93 (the deaths' ends).
- A placed corpse of a monster or player model is a dead monster's body (`vr_corpse_placed` 1, Ragdoll Settings >
  Placed Corpses Are Bodies; 0: scenery, the next map load): the monster's classname and death function (its head,
  gibs, corpse health, ragdoll settings, gore tables), FL_MONSTER, health -1, armed as a corpse (VR_Corpse_Arm); it
  is a corpse in the physics (pushable pose), shot, gibbed and burnt. It sleeps (`.vr_corpse_placed` 1: no ragdoll)
  until a spawned player comes within `vr_corpse_placed_wake` (160 units), then it is a ragdoll as room allows (no
  animation motion inherited). Room (VR_Knockdown_MakeRoom): a placed one no player is near (1.5 times the wake
  distance) lies as placed again instead of being removed; one a player is near is kept, as a dead player's body is. A
  woken one refused room sleeps again past 1.5 times the distance, to try again. Heads, gibs and the fish stay scenery.
- Safety net (vr_box3d.cpp settleCorpses): a dead monster with a rig, no ragdoll, still for 2 s in a death frame
  that is not its last (drawn upright or mid-fall) is put at that death's last frame, once; the first in a map is said
  in the console (`corpse: ... lay still ... put at its death's last frame`), the rest under `developer`.
- A rig that cannot be made (another .mdl or an .md5 in Quake VR's model's place) is now said in the console once,
  not only under `developer`: that monster's dead have no ragdolls.
- `vr_corpse_poses [1]` (Debug > Tests): every dead monster and placed corpse, its frame and where that is in its
  death, a ragdoll or why not, "NOT LYING" for mid-death ones; a count line.

**Tests** (`Misc/quakevr/ragdoll/placed_corpse_test.sh deadpose`): poses, before 7 ogres not lying in start (of 58
placed) and 7 in map2b (of 29), after 0 and 0; scenery
(placed 0): 29 lying, no ragdolls; grab: a placed ogre near you is a ragdoll, the off hand takes it by the head; walk
past all 58 of start's: at most 8 ragdolls, 58 still there (the rest "lies as placed again"); gib: a 300 blast gibs 3.
The safety net with the old table: map2b 6 not lying -> 0 after 2 s. Placed corpses rest where placed (0.0 units
moved, all asleep), start's physics step 0.015 ms against 0.012 as scenery. MG1 start and hub: 20 and 1 placed, all
lying. Save and load keep them (and their sleep). ragdoll_test flat, cap, save unchanged; e1m1 smoke clean.

**In the headset:** MG3's start map and map2b: the placed ogres (and shamblers) lie on their blood pools; walk up to
one, take a limb, lift and drag it, cut it, shoot it to gibs. Walking on, the bodies left behind lie still as placed.
Open question: a placed ogre is heavy (200 kg) lifted by the head in the mock (24 units behind the hand after 30 up).

## The physgun's sticks without X/A; a light prop's hold no longer dilutes a lift's carry (2026-10-11, worktree `physsticks`)

### The physgun: both sticks drive the hold (review g2 F2)

His design: while the physgun holds something, both sticks act on it with no button, and the player neither walks nor
turns until it is let go. `vr_toolgun.cpp` (`physgunHolds`, `sticksFrame`, `sticksTaken`, `button`):

- The mapping is the X/A one it had: the gun's stick forward/back pushes it away / pulls it in, left/right turns it
  (about Sticks Turn, `vr_toolgun_turn_axis`, yaw by default); the other stick raises/lowers it and moves it aside. The
  wrist still turns it as before.
- Stick presses during a hold: the gun's (with or without X/A, and even when bullet time owns that stick) and the other
  hand's (its `+speed`, `+reloadmain` or bullet time) are swallowed: a hard push on a stick never changes the tool, drops
  the prop, reloads or starts bullet time.
- Snapping is now only the modifier: X/A held snaps its angles to `vr_toolgun_snap` (15), so stick turns are smooth
  without it. New: letting go of X/A keeps the snapped angles (the snap's correction is baked into the turn, both in
  wrist mode and `vr_toolgun_physgun_wrist 0`) instead of springing back to the unsnapped ones. F2's "wrist 0 also
  snaps" is moot: nothing snaps without X/A.
- Spawn's ghost, Scale and Brush keep X/A held for the sticks (no hold to own them otherwise).
- The Physgun page's text, TOOLGUN.md's buttons and sticks tables, the headers' comments updated.

**Tests** (`Misc/quakevr/toolgun_panel_test.sh physsticks`: PASS): holding a crate, no button, the gun stick (0.8, 1)
and the other (1, 1) for 30 frames: distance 68 -> 95, raised 45 -> 66, turned (yaw 2.4 -> -27.9), unsnapped; the
player's position and yaw unchanged; each stick's press swallowed (still Physgun, still holding); X/A: angles
(-5.7, -27.9, 22.9) -> (0, -30, 30); X/A let go: still (0, -30, 30); the crate let go: the off stick walks 134 units.

**In the headset:** take a crate with the physgun: both sticks move and turn it smoothly and you stay put; push a stick
hard (click): nothing happens; hold X/A while turning: 15 degree steps, let go of X/A: it stays at the step; let go of
the crate: you walk and turn again.

### Climbing: a light prop's hold no longer halves a moving hold's carry (review g2 finding 2)

`vr_climb.cpp` (VR_ClientClimb): the carry (how far the holds moved this frame) was summed over the non-light holds but
divided by every hold, so one hand on a light prop halved what the other hand's moving hold carried. It is now divided
by the holds that carry (none: no carry).

What it changes, measured: **not the lift.** A plat, train or door carries a body hanging from it through
SV_PushMove (VR_ClimbHangsFrom / VR_ClimbCarried; its hold adds nothing to the carry), so one hand on vrclimb's lift and
one on a light crate already rode at the lift's speed (identical before and after: the main hand's sync 0.6, z 25 -> 45
over 4 s). It changes holds moved otherwise: a heavy prop moving (pushed, flung, knocked, riding something) with the
other hand on a light prop. There the body lagged by half the prop's motion until the arm was straight.

**Tests** (`Misc/quakevr/climb/prop_climb_test.sh physsticks`: all 11 PASS): two new cases.
- `movelight`: off hand on a light large crate (`vr_climb_prop_mass 70`), main on a heavy explosive box beside it
  (80.5 kg), the box flung away at 150 u/s, the arms with room to bend (`vr_body_shoulder_reach 0.5`, reset after):
  the main hand's tracked hand drifts at most 0.00 units from its hold while both hold (before the fix: 11.0), the
  body carried 26 units (before: 18, dragged by the arm's reach).
- `liftlight`: the lift's lip in the main hand, a light crate beside the lift in the off hand: sync 0.00, carried up
  (a guard for the above).

**In the headset:** with `vr_climb_prop_mass` 70 (a large crate light), one hand on a large crate, the other on an
explosive box someone shoves (or a blast moves): you go with the box, your arm doesn't straighten first.
## Ragdoll tests, tuning, self-collision (feature branch, 2026-10-12, worktree `ragtools`)

His asks: a testing menu that triggers each active-ragdoll behaviour on the nearest supported enemy; customization
options (strength, chance, time) for them; and "ragdoll limbs can overlap with each other, is that the case?" (yes: a
ragdoll's parts were one negative collision group, `filter.groupIndex = -num`).

### Debug > Tests > Ragdoll Behaviours (`vr_ragtest.cpp`, `vr_menu_ragtests.inc`)

- One table (`actions` in `vr_ragtest.cpp`) is the menu's rows and the console's `vr_ragdoll_act <name> [entity]` (no
  name: the list; `status`: fired, began, missed). Each row: its settings that must be on, a Fire (how it is set going)
  and a Began (what of `ragactive::State` says it is under way), and a time to begin by. The console says
  `ragact: <name> on <class> (<num>): began after N s: <what>` or `NOT begun in N s (<why>)`. A new behaviour (ragmove's
  backpedal, a pull-up, a hop) is one row plus its two small functions; the menu and the test follow.
- Target: the nearest grunt, enforcer, knight, death knight, ogre or zombie standing (box3d::activeKind, not a ragdoll
  now; the zombie only for what it can do: it dies only beheaded and never crawls). QC's test aids take it alone through
  the new QC global `vr_test_only` (`VR_TestOnly`, `VR_TestSkips`: vr_test_kill, vr_test_hit, vr_test_cut, which then
  reaches past 400 units, and VR_Knockdown_Test); `vr_foegrab_throw_test` takes an entity as its third argument.
- Fired in the server's frame (`ragtest::frame` from VR_PhysicsFrameEnd), not the console command's: a QC blow's
  messages (the flinch, the blood) sent from a command were cleared before they reached the client.
- Rows: deaths (body, head: collapse, arm, leg, holding the wound, stumbling on, crawling away), lost limbs (arm,
  forearm, leg: one-leg balance, shin, both legs: crawl on the arms, beheaded), liquids (paddle, lava and slime flail),
  walls and ledges (lean and slide, grab a ledge and hang, catch the fall), living monsters (flinch: body, head, arm;
  knee buckle and limp; stagger and recover; stagger and fall; the two-hand throw; knocked down then get up).
- Forcing, for one death only (`ragtest::Force`, read by vr_box3d.cpp's startActiveDeath, deathMotion and driveActive):
  the wound's or the crawl's chance made sure (the cvar's value swapped round `beginDeath`), no pelvis pin, a stumble's
  walk (1.5 x Stumble From, at least 160 u/s, no kill push), a monster moved by the test standing still (no stumble
  from the jump), a liquid felt everywhere when he stands in none (on dry land: his strokes, no buoyancy).
- Places: the lean moves him to the nearest wall within 384 units (16 rays from his chest, room for his box, his back
  to it, 20 units off); the ledge to the nearest lip within 512 (a drop of 96 or more, his way clear, his back to it,
  no kill push: env_test.sh's scene); he is killed 0.15 s after (his place drawn first). The crawl and the catch turn
  him away and shoot him in the back (he lands face down; the catch harder, Killing Blows 2 and no pin). Other
  settings a row needs are set round its one call and put back (Knockdowns' Chance 0.01 or 100, Killing Blows).
- The page also has: a grunt, enforcer, knight, death knight, ogre or zombie ahead, Notarget On/Off, Clean Slate
  (every monster removed, `vr_scene_clean all`), Draw Their Targets, Log Each Behaviour, How Many Began, Active
  Ragdolls Now, and a link to the settings. With the menu open the game runs only while nothing hunts you
  (`VR_MenuRunsGame`): a test begins as the menu closes otherwise.
- `vr_menu_press <page title> > <row label>`: a menu button pressed from the console (the headless test's).

### Tuning (Gibs and Corpses > Ragdolls)

Limbs Meet Each Other (below), Test Them (the page above), and sliders for the cvars that had none: Muscle Stiffness
(`vr_ragdoll_active_hertz` 2-20 Hz), Muscle Damping (`_damping`), Muscle Torque (`_torque`), Thrown by His Death and
Thrown For (`_pin`, `_pin_time`), Headshot Strength (`_headshot`), Stumble From (`_stumble`), Crawl Time and Crawl Pull
(`_crawl_time`, `_crawl_force`), Writhe Size (new `vr_ragdoll_active_writhe` 1: times the writhe's swing), Arms'
Strength (new `vr_ragdoll_active_env_arms` 1: times the reach strength of a catch, lean, ledge hold or brace), Shove's
Push, Stagger Travel, Stagger Balance, Stagger Recovery Blend (`_stagger_push`, `_travel`, `_pin`, `_blend`), Get-Up Lift,
Pace and Blend (`_getup_pin`, `_pace`, `_blend`). The two new cvars are one multiplication each in vr_ragactive.cpp (the
env hands' `apply`, the writhe's amplitude); nothing else there changed.

### Self-collision (`vr_ragdoll_self_collide` 1)

- Its parts no longer share a group: catCorpse in their mask, `shouldCollide` deciding (`selfMeets`): jointed
  neighbours never (Box3D's collideConnected), a loose piece (his gun) never, and parts whose cores (70% of each
  shape) overlap not yet: asked again as their boxes move, they meet once out of each other's core; a shallow overlap
  is pushed out by the contact. Other ragdolls meet them by `vr_ragdoll_collide_each`, decided there too.
- Each step (`preSolve`'s `selfTouches`; the shapes ask for it only with self-collision on): no contact for an arm its
  drive reaches with (a hand to a wound or a stump, a catch, a lean, a ledge's hold) nor for parts deep in each other.
  Without it the hand to the stump was held 39 units off (ogre; 9 before) and a hand let go inside his side was pushed
  out at once (4400 J at 2 s, an ogre's death).
- `vr_ragdoll_list` prints `self: N of its M unjointed part pairs overlapping`.

Measured (`Misc/quakevr/ragdoll/selfcollide_test.sh`; off -> on):
- overlap: 8 monsters (grunts, enforcers, knights, death knight, ogre) killed by body, head, arm and leg shots, at rest
  after 8 s: 49 -> 10 overlapping pairs in all (another run: 42 -> 17 before the preSolve).
- cost (8 grunts killed at once, --exclusive): the step 0.405 -> 0.468 ms while they fall, 0.200 -> 0.278 ms while they
  settle (pairs 0.016 -> 0.020); about 0.01 ms a ragdoll.
- pile (4 grunts on one spot, e1m1): every part asleep at 10 s both ways, the same heights.
- active_test (deaths, head, crawl; 5 monsters), dismember_test, env_test, stagger_test with `EXTRA="vr_ragdoll_self_collide
  0/1"` (new: EXTRA in the first three): no error or NaN; energy and spin once let go 0 in all but env_test's knight wall
  (65 J once; its seeds 0-3 off and on both slide on to 4 s); hand to stump as before (ogre arm 9, forearm 6). stagger_test's
  ogre steady/fall and the death knight's and ogre's thrown-and-landed FAIL both ways (as on next); its `taken` case
  missed its moment once with it on (the knockdown lay 0.8 s longer). A knight's get-up pelvis error 15 units (3 off).

### The headless test (`Misc/quakevr/ragdoll/ragtest_test.sh <agent>`, MON=n)

Reads the table, presses every button (vr_menu_press) on a fresh monster on vrtesthall, one game for all, and prints
PASS/FAIL per button. Grunt 27/27, knight 27/27, enforcer 26-27/27 and death knight 26-27/27 (the ledge: a backward
death falls past the lip about half the time), ogre 26/27 (two-hand stagger: the ogre recovers), zombie 13/13 (14 not
its). The catch is pressed three times (a grunt's second death falls back whatever: no catch; the help says press again).

**In the headset:** Debug > Tests > Ragdoll Behaviours: A Grunt Ahead, Notarget On, then each button; the console's
`ragact:` line says whether it began. Gibs and Corpses > Ragdolls: Limbs Meet Each Other on and off on a pile, a body
folded over a crate, a wound held; the new sliders. Known (ragmove's): the death knight's arms reach a lip but miss it
at speed; a crawling ragdoll's env log repeats "lets go (curl, crawling)" every frame.
## Arcane Dimensions P1b: AD's maps play (2026-10-11, worktree `arcane`, branch `feature/arcane`)

All of it is in ARCANE_DIMENSIONS.md: Vittorio's decisions in section 12, what was built and measured in section 15
(campaign 6 by the developer launch, AD's 38 maps with 0 missing spawn functions and no errors headless, AD's projectile
pellets with VR settings, a first version of AD's three weapons, saves switching AD's rules), the follow-up tasks in
section 11. In passing: the server's broadcast boundaries counted Quake VR's beam temp entities one byte short
(`vr_server.cpp tempEntityLength`: their beam id byte), fixed.

**In the headset:** Debug > Tests > Arcane Dimensions: Play (Developer); the hub, a map through to its exit and back;
VR Settings > Combat > Arcane Dimensions.

## Arcane Dimensions: screen tints, bugs 4 and 5, resist tells everywhere (2026-10-11, worktree `arcane`)

All of it is in ARCANE_DIMENSIONS.md 16.14 (his decisions 32-38 in section 12): AD's `v_cshift` tints (debuffs, books,
the cinematic camera's fades) drawn by the engine from two stats, eased, with a Screen Tints setting; AD's bugs 4 and 5
fixed; AD's resist tells wherever resistance applies (precached on demand outside AD maps); the upgrades asked through
one per-weapon accessor (per weapon itself: BACKLOG.md).

**In the headset** (an AD map):
- [ ] Debug > Tests > Arcane Dimensions: Burning, Poison, Bleeding: a soft green, then amber tint easing in and out,
  not uncomfortable; VR Settings > Combat > Arcane Dimensions: Screen Tints at 0.5 and 0.
- [ ] A book (a lectern's `misc_textbook`): the world darkens behind its text, and back when you leave it.
- [ ] Arcane Dimensions: Play a Cinematic Camera (ad_tears): a fade to black hides the jump, then fades back.

## Arcane Dimensions P2: AD's monsters get Quake VR's treatment (2026-10-11, worktree `admon`, branch `feature/ad-monsters`)

All of it is in ARCANE_DIMENSIONS.md section 17: AD's damage and death code now calls Quake VR's hooks (beheading,
limbs, knockdowns, corpses, gore, highlights; five importer patches), and AD's monster models have ragdoll rigs, head
zones and get-ups in usage order (31 seed tables over 41 models: every AD monster placed in AD's maps but the ones that
burst or shrink away at death, the eidolon and the fishing ogres). Headless, per monster: knocked down and up, killed
down, beheaded, limbs cut and popped, a corpse gibbed (`Misc/quakevr/ad_monsters_test.sh`); AD's maps fought through without
errors; e1m1, hip1m1, r1m1, MG1 and MG3 unchanged.

**In the headset** (an AD map; Debug > Tests > Arcane Dimensions Monsters spawns each one ahead):
- [ ] A death knight: behead it with a sword, cut an arm off, knock it down with a shove (it gets up, its death
  backwards), parry its sword.
- [ ] A zombie knight: shoved down, it gets up as AD's zombies do; beheaded, it dies for good (lying).
- [ ] The ragdolls of the crossbow knight, the hunter ogres (cleaver, mace, hammer: the weapon stays in the hand), the
  spider (its legs), the death guard, the minotaur, the death fury (two swords), the golem and the death sergeant (the
  flail dangles).
- [ ] The poorer fits (the gaunt, the seeker, Freddie): their dead poses look whole.
- [ ] A corpse of each, gibbed by shots: AD's own head and weapon fly.


## AD pellet hits and ammo pickup models (2026-10-12)

Voice notes: projectile shotgun, super shotgun and dropped burst rifle often needed about eight shots on a grunt;
a held shotgun briefly looked like a nailgun until firing.

The pellets and their tracer were consuming one another at the muzzle. Both the movement trace and the generic
solid-overlap touch path could invoke AD's bullet touch on a sibling pellet. AD's own touch then hides/removes it.
The isolated engine helper VR_AD_PelletsPass now excludes only pairs of AD proj_bullet entities, including AD's
small-projectile FLY mode, in both paths. Monsters, world, props and other projectiles still block these pellets.
AD's original damage, pellet counts, shotgun spread and speeds remain; the dropped burst rifle retains its own
0.006 spread instead of receiving the shotgun's 0.04. AD's Sharp Shooter spread now also applies to projectiles.
No cvars/defaults changed.

AD ammo/backpack refresh called AD's W_SetCurrentAmmo, where our shotgun id 4 means the vanilla nailgun item bit.
A reviewed importer hook delegates player ammo/model refresh to both VR hands through W_SetCurrentAmmoFor.
The imported source and AD_IMPORT.txt were regenerated from the locally installed pinned AD paks.

Debug > Tests > Arcane Dimensions exposes Shotgun Hit Test (40), Super Shotgun Hit Test (41), Burst Rifle Hit Test
(42), and Ammo Model Test (44), through vr_ad_test. The flight tests place a frozen, high-health AD grunt straight
ahead and report damage after 0.8 seconds, then remove the test target. Run on open ground; scratch1 is distance
(default 128). Their source/target trace reports make an obstructed test visible.

Verification: Release build and QC/FGD/statics/precedence checks passed; QC had 0 warnings. Targeted headless checks
on vrfiringrange, precise hits on: aligned pellets (Pellet Spread 0) dealt exactly 24 SG, 56 SSG and 6 per burst-rifle
round. Before the overlap fix, the same aligned blasts delivered only 4 damage; touch logs proved sibling impacts.
At original AD spread, this run delivered SG/SSG 24/32 at 128 units and 20/8 at 256; rifle round 6 at both ranges.
These are samples of AD's deliberately wide scatter, not a damage buff. Ammo refresh preserved main shotgun id/model
4/progs/v_shot.mdl and off-hand SSG 5/progs/v_shot2.mdl. e1m1 and active AD ad_e1m1 both ran regular player firing
without errors; AD smoke reported 934 AD spawns and 60 shared VR spawns. eval.sh skipped the archived melee takes.

VR follow-up: compare SG/SSG and burst-rifle kills at short and medium range in AD; walk through ammo/backpack
pickups while holding different guns in each hand and verify their models stay correct. AD's wide SSG scatter is
retained for the author's assessment with the existing Pellet Spread slider.
## Crate ledges, torso-safe carry and load-dependent walking (2026-10-12, worktree vrgrabs)

The large crate's walkable bevel supplied a plane which was extended inward across the flat top. Its apparent top
rose with arm extension: a hand 5 units over the lip was rejected through the crate, and farther reaches found no
hold. Prop ledges now use the most upright connected walkable face for their top plane, retaining the outer lip.
World/brush ledges keep their existing rules. The change also applies to tilted props in their current body frame.

Solid carried props (crates, barrels, explosive boxes, carry_rigid == 2) now sweep their drawn oriented box against
the carrier's torso, using the physical body's width. A continuous 15-axis sweep stops even a fast hand moving all
the way through the body. Initial overlap from body movement or rotation separates on its current side; the level
collision solver checks that separation. A prop trapped against the level, or pulled more than 32 units beyond its
torso stop, releases through the existing stuck-prop path. Both server placement and local drawing use the constraint;
one- and two-handed holds are covered. Portal-held props and the existing teleport grace retain their mapped poses.
This constrains solid props only; ordinary pickups, weapons and wearable armour keep their interactions.

Walking/swimming commands and the maximum wish speed now include the solid carried load. Masses and the longest
model dimensions add across distinct props; the same prop in both hands counts once. The speed fraction is
max(minimum, 1 / (1 + max(mass / mass_scale, max(0, size / size_scale - 1)))). Room-scale movement, throws, melee,
knockback and jumping impulses keep their existing rules. Archived settings: vr_carry_body_collision 1,
vr_carry_move_mass 100 kg, vr_carry_move_size 1 metre, vr_carry_move_min 0.35. Zero mass/size scales disable that
load term; minimum 1 disables slowdown. A 65 kg large crate gives 0.606 speed (193.939 units/s with full stick,
versus 320 empty-handed), including both-hand deduplication.

Regression: python Misc/quakevr/climb/prop_grab_test.py <agent> covers four directions and four top extensions,
reach limits, real full-stick movement, disabling slowdown, physical/drawn torso overlap and releasing a long pull.
vr_carry_load_test additionally checks continuous rotated/diagonal pulls, initial overlap and overhead clearance,
then reports current carried load, actual speed and torso overlap. The existing prop mantle tests cover the large
crate, large stack and sideways crate. Human VR follow-up: grip each face with different reaches, bring a crate or
explosive box towards the torso with one/both hands, and tune the carry slowdown/body stop feel if needed.

Verification results: 16/16 exact top grips across four faces at 0, 4, 10 and 14 units over the lip; two unreachable
positions rejected. Full-stick speed: empty 320.000, 65 kg crate 193.939, slowdown disabled 320.000 units/s.
An actual native explosive box held in both hands gave 157.480 units/s (0.492 fraction), counted once, with physical
and locally drawn torso overlap both 0.0000 units. A long pull released at the torso stop; the continuous rotated
fast-pull, initial-overlap and overhead-clearance tests passed. Existing mantle tests passed at z 72 (large), 120
(stack), 64 (sideways). Build and e1m1 smoke passed; eval.sh skipped because the author's takes are archived.
The regression script optionally takes individual case names after the agent name for targeted reruns.
## Active ragdolls: drowning, ledge pull-ups, grabbing others, guarding the head (2026-10-12, worktree `ragmove2`)

His answers to ragmove's "not done": drowning "Yes"; head protection "Maybe it would make more sense during melee fights
rather than gunfights"; catching on others "Yes"; ledge pull-up "If possible, yes"; recovery pain frames no. Each a
chance cvar at the end of the active ragdoll block, a Ragdoll Settings row (after Active Get-Ups) and a Debug > Tests >
Ragdoll Behaviours row (`vr_ragtest.cpp`'s table: `drown`, `pullup`, `grab`, `guard`).

### Drowning struggle (`vr_ragdoll_active_drown` 1; Drowning Struggle)

- QC `VR_Knockdown_Drown` (vr_knockdown.qc), from vr_liquids.qc's first drowning tick (12 s of breath gone, head under):
  a grunt, enforcer, knight, death knight, ogre or zombie standing (Knockdowns on, its kind driven alive:
  `ragdolldrown(e, 0)`, Swim and Flail on) is knocked down instead of that tick's damage. The engine's living swim
  takes it (ragmove's `livingSwim`), frantic at first (`ragdolldrown(e, 1)`: `Swim::panic`, the strokes 1.9 times as
  quick and 1.5 times as hard, easing to its usual over 3 s), its head up, towards where it can stand; it gets up there
  as any knockdown in water.
- vrtesthall's pool, a grunt on the bottom: knocked down at 12 s, paddles from (337 -256) to (298 -193), head under 34
  of 58 traced frames, one drowning tick taken while still under, up after 2.4 s at z -24 (its head out: no drowning
  for 37 s more).
- Drawbacks: it stops fighting while it struggles; it can still take a tick or two while its head is under; with no
  place to stand within reach it gets up floating (vr_knockdown_water_giveup), sinks and, 12 s later, struggles again
  (a cycle, each a knockdown); each struggle takes a ragdoll slot (vr_ragdoll_max may remove the oldest dead body);
  every such monster on a map flooded on purpose would struggle.

### Ledge pull-ups (`vr_ragdoll_active_pullup` 1; Pull Up From Ledges)

- A living knockdown falling past a lip near a hand (a fall's: drive()'s `livingLedge`; a body not driven, dropped or
  toppled off it: vr_box3d.cpp `livingClimb` begins a drive for it, every third frame while it falls) grabs it with
  ragmove's `findLedge` (the env's Ledge holds, a living grip twice a dying one's). `Climb` (vr_ragactive.cpp,
  "Climbs"): it hangs 0.45-0.9 s turning to face the wall, its standing frame held (straightened out), then by the chance
  pulls itself up: its pelvis pinned (3 body weights, 8 Hz, a weak turn: held by a hand off its middle a strong one
  wrenched it) along a way that waits for it (a quarter as fast while its pelvis lags 0.3 of an arm), up under the lip
  (0.9 s), over it (0.5 s, pitched forwards 0.6 rad, its hands holding), in onto the top (0.6 s, let go, pitched 1.5
  rad), its muscles holding 0.9 of its weight, its arms giving (strength 0.3: stiff, they held it down), its legs reaching
  up. Lying on the top it is a knockdown again; its get-up waits while it climbs (`ragdollGetUp` returns 0).
- Not pulling, or not hanging (its legs still on the top: it lets go; on the top already: up), struck off, too weak
  (its way 1.5 s late), stuck at the lip: it drops (a fall, its arms bracing). Killed: released as a stagger's.
- A shove over a ledge (ragdollshovetopple's ledge launch, 300 u/s) flies past the lip: no grab. A body toppled off
  gently or dropped from a hand at the lip does (vrtesthall's west lip: `climb_test.sh`, vr_knockdown_push 0.15).
- `climb_test.sh pull` (N=2, six classes) before the merge: grunt 2/2, knight 2/2, ogre 1-2/2, zombie 2/2 (it caught
  itself still on the top), enforcer 1/2 (the other not hanging), death knight 0-1/2; with self-collision see "Also".
  drop, hit, off 2/2 each. Ragmove's death knight missing the lip at speed (ragtools' note): its shoulders fall
  31-34 units off the lip, out of its 27-unit reach; a wider reach (1.4) only had it reach in vain, left at 1.15. A lip
  passed at more than 3 m/s is now grabbed from half an arm off (its hold pulls the hand in).

### Grabbing others falling (`vr_ragdoll_active_grab` 0.7; Grab Others Falling)

- Falling (a living fall: drive()'s `fallEnv`, the first second; a death: envFrame's sensing, the first 1.2 s), another
  monster near a hand (its box, catActor; or its ragdoll's or corpse's parts, catCorpse, lower too: it falls onto them),
  above its waist, within 1.1 arms: the hand reaches for its nearest point to the hand (not the shoulder's: unreachable)
  and holds once within 0.4 of an arm (0.55 falling alive) by a capped motor joint (1 body weight; a heavy one less)
  where the hand is, 0.5-1.1 s; let go on its time, its load, landing, a hit, pulled 0.8 of an arm off, the other gone.
  A ragdoll held is dragged along. Once a drive, by the chance (rolled once).
- `grab_test.sh` (vrtesthall, A 110 units ahead, B behind): death 5/5 reached, about 2 in 3 held 0.6-1 s; B's ragdoll
  lying (rag) 5/5 reached, 3 held; a beaten shove into B (shove) reaches, held 0.2-0.4 s in 3 of 5 (a fast fall lands
  first); off 5/5 none.

### Guarding the head in melee (`vr_ragdoll_active_guard` 0.7; Guard the Head)

- QC `VR_Guard_Frame` (vr_hitreact.qc, StartFrame): each player's hand moving faster than vr_melee_speed with
  something to strike with (a weapon, a fist, a gun's bash: VR_Melee_Thing), its way over the next 0.2 s passing within
  22 units of a living rigged monster's head and coming at it: once a swing (0.6 s), by the chance, it guards for 0.6 s.
  Shots never. Looks only: the blow lands where it lands.
- Standing: a hit reaction (hitreact's part 9, its hold in hundredths) the client poses afresh each frame
  (vr_hitreact.cpp `guardTurns`: each arm by two-bone IK, its hand before the face, towards the blow and to its side,
  the elbow down and out; in over 0.12 s, out over 0.25 s); with Hit Reactions on Limbs.
- A ragdoll (staggering, falling, down, rising; `ragdollguard`): `Guard` (vr_ragactive.cpp): its arms' reaches up before
  its face over what leads them; lying undriven, a drive begun for its arms alone.
- `guard_test.sh`: a mock swing at a grunt and a knight 45 units off: 2 guards each, arms turned 92 and 155 deg at most;
  head shots: none; a stagger: guarded, recovers; knocked down: guarded, gets up; off: none. `vr_test_guard 1` (Debug >
  Tests: Nearest Monster Guards Its Head).

### Also

- Self-collision (ragtools' `vr_ragdoll_self_collide` 1) no longer touches while a living body staggers or rises
  (`selfTouches`): it follows its animation's frames, whose limbs pass through each other (ragtools' knight get-up 15
  units off its pose, 3 without: now 1.4; the next's `stagger_test` grunt steady FAIL: 1 of 40 steady grunt shoves
  tripped over, 77 deg at 0.6 s, with it touching; 0 of 40 now, the full steady case 0 FAIL). Nor an arm whose hand only
  wants a hold yet (stuck on the head, it missed a lip falling past).
- Climbs with self-collision: it topples off faster (3.5-4 m/s, 1.8 without), so a lip passed is grabbed from a quarter
  of an arm per m/s off (at most one: its hold pulls the hand in), a hand that missed reaches again (twice), its chest is
  led off the wall and in over the top ahead of its pelvis (`climbChest`: it had folded at the lip's corner and tipped
  back), and it lets go only with its chest over the lip. `climb_test.sh pull` N=2 now: grunt, knight, zombie 2/2,
  death knight and enforcer 1/2, ogre 0/2 (it falls past too fast: "lost its hold"); about 8 of 12.
- A crawler no longer logs "lets go (curl, crawling)" every frame (its curl not set again once done).
- The ragtools merge (origin/next ae437c66c) taken; vr_ragtest.cpp's living swims feel a test's forced liquid
  (`liquidFor`); `ragtest_test.sh` places `pullup` at the ledge and `grab` with a second monster.

### Checked

Headless: `liquid_active_test.sh drownkd drown` PASS / as before; `climb_test.sh` (above); `grab_test.sh` (above);
`guard_test.sh` 8/8 PASS; `ragtest_test.sh` 31/31 (one long run: crawl missed once, 3/3 alone); `env_test.sh ledge wall`
grunt and knight as before; `stagger_test.sh steady` (all six, N 8) 0 FAIL, `steady getup` (MONS 0 5, N 8) 0 FAIL;
`liquid_active_test.sh kdswim drownkd` PASS. Build 0 warnings.

**In the headset**: hold a grunt's head under water (or let one walk into a deep pool) 12 s: it should thrash to the
surface and paddle to the edge. Knock a grunt down at a ledge's edge (or drop a knocked-down one off it from your hand):
it should grab the lip, hang, and pull itself up onto the top. Shoot two grunts standing close: the falling one should
clutch the other a moment. Swing your sword or fist at a grunt's head: it should throw its arms up before its face.
Ragdoll Settings: Drowning Struggle, Pull Up From Ledges, Grab Others Falling, Guard the Head; Debug > Tests > Ragdoll
Behaviours: the four new rows.

## Idle wrap, authored arms and body blood (animbody)

- The grunt's native frames have empty names; named appended shove frames incorrectly made all its frames trust
  file order. Ease 0 predicted death frame 8 at idle frame 7 instead of the learned wrap to 0. Successor reliability
  is now per frame; seed-table death endpoints prevent predicting a death from the preceding idle frame. Looking
  up those endpoints no longer builds a rig with Smooth Limbs off. Ironwail's animation toggle preserves mode 3.
  Normal `r_lerpmodels 1` stays the default pending headset testing.
- All sixteen skins retain the author's repaint (24fa2de6d). `refine_body_skins.py` derives a 512x2048 atlas from
  those originals: skin/bracer colour interpolated with small fine grain, including existing blood; other islands
  retain their exact RGB texels. Bracer/boot/shaft islands repacked; normals rebaked. Bare arms have 24 sides and
  a 0.3 mm wrist overlap. Gadget ring straps also fit bare arms. Regeneration uses `--keep-edited`, followed by
  refinement from the original 256-square sources and `bake_normals.py body`; refined sources reject refinement.
- New `vr_wounds_body_res 1024` / Wounds > Body Wounds' Detail is a body-only minimum. It preserves the author's
  `vr_wounds_own_res 0` and the hands' resolution; 0 opts out. No saved-setting migration. Shared fine-array
  allocation follows the larger setting; fine and pooled own models now reserve separate foreign-blood slots.
  Changing routing rebuilds masks even when the shared array size stays the same.
- Measured texel density: hand diffuse ~1400/m; bare wrist ~1333/m around, 1259/m along; bracer ~1043/1775/m.
  Live blood: chunky hand ~700/m; body at 1024 ~667/629/m (effective mask 256x1024; opt-out 64x256).
  Six before/after eye captures cover clean/authored blood/live blood, bracers both ways; seams 0 mm in the fixed
  bent-wrist pose, two gadget straps visible with bare arms. Composite: `scratch/animbody_compare_visual.png`.
- Cost: `imagelist vrbody` reports 16 -> 96 MiB (+80 MiB, diffuse skins plus mipmaps; normal-map cost unchanged).
  Body detail adds the shared 20 MiB fine+foreign array at 1024. Painting stays on GPU, with no CPU texture upload. Exclusive mock comparison,
  100 wound events/899 measured frames: total GPU 0.84 ms opt-out / 0.83 ms at 1024; eyes 0.75/0.74 ms. The wounds
  stage was below the profiler's 0.02 ms display threshold for both CPU and GPU. Thirteen CPU paint samples each:
  median 0.022 ms both; maximum 0.103/0.090 ms. These are mock measurements, not headset frame-time guarantees.
- Idle trace (600 requested/598 measured): single-vertex acceleration p95/max 0.279/1.272 -> 0.036/0.058 units;
  mean p95/max 0.116/0.541 -> 0.011/0.021. Exact keys pass; 0.001 off key <=0.045 units; 570 splines swung.
  Disabled-rig info reports no rig and one learned wrap. Three pinned idle segments inspected visually.
  Calibration menu paths: 4 found, 0 missing. Targeted mask tests cover 0/1024/512 transitions and independent
  foreign-blood slots. Build and e1m1/e1m2 smoke pass. Reproducers: `animsmooth_test.sh`, `body_detail_test.py`.

In VR: check Ease 0 grunt idle gun over several loops; bare/bracer wrist bends, gadget straps, authored scars and
live blood beside the hands. Confirm the memory/detail tradeoff before changing any wider defaults.

### Bake copies, firing range recovery, and AD spawn menus (toolbake)

Bake Copy now exports a new playable map (default map_baked1, map_baked2, etc.; optional explicit new name) and refuses
existing map names. It preserves the map's effective loaded entities, including external CRC-qualified overrides,
and appends the current brushes as permanent solid func_wall models. No source scene reload or map override is
written. Augmentation failure aborts the bake. The no-light bake writer has the same source entity/name protections.

The reported firing range loss came from its external vrfiringrange@3647.ent: 188 entities versus 106 embedded in
the original BSP. The old bake changed the entity CRC and lost that override. The original BSP remains intact;
the damaged relit override contains intact geometry and three additional brush models. A recovery copy in the
worker's scratch embeds all 188 entities plus three solid brush entities, preserving every other lump and its BSPX
lighting byte for byte. RECOVERY.txt beside it gives exact backup/placement steps; no author files were changed.
Recovery restores those three brushes as permanent geometry; an editable runtime descriptor was not present.

The toolgun grid, Debug monster page, Debug weapon pickups, and Debug Thing selector now share 56 AD choices:
44 monsters, nine pickups, and three upgrades. Monsters and standalone items spawn outside AD maps with data
mounted and AD Everywhere enabled; upgrades keep their existing AD-map requirement. Unavailable entries remain
visible and cannot be selected or activated. The generator owns the catalogs and dispenser dispatch. Explicit AD
class variants retain canonical entity names, including names that overlap official expansion monsters.

Validation: source SHA256 f43f59da673a44dc34502c9d2c7daf9d30d088a593ed53afb8264f27d2e065b3 unchanged; no-light
and real ericw-tools copies each retained 188 original entities plus one brush, all original geometry/model data,
and collision at exactly z=81 (trace fraction .607). The recovery copy has 191 entities and 12 models; its top brush
collides at z=149.2. All 44 AD monster classes spawned outside AD rules; a death knight completed initialization,
registered/rigged/alive, through both physics and debug dispensers. Outside AD: 53 ready/3 disabled; in an AD-marked
fixture: 56 ready/0 disabled; AD Everywhere off: 0 ready/56 disabled and selection rejected. e1m1 smoke and calibration
menu path check passed (4 paths, 0 missing). Archived melee evaluation skipped as expected. Menu rows also expose
vrgrabs' carry/body load settings and ragnotes' four corpse-limb regression actions; those workers own their logic.


## Voice-note fixes: vore reach, corpse self-collision and blunt limb damage (2026-10-12, worktree `ragnotes`)

The vore stood beside the player without attempting its shove; the photographed grunt's arm went through its torso;
blunt gun/hammer/brick blows on corpses should pop the piece struck, while axes still cut. No new cvars/defaults.

- `QC/vr_enemyshove.qc`: a vore's reach includes its drawn horizontal bounds (`drawnbounds`, including model scale/
  offsets), rather than only its Quake walking hull. The existing Too Close setting remains the gap beyond a grunt's
  side. Default vore reach is now 90 units (formerly 68). Targeted spawn distances 64/100: shoves at 54/76 units;
  distances 160/240: none during the observation. The same reach is used when the blow lands, with its existing margin.
- `Quake/vr/vr_box3d.cpp`: a corpse's deep contacts and arms reaching to wounds now stay physical. Their former
  collision bypass let arms pass through and settle inside the torso. Preserve ragmove2's animation/pin exceptions
  for stagger, rise and climbing, and its reaching-arm/deep-contact safeguard on living knockdowns. Parent-child
  joint neighbours and loose equipment remain excluded. `vr_ragdoll_list` reports 70%-core overlaps as `deep` as well
  as ordinary surface contacts: these distinguish penetration from small contact slop in the fitted hulls.
- `QC/combat.qc`, `QC/vr_limbs.qc`: after the existing immediate chance-based pops, blunt/fist/prop/bash corpse hits
  accumulate damage separately at the struck limb joint (and separately at the head). Reaching that monster's original
  corpse-health threshold pops that piece; hits on the torso take the whole-body corpse health (enough gibs it). Blades, chainsaws,
  shots and blasts retain their existing paths. With Limb Gore/Corpses disabled, the previous whole-body path remains.
  Per-piece damage uses the existing corpse damage multipliers and persists with ordinary QC entity fields. Untagged
  flung props use their own position/velocity, matching the existing living thrown-limb hook; held blows use their tagged
  contact point.
- `vr_limb_test` 24: two blunt 48-damage blows at one forearm (grunt threshold 80), 25: an 800-damage pelvis-surface
  blow, 26: two physical-prop blows at that forearm, 27: an 8 m/s axe corpse slash through the real cut classifier.
  24/26/27 leave corpse health 80 and `gib 0` (24/26 pop one piece, 27 cuts one); 25 gibs the corpse (review fix). Debug > Gore
  Tests > Limb Gore Tests has the buttons (toolbake). `Misc/quakevr/ragdoll/notes_regression.sh <agent>` reproduces
  the reach, corpse, overlap and living-transition checks.

**Validation:** built QC/engine with zero QC warnings and passing FGD/statics/precedence checks on `next` including
ragmove2 (`83f9f04ed`) and toolbake (`d308fad65`). Eight mixed dead monsters: ordinary overlapping pairs 27 off / 11
on; deep pairs 8 off / 0 on. This checks penetration, rather than requiring fitted hull contacts to be exactly zero.
Grunt arm/forearm stump reaches settled at 0 J and 0 rad/s. Living behaviour buttons: stagger, get-up, pull-up and
head guard on grunt plus knight get-up all began (5/5). Numeric grunt/knight get-ups handed back in all 4 active/on-off
controls; active pelvis gaps 1.9/4.3 units in the final living-preservation revision. Repeated steady/overwhelmed staggers:
initial 3-trial runs recovered 6/6 and fell 6/6; final 2-trial runs recovered 4/4 and fell 4/4, zero errors. Corpse Limb Gore disabled: original whole-body gib at 80 damage retained.
e1m1 smoke exited 0; `eval.sh` correctly skipped the author's archived takes. Headset checks: shove at the vore's
visible side, force a corpse's forearm against/along its chest with a hand, repeatedly hit an individual limb with a
gun butt/hammer/brick, and compare axe cuts; also watch living get-up and ledge pull-up for contact-induced stalls.

**Ordinary living AI/combat: feasibility audit only.** The necessary rig skinning, frame clips, capped joint drives,
pelvis pin, feet/step logic and physical hand reaches already exist. Living staggers, falls, get-ups, swims and ledge
pull-ups use them; ordinary living hits still use the inexpensive animated skeletal reaction overlay. Keeping a
living ragdoll permanently driven is feasible, but enabling today's knocked-down state everywhere is not sufficient:
that state suspends normal QC movement/attacks, and `writeRagdoll` writes physical pose/origin back to the entity.
Normal AI navigation (`walkmove`/`movetogoal`), animation-root motion, physical pelvis/feet movement and hand-based
weapon/attack origins need one consistent owner. Otherwise the AI and physics fight over position and attack timing.

A conservative future stage can retain QC navigation/attack timing and its collision hull, pin the physics pelvis to
that navigation target, follow each live animation clip with strong muscles, and allow bounded physical limb/contact
reactions. Then deliberately reconcile feet, ledges and attacks before introducing fully physical locomotion. Preserve
existing knockdown/get-up transitions and verify grunt/enforcer firing, knight melee, stairs/ledges, held limbs, crowds,
save/load and actual headset animation quality. Current full-body drivers cover a limited class list and permanent
awake bodies add physics cost: crowd cost must be measured for this stage; no benchmark estimate is claimed here.
No broad locomotion redesign or always-on living mode was introduced by this fix.

## Dedicated melee recording room (2026-10-12, worktree `meleeroom`)

`map vrmelee-recording` is an authored, empty 1280 x 1280 x 384 unit room: one central training dummy and seven
floor pickups along the south wall (axe, Mjolnir, knight/hell knight swords, chainsaw, crowbar, Super Axe). Fists need
no pickup. The practice area has no furniture, props, extra enemies or map buttons. Neutral studio lighting keeps
the dummy and pickups visible. `make_vrmelee_recording_map.py --compile` builds the source/BSP/LIT/LUX with the
installed ericw tools, using the project's own WAD; compile intermediates stay in the worker's scratch folder.

Motion Recorder and Play both offer Melee Recording Room; Firing Range stays available as the sandbox. Direct mock
replay automatically loads the recording room; `vr_motion_play ... map <name>` retains explicit alternate fixtures.
Evaluation and Review Takes' background evaluation default to the room. Synthetic take headers and the crowbar,
empty-melee and melee-phasing test scripts use it too. Existing recordings are never rewritten: target-relative
placement, hand settings, legacy fallback rules and melee behavior remain unchanged. No new cvars or melee tuning.

Validation: QC/engine/FGD build passed; qbsp/vis/light all exit 0 without warnings; authored BSP contains exactly one
dummy and seven pickups. Two 1200-unit player-width sweeps are completely clear; floor trace fraction 0.500. The
targeted `melee_room_test.py meleeroom` records 158 frames, replays directly from e1m1 in the room, evaluates both the
new take and an otherwise identical old-map-header take (2 PASS), preserves their bytes/events and reproduces hand
poses within 0.01 units (measured 0.001). Existing melee phasing: 7/7; empty melee: 2/2 (axe 0 clicks, shotgun 2);
synthetic crowbar swing: 27.3 damage. e1m1 smoke and final hidden screenshot passed; no full melee tuning/eval run.
Explicit `map e1m1 noplace` stays in e1m1; an absent map is rejected cleanly before loading.

## AD projectile impacts and flying ordnance (2026-10-12, worktree `adshots`)

AD shotgun/SSG projectiles now enter shared VR damage, positional multipliers and selective head/limb hooks at their
actual flight impact. AD shell resistance stays applied exactly once. Each player pellet retains its shot serial,
weapon, total pellet count, firing hand and launch position across frames and later weapon switches. Head/limb share
accumulates for that shot; existing head/limb eligibility, chance, distance, live/corpse and weapon controls still apply.
The bridge captures travel direction before AD's deferred removal zeros velocity. Close-shot retraces cannot extend
behind their launch point and incorrectly select another corpse limb. Small AD player projectiles retain MOVETYPE_FLY
and their normal movement hull, with model/corpse collision enabled specifically for `proj_bullet` in the AD engine helper.

The held Enforcer rifle now sets the shot damage context and uses the actual victim/impact for positional damage.
Its laser pop hook admits corpses under the existing corpse toggles. A failed selective projectile pop leaves the
corpse whole instead of draining its whole-body gib pool; torso shots retain existing corpse damage behavior.

AD GL/GLMON ordnance uses the same `progs/grenade.mdl` as vanilla and shared grenade setup: force grab, physical
catch/carry/throw, batting, rigid collision and shooting. AD's fuse, explosion, resistance and radius callbacks remain
its own. Other grenade-like AD projectiles (eggs/flesh) retain their models/behavior. AD grunt/launcher rockets join
shared shot interception and trigger AD's own missile explosion. AD entities registered as shootable ordnance route
damage to the shared ordnance handler rather than the AD monster damage route.

`vr_ad_pellet_speed` remains the player's archived multiplier (default 1); new `vr_ad_enemy_pellet_speed` is independent,
also archived/default 1. Both clamp to a minimum of 0.1, preserving authored speed/skill rules at defaults. The enemy
multiplier runs once at raw AD Launch_Bullet, covering both native AD monster launches and VR's vanilla-monster bridge.
AD importer hooks use uniquely checked function boundaries and are recorded in the generated import audit.

Regression probes: `vr_ad_test 50..57` fire actual SG projectiles/Enforcer bolts against AD/vanilla grunts, alive or
settled corpse; scratch1 selects head (0), upper arm (1), torso (2), shin (3), scratch2 selects killing/damage-only.
scratch4 selects AD's small MOVETYPE_FLY mode; scratch3 chooses real SG/SSG volley size (0 is one probe pellet).
58 checks independent speeds, 59 real raw AD grenade creation/bat/grab, 60 raw AD grunt rocket creation/hitscan intercept.
`Misc/quakevr/ad_shots_test.ps1 <name>` checks small pellets and complete 6/14-pellet volleys; `-Full` also checks normal
pellets/lasers, damage-only cases and ordnance. `-VolleysOnly` repeats only complete volleys; `-CheckLog` validates the
saved records without another game run. Results stay in scratch.

Validation so far: QC/engine/FGD build passed with zero QC warnings. Normal-flight matrix covers all three regions,
both games, alive/corpse and pellet/laser; head 1, upper-arm mask 96, torso no selective cuts, no whole gibs. A corpse
arm missed during early test flight; shortening the probe flight to 20 units passed its repeat. The new small-pellet
matrix passes the same regions, including the close corpse head that previously retraced into a foot. Healthy AD grunt
pellets do 6 head/4 body; laser 27/18. Positional damage disabled gives 4 at the head/shin; head multiplier 3 gives 12,
and leg multiplier 0.6 gives 2.4 (entity health rounds). Player/enemy speed measurements: 1996/957 default, 4001/948
player x2, 2004/1896 enemy x2. Grenade flags catch/force/rigid/shoot all 1, shared drawn size 6.279 x 2.325 x 2.209,
fuse 2.50 s, bat speed 300/spent 0, grabbed 1. Rocket real hitscan interception spent 1/AD explosion callback 1.
e1m1 and AD campaign `ad_crucial` smoke exit 0; AD reports rules on, 1461 AD spawns, 238 shared spawns, zero outside rules.
Complete AD_FirePellets launches (12 SG, 12 SSG) pass all 24 live/corpse AD/vanilla head/arm/torso cases: each selected
head/arm pops and remains a whole corpse; torso damage retains authored gibbing (one live AD torso SSG did gib whole).
The final checker explicitly permits that torso behavior and verifies actual launch metadata counts 6 and 14.
Final integration includes shipped next `aae2d0708` (corpse2/wrist2); clean merge, QC/engine/FGD build passed again.
Six selective corpse impacts and grenade/rocket interactions passed, followed by both map smokes. Forced failed rolls
(head/limb chance scales 0.5, roll 1, Quad override off) also preserve both corpses: damage 0, head 0, limbs 0, gib 0.
`Misc/quakevr/ad_shots_failed_roll_test.ps1 <name>` preserves this two-case actual-flight regression. Chance scale below
1 is necessary: a guaranteed near-range pop skips rolling even with forced roll 1. The standard eval harness found
its canary empty and automatically expanded to 177 synthetic takes (49 pass, 128 existing fails); zero baseline
differences. No separate full-suite invocation or melee tuning was requested/performed.

Headset checks: SG/SSG and Enforcer aim at each live/corpse body region in vanilla and AD maps; compare positional
damage toggles and weapon-specific pop controls. Catch/force-grab/bat/rethrow grenade grunt/ogre ordnance and shoot a
grunt rocket in flight. Independently adjust player/enemy pellet speed. No melee settings were tuned. Preserve all
source assets; scratch test helpers/logs and worker-local crash reports from early scaffolding runs are disposable later.

In VR: enter from Motion Recorder, pick up each melee weapon, circle the dummy and record your new takes. Confirm
the simple room's space, lighting and pickup spacing feel suitable. Files worth deleting later: worker `scratch/`,
`quakevr/meleeroom_check.cfg`, and its generated `quakevr/motions/no_hit_room_check_*`, `room_*` test outputs.


## Corpse shoulder contact, AD weapon cleanup and dummy recovery (2026-10-12)

Voice notes: vrfiringrange 10:57:12, 10:57:53, 10:58:49; ad_crucial 11:11:20, 11:27:57,
11:31:35, 11:33:27. The upper arm was directly jointed to the chest with connected collisions disabled;
previous overlap diagnostics excluded jointed pairs, so a buried shoulder/elbow escaped both contact and reporting.
Corpse shoulder joints now permit contact. Pre-solve spares only the shared socket (40% of upper-arm length),
allowing the distal upper arm/elbow to meet the chest. Living joints and stagger/get-up/ledge/reach protections stay
as before; a monster killed while down enables the same shoulder contact on its retained joints.
`vr_ragdoll_list` additionally samples distal arm points against an 80% chest core, including directly jointed arms.
`vr_ragdoll_arm_test` (Debug > Tests > Ragdoll Behaviours > Fold Corpse Arm Into Chest) makes a reproducible intersecting
corpse-arm pose; it excludes living ragdolls. No new cvars or changed defaults.

Armed AD soldiers, defender/eliminator, centurion, pyro and fumigator shed their built-in guns on death and drop one
single-use native burst rifle as requested. The centurion also sheds its hover board; enforcer-family muzzle effects
and pyro/fumigator gun hose are excluded from corpse geometry. All modifications guard model identity and exact mesh
vertex/triangle counts. Tiny muzzle geometry and a spanning disc/hose must not become dense or articulated body parts.
Weapon-free left-arm seed centres/joints and rig authoring descriptors were corrected; the original gun-centred left
forearm seeds otherwise produced an empty forearm in the enforcer/centurion after removal. Pyro tanks remain visible.
Matching native AD variant weapons and complete active grunt/enforcer variant coverage are explicit BACKLOG items,
as the author permits deferring the latter.

Dummy recovery now schedules `vr_dummy_think` and returns to its configured idle pose/home. A plain get-up previously
entered `army_run1`/`ai_run` before the wrapper detected completion, calling absent dummy AI and crashing; active
recovery could instead leave the enemy stand callback frozen. Dummies now bypass ordinary enemy AI on handback;
normal monsters retain their existing animation/AI paths. Existing `vr_dummy_test 4` reports callback, frame, movement,
knockdown state and distance from home for repeatability.

Validation: build passed, QC 0 warnings. `corpse_notes_test.sh corpse2` tested eight AD variants: 8/8 corpse rigs,
one placeholder drop per kill, all settled part speeds 0.0 u/s after about 9.5 s. Four forced shoulder poses with
self-collision disabled retained 2/5/2/2 buried distal samples (11 total); enabled, all four resolved to zero.
Six repeated dummy recoveries (three each with active get-up off/on) kept knockdown 0, dummy callback scheduled,
MOVETYPE_NONE and home gap 0; changing idle frames confirmed animation continues. Living stagger/get-up/pull-up/guard
regression: 4 passed, 0 failed. The full available motion set: 177 takes, 49 pass/128 baseline fails, 0 differences
from baseline (archived canary absent). e1m1 smoke passed; calibration 4 paths found, 0 missing. Visual QA of centurion,
pyro and the folded grunt passed: `scratch/corpse_cleanup_visual.png` (disc/guns absent, pyro tank retained, arm freed).
Existing screenshots preserved. In VR, retest corpse shoulder/upper-arm manipulation, centurion/pyro kills and repeated
dummy shove/get-up cycles. No deletes performed.
## Wrist seam, gadget bands, blood and skin tone (wrist2)

The bare arm now uses the fitted hand contour at the loft's actual resolution (144 samples, previously a 24-sided
loft snapped onto a 12-point contour). Wrist/lip clearance is 0.128–0.823 mm across all three builds and both sides.
Bare models have 5,732 vertices / 10,874 triangles; braced models remain 1,250 / 2,186. This spends geometry on the
concave hand outline. All 16 authored body diffuse repaints and hand LMPs are byte-for-byte unchanged (--keep-edited).

The casing keeps one mount in both bracer modes. Two bands fit skin or leather separately; the bare girth table now
includes the thick skin before the 0.15 m taper, so the elbow band is no longer buried. Straight/bent wrists and
0/60-degree forearm roll were inspected in eye captures. Four runtime cases draw two bands, maintain gadget origin
within 0.0004 engine units, and report eight wrist endpoints at 0.00 mm separation.

A wound on your hand or body reuses world-space splats on adjoining models. Gib-hand smears reuse the same patch
noise on the arm, bounded to 12 cm from that wrist. Bidirectional Debug > Gore tests (vr_wrist_blood_test hand|arm
[main|off]) verify both masks gain blood: body +6,792–12,656 texels, hand +18,450–21,191. A separate gib-smear check
gains 13,915 body blood texels. Healing ownership is retained.

Body > Skin Tone (vr_skin_tone, archived, default 0; range -1..1) multiplies authored skin in the shader only.
Twenty small masks exclude nails, clothing and static wounds; dynamic blood and char bypass tone. Masks load before
alias batching (cache-safe) and use sampler 6, leaving retro LUT sampler 3 intact. Only bare forearms draw the
hands in the body's light; with bracers each hand samples its own (review fix, his decision). The calibration board links the slider. No diffuse tone variants are generated.

Validation: QC/engine/FGD build passed (0 QC warnings, 367 entities). wrist_test.py verifies contour clearance,
unchanged paints and zero clothing/static blood coverage. wrist_runtime_test.py passes blood, mount, wrist and menu
checks plus 14 real eye captures. Dark/light changes 230,263 bare / 219,341 braced pixels; opaque blood regions retain
colour on 99.97% / 100% of 37,181 / 35,237 interior pixels (95th-percentile delta 0; bloom/reflections off for isolation).
Calibration qbsp/vis/light exit 0; 5 menu paths found, 0 missing. e1m1 -> calibration smoke with nonzero tone passes.
Additional close strap and bidirectional seam-blood eye captures inspected. No gameplay/physics or melee changes.

VR follow-up: test bent wrists with actual hand calibration, roll the gadget arm with bracers on/off, smear gib
blood across the seam, and tune Skin Tone under several room lights. Review the geometry cost on the headset.
Disposable worker outputs (not deleted): scratch/, eyeshots/, build logs/crash dumps, test cfg; generated id.wad stays
local and ignored, never committed. Visual sheets: scratch/wrist_visual.png, wrist_straps.png, wrist_bridge.png.

### Ammo-pouch destination, loaded grip grace and magazine reinsertion (reload2, 2026-10-12)

Voice notes AD 11:20:14, 11:32:35 and 11:34:30. Boxes, shells and magazines now shrink toward the rendered leather
bag's centre rather than the hand's reach hotspot. The point follows the same model transform as the pouch, including
body clearance, crouch, configured offsets/turn and size. Its local centre follows make_ammo_pouch.py's leather outline
(BACK_X + depth * 0.45, z -0.4); stocked/empty depths 2.5/1.3. Round-specific bounds are deliberately excluded.
Hidden pouches retain the old reach-point fallback. vr_dumpview reports the centre, and vr_debug_collect_fx 2 reports
the animation endpoint's error against it.

After loading a save, OnLoadGame grants the weapon instances actually restored in each hand a 0.75-second grip grace.
Game time starts the window after the load pause, so the loading screen does not consume it. VRWeaponLetGo suppresses
unpressed-grip releases only for those instances during the window; new weapons, fresh-map starts and drops after it
follow the existing rules. New archived vr_weapon_load_grace defaults to 0.75, with a 0..2 s "Grip Grace After Loading"
slider beside Weapon Grip in VR Settings. 0 disables the grace. The weapon-instance debug report includes time left
and whether each hand still holds the restored instance.

The former fixed extra 4-unit withdrawal before reinsertion is archived vr_reload_reinsert_clearance (default 4,
negative values clamped to 0). "Magazine Reinsertion Clearance" in Advanced VR Options > Weapons > Reloading exposes
0..16 units (extended to 64). Held magazines must leave the insertion reach plus this margin; loose magazines still
clear both hand guns' collision leniency and the existing eject cooldown. This also governs a replacement magazine
that bumped the previous one out. No existing reload thresholds or authored settings were retuned.

Debug > Tests > Reloading > Magazine Reinsertion Boundaries (vr_reload_test 28; impulse 125) exercises actual held
and loose paths at both sides of the threshold for -2/0/4/10 units, latch retention, both guns and cooldown: 22/22 PASS.
Misc/quakevr/reload/voice_20261012_test.sh runs that test, real save/load with both held guns ungripped, disabled grace,
ordinary fresh-map drops, and nine real collection animations (box/shell/magazine × standing/crouched/offset-and-turn).
Collection endpoints have 0.0000-unit error against the rendered bag centre. The default load grace retains both guns
through 0.728 s, then releases them by 0.826 s; zero grace and fresh-map drops follow the original release path.
Release prediction used by firing shares the same guard: the restored gun remains usable during the window
(one shotgun shot consumes 8 -> 7 shells without holding the grip).
QC/engine/FGD build passes (0 QC warnings); calibration menu paths: 5 found, 0 missing.

VR follow-up: return every ammo kind to the pouch while crouching; reload an autosave without gripping, then regrip or
let the grace expire; tune Magazine Reinsertion Clearance for the out-and-back motion. Saves and probe logs remain
local to this worktree (voice_reload2_grace/reload2_grace/reload2_grace_exact and scratch/); none were deleted.

### AD world contacts, physical backpacks and real ladder rungs (October 12, adworld)

Notes ad_crucial 11:06:00/11:10:40/11:21:55/11:22:29: laser buttons accept hands, guns and props;
AD backpacks match vanilla size/weight and require grip plus holster; ladder grips meet real steps,
with no jump climb or oversized keyboard approach obstruction.

- AD touch buttons install common hand/whole-weapon/held-prop/thrown-prop handlers. AD dispatch preserves original
  movement, targets, activator, estate gates, shoot-only distinction and timing. Native paths are AD-tagged.
- AD map/dropped backpacks preserve skins, inventory and AD respawn callbacks. Bounds normalize to vanilla bag
  dimensions at its 0.75 scale; existing backpack density supplies comparable mass (sample AD bag 16.8 kg).
  Rigid physics, forcegrab/grip carrying, strict holster consumption; AD floor-check teleporting disabled.
  Restore/toggle repeats setup without compounding scale; native respawn does not overwrite AD-owned bag respawn.
- Ladder volumes never manufacture holds. Normal ledges run first; closely spaced rungs get a ladder-only real
  solid-top trace within hand-touch tolerances. Standard hanging/pulling/mantling remains. Tracked hands bypass
  AD's jump and velocity ladder callbacks before they freeze, propel or redirect the body; flat play retains AD.
- Reviewed importer hooks regenerate cleanly. Debug > Tests exposes buttons 61, backpacks 62, ladder movement 63 and
  actual rung/gap probes. Existing Hand-Climbed Ladders default 1; no new cvars.

Real ad_crucial: laser1 button *389 opens with hand, whole-shotgun contact, thrown prop and held prop;
all four retain player activator; disabled control stays shut. AD dropped bag 13.1174 x 13.8088 x 18.9188 units,
120 s expiry; walkover 0 shells, real grip holds, shoulder release gives 5 shells and clears held slot.
Map bag restore/toggle stays physical, same dimensions, holster-only. Both ladder modes preserve 21, 0, -17 velocity
and gravity 1. Seven real rungs hold; seven gaps do not; setting off rejects the same rung. Calibration 0 missing.
Automated: Misc/quakevr/ad_world_test.sh <agent>. Build/QC/FGD and e1m1 smoke pass;
melee replay 49/177 pass equals baseline (0 differences), no melee tuning.
Human VR: press laser buttons with each contact mode; grab/holster colored ammo/armor bags; approach ladders closely,
grab alternating real rungs including underside touches; confirm jump/gaps never climb.

## 2026-10-12 voice-note rendering audit (lightaudit)

**Stationary AD lights.** The author clarified that brightness pulses while stationary, rather than lights switching
with head movement. Reproduced small stationary brightness variation at the 11:23:22 note's `ad_crucial` position
`-215 -2287 136`, pitch -24 / yaw -35, using the author's current render settings read-only. The BSP deliberately has
four nearby style-1 flicker lights (about 413–503 units away), plus a switchable style-34 `sparkli2` light. Style 1 is
Quake's `mmnmmommommnonmmonqnmmo` flicker pattern. Its contribution is local to faces lit by those lamps, explaining
why some positions show it. A six-direction +/-8-unit camera sweep, repeatedly returning to the same view, kept
exactly the same map-shadow lights 295/76; no slot oscillation or camera-dependent light failure was demonstrated.

Diagnostic `r_flatlightstyles 1` makes every lightstyle constant at its average, including switchable styles; it is
an investigation control, not a shipped fix. In the original sweep's valid center-return eye images, normal styles
changed the upper architecture ROI by at most 0.360/255 mean channel value, 23/255 maximum, with 320 pixels changing
more than 12/255. Flattened styles changed no pixel by more than 1/255. Four stationary flattened-style eye captures
were byte-identical. Disabling torch lights, map shadows and dynamic shadows after flattening added no change.
Keep intended map flicker and shipped settings unchanged. If the author wants less intentional flicker, a separate
opt-in AD style-amplitude control should treat built-in animated styles independently of >=32 switched styles.
No map/lightstyle patch was made. The exact 1205.9-second progression/switch state from the note was unavailable.
Some hidden mock readbacks in both eyes were wholly blank (including with flattened styles); those frames were
excluded from brightness comparison. This separate capture limitation was not attributed to the reported lamp
pulsation or silently "fixed". `Misc/quakevr/ad_light_flicker_probe.ps1 <agent> [render.cfg]` preserves the 26-view
probe; its logs label every view and show the selected shadow lights. Numerical scripts/JSON and the single contact
sheet remain under worker scratch; they are not assets to distribute.

**Developer-output crash.** The early adshots dump `2026-10-12_12-42-56.txt` conclusively names the Map parser thread:
`COM_FindFile -> Con_DPrintf -> Con_SafePrintf -> Con_Printf -> SCR_UpdateScreen -> GL_AcquireFrameResources`, sync
failure 0x0000. `Con_SafePrintf` toggled the main thread's loading flag concurrently; another writer could clear it,
allowing the parser to enter rendering without an OpenGL context. This is a general console threading defect,
independent of AD lightstyles. `Con_Printf` now still writes background messages through `Sys_Printf` but returns
before console buffer/screen access. `Con_SafePrintf` routes worker messages directly there without touching the
loading flag. Main-thread output is unchanged, and existing `VR_OnMainThread` is initialized before Map parser starts.
One baseline attempt did not reproduce the intermittent failure; no claim of deterministic before/after frequency.
Build/QC/FGD pass (0 QC warnings); five developer-1 launches with three map loads each pass, with 1,804–1,819 file-log
lines per launch preserved, including e1m1/e1m2 smoke. Reusable `Misc/quakevr/map_parser_console_test.ps1 <agent> [runs]`.

**AD relief generation feasibility (audit only).** `VR_LoadNormalMap` already tries authored `_norm` then `_bump`,
and otherwise derives a normal map from the diffuse texture's shading. `TexMgr_ShadingToNormals` uses luminance and
a wrapping Sobel gradient; this makes cheap bumps, not recovered physical geometry. The installed AD paks contain
no `_norm`/`_bump` sidecars. `ad_crucial` contains 227 embedded textures (2,480,640 texels); AD's 38 playable BSPs use
2,538 unique texture name/dimension tuples (duplicates with different pixels are not counted separately). At their
native dimensions, RGBA normals including height alpha and mipmaps would occupy about 12.62 MiB for Crucial and
238.77 MiB across all those textures; these are estimates of normal textures, not added memory when replacing an
already-generated normal. Upscaling all 227 to 512 square would instead require about 303 MiB, without restoring
missing material detail automatically. Actual per-map use only loads that map's textures.

The author's `vr_texture_smooth 1` plus nearest magnification disables generated parallax heights for embedded
indexed world textures (`TexMgr_IndexedSmooth` gate); generated normals still work. Replacement images can carry
heights. Mode 2 already enables smooth indexed textures/height relief and is the inexpensive comparison to try;
it cannot distinguish a painted shadow from a groove. A convincing pass can be mostly automated in production,
but should start with 10–20 representative metal/stone/wood/painted textures: separate base color from baked shading,
define material height masks/scale, make seamless height/normal candidates, preview at VR distance under several
light directions, then review/correct results and expand by material family. A raw luminance conversion makes
paint, highlights and old baked shadows into false cavities/ridges. The engine already accepts `_bump` height
sidecars or `_norm` RGBA, with height in alpha (255 top surface); an RGB normal alone does not supply parallax depth.
No additional neural tooling, downloads, whole-mod generation or rendering defaults were introduced.

**Quake vertex pulsation feasibility (audit only).** The author means surface "breathing" even when an animation
plays smoothly. Original MDL coordinates are quantized bytes multiplied by per-model axis scales (`gl_mesh.c`,
alias shader decode), and each key pose may genuinely deform the surface. Vanilla soldier's increments are
0.290806/0.261986/0.264820 units (roughly 7 mm at this port's scale), so small per-vertex changes can be visible in VR.
Across QC's eight idle keys, global rigid fitting still leaves up to 0.205-unit RMS / 0.350-unit p95 residual;
that includes deliberate limb motion and is evidence of pose variation, not a direct measurement of encoding error.
Smooth mode 3 reduces velocity discontinuities but preserves exact authored key poses; it cannot remove their
breathing. Increasing precision after loading cannot recover the positions already lost in the source MDL.

A reasonable cheap prototype is an optional, render-only, per-bone rigid-fit/residual filter on one grunt idle
clip using the existing ragdoll rig. Keep joint motion, foot contacts, weapon/hand alignment and large residuals;
suppress only tiny non-rigid residual changes in regions expected to be rigid. Cache filtered poses at model load
and A/B their silhouette, gun, feet and timing. This deliberately changes key poses, so keep current exact mode as
the default and separate the filter from temporal smoothing. Clip boundaries and useful thresholds need per-model
review, especially anonymous native frame names, jaw/finger movement and reloads. An offline bake retaining float
poses or a supported skeletal/IQM asset is another route. Full clean rigs/reanimation offer the best result but are
substantial asset work. No automatic all-monster retargeting or animation change was implemented in this audit.

Disposable outputs (preserved): lightaudit scratch, screenshots/eyeshots, build log, test cfg; no id/AD data extracted
or committed. No new cvars, defaults, migrations or checklist changes in this batch.
### AD tweaks, ammo models and safe feature scopes (2026-10-12, admenu2)

Voice notes 10:55:27, 11:01:16, 11:02:54 and 11:27:41. Advanced VR Options now links directly to a registered
Arcane Dimensions page (the previous Combat link referred to a function missing from the page table). The Combat
shortcut remains. Player Pellet Speed and Enemy Pellet Speed expose independent existing multipliers,
both default 1. A fixed-seed flight check at player 1.75 / enemy 2.5 measured 3492 / 2369 units/s, matching
3500 / approximately 2375 including AD's small speed variation.

New archived controls: `vr_ad_ammo_models=1`, `vr_ad_resistance=2`, `vr_ad_upgrades=1`, each 0 Off / 1 AD Maps Only /
2 Everywhere. Ammo Models chooses all eight AD shell/nail/rocket/cell MDLs at spawn, preserving normal ammo
quantities and the VR hand-touch/force-grabbable path. MDL-centred bounds and ordinary-map corner placement are
adapted before existing item physics takes over. Reload the map to change existing ammo appearance. Missing AD
data/model falls back to the original box. No assets are shipped. Resistance scope is shared by our damage bridge
and the imported AD resistance function, via a checked importer patch. Upgrades retain their owned bits while
Off; Everywhere permits their existing pickups and effects in ordinary maps. Spawn menus respect the same scope.

The existing projectile rule cvar remains numerically compatible (1 Off, 0 AD Maps Only, 2 Everywhere);
AD-specific enemy AI still owns its projectiles. Additional Content keeps its existing 0 AD-only / 1 Everywhere
semantics. AD map monsters, keys, world triggers, exits and scripted inventory are required for map progress:
the unsupported Map Logic / Inventory Override row is disabled and explains this. VR hit/physics fixes remain
active independent of optional AD features. Native AD inventory/UI coverage is recorded in BACKLOG.md as requested;
it is not claimed complete.

Checks: Release build, QC 0 warnings; e1m1 and ad_crucial smoke exit 0. Three scopes in each campaign: all 48 ammo
boxes selected the correct model, retained counts (20/40 shells,25/50 nails,5/10 rockets,6/12 cells), became
rigid force-grabbable props and had finite centred bounds. Resistance probe 10 damage ->10 while disabled,
->5 with 50% resistance active. Upgrade effects match map/scope and three upgraded spawn catalog rows are ready
outside AD when Everywhere. Both held SG/SSG models survive AD ammo refresh in each campaign. Calibration boards:
5 found,0 missing. Repeat with `bash Misc/quakevr/ad_scope_test.sh <agent>` then
`python Misc/quakevr/ad_scope_check.py C:/OHWorkspace/qvr-agents/<agent>`.

Recent Searches stores ten entries (six previously). Seeded ten real settings and checked 10 kept / 10 shown /
room for 10, all above help and under the keyboard; stale entries still resolve safely and existing adaptive
clipping remains. Visual composite `scratch/admenu_ui.png`: AD page has20 visible rows, direct Advanced parent;
Search has all ten readable rows. No calibration paths moved. User VR checks: ammo appearance with scope/reload,
upgrade switches outside AD, pellet speed feel and the expanded Recent list.
## Melee takes of 2026-10-12: a reproducible suite, waves and wiggles, the retraction double hit (worktree `melee2`)

**The suite.** `eval.sh` already ran the takes unpaced, undrawn and silent in parallel copies, a map load per take. On
his 177 new takes it takes 10-15 s with 8 copies (45 s with one; the first run of a new agent's copies 67 s: their
game folders are made then). Three fixes made its verdicts mean something:

- The dummy retyped as an invincible grunt names its hits `monster_army`: every hit take failed "no melee/..." (49/177).
  A hit on the target's classname now counts (`vr_motion_play.cpp`, Report::targetName): 152/177.
- A take's replay depended on the one before it in the same copy (two rows differed between 8 copies and one): the
  head's last position was a step into the next take (the lean) and the body's gait and stance went on. Each playback
  now starts afresh (`hands::resetForPlayback`, `avatar::resetMotion`): 8 copies, one copy and 22 takes alone give
  the same rows, every column.
- The setup's grips closed beside the dummy took it with Holding Enemies (`vr_foegrab`) instead of the weapon, which
  fell (three axe and sword takes slapped with an open hand): foe grab is held off during the setup only. Replays that
  hit as live: 152 -> 157 of 177.
- The take's `vr_dummy_anchored` (next, 2d457670e) is applied; takes from before it: 0.

**The no-hit takes** (23: fists, axe, sword waved, wiggled, pulled back near the dummy; 16 hit, most also live). The
blows' numbers (developer 2, `melee debug`) split them from his 105 hits:
- pulled back: the far end going back along the line, cos -0.4 to -1 (his hits -0.26 to 1): `VR_MELEE_PULLBACK -0.35`;
- slow and short: swings at 1-1.6 times their least speed coming 0.22-0.51 m (his slowest cuts, 1.27-1.43 times,
  came 0.66-0.86 m): the run-up a swing needs grows from `VR_MELEE_RUN` 0.2 at 2x to 0.6 at 1x (`VR_MELEE_SLOW_*`);
- the wrist barely carried: swings 1.3-2.8 m/s (cuts 3-7.6): `VR_MELEE_WRIST_SWING 2.3` (x vr_melee_wrist_speed);
  pommel strikes 1.3-2 (his 4.6-5.6): `VR_MELEE_WRIST_POMMEL 2.5`, and they come `VR_MELEE_POMMEL_RUN 0.35` m.
Guns keep their own numbers (no gun takes in the set) but are pulled back too. The whoosh (stroke) follows the same rules.
no_hit 7/23 -> 20/23, every hit label unchanged (168/177).

**Retraction double hit.** What a hand struck it strikes again only entering it (`mh_struck`; the sweep starting in its
model or box strikes nothing); a first blow isn't held to it (his punches and two-handed thrusts start their landing
frame within a point's reach of the model: two takes lost their hit when it applied to every blow). Synthetic takes
(`scratch/retract.py`): a punch held in the dummy then drawn back hit twice, now once; a punch drawn out and thrown
again still hits twice.

**Still failing (9), for his review:** no_hit 12-25-50 (a punch by every number: 7.2 m/s, 2.25x, came 0.48 m, forward;
live it didn't land), no_hit 12-37-24 and 12-37-28 (sword: slow cuts, 1.3-1.6x, came 0.54-0.6 m, wrist 3.7, the
blade going 104-108 degrees back from its line; live they hit), not_parry_pose 12-38-35 and 12-38-38 (sword held in
the parry pose 174/174 and 46/180 frames), parry_pose 12-31-42 (axe: 0/134 frames), slash_horizontal_ltr 12-29-35
(axe: a one-handed bash, live too), punch_overhead 12-23-27 (the fist closed 0.27 s before the punch: Holding Enemies
takes the dummy in the replay, not live: lateGrab's 0.2 s, frame timing), slash_backswing_up_left 12-35-23 (the
helping main hand sits in the ammo pouch's reach at the take's start: the setup's grip takes a shell).

**Damage per weapon and type** (`eval_damage.py`, printed by the full set): punches 19.2 mean (7.3-39), axe chops 38.5
(21-63), sword slashes 30.2 (9-60), stabs 29.9 (15-73), slaps 17.6, axe pommel 11.7, axe bash 7.3. Spread comes from
the zone (head x1.5, limbs less) for chops, slashes and punches, from the strength (speed) for stabs and slaps.

## Melee damage: flatter curve, sword tip speed, hard caps per blow (worktree `meleedmg`, 2026-10-12)

His three decisions on the damage table:
- **Flatter curve:** `VR_MELEE_STRONGEST` 2 -> 1.75 (vr_melee.qc; the rest of the curve as it was).
- **A sword measured at its tip:** a sword's slash or stab takes its strength from its tip's speed (`mh_dmgspeed`, the
  dummy's report shows it) over the least speed times `VR_MELEE_TIP_SLASH` 3 / `VR_MELEE_TIP_STAB` 1.5: fitted on his
  takes (developer 2) so the mean stays (his slashes swing the tip 2.96x the grip, median, 1.6-4.0; stabs 1.54x,
  1.0-2.7). A wrist-whipped cut counts more, an arm-pushed one less. Whether it lands (and `mh_speed`: grenades, gibs,
  blunt pops) still goes by the grip. A sword carried by its blade, near end out, keeps the grip's speed.
- **Hard caps:** `vr_melee_dmg_min` 0.5 / `vr_melee_dmg_max` 2.25 (Combat > Melee: Least / Most Blow Damage). The blow's
  speed strength (`mh_core`) times the zone's multiplier is clamped to them (combat.qc `VR_Melee_CapDamage`, in
  T_Damage_VRMelee, for a blow VRMeleeDmg priced); everything else multiplies after: a pommel's or the hook's share, the
  punch and slap mults, a counter, stamina, Melee Damage x and weight, `vr_melee_dmg_multiplier`, Quad. Test and
  headbutt damage isn't capped. Check: min = max = 1 makes every chop 22.0, slash and stab 24.2, punch 13.8 (sd 0).

First hits on his 111 hitting takes, before -> after (mean, min-max, sd); verdicts unchanged (168/177, 0 flips):

| weapon, type | before | after |
|---|---|---|
| axe chop (17) | 38.5, 21.1-62.8, 12.3 | 37.3, 21.1-49.5, 11.1 |
| axe pommel (6) | 11.7, 8.3-17.3, 3.2 | 11.7, 8.3-17.3, 3.2 |
| sword slash (32) | 30.2, 9.2-60.5, 13.2 | 30.1, 12.1-54.5, 13.5 |
| sword stab (15) | 29.9, 15.1-72.6, 14.5 | 27.5, 16.5-54.5, 10.8 |
| sword pommel (2) | 24.7, 15.1-34.2, 9.6 | 23.9, 15.1-32.7, 8.8 |
| punch (28) | 19.2, 7.3-39.0, 6.2 | 19.0, 7.3-30.9, 5.4 |
| slap (8) | 17.6, 9.4-20.6, 4.3 | 14.2, 9.4-15.5, 2.3 |

The caps cut the fast head hits (chop 62.8 -> 49.5 = 2.25 x 22; slash and stab 54.5 = 2.25 x 24.2) and lift the slowest
slash (9.2 -> 12.1 = 0.5 x 24.2). Slaps drop with the curve (their head hits ran x2). The axe's bash (7.3) isn't a speed
blow: unchanged.

## Review fixes: AD ladders, grenades, pellet pops, menu (worktree `fixad2`, 2026-10-12)

From review r1 (and r3's AD isolation note):

- Ladders are never unclimbable. AD's keyboard/jump climb is off only while Hand Climbing and Hand-Climbed Ladders are
  both on (QC VR_AD_LadderManual, used by trigger_ladder and AD_PlayerPreThink); with either off, AD's own climb (hold
  jump) lifts you. A ladder volume with no real rung within reach of the hand (vr_climb.cpp rungNear: a walkable top
  within 24 units above or under the hand, up to 32 deep into the ladder, above Lowest Ledge) is held anywhere in its
  volume, as before the rung-only change, so flat ladder walls and far-apart rungs climb again. ad_world_test.sh: 3
  settings x 2 ladder modes; a flat ladder (vr_ad_test 64, Debug > Arcane Dimensions: Flat Ladder Test) held twice and
  refused with Hand-Climbed Ladders off; ad_crucial's 7 gaps between rungs still hold nothing.
- AD grenades keep AD's point hull after the grenade.mdl swap (setmodel had given them the model's bounds, so an
  uncatchable one bounced off walls on hull 1). Test 59 prints the hull; ad_shots_test -Full asserts 0 0 0 with
  vr_grenade_catch 0 (-Full's script now fits under the console's 8191 characters, using an alias).
- AD projectile pellet pops: a blast's odds are one roll's at its whole share, as hitscan's. Each roll after the blast's
  first on a victim only tops it up (VR_AD_PopTopUp: q = (P - Pbefore) / (1 - Pbefore)); a pellet landing on the body
  its blast has just killed rolls the top-up as a late cut; a body its blast popped isn't gibbed by the rest of it.
  ad_pop_odds_test.sh: 3 volleys whose rolls compound to exactly their final chance (0.736, 0.694, 0.749); a late
  top-up popping the head, no gib.
- Debug's Buttons Test runs test 61 again; the Projectile Rules help names its labels; vr_ad_test 40-65 documented.
- Isolation: the ammo box fit (AD_AmmoBoxFit, now with AD's frame_box rules) and the corpse part-shot rule
  (VR_AD_CorpsePartShot) moved to the bridge; `// AD support` markers in vr_monstermods.cpp and buttons.qc.
- ad_hits_test.sh asserts tests 40-42: no spread, SG 24 and SSG 56 at 128 and 384 units, a burst round 9.
- Still open: ad_shots_test -Full's vanilla corpse volleys (mode 53) sometimes miss the corpse (no pellet hit: the
  ragdoll settles elsewhere), so -Full is flaky; the default run passes.
## Review fixes: ragnotes, toolbake, animbody, wrist2, dummyhold, vrgrabs, meleeroom, reload2 (2026-10-12, worktree `fixmisc2`; review r2/r3)

- ragnotes (his decision): blunt/fist/prop/bash hits on a ragdolled corpse's limb or head wear down that piece only;
  torso hits take the whole-body corpse health again (enough gibs it), as does a rig without limbs (VR_Limb_Allowed).
  `vr_limb_test` 24-28 print PASS/FAIL (25 now gibs); 28, a save/load, then 29 shows the per-joint damage persists (the
  field array saves every element: joint 4 kept 48.0). `notes_regression.sh` asserts 7/7.
- toolbake: Debug > Tests is rebuilt when `vr_test_spawn` changes, so Put It There's lock follows the chosen Thing;
  `vr_menu_press` refuses locked rows. `brush_test.py` bakes to new names each run (32/32 ok); `vr_ad_everywhere`'s
  description names AD monsters (it also spawns them in non-AD custom maps that place them).
- animbody: shader "vrbody"'s skins load once, unowned and persistent, for every body model (`imagelist vrbody` with
  athletic, lean+bare and brawny+bare loaded: 384 -> 96 MB); `vr_model_reload` no longer re-reads them. The Animations
  row goes back to Classic (remembering mode 3); the seed table's death boundaries apply only to the model it fits.
- wrist2: Skin Tone tones the skin's share of a wound's soft edge (no untoned ring: 176 -> 0 edge pixels at tone -1 in
  `wrist_runtime_test.py`); skin-mask lookups are cached (a missing file searched once; cleared when textures are
  freed). His decision: hands borrow the body's light only with bare forearms again (`drawnBare`), asserted per mode.
- dummyhold: a dummy getting up through QC (active get-up off, or a type without one) eases up through its get-up or its
  death backwards; its enemy's run is kept (`vr_dummy_th_run`) as the get-up's end, so Step no longer calls ai_run.
- vrgrabs: a drawn prop is kept out of the torso only while held, once a frame; a prop's lip takes the connected top's
  plane only across a bevel of at most 6 units (a wedge's slope keeps its own lip; not tested on a wedge prop).
- meleeroom: under the mock headset `vr_motion_play ... noplace` or `target ...` keeps the running map (TESTING.md).
- reload2: the vacuous "return inside" reinsertion check is gone (18 checks); the pouch endpoint check says it proves
  routing only.

## Corpse limbs: any weapon wears them off (worktree `limbhp`, 2026-10-12)

His decision ("any weapon can pop the limbs of a ragdoll"): ragnotes' per-piece corpse health, which only blunt, fist,
prop and bash hits used, now takes every weapon's hits. No new cvars or defaults.

- `QC/vr_limbs.qc` `VR_Limb_CorpseWear` (replaces `VR_Limb_CorpseImpact`; called from `VR_Corpse_Damage` after the pop
  roll, the small gibs and the kind multiplier `vr_corpse_dmg_<kind>`): a hit on a ragdolled corpse's head or a limb
  adds its damage to that piece (`vr_limb_corpse_dmg[]`, 0 the head, saved with the entity). When the piece's total
  reaches the monster's corpse health, it pops (blades and the chainsaw cut it off), whatever the weapon's pop chance or
  pop toggle. The pop roll still runs first; if it pops the piece, the hit wears nothing. Torso hits go to the
  whole-body health (`vr_corpse_hp`) as before.
- Hit point, in this order: a hitscan blast (shotgun, super shotgun) splits its damage by where its pellets landed
  (`VR_Limb_WearPellet` counts them per piece in `VR_Decap_Pellet`); each piece's share wears that piece and the
  torso's share goes to the whole body. Otherwise the lightning gun's bolt point (new globals `vr_limb_wear_*`, set in
  `VR_LightningHit`), a tagged hit (`vr_hitpos`: melee, rifle lasers, AD pellets, Mjolnir), the shot's
  `PositionalDamage` point (nails), or the flying thing itself (thrown props, enemy lasers). With none of these, the
  hit goes to the whole body.
- Unchanged: explosions (`QVR_CDMG_EXPLOSIONS`, splash), burning and a stuck axe's bleeding keep the whole-body path and
  their blast pops (`VR_Limb_BlastPops`). Folding blasts in was not trivially consistent: radius damage has no hit point.
  The whole-body path also stays with Limb Gore or Corpses off, on a rig without limbs, and on a body that is not
  ragdolled.
- AD: `VR_AD_CorpsePartShot` is gone (an AD pellet or rifle laser at a corpse's head or limb used to deal nothing).
  These hits now wear the piece through the shared path, so `combat.qc` has no AD line here any more.
- Tests: `vr_limb_test 30-38` (`QC/vr_limbs_wear_test.qc`; Debug > Gore Tests > Limb Gore, "Wear a Corpse Limb Off: ...")
  fire each weapon's real hit code at the nearest corpse's forearm until it comes off.
  `Misc/quakevr/ragdoll/limb_wear_test.sh <agent>` runs them with pop chances 0 (11/11 PASS). Hits to pop a grunt's
  forearm (corpse health 80): shotgun 4, super shotgun 2, lightning 3, nails 9, rifle lasers 5, AD pellets 20, axe 4
  (cut). Whole-body health stayed 80. Shotgun at the torso: gibbed after 4 blasts, nothing worn into a piece. Shotgun
  at the head: popped after 4. 28, then save/load, then 29: the worn damage persists.

For VR: with Gore > Head Chance and Limb Chance at 0, shoot a corpse's arm with the shotgun, nailgun and lightning gun:
it should pop after a few hits while the body stays. Shoot the chest: it gibs as before. A rocket next to it: unchanged.

## Menu quicksave and quickload shortcuts (`menusaves`)

The author's top-left menu shortcuts now include Quicksave and Quickload immediately below Back to Game. No new
cvars or defaults. `Quake/vr/vr_menuui.cpp` adds them to the existing toolbar's labels, icons, layout, laser/mouse hit
tests and stick navigation, including the flat desktop icon row. Both dispatch the keyboard bindings' existing
`save quick` / `load quick` commands (and messages), so the usual save format, campaign/addon handling and load UI
still apply. Quicksave returns to the game and remembers the current menu page; Quickload reopens Main next time,
as the Load menu does.

Unavailable buttons stay in place, with dim icons and text; pressing prints the reason and keeps the menu open.
Saving uses the host command's restrictions: local server, one player, no intermission, no `nomonsters`, living
player. Loading checks the same current-game/base-directory `quick.sav` lookup as the host and waits until an
asynchronous save finishes. These checks run again on activation.

At the smallest Menu Height / Row Spacing the additional buttons would overlap Recording. The headset column now
fits proportionally above Recording (or OBS, when shown), with drawing and hitboxes sharing its reduced dimensions.
Normal panels retain the author's 20-pixel buttons and 4-pixel gaps; the desktop row is unchanged. At both settings
1 with Developer detail, the column ends at y 148.1, Recording starts at y 152.1: 4 pixels clear (previously the
column ended at y 288.1). No VR Settings page or row moved or changed name; calibration board paths are unaffected.

Validation: kit build passes, QC 0 warnings, FGD 367 entities covered; `git diff --check` clean. The targeted mock
script (`scratch/menu_shortcuts_test.ps1`, ignored) verifies save/load via laser, stick (buttons 1/2 after Back) and
desktop mouse; laser save/load also passes at minimum height/spacing. Each action closes the menu and logs saving
or loading `quick.sav`; a loaded game reopens Main. No-local-game, missing quicksave, zero health, `nomonsters` and
multiplayer refusals verified. The final run has 4 saves and 5 loads (the extra load from the no-game menu confirms
loading remains available with an existing quicksave). Default VR, minimum VR and flat-menu screenshots inspected
as one contact sheet. Final e1m1 60-frame smoke: exit 0, no engine error/crash. No melee/physics change.

For VR: from a settings page, laser Quicksave, reopen the menu, then select Quickload with the stick; check the page
is remembered after saving and Main opens after loading. Check the dim saving action after dying. On the smallest
panel, check targeting and separation from Recording. Files to delete when finished: ignored shortcut test script,
`scratch/menu_shortcuts.png`, test screenshots and `quakevr/quick.sav` (plus its addon sidecar, if present).

## CPU profile follow-ups: throw estimate, liquid volume, file listings, prop sizes (worktree `perfopt`, 2026-10-12)

From his VTune hotspots run of 2026-10-12 (`qvr_flat/r000hs`, flat screen, a save loaded). Same results, less work. No
new cvars or defaults.

- **Throw estimate** (`vr_throw.cpp`; 2.3% of the run's CPU, the main thread): `buildMove` asks for an estimate each frame
  and hand while it isn't gripping and hasn't let go yet, which is always on a flat screen. `localFit` (about 250 fits an
  estimate) found each Gauss point's sample by a binary search (`Track::after`). It now steps from the piece's own sample
  (`Track::afterNear`): the same index, since a history's sample times only increase (`push`). The spin track keeps the
  search, as each sample's spin lag is its own. `withOwnAim` ran `releasePeak` again on the controller's own history,
  which outside slow motion holds the very same samples: `sameSamples` (a `memcmp`) then reuses the hand's estimate.
  Checked with a temporary build that ran both paths: over a million fits over `throw_slowmo_test.sh`'s four throws at full
  speed, in bullet time, and made slowly with `vr_throw_spin_from_pose 1`, bit-identical. The reuse was identical too.
  The fits took 25 ms where they took 33, and half of them are gone outside slow motion. The `throw main:` lines match
  the build before, at full speed and in bullet time.
- **Liquid volume** (`vr_water.cpp` `buildVolume`; 2.5%, every map load): up to 4M cells each ran `Mod_PointInLeaf`.
  `VolumeFill::fill` now sends boxes of cells down the BSP tree: a box all of whose centres lie on one side of a node's
  plane goes to that side, and a leaf fills its whole box. A box the plane may cut is halved; a single cell takes
  `Mod_PointInLeaf` itself. The plane test is done in double over the box's corner centres, with a margin of 1e-5 of the
  terms (the float test's rounding is a few 2^-24), so the cells are the same. Texel hashes (developer 1) were identical
  over 19 maps (e1m1-e4m5, start, dm3: 17 volumes). On one thread (`vr_jobs_parallel 0`) the first took 14.5 ms
  instead of 82.9, the largest 32 ms instead of 176, and the 17 together 198 ms instead of 946.
- **File listings while loading** (`vr_fscache.cpp`): every file written through the engine forgot every listing. The
  texture caches written in the background while a map loads had the main thread list the same directories again and
  again (`FindNextFileW`, `GetFileAttributesW` in the profile). Now `Sys_fopen` for writing, `Sys_remove`,
  `Sys_ReplaceFile` and `Sys_rename` forget only the file's directory (`VR_FileCacheForgetFile`, `VR_FileCacheForgetMoved`
  for both ends of a move), and only after the change, so a lookup between can't list the directory without it. That
  directory is known by its identity (volume serial and file index on Windows, device and inode elsewhere), not its
  name, so another spelling of it (relative, `.`, backslashes, case, the game folder's `quakevr` junction) is the same.
  A listing of a directory that wasn't there goes with any forget. Every listing still goes when that can't be told: the
  directory can't be opened, a directory is moved (`to` not a file), made (`Sys_mkdir`) or removed
  (`qvr::files::remove`/`removeAll` now forget too: one made again by the same name is another directory), and the map
  installer's and relighting's bulk calls. Other threads queue their directories for the main thread's next lookup.
  `vr_fscache_test [the game directory by another name]` (19 checks; the kit's run passes the worktree's `quakevr`, the
  game folder's junction target): a file written by another spelling, through the junction, removed, moved, in a
  directory made for it, on another thread, a directory moved, and another directory's listing kept. All pass. With the
  forget left out, 5 fail.
- **Prop sizes** (`vr_props.cpp` `ownScale`; 0.5%): Quake's grenades and the rocket in flight were told by four `strcmp`s
  on each `size()` / `drawnSize()`, many times a frame for every entity drawn or touched. A slot's kind is now kept per
  slot (`slotOwn`: found once the settings are registered and again with `slotCache` whenever a `vr_prop_id_NN` changes,
  as `Cvar_SetQuick` calls `onChanged` on every change; only read in between). A model without a slot is told by `modelmeta`'s identity (new `Id::Missile`; the grenades' ids were there),
  kept per model and emptied at a map change, a game directory change (after `Mod_ResetAll`, the only place a model slot
  takes another name) and a model reload. `Mod_FindName` never makes two models of one name. Every model drawn has its
  slot in `mod_known` (the ragdolls' `#rag` copies too), and `drawnSize` was already main-thread only (`slotForModel`'s
  caches), as `modelmeta` is. Checked with temporary builds comparing every call with the by-name answer: over 140,000 slot
  and 1.4M model calls across slot ID changes, `resetall`, e1m1, r1m1 and hip1m1 (game directory switches), identical;
  each of the four models loaded in each game gave the same kind both ways.
  `vr_modelmetadata_test` stops at "legacy path predicate" with or without this change (it fails on the build before too).

For VR: nothing should look or feel different. Throws, the caustics under water, and the grenade and rocket sizes should
all be as before.

## Content-addressed normal and height companions (adpbrload)

The generic `vr_pbr` loader (default 1, archived; next map) reads
`textures/pbr/<first32hex>_norm.png` for the diffuse actually selected by the renderer. The full source digest is
SHA256 of `qvr-pbr-v1` including its terminating NUL, width and height as unsigned 32-bit little-endian integers,
then row-major RGBA. Indexed images expand through the current source palette with alpha 255, including index 255;
RGBA replacements hash their pristine decoded bytes before texture upload. Texture names and map names are excluded,
so differently painted same-name textures cannot share companions, while identical art can share one.

Authored `_norm` RGB and nonflat alpha take precedence component by component; an authored `_bump` supplies missing
height. Companions can therefore be used without any diffuse replacement. The existing external pack keeps its
accepted normal RGB, and receives companion height only when its alpha is entirely 255. Its original green conversion
runs once before adding that height. Complete authored maps retain their original upload source and avoid a duplicate
generated GPU upload. Diffuse, cutout and fullbright handling stay in their existing paths. Animation surface selection
and normal mip generation use the existing engine machinery. No AD names or rules appear in shared engine code.

Merged sources have reloadable `pbrmix/` recipes containing filenames only; decoded images are temporary hunk data.
Unmerged companions load directly from their hash path. Graphics has Generated Companion Maps; Debug has Companion
Maps and Companion Map Tests. `vr_pbr_stats all` prints each queried hash and the selected GPU normal source with its
actual alpha range. `vr_pbr_test` checks the hash contract; `fixtures` also checks component composition.

Verified: kit build/QC/FGD/statics/precedence checks passed; engine tests 8/8; six GPU RGB/alpha hashes passed authored
normal-only, complete authored normal/height, height-only, generated-only, external normal-only and complete external
map cases. A video restart retained all 838 texture hashes (sum `ccd37408ad22e72f`) exactly. e1m1 exited 0; calibration
menu paths reported 0 missing. `Misc/quakevr/pbr_loader_test.py` stages and verifies the private fixtures with only
Python's standard library; `retire` moves its outputs under scratch without deleting them. Production AD measurements
and actual-eye comparison are recorded below when the companion bundle is integrated.
## AD PBR generation pipeline (adpbrgen)

`Misc/quakevr/pbr_generate.py` now scans installed PAK/loose BSP miptex data and loose image directories, deduplicates
canonical diffuse RGBA content, and produces `textures/pbr/<128-bit-source-prefix>_norm.png` companions. The loader
contract is SHA256 of `qvr-pbr-v1` NUL, little-endian uint32 width/height, then RGBA; indexed pixels use the raw palette
and alpha 255. No original diffuse textures, BSPs, palettes or model weights are exported or committed.

Full AD 1.8 corpus: 283 BSPs, 2455 eligible names, 2348 unique embedded diffuse hashes; another 218 hashes cover the
read-only Quetoo comparison images if used as external diffuse replacements. All 2566 outputs succeeded in 85.10 s
on RTX 4090, 145650884 PNG bytes (138.90 MiB). Embedded-source outputs account for 111.32 MiB; extra comparison-image
coverage is not necessarily drawn by AD. Canonical RGBA hashes distinguish 85 colliding embedded names; differences
in palette indices that expand to identical RGB appropriately deduplicate. 77 excluded names (326 occurrences)
include 16 sky and 56 liquid names, plus tool textures. 65 source hashes have cutouts; 315 have emissive pixels.

Pinned PBRify_Remix 1.7.2 NormalV3 and Height weights run directly through the same SPAN model loading stack used by
chaiNNer, via PyTorch 2.8.0+cu128/spandrel 0.4.1 (numpy 2.5.2/Pillow 12.3.0). The chain's Electron UI was not launched;
native 1x inference preserves diffuse art. Model/archive checksums and the complete repeatable workflow are in
`docs/vr-port/PBR_PIPELINE.md`. This also covers fresh PAKs, loose PNG folders, palette precedence, CPU/ONNX, refresh,
cache resume, validation, installation and Quetoo attribution. Roughness/spec conversion is omitted because the
engine's specular controls do not have an established physical conversion from learned roughness.

Generated maps have periodic 48-pixel context, cropped 256-pixel tiles, unit hemisphere normals, and global height
depth 0.25 (alpha 191..255). Cutout/emissive input colours are periodically filled before inference; generated holes
have flat relief. 224 animation hashes share a matching stationary family lead; moving patterns and different shapes
keep their own relief. Five ambiguous animation-family hashes conservatively use their own source. Authored
components are retained independently, with green handedness converted only when necessary. The engine's 0.5
detail-signature match rejects 116 unrelated same-name material associations; 188 outputs retain both authored
normal and height components, 169 requiring green conversion. A missing single counterpart is supported and tested,
although no such partial material occurred in the scanned corpus. Originals remain unchanged.

Validation: all 2566 file checksums, dimensions, generated normal units/hemisphere and depth bounds pass; maximum
generated normal length error 0.00653. Real-model periodic translation max float delta 0.000000268; changing inference
tile size on a random periodic fixture gave max 0.002092 (about half an 8-bit normal-channel unit). Ten focused tests
pass: PAK/loose precedence, name collisions, authored RGB/alpha/bump preservation, cache resume, model checksum
rejection, loose-image exclusion/dedup, indexed fences, stationary and scrolling animations. A complete rerun reused
all 2566 cache entries with zero failures. Required build and e1m1 mock smoke pass (exit 0). The companion engine-loader
worker validates actual GPU lookup and AD use after integration; these tools alone do not enable the runtime loader.

VR review: inspect AD brick/stone/wood, shallow angles and seams, fences, glowing buttons and animated/scrolling
materials. Painted shadows can still become learned relief; the upstream height model explicitly has this limitation.
Cleanup candidates: ignored `scratch/pbr-cache`, `scratch/pbr-sample`, `scratch/pbr-resume`, `scratch/pbr-tests-*`,
`scratch/pbrify172` and `scratch/env` when no longer needed. Keep committed companions and their manifest/attribution.

## AD PBR effective replacement coverage (adpbrgen follow-up)

Actual AD loader testing found that `textures/{speedbz_grid1.tga` is an RGBA diffuse replacement inside AD's pak0,
so its rendered hash differs from its BSP's indexed miptex hash. Source discovery now checks effective packed/loose
replacements automatically in each source game. Repeatable `--texture-game` adds game roots in highest-first engine
priority; the source game is appended when not listed explicitly. Lookup matches the engine: map-specific filename
before global filename, PNG then TGA then JPG across all roots, and numeric PAK/loose precedence within each root.
The selected replacement's RGBA, including transparency, determines its companion hash. Originals remain unchanged.

The resumed AD batch discovers 117 override occurrences and 71 new unique selected RGBA hashes: 2637/2637 outputs,
zero failures, 150637638 PNG bytes (143.659 MiB), 55.18 s. All previous 2566 outputs reused cache/migrated verified
bundle pixels and remained byte-identical. Discovery-only updates now retain inference through an explicit
`CACHE_VERSION`; model/runtime/parameters and material/animation identity still guard reuse. The migration checks
the old manifest's output checksum and inference configuration before copying previous output into the cache.

The reported fence replacement resolves to full source hash
`7006210e9aff42608264e18879428c87e6f76cb6dcc7c5604ccbf57e367a79e9`; its new companion is 64x64, 7142 bytes,
alpha 207..255, max normal length error 0.00492. Its cutout holes retain flat generated relief. Twelve focused tests
pass, including packed/loose precedence, map-specific versus generic lookup, PNG versus TGA across game roots and
discovery-only cache migration. All 2637 companion checksums/normal/depth checks pass; e1m1 smoke exits 0. The guide
now includes complete game-root ordering and automatic packed override discovery. The loader worker checks the
previously missing fence in actual AD after this follow-up ships.

## AD companion runtime verification (adpbrload)

The final bundle resolves every eligible selected diffuse on both tested real AD maps: ad_crucial 219/219 (219/222
including intentionally excluded clip/skip/trigger), ad_test3 95/95 (95/98 including those three tool textures).
The packed replacement `{speedbz_grid1` selects `7006210e9aff42608264e18879428c87`, GPU 64x64 with alpha 207..255;
ad_test3's `window03_d` selects the independently extracted expected hash `0039dc89b8b3640f8f06ea10bc8d6bdb`,
GPU 256x256 with alpha 194..220. Existing Quetoo relief remains selected where appropriate: e.g. twall1_1 still
uses `vrext/twall1_1_norm`, 192x384, alpha 23..185. Core engine hash checks pass 4/4 and calibration menu paths 0 missing.

Actual mock-headset before/after images use the same eye coordinates and angles, and both captures were checked
against blank frames. `scratch/adpbr_actual_eye_pair.png` shows the established lamp view at
setpos -215 -2287 136, mock pitch -24, yaw -35. All 227 diffuse GPU hashes are identical with companions off/on.
The close stone comparison `scratch/adpbr_stone_actual_eye_pair.png` uses setpos 577.362 1578.749 -558.066,
mock pitch 0, yaw -28.301, head flashlight; the head ray picks AD-specific rock_1732 in both views. Its active
normal source is `textures/pbr/d17ac75c6ca78698f69972a1f8b3a3bc_norm`, 512x512, alpha 205..228. In the left eye,
mean absolute RGB difference is 1.834/255 and 15.1% of pixels change by more than 3/255. The stone's learned relief
changes the lamp response while the source art and surface boundaries remain intact. Human headset review still
needs shallow-angle depth, seams, fences and animated materials (the existing AD texture relief checklist row).

One exclusive warm off/on sample at the lamp camera collected 2093/2092 frames over 8.5 seconds each. CPU busy
is 1.33/1.33 ms, host frame 1.40/1.40 ms, GPU 1.23/1.15 ms: no observed rendering cost, with the small GPU difference
treated as measurement noise. Managed texture memory is 508.7/510.4 MB (+1.7 MB); working set 1237.2/1230.5 MB,
private RAM 3388.1/3361.5 MB (process totals vary). Warm ad_crucial load is 521.4/583.9 ms (+62.5 ms); its first
load is 950.8/1274.0 ms (+323.2 ms). Companion lookup/decoding/upload work totals 123.1 ms across 277 calls in the
warm load; avoided procedural normal generation offsets part of that work. This is a single load sample, not a
claim of reproducible speedup or exact overhead. Logs and the repeatable script are `scratch/adpbr_metrics_*`
and `scratch/adpbr_metrics.ps1`. The final packed cutout follow-up preserves all previous output bytes.

Cleanup candidates: ignored `scratch/pbrtest_retired`, `scratch/pbrtest_ext`, `scratch/pbrtest-files.json`,
`scratch/adpbr_*`, private actual-eye screenshots and `quakevr/imagehash.txt`. No assets/maps or test-generated
config were committed; no melee or physics code changed.

## Arcane Dimensions: soldiers' own guns, upgrades per weapon, the gadget's AD page, P1b's loose ends (worktree `adweapons`, 2026-10-12)

His four asks (he can't test in VR now): the AD soldiers' own weapons, all of them, with many player-only tweaks to
balance them; upgrades belonging to weapons; the inventory/UI audit, the gadget's unused outer button paging to AD's
information; P1b's loose ends.

### The soldiers' own guns (QC `vr_ad_enemyguns.qc`, `vr_ad_guns_defs.qc`; engine `vr_adguns.cpp`)

- Eight new weapons, single use as the grunts' burst rifle (their clip their only ammo, AID_NONE, never refilled; empty:
  the click, the spent smoke and crackle, still a club; under the dropped-weapon cap): WID 20-27, IID 50-57, Hipnotic item
  bits 12-19. AD's rocket soldier drops his launcher (a rocket: a direct hit and a blast, AD's rocket model), the grenade
  soldier his (bouncing grenades on a fuse, set off by a monster they strike, catchable as yours), the plasma soldier, the
  eliminator and the centurion their plasma guns (AD's plasma bolts, launched as theirs: a hit and a small blast), the
  defender his shotgun (a spread of pellets, AD's projectile pellets where its rules apply), the pyro his flamethrower (a
  stream of AD's flame sprites: each flame's damage once, what it touches set on fire with Quake VR's burning) and the
  fumigator his poison sprayer (a stream of poison puffs: their damage, and a poison of its own on what they touch,
  damage a second for a while). AD's plain soldier and enforcer now drop Quake VR's burst rifle and laser rifle (their
  own kinds of gun) instead of the placeholder burst rifle for everyone.
- Damage is Quake VR's (T_Damage; AD's monsters take it through AD's damage with their ammunition resistance: rockets for
  the rockets, grenades and flames, cells for the plasma and poison, shells for the pellets).
- Every number of your use of them is a setting (`vr_adgun_<gun>_<what>`, 43 cvars, and `vr_adgun_drops`): damage, blast, speed, rounds and time
  between shots; the grenades' fuse; the shotgun's pellets and spread; the flamethrowers' fuel, reach, rate, burn time,
  poison damage and time; `vr_adgun_drops` 0: the soldiers drop nothing. VR Settings > Combat > Enemy Weapons > Arcane
  Dimensions' Guns (also from Advanced > Arcane Dimensions > Soldiers' Guns). Defaults: the AD monsters' own numbers where
  they read well for a player (rocket 25 + 30 blast at 800 u/s, 5 rockets; grenade 60 blast, 6; plasma bolts 18 + 15, 20
  to 30 bolts; defender 10 pellets of 4, 8 shots; flamethrower 3 a flame and 3 s of burning, poison 2 a puff and 6 a second
  for 4 s, 100 fuel at 10 a second). The monsters' own attacks are unchanged.
- Their models are AD's monsters' own guns, cut out of the player's AD monster models as they load (nothing of AD's is in
  our files): `vr_adguns.cpp` makes "<monster model>#gun<skin>" (`VR_DerivedModelFile`, as the limb models) from the gun's
  vertices (the ones `vr_monstermods.cpp` hides in the soldiers' deaths: the soldier's 463-548, the enforcer's,
  centurion's, pyro's and fumigator's 16-vertex gun, the pyro's hose left out) in the firing frame (`shoot5`,
  `attack5`: the barrel along the model's x), laid as our enemy guns are (the barrel along +x, the hand's place at the
  origin, 46 model units long) in the soldier's skin of that kind (one skin each: rocket 4, grenade 6, plasma 8; defender
  3, eliminator 6). Packed from the corner of the Quake VR gun whose settings they inherit: weapon slots 27-31
  (`vr_wofs_version` 43), the soldier's gun inheriting slot 21 (`v_gruntgun.mdl`, the same scale_origin: the same Offsets
  and hotspots hold it), the enforcer-mesh guns slot 22 (`v_enfrifle.mdl`, a corner 1.41 units further out on y: their
  Offset Y and hotspots moved for it); a slot's ID without the skin names every skin's gun
  (`weapons::slotForModel`'s `namesModel`). Their muzzles are their own (`vr_adgun_models` prints them). The models are
  made and precached as the soldiers spawn (`VR_ADGun_Prepare`: no hitch at the death); without AD's data the stand-ins
  are our burst rifle and laser rifle.
- Checked headless: `vr_ad_test 70` fires each gun from the main hand at a target: 8/8 PASS (rocket 25, grenade 44,
  plasma 18, defender 20 hitscan, eliminator and centurion 18, flamethrower 18 from 6 flames, poison sprayer 21 with its
  poison). Spawned dead AD soldiers (`vr_test_spawn 211/227/223/212/217/220/232/235`, `vr_test_spawn_dead 1`, `impulse 241`)
  each dropped its gun (`adgun: ... drops its ...`; each gun model made in ~4 ms). `vr_ad_test 71`: each dropped from a
  soldier's place with its model and full clip, 8/8. Screenshots of each held (side view by the grunt's and enforcer's
  guns): the same hold and orientation, the AD skins; a flamethrower's stream from its muzzle.
- For VR: hold each, check the hand on it (Weapon Offsets: slots 28-32) and its muzzle; fire each; balance on the page.

### Upgrades belong to weapons (QC `vr_ad_bridge.qc`, `vr_weaponinst.qc`, `vr_reload.qc`)

- An upgrade is the weapon's record's (`.wi_adup`: AD's Shadow Axe, Widowmaker, Plasma Gun bits), set by the upgrade's
  pickup on the weapon it gives (`AD_UpgradeGiven` in `weapon_touch`): it goes with that weapon through hands, holsters,
  drops, saves (the record is an entity) and level changes (`parm64`: 3 bits a weapon, the hands' then the holsters').
  A plain axe found later stays plain; a Shadow Axe and a plain axe can be held together. `AD_WeaponUpgraded` answers from
  the record (and the Weapon Upgrades scope); the Widowmaker's hip reload asks its record (`VR_Reload_ManualForInst`,
  `VR_Reload_ManualInHand`); AD's own "TSG pellets kill zombies" flag now from the firing weapon's upgrade.
- Older games (a player's own `.ad_moditems` upgrades, saves or level changes made before): put on every weapon of their
  kinds he carries as he spawns or loads (`AD_UpgradesToWeapons`), then his no more.
- Checked: the Widowmaker pickup upgraded the SSG in hand (`vr_ad_test 6`: code 2), a plain SSG cheated into the other
  hand stayed 0; after `changelevel e1m2` and after a save/load the same; `vr_ad_test 7` (an older game's three
  upgrades) put the Widowmaker on both SSGs and the Shadow Axe on the holstered axe. `ad_scope_test.sh`: upgrade 0/1 by
  scope as before.

### The gadget's Arcane Dimensions page (engine `vr_gadget.cpp`, `vr_gearlights.cpp`; QC `vr_ad_items.qc`)

- The audit: the main page already showed the artifacts' icons, only the soonest one's time, the custom keys' icons and
  the debuffs as blinking words, crammed into the level line; nothing showed the weapons' upgrades or each artifact's own
  time. The outer of the gadget's two lower buttons (unused; the inner one is the gear lights') now pages to an Arcane
  Dimensions page and back while AD's data is loaded (otherwise it does nothing): every artifact held with its own seconds
  left ("--": no end), the custom keys, the debuffs with the poison and bleeding still to come and the burning's
  seconds, and each upgrade's weapons carried and where (MAIN, OFF, BACK L/R, HIP L/R, CHEST L/R). The same press
  detection, cooldown and tuning as the inner button; a click and a tick. `vr_gadget_page` / `vr_gadget_page_press` for
  tests and flat play; `vr_gadget_test_state 6` fills it with made-up readings.
- New stats STAT_QVR_ADART1/2, ADDEBUFF, ADUPGRADES (QC `AD_HudUpdate`, 8 bits a reading).
- Left as they are (audited): the custom keys' names (only their icons: names are strings; the pickup's message names
  it), books (the hologram's centre print and AD's dark background), the jump boots' power level, coop.

### P1b's loose ends

- The worldspawn keys `give_weapons`, `take_weapons`, `no_axestart` on our hands and holsters as an AD level starts
  (`AD_WorldWeaponKeys`, on the parms the records are made from): no_axestart (not in Evil) starts with nothing, no
  upgrade; take_weapons takes those weapons from the hands and holsters (never the axe); give_weapons puts each one not
  carried into the first free holster (the hips', the chest's, the back's, then a hand), owned. `vr_ad_test 8` (take the
  shotgun, give the nailgun and rocket launcher) and `9` (no_axestart) on the current loadout's parms: as described.
- AD's sky changes as the engine's own messages (`ad_import.py`'s two new reviewed patches): a trigger_skybox's sky by
  svc_skybox to every client (AD stuffed "sky" to one), sent again after a load (`vr_ad_skysent`); AD's sky fog by the
  player's STAT_QVR_ADSKYFOG (saved with him; `gl_fog.c`/`gl_sky.c` take it over the map's: `VR_AD_SkyFog`), never the
  player's r_skyfog. `vr_ad_test 66`: in ad_crucial the green sky loaded, and again after a load (outside AD's campaign
  AD's sky files aren't on the search path, as before).
- AD's triggers and items as our entities' activator wherever AD's data is (`AD_SyncActivator`, `AD_ShareActivator`: AD
  maps only before), so AD content in other maps fires ours for the player.
- `ad_shots_test.ps1 -Full` is no longer flaky: mode 53's arm (a big pellet's box meets the body early) popped the head
  when the approach passed near it; the arm's approach is now looked for keeping the head 12 units off its line first.
  -Full 4/4 PASS (72 flight reports), the default run PASS.

Regressions: `ad_hits_test.sh` PASS, `ad_world_test.sh` PASS, `ad_scope_test.sh` as before, e1m1 smoke; QC 0 warnings.
## Native AD start sharp-filter height regression (adpbrload)

The reported lack of visible relief on AD's start map was reproducible with the author's default
`vr_texture_smooth 1` and `gl_texturemode GL_NEAREST_MIPMAP_LINEAR`. Indexed world loading omits
`NORMALMAP_HEIGHTS` when the diffuse uses nearest filtering. Content companions and file-authored world maps
inherited that flag, uploaded as RG8, and lost their height alpha even with normal mapping and parallax enabled.
Earlier loader validation explicitly used smooth=2 and therefore missed the default-setting regression.

Generated/composed companions and file-authored world maps now retain height alpha independently of the diffuse
filter. Procedural maps and model skins keep their existing caller flags; diffuse pixels/filtering, cvar defaults,
authored component precedence and external-map orientation remain unchanged. The fixture helper now documents
sharp/default filtering and temporarily backs up/restores production companions, allowing its GPU precedence test
to run alongside the shipped bundle.

Native reproduction uses the actual Steam Quake base directory and AD pak0/pak1/pak2, with the assigned worktree
as the final base directory for private QuakeVR resources and writes. `vr_campaign_native ad` mounts
`id1;hipnotic;rogue;ad;quakevr`; the author's Steam QuakeVR junction is never selected for writes. On start,
161/276 selected-source hashes find companions (the remaining selected replacements/tool textures are outside
this bundle's matching hashes). `rock2` selects `b7622e9fd27a2d1be6050d402d3dc493`: its active GPU alpha changes
from 255..255 before the fix to 205..220. `stone_grey_1_3` selects `7287bdce5a5494f227132536daf96dab`, GPU
512x512, and now retains alpha 206..236 instead of 255..255.

Off/on captures use identical normal/parallax settings, smooth=1, nearest mip filtering, fixed random seed and
the same eye coordinates/angles. All 324 diffuse GPU hashes are identical. The native spawn-view contact sheet
`scratch/adpbr_native_start_actual_eye_pair.png` has mean absolute RGB difference 1.203/255, with 14.44% of pixels
changing by more than 3/255. A nearby stone close-up at setpos -243.273 1034 443.642, mock pitch 0, yaw 90,
head flashlight, confirms `stone_grey_1_3` through the head ray in both views. Its contact sheet
`scratch/adpbr_native_stone_actual_eye_pair.png` has mean difference 3.457/255 and 32.34% of pixels changing by
more than 3/255. Both pairs were inspected and are nonblank; no diffuse smoothing is needed to see the relief.

Validation: full kit build passes; engine hash/composition checks 8/8 pass; actual GPU authored/generated
precedence 6/6 passes at sharp/default filtering, including authored RGB plus generated height, complete authored
RGBA, authored bump height and external normal-map precedence. Production companion backups were restored
byte-for-byte (no bundle Git changes). Stock e1m1 smoke with default sharp filtering exits 0. Latest published
next 4e9ec1b8b is merged. No melee/physics code changed and no author checkout files were modified.

## Native Steam AD HD source coverage (adpbrgen)

After merging next 65561c5f6, generation was resumed against the actual Steam AD installation and the native
highest-first texture roots: read-only author quakevr, Steam AD, Steam rogue/kit rogue, Steam hipnotic/kit hipnotic,
Steam id1/kit id1. The previous AD-only roots missed lower-root HD PNGs selected by the engine. Existing automatic
replacement discovery handles the full effective path order; no per-texture exception or generator inference change
was needed. `PBR_PIPELINE.md` now gives a reusable native Steam command including the fallback mission-pack/id1 roots.

283 BSPs and 1305 selected replacement occurrences yield 2987 unique companion sources. All 2987 outputs pass,
zero failures, 153.84 s; 350 new source hashes and all 2637 previous PNGs cache-reused byte-identically. Total PNG
size is 340304475 bytes (324.54 MiB). 335 outputs retain complete authored normal/height components; the original
diffuse and author material files remain unchanged. Existing attribution applies to authored-derived channels.

Saved native-start inventory coverage is 272/276, which is every eligible selected source. Only the documented
clip, skip, trigger and constant-black exclusions remain. Black is intentionally excluded as a flat/tool material
(`a4370418ab1010b8059358593341173c`); it has no inferred relief to gain. The 15 animation and other actual HD material
misses are now covered through source hashes, rather than name-based substitutes.

Two independently checked examples are native Steam id1 loose 512x512 PNGs: `metal1_3` full canonical hash
`d2d7d2ac1eff6da463b3df175bf1bba294e38ac6783c4e575333e3691bafee71`, generated alpha 208..218 (674627 bytes), and
`wizwood1_5` hash `f6fde43fb7f4f2a7c5f6ed8e0351f37ae344f73e174dc8c50fd0abffa714ed52`, alpha 202..224 (510243 bytes).
Both hashes match the native engine inventory exactly. Max generated normal-unit error over the whole bundle is
0.00653. Twelve targeted pipeline tests, all 2987 checksum/normal/depth checks, required build and e1m1 smoke pass
(exit 0). The loader worker verifies actual native GPU selection after these assets ship; its sharp-height filtering
fix is already included in the merged engine. No source diffuse, maps or model weights were distributed.

## Generated-channel provenance emission (adpbrgen)

Merged origin/next 2ce263a14 (including calibrated generator 94e3b6bdb and PNGs) and added emission of
`textures/pbr/provenance.txt` on each 50-source checkpoint,
completed generation, refresh and cache resume. The ASCII header is `qvr-pbr-provenance-v1`; sorted entries hold
source prefix32, output canonical SHA256 hex64, and independent decimal generated-channel bits (1 normal RGB,
2 height alpha). Output canonical hashing uses `qvr-pbr-v1` plus NUL, LE uint32 width/height and decoded RGBA,
retaining alpha height. The index therefore binds flags to exact output pixels and dimensions, allowing the engine
to conservatively treat missing/stale bindings as authored. `provenance --output quakevr` rebuilds metadata offline
only after verifying every manifest PNG checksum; no model load or inference is needed. `validate` now checks it.

Rebuilt the shipped index from all 2987 current companions without changing any PNG, census or manifest: 2652
entries flags 3 and 335 entries flags 0, 298722 bytes. Index SHA256:
`2636e45d509ddfb3b9055ae9d2177f81872febfd2ec1235760ad88b661c11531`.
Eighteen pipeline tests and the calibrated generator self-test pass, covering independent alpha/dimension hashing,
all four flag values, stale file refusal,
exact full shipped-index reconstruction, cache resume, offline refresh, checkpoint counts 50/51 and cache-only
source removal (50 retained, zero inference). All 2987 full checksum/normal/depth/index validations pass. Required
QC/FGD/engine build succeeds and hidden mock e1m1 smoke exits 0; no melee or physics code changed.

The engine worker owns generated relief settings and VR verification. Four requested box sources all have flags 3:
`ef2fac7a255961d0b9f06d41d1d16036` and `79469e258e12ccef0fd24caac20db7c5` are 32x64, alpha 128..255;
`9cf100e79ecce5eadd1a8572f78ec291` and `7f42379bf902b036f40ac764027ee7da` are 32x32, alpha 128..255.
This corrects the initial index emitted before the calibration merge; that uncalibrated index is superseded.
Relative to origin/next, the PNG bundle, census and manifest are byte-identical: only provenance, its tests and
emission notes differ. The alpha mutation fixture flips one byte so it also exercises calibrated flat output.
Disposable worker artifacts are `scratch/provenance_audit.py` and new `scratch/pbr-tests-*` fixture folders.

## Generated world relief calibration (adpbrload)

Loading the correct height map was insufficient for the reported flat appearance. The previous generated packing
multiplied the model's absolute brightness by a 0.25 bound, retaining a large uniform recess and very little
relative relief. Native HD metal1_2 had only 0.078 Quake units of height variation at the author's depth 2.5;
metal1_3 had 0.098, and the embedded city5_7b trim 0.157. Increasing a global depth mostly slid the whole image.
The desktop and mock-VR world shaders both walk the height field; no separate VR permutation bypass was found.

Generated height now anchors its learned 95th percentile at the surface, maps its 5th..95th percentile span to a
bounded relative range, limits contrast gain to 8, and leaves spans below 0.02 flat. Cutouts do not set the range.
The generator's --depth default is 0.5, at most 1.25 units of recession at vr_parallax_depth 2.5. Cache version 2
invalidates the old packing; normal green orientation still uses its previous height convention, independently of
this calibration. No engine cvar or user rendering setting changes. Complete authored alpha bypasses calibration.

All 2987 native-root sources regenerate offline, zero failures, 363963231 bytes. Pixel comparison verifies all
2987 normal RGB maps unchanged, all 335 authored alpha maps unchanged, and exactly 2652 generated alpha maps
changed; 55 generated predictions are now intentionally flat. Differential depths: native HD metal1_2 0.775,
metal1_3 1.127, embedded city5_7b 1.118, stone_grey_1_3 1.245, and embedded city5_3 0.706 units. Original diffuse,
material source files and maps remain untouched; only existing content-addressed companions and manifest change.

The native selected city5_3 source is e2d910f45c5220ce03c85512a79cab61d372b10db4061fb472815c05dc14b822,
Steam id1/textures/city5_3.png. Its authored normal and genuine height alpha come from
quakevr/textures_quetoo/city5_3_norm.png (SHA256 f656efed2fec4f6a3b23c1cb0ec4db45fec5bd537c391212414927853c0b05a4):
256x256 RGBA, raw alpha 61..211. Its output alpha is preserved, unlike embedded d088fa06acb2d8c6fc4366f3d046a904,
whose authored match is rejected at 0.01064. The preserved native authored source still spans 1.471 units; this
change must not be presented as a fix to authored stone relief or as proof that every wall looks sculpted.
Native GPU selection confirms vrext/city5_3_norm, 256x256, alpha 61..211. Its wall-only ROI under author
settings changes by 7.1525/255 with parallax off/on (79.18% pixels over 3), independently of generated calibration;
the calibration-off authored ROI is pixel-identical. Thus authored UV displacement is active, rather than suppressed
by a VR/retro draw flag. The cropped proof is scratch/relief_authored_city5_3_uv.png.

Lighting-independent native wall comparisons retain the author's exact cfg (sharp filtering, retro block 0.5,
normal maps/parallax/depth writing) with r_fullbright 1. Original and calibrated parallax-off brick wall ROIs are
pixel-identical; all 1093 diffuse GPU hashes match across original, relative-span 4x/8x and calibrated runs.
At setpos 384 440 344, eye pitch -10/yaw 140, the cop1_1r/brick view's calibrated off/on ROI differs by 3.845/255,
49.50% of pixels over 3. At the closer grazing setpos 384 484 344, pitch 15/yaw 155, the head ray confirms
city5_7b: local trim/mortar bends more, but the appearance remains restrained. The copper colour pattern still
reads fairly flat; learned heights can follow colour stains rather than structural tile boundaries. Real-headset
judgment and any authored-height override are outstanding, separate choices. Evidence is in scratch/relief_wall_all.png,
scratch/relief_close_wall.png, and scratch/relief_stone_wall.png. No global authored-depth inflation is included.

Validation: required QC/engine/FGD build passes; 14 targeted pipeline tests and the offline self-test pass;
all 2987 checksummed companions validate. Native desktop vr_enabled 0 captures parallax off/on and exits 0;
stock e1m1 smoke at sharp defaults exits 0. No melee or physics code changed. All native reads use Steam data plus
the worker base directory for private output; no author checkout or process changes.
## Generated-channel provenance emission (adpbrgen)

Merged next 2a60c240d and added generator emission of `textures/pbr/provenance.txt` on each 50-source checkpoint,
completed generation, refresh and cache resume. The ASCII header is `qvr-pbr-provenance-v1`; sorted entries hold
source prefix32, output canonical SHA256 hex64, and independent decimal generated-channel bits (1 normal RGB,
2 height alpha). Output canonical hashing uses `qvr-pbr-v1` plus NUL, LE uint32 width/height and decoded RGBA,
retaining alpha height. The index therefore binds flags to exact output pixels and dimensions, allowing the engine
to conservatively treat missing/stale bindings as authored. `provenance --output quakevr` rebuilds metadata offline
only after verifying every manifest PNG checksum; no model load or inference is needed. `validate` now checks it.

Rebuilt the shipped index from all 2987 current companions without changing any PNG, census or manifest: 2652
entries flags 3 and 335 entries flags 0, 298722 bytes. Index SHA256:
`533f12b5fca6069aa1ea14d557fedd2ac6c2ea115a4e72d23bff8733b5c16358`.
Sixteen pipeline tests pass, covering independent alpha/dimension hashing, all four flag values, stale file refusal,
exact full shipped-index reconstruction, cache resume, offline refresh, checkpoint counts 50/51 and cache-only
source removal (50 retained, zero inference). All 2987 full checksum/normal/depth/index validations pass. Required
QC/FGD/engine build succeeds and hidden mock e1m1 smoke exits 0; no melee or physics code changed.

The engine worker owns generated relief settings and VR verification. Four requested box sources all have flags 3:
`ef2fac7a255961d0b9f06d41d1d16036` and `79469e258e12ccef0fd24caac20db7c5` are 32x64, alpha 203..222;
`9cf100e79ecce5eadd1a8572f78ec291` and `7f42379bf902b036f40ac764027ee7da` are 32x32, alpha 204..225.
Disposable worker artifacts are `scratch/provenance_audit.py` and new `scratch/pbr-tests-*` fixture folders.

## The super shotgun closed by B/Y (accessibility, 2026-10-12)

A user's accessibility request: `vr_reload_ssg_close_button` (off by default; Advanced > Weapons > Reloading > Super
Shotgun > Close by B/Y): open, the B/Y of the controller holding it snaps it shut, as Open by B/Y breaks it open, so it
opens and closes with the button alone (QC `VR_Reload_SsgFrame`, the same press edge and 0.35 s between toggles).
`reload_test.sh`: a new check (off: B/Y again leaves it open; on: it shuts it), 104 PASS, self-test 67/67.


## Generated-only relief strength and stock explosive-box materials (adpbrload)

vr_pbr_strength is archived, defaults to 1 and clamps to 0..8. Generated Relief Strength is live under
Advanced VR > Graphics > Surfaces and Options > VR Settings, normally 0..4 with extended range to 8.
It multiplies generated normal tilt and generated parallax depth independently, after the actual map
(including animation/external-pack overrides) is selected. Authored components keep multiplier 1, including
mixed maps. Alias-model maps retain their existing controls. No global normal/parallax depth setting is inflated.

The loader verifies textures/pbr/provenance.txt against the selected decoded companion's canonical RGBA SHA256,
including dimensions and alpha. Missing/stale/unknown provenance stays at 1. The calibrated index SHA256 is
2636e45d509ddfb3b9055ae9d2177f81872febfd2ec1235760ad88b661c11531, with 2987 records: 2652 generated
normal/height companions and 335 complete authored companions. Runtime composition clears flags independently
when an authored component replaces them. Existing per-draw extmat x/w carry normal/height multipliers; y/z
retain external sheen/hardness. Solid shading and its depth prepass use the same multiplied parallax depth.

Both stock explosive-box brush families shared exact diffuse hashes with AD world textures, so the world
corpus displaced their established procedural relief. Side hash ef2fac7a255961d0b9f06d41d1d16036 and top
9cf100e79ecce5eadd1a8572f78ec291 match, as do their alternate frames. Both models now retain their original
material loader and authored file/pack overrides; PBR also leaves their external maps' missing height alone.
Sharp-filter GPU side alpha returns to 0..255 and top to 43..255, versus generated companions' 128..255.
Other world uses of those hashes still get PBR.

A private e1m1 entity fixture places both box sizes together. Fixed random seed, identical eye/angles, head
flashlight, normal strength 1, depth 2.5, items 1.5, smooth 1 and nearest mip filtering isolate the regression.
Interior face differences from the pre-PBR procedural baseline fall from 10.23476 to 0.08201/255 (large) and
5.24991 to 0.16307/255 (small). scratch/boxes_restored_proof.png shows baseline/regression/restoration; nearby
world PBR and lighting explain remaining image differences. No id map/assets from this fixture are committed.

Native Steam AD start confirms city5_7b at eye 384,485.01,375.54, pitch 15/yaw 155, active alpha 141..255.
With author sharp/retro settings and fullbright isolating displacement, strength 1 versus 3 changes the trim/wall
ROI (320,360)-(970,625) by mean 8.83214/255; 78.812% of pixels change by more than 3.
scratch/pbr_strength_native_pair.png was inspected and shows visibly deeper local relief. At strength 3,
authored city5_3 remains normal/height 1/1, vrext/city5_3_norm alpha 61..211. This proves an adjustable effect,
not physical accuracy of inferred shapes; default 1 retains the calibrated appearance.

Validation: required QC/engine/FGD build passes; engine hash/provenance/component tests 13/13, composition
fixtures 17/17, active generated/mixed/authored GPU strength 6/6 and uploaded component precedence 6/6,
including vid_restart under sharp filtering. Stock-box material checks pass 20/20. Native captures and e1m1
smoke exit 0. Calibration queries {menu:Graphics - Surfaces>Generated Relief Strength} and
{menu:VR Settings>Generated Relief Strength} each find 1 and miss 0. Current next c48c71fd9 is merged,
preserving calibrated generator/index and shotgun changes. No melee/physics or author files changed.

VR follow-up: try strength 1..3 on generated AD masonry and check both stock explosive-box sizes close up.
Authored walls, hands and weapons should retain their relief. Cleanup candidates: ignored scratch/pbr_strength*,
scratch/boxes*, private pbr_boxproof.bsp, fixture backup/retired evidence, private eyeshots/imagehash and the
retained renderer integration stash. No files were deleted.
## Arcane Dimensions' weapons in any map, and in the toolgun (worktree `adweps`, 2026-10-13)

His ask: AD's weapons, the player's and the ones its enemies drop, spawnable in non-AD maps by default and in the
toolgun's menu.

- **The gate is the existing one**, Additional Content (`vr_ad_everywhere`, 1): with AD's data installed, AD's
  weapons spawn in any map that places them (a custom map, the toolgun, the Debug spawner). No new cvar.
- **The upgrades** (Shadow Axe, Widowmaker, Plasma Gun; `weapon_upgrade_axe/_ssg/_lg`, AD's aliases `weapon_hammer`,
  `weapon_plasmagun`): Weapon Upgrades' 1 is now "With AD Content" (`vr_ad_upgrades`, still 1): AD maps, and any map
  while Additional Content is Everywhere. An upgraded weapon is AD's content and keeps its rules wherever it is
  (decision 9), as its upgrade is the weapon's own (decision 38). Every question asks QC `AD_UpgradeScope()` (the
  pickups' spawn, `AD_WeaponUpgraded`, the gadget's page; the engine's `ad::spawnAvailable`). 2 any map, 0 none, as
  before. With Additional Content at AD Maps Only, 1 is AD maps only, as before. AD's grappling hook pickup stays
  removed (Quake VR's hook is the player's own).
- **The soldiers' guns** (WID 20..27): `func_weapon_grabbable` with "weapon" 20..27 (a map's, the toolgun's) lays one,
  full; `VR_ADGun_Placed` makes its model and precaches its sounds first (`AD_WeaponsOnDemand`: the sounds AD's
  soldiers and world precache where they are; precache_sound may come late in our progs). A map's
  `func_weapon_grabbable` of one needs no AD rules; without AD's data it is our stand-in gun.
- **The toolgun**: the Weapons tab has Arcane Dimensions' group from a fresh page (the three upgrades, then the eight
  guns; `vr_ad_spawnguns.inc`, generated by `ad_monsters_gen.py`'s new GUNS table); every tab's AD entries start on
  their own page now (the panel pads the page before them). The guns' thumbnails and ghost come from the derived model
  (`vr_toolgun_icons.cpp`, the ghost's `.mdl#` check). The Debug spawner (Debug > Tests > Arcane Dimensions Monsters,
  its Pickups) has the guns as 312..319; the upgrades' 309..311 now follow the scope instead of AD maps only.
- **A fix met**: the Shadow Axe's pickup asked the axe's ammo (none) in `weapon_touch`: two assertion prints on every
  take, in AD maps too (items.qc, `// AD support`).
- **Tests** (headless, fast): `+grabmain; vr_ad_test 81; -grabmain` makes each of the 11 ahead, takes it into the main
  hand and fires once: 11/11 PASS in e1m1, mge1m1 and ad_crucial (Shadow Axe blow x1.75, Widowmaker 3 shells, Plasma
  Gun 1 cell, each gun a round of its clip). `vr_ad_test 82` lays them all and puts a Widowmaker in hand; after save
  and load (`83` reports, `84` fires the hand): e1m1 and mge1m1 8 guns with their models and rounds, 3 pickups, the
  Widowmaker's upgrade 2 and its 3-shell shot. The toolgun (tiles 99..109 chosen, the trigger): 11 spawned, 8 guns
  and 3 pickups counted; the panel's AD page draws every thumbnail (`vr_toolgun_icons`: 0 failed). Scopes: Additional
  Content 0 in e1m1: 0 of 64 AD tiles ready and none made; Weapon Upgrades 0: 3 disabled, the guns still made.
  e1m1 and mge1m1 monster totals unchanged (23; 90 + 20 waiting), no AD spawns there; `ad_scope_test.sh`: as before
  but "upgrade 1" at Weapon Upgrades 1 in e1m1 (the change above); e1m1 smoke.
- **Seen, not fixed** (not AD's): a weapon pickup taken 32 units ahead was taken again after a load (its hidden
  pickup re-touched: +3 shells); at 400 units it was not.

## The main menu centred, the VR banner beside the buttons (menucenter, 2026-10-13)

The author's note: the VR main menu looked misaligned and cluttered (the list right of centre, the vertical Quake VR
banner under the shortcut column), and the flat main menu was misaligned too.

- menu.c `M_Main_Shift`: the main menu's rows, their spinning cursor and (flat) the banner move left so the span from
  the cursor's column to the widest row's right end is centred under the title (x 160); never right of Quake's place,
  never left of x 0, nor so far that a flat screen's banner leaves the canvas (it moves with the rows). Measured with
  `VR_BigFont_WidthScaled` (new, vr_bigfont.cpp: the lettering's width without drawing) or the picture's width.
  `M_ContentLeftBelow` follows the shift (the laser/corner-button split). The mouse picks rows by y only, unchanged.
- vr_menubrand.cpp `VR_MenuDrawBannerColumn`: with `vr_menu_banner_beside 1` (new, default 1; VR Settings > Menu >
  Banner Beside the Buttons) the VR style's banner stands in the panel's margin left of the corner buttons, its top
  level with the first button's (`menuui::toolbarTop`, new). 0, or a margin too narrow for `columnMinHeight`: under
  the buttons as before. `VR_MenuBannerExtent` gives M_Main_Shift the flat banner's span.
- `menu_vr pos` prints the VR banner's place ("left of" / "under the buttons") and `main rows x a..b (middle m)`.

Numbers (lettering rows): rows x 19..300 (middle 159.5; before 54..335, middle 194.5) in VR and flat; VR banner
x -336..-266, y -216..0, buttons from x -258 (before: x -232..-162 under them, y 52..268). Flat banner x -28..18 (was
7..53). Picture rows (`vr_menu_bigfont 0`): rows 30..290, middle 160, flat banner -17..29. Mock laser at (25,45)
picks VR Calibration, (290,150) Single Player; flat mouse the same rows. `vr_menu_path_check` 0 missing; e1m1 smoke
exit 0.

VR check: the main menu's list centred under MAIN; the banner left of the shortcut buttons, not cut off, clear of
them; the laser still picks the row it points at, including the left end of the long rows.

## Piled-up blood: a mark limit a spot (worktree `decalcap`, 2026-10-12)

The author's note: blood marks piled up where a corpse is cut apart in one place drop the frame rate a lot when looked
at. Each pixel of the world walks every mark listed in its 32-unit grid bucket (vr_decals.cpp, the world shader;
PERF_DECISIONS.md 15), and a bucket listed up to 255 (its header's 8 bits) since "Pools piled up: no square holes".
His decision: option C now, a configurable limit a bucket.

- `vr_decal_bucket_max` (default **32**, 1-255, archived): how many marks a bucket lists. The rule of "no square holes"
  stays: newest first, a mark is listed only if every bucket it is in has room, so past the limit the oldest of a pile
  go whole. Changing it lists the grid again (VR_DecalsFrame compares it with the limit the last grid was made under).
  Menu: Advanced VR Settings > Gore, Marks (with Max Decals; Graphics links there), "Decals Piled in One Spot"
  (8-255, steps of 8), with help text.
- `vr_decal_count` prints the limit in use: "N over L marks (M marks left out whole)".
- Bench scenarios `decals_pile` (32) and `decals_pile_255`: 768 marks (`vr_decal_stress 64 32 cluster` x 12) on one
  spot 96 units ahead, looked at (`vr_mock_look 35 0`).

Measured (eyes 2048, `run.sh --exclusive`). bench.sh's separate runs were too noisy to read (GPU clocks: paced 90 Hz
world+brush 3.3 vs 3.8 ms, unpaced 1.6-3.6 vs 3.6-6.3 ms). Interleaved in one process instead (the `decals_pile`
set-up, then 300-frame windows alternating 32 / 255, 4 each, two runs):

| run | 32: world+brush ms | 255: world+brush ms |
|---|---|---|
| 1 | 2.72 1.55 1.46 1.52 | 6.19 6.36 3.59 3.36 |
| 2 | 3.24 3.35 2.76 3.24 | 6.10 6.12 5.62 6.12 |

255 costs +1.8 to +4.8 ms of world pass in every pair (whole 3D refresh: 2.7-6.1 vs 4.6-9.0 ms). Grid (`vr_decal_count`,
768 marks on the spot, 62 buckets occupied, up to 560 marks reaching one): 255 leaves 347 marks out whole, 32 leaves
669, 8 leaves 727; 300 clamps to 255. Screenshots (eye 1024): at 255 the pile is a dense dark pool; at 32 it is thinner
(single splatters with floor between), with whole marks and no cell-edged holes.

For option A later (baking old marks into per-surface blood layers): the marks a limit leaves out are exactly
`worldMarkListed[k] == 0` after the grid's listing pass (newest first), so a bake would take those, the oldest of each
pile, and nothing listed changes. A pile of 768 marks on a 64-unit spot reaches about 62 buckets (the parallax reach
adds cells round it), so one pile touches only a few surfaces: a per-surface layer needs only those, at a much finer
resolution than the lightmap's 16 units a texel (the splatters are a few units across). The shader already projects
each mark onto the surface along its normal and follows the parallax relief; a baked layer has to be sampled in the
same space (the surface's texture coordinates after the parallax offset) or the baked and live marks will slide apart
at grazing views. The fading over the last five seconds of `vr_decal_life` would need the layer to fade too (or the
bake to happen only for marks with a long life left).

VR check: cut a corpse apart in one spot (or `vr_decal_stress 64 32 cluster` a few times, looking at the floor ahead),
look at the pile with the profiler panel; try the slider at 32 and 255 and see the cost and how thin the pile looks.

## The vore never shoves; shoves, slaps and bashes draw no blood; his parry/bash tweaks as defaults (shovefix)

### The vore never shoves

His report: with `vr_enemy_shove_vore 1` the vore never shoved him, only threw balls, even touching it. Two causes
(vr_enemyshove.qc, ai.qc ai_run):
- `VR_EnemyShove_Ready` read `pain_finished` as "staggered". For a vore it is its 3 s pain-sound cooldown (its pain
  frames last 0.5 s and the check never runs in them): any hit of yours, or its own ball's blast up close, kept it
  from shoving for 3 s while it went on firing. A vore now ignores `pain_finished` there.
- During `vr_enemy_shove_delay` (and while turning to face you) the check returned FALSE and `ai_run` went on to
  `CheckAnyAttack`, whose ball (about 90 percent a frame up close) always came first. A vore in reach, its cooldown
  over, now holds its fire (`vr_eshove_holdfire`): it turns to you until the shove starts. During the cooldown it may
  still fire.

Test: `Misc/quakevr/voreshove_test.sh` (vore spawned touching you, walking into it ~20 s): 2 shoves before, 8 after.

### Shoves, slaps and bashes draw no blood

His report: shoving enemies made them bleed, and slaps and parry bashes too; they are meant as bloodless hits.
`VR_HitBloodless()` (combat.qc: hit kind SHOVE or BASH, or a MELEE slap) now skips `VR_Gore_Hit` (the sprays on the
walls), `VR_Wound_Hit` (wounds painted on the model), `VR_SmallGib_Hit` (it already skipped slaps) and, in
`VR_Bash_Hit`, the blood splash (wood still splinters, metal still sparks). Damage, knockback, stagger and knockdown are
unchanged; a kill still leaves its pool of blood (death, not the blow). Headbutts still bleed (not asked).

Test: `Misc/quakevr/bloodless_test.sh` (`vr_smallgibs_test 23`, also Debug > Gore Tests: 10 shoves, bashes, slaps and
punches on a grunt). Before: shove and bash 10 gore hits, 10 wound events, 10 blood particles; slap 10/10/0. After: all
0, damage still dealt (103, 100, 250); the punch control 10/10/10. Melee eval: canary 36/45, 0 differences before;
full 168/177, 0 differences after.

### His parry and bash tweaks are the defaults

Compiled in (vr_cvars.inc) from his config: `vr_bash_speed` 2 -> 3, `vr_shove_speed` 2.4 -> 3.5, `vr_bash_sound` 1 -> 0.7,
`vr_knockdown_bash` 0 -> 1, `vr_parry_push_enemy` 1 -> 0.55, `vr_parry_push_player` 1 -> 0.8, `vr_parry_wobble` 1 -> 0.7,
`vr_parry_drop_chance` 0.5 -> 0, `vr_parry_stamina` 0 -> 1, `vr_parry_stamina_cost_2h` 12 -> 10. All but the two speeds
were already in `quakevr/vr_defaults.cfg`; those eight lines are gone from it (now equal to the compiled-in values, as
`vr_savedefaults` would write it). Config version 129 (vr_cvars.cpp) moves a config still at the old shipped bash or
shove speed (2, 2.4) to the new one; the other eight shipped at their new values already (vr_defaults.cfg), so a config
at an old compiled-in value there was set by hand and keeps it (review fix: 129 first flipped those too). Test: `Misc/quakevr/config129_test.sh` (PASS). Melee eval, full set: 168/177,
one FAIL -> FAIL take whose one-hand bash push is now seen at 0.391 s, not 0.375 (the firmer 3 m/s bash speed).
e1m1 smoke clean; the vore (6 shoves) and bloodless tests pass with the new defaults.

## Crouched, slower and silent (worktree `crouchspeed`, 2026-10-13)

The author: crouched, the most speed drops to about 0.75 of normal, and stays under the speed at which unalerted
monsters hear your steps. `vr_crouch_speed` (1, on) and `vr_crouch_speed_scale` (0.75; 0.2-1), on Locomotion
(Settings > Locomotion: Slower When Crouched, Crouched Speed). New cvars, so no config migration.

- **The threshold.** The stealth AI's footsteps (vr_stealth.qc `VR_Stealth_PlayerFrameNow`): on the ground, horizontal
  speed over `vr_stealth_run_speed` (280), a step noise (`vr_stealth_noise_run` 400) every 0.25 s. Running is
  `sv_maxspeed` 320, so 0.75 gives 240, under 280 as asked. Whatever the scale, the crouched most speed is also capped
  at 90% of `vr_stealth_run_speed` (252 by default; it follows that slider), so crouched is silent at any setting.
- **Crouched** is the engine's crouch (vr_hull.cpp, the crouched hitbox's heights, measured whether or not
  `vr_crouch_hull` is on): eyes under the tallest crouched box's top (52 with `vr_crouch_height` 36 / `vr_crouch_step` 8;
  the mock's standing eyes are 56.6 over the feet), fully slowed 8 units lower (44), between in proportion. There is no
  crouch button in the port. The slowing's share eases at 4/s (0.25 s from none to all), so crouching mid-run slows
  you at 320 u/s² (Quake's ground friction alone is about 1100) and standing gives the speed back the same way.
- **Where.** One call, `VR_CrouchSpeedMax` (vr_hull.cpp), in SV_AirMove after stamina and carrying (sv_user.c). It caps
  only the stick's wish speed: walk, run, the speed button, any hand-steered move. Jumps and falls (air control is
  capped at 30 anyway), knockback, movers and swimming are untouched.

Test: `Misc/quakevr/crouch_speed_test.sh <agent>` (start map, speed button held, a grunt 150 units off in the dark,
`vr_stealth_debug 2`): crouched (head 0.9 m) top speed 240, no step heard; crouched at scale 1: 252, unheard; standing
320, heard ("hears noise 1"); `vr_crouch_speed 0` crouched 320, heard. 4 PASS. e1m1/e1m2 smoke clean.

## Magazine re-pull and Reinsertion Delay (magfix2, 2026-10-13)

Author notes: (1) a magazine pulled out, put back in and pulled again without letting go of the grip would not come out
a second time (it took a fresh grip); (2) besides the reinsertion clearance, a short time after a magazine came out (or
knocked the old one out) it should seat even if kept close, "maybe 0.2 or 0.3 s", with a slider next to the clearance.

(1) vr_reload.qc: VR_Reload_SeatMagInto marks the hand that seated the magazine it held (.vr_mag_rehold, bit 1 off,
2 main). The next VR_Reload_MagFrame of that hand consumes the bit and, if it still grips and is empty, latches the
magazine hold at once (no fresh grip edge, no hotspot/reach test), logging "(still gripping it as it seated)". A pull
then takes it out as from any hold. Cleared on death.

(2) vr_reload_reinsert_time (default 0.25 s, archived; 0: by the clearance only): VR_Reload_HeldFrame also rearms a
fresh held magazine (r.vr_ammo_wid set; shells unchanged) once that long has gone since VR_Reload_MarkFresh (pulled
out, or the bumping magazine that knocked one out). The well check is unchanged, so it still seats only within the
insertion reach. Loose magazines keep the eject cooldown and the clearance. Menu: Advanced VR Options > Weapons >
Reloading > "Magazine Reinsertion Delay" (0..1 s, step 0.05, extended to 5), right below Magazine Reinsertion Clearance.

Tests: reload_test.sh section 8 adds the out/in/out/in/out never-let-go run (3 out, 2 in, 2 re-holds) and the bumping
magazine kept at the well with Delay 0.25 (out at 0.14 s, seated by 0.32 s) and 0 (stays out); vr_reload_test 28 adds
3 direct checks (21 passed). The self-test sets the cvar with sprintf("%g"): ftos keeps one decimal (0.25 -> "0.2").
Whole reload_test.sh: 108 PASS, 0 FAIL (an earlier run had a flaky lying-shell pickup and the Firing Animation Speed
timing, both unrelated and passing on the rerun).

VR: pull a magazine, push it back without letting go, pull again (repeat); bump a magazine out with a new one and hold
the new one at the well: it should seat after about a quarter second, not at once. Tune Magazine Reinsertion Delay.
## Pouch reloading (worktree `pouchreload`, 2026-10-13)

The author: reload a gun by bringing it to the ammo pouch, above all the super shotgun broken open (two of them
dual-wielded with manual reloading: no free hand to fetch shells), and most guns where it makes sense; the receiver
should touch the pouch's drawn rounds at an angle that matches them, the super shotgun at any angle; per-gun toggles
and leniency settings (spec and his answers: the kit's scratch/POUCHRELOAD.md). Immersive reloading only.

- **Where the rounds are.** The pouch is the client's (its place, turn, size); the engine now sends the box its rounds
  stand in (`view::ammoPouchRounds`: the full pouch's depth, the rounds' row ±3.2, from the rim at 1.5 to the magazines'
  tops at 2.9, in its model units; middle and half-axes, z up along the rounds) with each move (`VrMove::ammoPouch` ->
  `.ammopouchpos`, `.ammopouchx/y/z`; carried with the player's origin between moves as the hands are). The pouch is
  now placed even when `vr_ammo_pouch_show` is 0 (not drawn), since it still works there.
- **The rule** (QC `VR_Reload_PouchFrame`, per hand each frame, from `VR_Reload_HandFrame`): the gun's load point (the
  engine's `.loadportpos`, as drawn) within the contact of that box, the gun able to take rounds and lying right:
  - **Super shotgun**: broken open (shut: nothing), its breech within `vr_reload_pouch_sshot_contact` (10 cm) at any
    angle (`vr_reload_pouch_sshot_angle` 180; lower: the breech facing down into the pouch within it): its empty
    chambers filled from the reserve (two; one if one shell is left). It stays open (Close When Loaded closes it, as for
    a pair by hand).
  - **Shotgun**: its port's opening (`.loadportface`) facing down into the pouch within `vr_reload_pouch_angle` (35
    deg), within `vr_reload_pouch_contact` (4 cm): one shell per touch (the gun's tube loads one at a time by hand too).
  - **Nailgun, super nailgun, thunderbolt**: only with the well empty (the magazine dropped, pulled or knocked out); the
    well's axis (`.loadportaxis`, a magazine's feed end up it) along the magazines standing in the pouch within that
    angle: a fresh magazine seated, min(its size, the reserve), with the hand-seated magazine's sound, buzz and the
    cell's sparks. A magazine in: nothing (the touch used up, so ejecting at the pouch needs the gun taken away first).
    The super nailgun's well is on its left: the gun rolled onto its side.
  - **Not the launchers**: their rounds stand nose up in the pouch and go in butt first at the muzzle; no pose meets
    them sensibly.
  - A touch counts once (`.vr_pouch_touch`): the next after the load point has left the rounds by the contact plus
    `vr_reload_pouch_clearance` (4 cm). The reserve empty: the empty pouch's knock once a touch, nothing loaded.
  - **Holstering**: the gun's hand at a holster hotspot (hips, shoulders, upper) makes no pouch reload; the angle and the
    empty receiver guard the rest (a gun holstered hangs muzzle down: its port and well face sideways).
  - Feedback as by hand: the shell-in sound and the gun's buzz, the shell (or pair) drawn sliding from the pouch's top
    into the gun (vr_collectfx.cpp "into the gun"), the full tube's knock.
  - The pouch shows the rounds of the gun brought to it (`VR_Reload_PouchNearHand`): a nailgun at the pouch beside a
    shotgun in the other hand shows magazines.
- The gun's body is eased off the body's self-collision near the pouch by its load point too (vr_selfcollide.cpp), as a
  hand reaching into the pouch is, so the gun can reach the rounds.
- **Settings** (new cvars, default on; no migration): Weapons > Reloading, "Reloading at the Pouch": a toggle per gun
  (`vr_reload_pouch_shot`, `_sshot`, `_nail`, `_snail`, `_light`), Pouch Contact, Pouch Angle, Pouch Clearance; each
  gun's page repeats its toggle ("At the Pouch"), the Super Shotgun's with its own Pouch Contact and Pouch Angle.
- Debug > Tests > Reloading: "Empty and Break Open the Main Hand's Gun" (`vr_reload_test 30`). Mock:
  `vr_mock_hand_to <hand> pouchport [units]` (the hand's own gun's load point on the rounds' top, units above them).

Tests: `Misc/quakevr/reload/pouch_test.sh <agent>` (24 checks, all PASS; the mock's pitch 50 holds a gun level):
dual-wielded super shotguns emptied and broken open fill at the pouch (2 each, shells 34 -> 32 -> 30), the off hand's
again at pitch 90, -60/45, 0/90/90 and 45/0/180; 4 units above the rounds nothing, 1.5 fills; its toggle off nothing;
shut nothing. Shotgun: a shell per touch (1, held 1, away and back 2), upside down none. Nailgun, thunderbolt, super
nailgun (rolled): magazine in nothing; out and the wrong way nothing; right: a fresh magazine (nails 200 -> 176, cells
200 -> 164, nails 200 -> 164). No nails: nothing and the knock. At the left hip holster nothing; the hip's reach made to
win at the pouch nothing, back to normal it fills. Save, load: 2 kept, filled again after the load.

## Judo: the throw's direction, held throws and lifts (2026-10-13, worktree `foegrab3`)

His note: "The judo throw detection is still a bit inconsistent. I was pulling the enemy towards my left, and it fell
towards the other side." Then two new options (held throws, lifts) and, later, a neck snap (section 4).

### 1. The throw's direction

Two causes, both reproduced headless with the mock hands (`Misc/quakevr/foegrab/judo_test.py <agent> dir`):

- **The turn's pivot.** `throwCheck` measured the hands' topple of the enemy about its **box's middle** (r x v / |r|^2,
  r from the middle to the palms). A grunt's box middle is 8 units over its origin, about its belt: hands holding it at
  the middle or lower (its arms hang there, a zombie's or a short enemy's chest is there) gave r.z <= 0, so a pull to the
  left turned it about a level axis the other way round, or not at all (only the vertical twist, which is no throw). On
  the old code, `low left` was thrown **to your right** (311 deg/s), `mid` never thrown. Now the topple is about its
  **feet** (a standing body's pivot), way = (r.z v' - v.z r') / (|r| |m|) with r from its feet to the palms: its top goes
  where the hands drag it whatever the grip's height; pulled down at its front, towards you. (|m|, the old arm to the
  middle, still scales it so Turn to Throw keeps its order of magnitude: a 3 m/s pull reads 360-930 deg/s, was 305-390.)
- **The wheel's level term.** The two hands' wheel, d x dv / |d|^2, also turned the enemy over by their **level**
  speed difference when the hands are at different heights (one on its shoulder, one on its arm): which side depended on
  which hand was higher, amplified by 1/|d|^2 for hands close together. Only their vertical difference counts now
  (`-dv.z d' / |d|^2`: one shoulder pushed down, the other up; it tips towards the lower).
- And the way is smoothed over 40 ms (`throwWayTime`; a reversal, the yank after a backswing, takes the new way at
  once), and **the thrown ragdoll passes through your hands and you** for `vr_foegrab_throw_grace` (0.75 s; Holding
  Enemies > Passes Through You; `box3d::ragdollGrace`, the props' throw grace extended to ragdolls). The hands that
  held it are in it as it goes and still moving (the pull's follow-through): in the test they propped it at 30 degrees
  and it slid down **the other side** (uneven right: lay 34 units to the left); pulled towards you, it fell into your
  capsule, was pushed back up and landed away from you. The spin (the hands' twist about the vertical) is unchanged.

Test (16 cases: hands at its shoulders, its box's middle, low, and uneven (main hand 0.25 m higher, 1.6 times as fast
aside), each pulled left, right, towards you, away at 3 m/s): all PASS: thrown the pulled way, its head 36-39 units
along the throw at 0.2 s, and lying 12-32 units to that side of its feet (towards: 14-29 units nearer you).

### 2. Held on after the throw (`vr_foegrab_throw_hold`, on)

His ask: after the judo throw the enemy's ragdoll stays held in your hands, the topple and the forces still applied, not
detached but softly (not glued), so you can steer where it goes and pull it. Holding Enemies > Hold On After Throw
(`vr_foegrab_throw_hold` 1), Held On Strength (`_hold_strength` 0.3 of its weight a hand), Held On Spring (`_hold_hertz`
4 Hz).

- After `throwDown` (the knockdown, the topple, the slack), `handOver` gives each holding hand the ragdoll's limb nearest
  it (within 16 units) through the ragdoll limb hold that already exists (corpses, knockdowns: a kinematic body at the
  hand, a motor joint to the limb), made soft: `box3d::ragdollHoldSoft`, the `RagdollGrab` with its own spring's most
  force and rate, let go of when the limb lags 72 units for half a second (40 for the usual hold). The QC is told
  (`VR_FoeGrab_HandOver`: the hand's held field, as `VR_Ragdoll_Handtouch` sets it), and the foegrab holds hand the
  grip over (not hidden from the QC any more): from there the QC's `VR_Ragdoll_HandFrame` holds and lets go as with any
  ragdoll limb (the grips let go: dropped, or thrown with the hands' throw; its two-handed throw too).
- 0.3 of its weight a hand (`box3d::ragdollWeight`; a grunt's ragdoll weighs 2108 N, 215 kg of parts, an enforcer's
  2616 N, so a force in newtons fits no two kinds): two hands give 0.6 of it, so it still falls (the dir test, held on,
  still passes all 16) and is never held up, but lowered to it you drag it. The hold test's drag, its pelvis nearer over
  a 0.5 m pull, 4 runs each: 0.2 a hand 0-8 units (it lies on the floor: friction), 0.25 3-8, 0.3 5-19, 0.35 6-12 but
  it lies only 13-17 units to the side (the hold holds its fall back; 0.3: 18-25). From your chest the pull on a body
  lying on the floor is mostly up: lower your hands to it. (First made as newtons, 350 a hand; changed to the share
  with the lift below.)
- While held it passes through the holding hands (`carryStep`: `ragdollGrace` kept up; a grace is lengthened, never
  shortened). `vr_foegrab_status` prints the ragdoll held on: its pelvis from the player, each hand's distance to its
  limb. The code is organised for more two-hand moves (the lift below, the neck snap to come): `twoHandCheck` measures
  both hands on one enemy once a frame (`TwoHands`) and tries each move; `handOver`/`carryStep` are the moves' shared
  ragdoll hold.

Test: `judo_test.py <agent> hold`: thrown left and held on by 2 hands, lies 18 units to the left, then the lowered
hands drag it 0.5 m nearer: its pelvis 61.0 -> 42.2 units ahead, still 2 hands; Hold On off: let go as thrown, lies 28
units left. PASS (4+ units of drag asked: it varies run to run with how it landed).

### 3. The lift (`vr_foegrab_lift`, on)

His ask: walked close to a humanoid (soldier, enforcer, zombie, knight, death knight, AD's humanoid kinds; not ogres,
shamblers, fiends, vores), grab with two hands and pull **up** (instead of to a side: the judo throw): it becomes an
active ragdoll, flailing, which you can carry and throw down; it breaks free (you get shoved back) after a time, 2 s by
default; sliders for the thresholds and the time. Holding Enemies > Lift:

| cvar (row) | default | |
|---|---|---|
| `vr_foegrab_lift` (Lift) | 1 | on |
| `vr_foegrab_lift_speed` (Lift Speed) | 1.2 m/s | the hands' mean speed up, within 45 degrees of straight up |
| `vr_foegrab_lift_reach` (Lift Reach) | 48 units | you this near it (middles, level; held at arm's length ~36) |
| `vr_foegrab_lift_time` (Breaks Free After) | 2 s | |
| `vr_foegrab_lift_strength` (Lift Strength) | 1.2 | each hand's most pull, a share of its ragdoll's weight |
| `vr_foegrab_lift_up` (Lift Launch) | 160 units/s | it leaves the floor this fast |
| `vr_foegrab_lift_shove` (Break-Free Shove) | 48 units | how far it shoves you back |
| `vr_foegrab_lift_classes` (console) | grunt, enforcer, zombie, knight, death knight, AD soldiers/knights/mummy/skull wizard, ranged knight, infected | |

- `twoHandCheck` tries the lift first (`liftCheck`): an upward pull (mean velocity up at Lift Speed, steeper than
  45 degrees) is never a throw. Not a humanoid, or too far: nothing (`vr_foegrab_debug` says why). Else QC
  `VR_FoeGrab_Lift` knocks it down with an upward launch (`VR_Knockdown_Start`; its get-up held off past the lift
  time), the throw's grunt plays, both hands hold on (`handOver`, at Lift Strength and an 8 Hz spring: a corpse limb
  hold's) and it flails: `box3d::ragdollFlail` -> `ragactive::beginFlail`, a living swim in a new `Liquid::Air` (the
  lava flail's thrash: limbs thrown about, torso and head thrashing, at full strength, never left "out of the liquid").
- Carried: the QC's ragdoll hold, so walking carries it, and letting go of the grips drops it or throws it with the
  hands' throw (`VR_Ragdoll_HandFrame`; a knockdown's fall damage on landing as ever); let go of, its flail ends.
- At the lift time `breakFree`: both holds released, the flail ended, QC `VR_FoeGrab_BreakFree` clears the held
  fields, shoves you back (`VR_EnemyShove_Push`'s slide) and makes its get-up due at once: it rises (in the test
  through its get-up 1.75 s later). Killed while lifted: a body held, its flail a dying one's (no break free).
- Strength as a share of its weight: the ragdolls weigh far more than the QC's masses (a grunt 2108 N, an enforcer
  2616 N). At 0.75 a hand an enforcer (a hand on its forearm, a light part) sagged to the floor, its pelvis 11 units up;
  at 1.2 both kinds are carried (pelvis 31-35 units off the floor, the limbs 4-8 units from the hands).
- For the neck snap to come: `TwoHands` (both hands' palms and velocities on one enemy, measured once a frame), the
  moves tried in turn in `twoHandCheck`, and `handOver`/`carryStep` (the ragdoll held on afterwards) are shared.

Test `judo_test.py <agent> lift` (6 games): grunt and enforcer lifted by 2 hands, flailing, pelvis held up (11.0 and
8.5 units over the player's origin, ~19 off the floor lying), broke free after 2.00 s, you shoved 48-49 units; lifted
and thrown down at 1 s: let go of, no break free; an ogre: not lifted (not a humanoid); Lift Reach 24 with it 36 off:
not lifted; Lift off: not lifted. All PASS.

### 4. The neck snap (`vr_foegrab_necksnap`, on)

His ask: both hands holding a humanoid's head (where a headshot counts), twisted in a circle either way with enough
speed and turn (not pulled to a side: the judo throw; not up: the lift): a big bone crack, it dies on the spot; alive
standing, staggering or knocked down (an active ragdoll); then it stays limp in the hands as a ragdoll until the grips
let go. Holding Enemies > Neck Snap:

| cvar (row) | default | |
|---|---|---|
| `vr_foegrab_necksnap` (Neck Snap) | 1 | on |
| `vr_foegrab_necksnap_speed` (Twist Speed) | 200 deg/s | the hands' twist of the head at least this fast, |
| `vr_foegrab_necksnap_angle` (Twist Angle) | 45 deg | kept up while it turns this far one way |
| `vr_foegrab_necksnap_margin` (Head Grip Margin) | 2 units | the head zone grown by this for the holds |
| `vr_foegrab_necksnap_volume` (Crack Volume) | 1 | |
| `vr_foegrab_necksnap_strength` (Snapped Body Hold) | 1.2 | each hand's most pull on the body afterwards, a share of its weight |
| `vr_foegrab_necksnap_hertz` (Snapped Body Spring) | 24 Hz | how stiffly its head follows the hands afterwards |

- **At its head.** Standing (the foegrab's holds, `twoHandCheck`): each hold's spot, mapped to its model standing
  (`hitmodel_rest`), within positional damage's head sphere plus `VR_HEAD_MARGIN` and the margin (QC `VR_FoeGrab_OnHead`:
  `PositionalHead`, the headshots' one source). A ragdoll (staggering, knocked down, lifted or thrown and held on: the
  QC's ragdoll holds, `ragdollNeckCheck`): the part each hand holds one of its head's (`limbInfo` 1), or the palm on its
  head (`ragdollHeadAt`, the margin at the neck). The humanoids are the lift's list (`vr_foegrab_lift_classes`); others
  are measured and refused at the snap (`no neck snap, not a humanoid`).
- **The twist** (`neckStep`): the palms turning about each other, d x (v1 - v0) / |d|^2 (d off palm to main palm,
  metres; opposite motion across d: about the vertical, one hand forward and the other back; about the forward axis, one
  up and the other down), plus both wrists rolling the same way about the line between the hands (each hand's turn since
  the last frame along d, from its angles; the lesser of the two when they agree), smoothed over 30 ms. It is a twist
  when each palm's speed about the hands' middle (the roll at 8 cm) is more than their mean velocity: a pull together
  (judo, lift) is not, and while the hands twist the head the lift and the throw aren't tried (the throw's wheel term
  would read one hand up, one down as a throw). At Twist Speed or more its turn one way adds up (another axis, more than
  60 degrees off, starts again; slower, it starts again); at Twist Angle it snaps (no retry for 0.5 s).
- **The snap** (QC `VR_FoeGrab_NeckSnap`): standing, it is knocked down first (`VR_Knockdown_Start`, no push: its
  ragdoll made now); then `.vr_death_hit` the head's (a collapse, limp), health -1 (whole, no gib) and `Killed` by the
  player (counted; a zombie or a mummy with `VR_Decap_ZombieDie`, dead for good, lying; AD's monsters by `ad_Killed`,
  AD's zombies `VR_AD_ZombieKilled`); a knocked-down one's death code runs at once (`VR_Knockdown_FastForwardDeath`)
  and its ragdoll stays. The crack on its voice channel after the death code, so it replaces the death cry; both hands
  buzz. The training dummy shows "neck snapped".
- **Held limp.** Standing: both holds hand over to its ragdoll (`handOver` with `Why::Snapped`, at Snapped Body Hold
  and Spring), each holding its **head's part** (`ragdollHoldSoft`'s new `head`: the nearest part was the enforcer's
  chest and its loose gun, which let go). A ragdoll already: the hands' holds go on (a lift's flail ends), and it is the
  carried one (`carryStep`: passes through the hands; `vr_foegrab_status` prints `(snapped)`). Let go of the grips: let
  go of (dropped or thrown with the hands' throw, as any ragdoll). The head bears the whole body on a light part: at the
  lift's 8 Hz the enforcer's head hung 21-25 units below the hands, 16 Hz 7-10, 24 Hz 2-5 (the grunt's 2 at 16): 24.
- The sound: `Misc/quakevr/make_necksnap_sound.py` (pure Python, a fixed seed) writes `quakevr/sound/vr/necksnap.wav`
  (0.55 s, 22050 Hz): a hard broadband transient through bony resonances (1.1, 2.3, 4.1 kHz), a second lower crack
  11 ms on, the vertebrae's crunch (13 clicks spreading out), a cartilage pop (820 Hz), a gliding 170-85 Hz body thump,
  a faint crackle dying away; soft-clipped, peak -0.5 dBFS.
- Tests: the mock plays take `rel` keys now (a hand's keyframes from where it is when the play reaches its first: after
  `vr_mock_hand_to ... ragdoll head`, a twist about a lying ragdoll's head). `judo_test.py <agent> neck` (14 games):
  standing grunt twisted 75 degrees in 0.2 s clockwise, anticlockwise and rolled about the forward axis, an enforcer, a
  zombie, a knocked-down grunt both ways, a staggering one, a lifted one: all snapped at 46-49 degrees turned, 368-382
  deg/s, killed in the state asked (`necksnap: ... (standing|knocked down|staggering`), held by 2 hands 0.6 s later,
  let go of 0.4 s after the grips; an ogre's head: not snapped (not a humanoid); hands at its chest, 75 degrees in 1 s,
  Neck Snap off: nothing; hands at its head pulled left: still thrown to the left. All PASS. `judo_test dir hold lift`
  (24) and `stagger_test.sh judo` (6) still pass.
- In VR: a real twist's speed and angle (the defaults are guesses: a quick wrench reads 300-450 deg/s), whether the
  hold at its head is easy to take (the head zone is large: hands on a grunt's shoulders tops may count), the wrists'
  roll (once, by hand, not in the suite: both mock hands at a grunt's head pitched 75 degrees in 0.2 s, their palms
  still: the wrists read 320-430 deg/s, smoothed 360-384, snapped at 48 degrees), the crack's loudness, and how the limp
  body hangs from its head.

## Stealth: a touch no longer gives you away (2026-10-13)

His note: "make it so that just touching an enemy doesn't alert them -- but grabbing them does". The setting existed:
`vr_stealth_contact` (Advanced VR > Stealth AI > Touching Gives You Away), now off by default; grabbing a monster
(`VR_FoeGrab_Taken`) spots you whatever it says, as before. Config version 130 takes a config still at the old
default (1) to 0. `vr_stealth_test 1` step 6: touching it with the option off is unnoticed, then grabbing it spots you,
then with the option on touching it does (15 scenes PASS); `stealth_tests.sh all` PASS.

## Three launch flags imply what they already require (worktree `flatplay`, 2026-10-14)

The author's list, nothing else: `-flatscreenmode` implies no VR backend, `-evalcopy` implies `-noconfigwrite` and
`-noautoexec`, `-preparerelight` implies `-prepare`. No new flags, no renames, no aliases, no cvar changes.

- **`COM_CheckParmImplied (flag, impliedBy, why)`** (`Quake/common.c`, declared in `common.h`): the engine's
  `COM_CheckParm` for a flag another flag implies — it returns whether the flag is in effect (given, or implied by one
  that was given), and prints the implication once, at the first place that honors it:
  `Command line: -evalcopy implies -noautoexec: the evaluation's copy runs its own script, not the player's
  autoexec.cfg`. A registry of the pairs already announced (`implied_parms`, file-scope) keeps it to one line each.
- **`-evalcopy`** (`Quake/host.c` `Host_WriteConfiguration`, `Quake/cmd.c` `Cmd_Exec_f`): the evaluation's copy leaves
  the config alone and does not run the player's autoexec, asked with either flag now.
- **`-preparerelight`** (`Quake/vr/vr_prepare.cpp` `prepare::active`): the relight is a step of the preparation run, so
  the flag alone runs the whole preparation. Its progress file needs `-prepare`'s path, so an implied run prints its
  `Prepare:` lines to the console only (`vr_prepare.hpp` documents it).
- **`-flatscreenmode`** (`Quake/vr/vr_main.cpp` `startBackend`): the mode runs only while no headset session is up, so
  the flag is what asks for no backend at all — no OpenXR session, no mock one either, which is what
  `vr_backend none; vr_restart` used to be typed for. `flatplay::flagGiven()` answers it,
  `flatplay::noteBackendImplied()` says it once. Start the game without the flag to play in a headset.
  `docs/vr-port/FLATPLAY.md` follows (its "Turning it on" section).

Tests (headless, the kit's mock run; the flags passed with `-ExtraArgs`, so they are real command-line flags):

- `-flatscreenmode`: `Command line: -flatscreenmode implies no VR backend: none is started ...`, then
  `vr_flatplay_status` → `-flatscreenmode given, a headset session none, the mode on`, `vr_status` → `VR: inactive`,
  and no `VR: started mock backend`. Without the flag: `VR: started mock backend`, `VR: active, backend "mock"`,
  `a headset session up, the mode off` — as before.
- `-evalcopy` alone (`+vr_haptics_strength 0 +quit` to ask for a config write): both implication lines, no
  `execing autoexec.cfg`, no `Wrote ironwail.cfg.`. Without it: `execing autoexec.cfg` and `Wrote ironwail.cfg.`, no
  implication lines.
- `-preparerelight` alone: `Command line: -preparerelight implies -prepare: ...` then the whole preparation
  (`Prepare: start 1.0.1-dev`, `step maps 1..3` all ok, guns, models, campaigns, `step relight`, `done`, quit).
  Without it: no `Prepare:` line at all.
- e1m1 smoke: clean (`map e1m1; wait60; screenshot; quit`, no engine error, no crash).

Existing invocations left as they are: the motion review's copy (`vr_motion_review.cpp`) and the installer's
preparation run (`Installer/.../GamePreparation.cs`) still pass `-noconfigwrite -noautoexec` (and `-prepare <path>
-preparerelight`) explicitly. They are now redundant but harmless, and the explicit form is what keeps their own logs
free of the implication lines; the progress file still needs `-prepare`'s path.

## Flatscreen play: the hands grip (agent/flatplay, second commit)

**The cause, in one line:** a flatscreen hand never reported its grip — `handButtons(h).grab` comes only from the
controller's grip bindings, which do not exist on the desktop — so every hold check in the game code saw an open hand:
`VRWeaponLetGo` (`QC/weapons.qc`, `vr_weapon_grip_mode 0`: `return !grabbing`) let go of any weapon, and
`QC/vr_carry.qc`'s upkeep (`if(!VR_HandGrabUtil_IsHandGrabbing(self, xHand))`) dropped any carried thing. The force
grab's catch was already special-cased for the mode (`VR_Forcegrab_CanCatch`), which is why E could bring something
in at all; keeping it was never possible.

**The fix:** `gripping()` in `Quake/vr/vr_client.cpp` — `handButtons(hand).grab || flatplay::active()` — is what the
`QVR_VRBITS0_*HAND_GRABBING` bits, the throw estimate and `client::grabbing()` all read. While no headset session is
up and `-flatscreenmode` is given the hands are closed; every other run answers exactly as before (`flatplay::active()`
is false, so `gripping()` is the controller's grip). Documented in `docs/vr-port/FLATPLAY.md`, "The hands are closed".

**Measured** (`map e1m1`, then `impulse 154` the shotgun into the main hand, `impulse 176` the nailgun into the off
hand, then the game's own list `vr_test_weaponinst 0; impulse 120`, ~60 frames later):

- before: `weaponinst: main - | off - | holster 5 weapon 2 #5 | holster 6 weapon 4 #6 | lying weapon 4 #4 367u |
  lying weapon 4 #7 177u` — both hands empty, both weapons on the floor.
- after: `weaponinst: main weapon 4 #7 clip 8 | off weapon 6 #8 clip 24 | holster 5 weapon 2 #5 | holster 6 weapon 4 #6`
  — both held, nothing lying, full magazines.
- the same script with no `-flatscreenmode` (mock backend started, no grip pressed): unchanged, the hands stay empty.
- `eval.sh`: 168/177 pass, the same one take (`slash_horizontal_ltr_2026-10-12_12-29-35.csv`, FAIL -> FAIL, a
  one-frame difference in its first event) differs from the baseline before and after this change; the mode is off in
  every eval run.
- e1m1 smoke: clean.

Test-harness note: `+attack` / `-attack` in a kit run script stops the rest of the script from running, so firing in
flatscreen mode is not measured here; it is the mode's existing mouse path and reads none of the grip state.
## Incandescent blades (worktree `hotblade`, 2026-10-13)

The author: melee weapons become incandescent when their blade stays in a flame a few seconds, glow orange-red for ~30 s,
set enemies (and corpses) on fire and hit +10 harder; any flame (a torch held or on its wall, braziers, the map's flames,
lava); water quenches them with a hiss and steam. His answers: the axe (AD's Shadow Axe too), the knights' swords, the
crowbar, the MG3 Super Axe and the ogres' chainsaw; never Mjolnir. Full glow most of the time, fading over the last
seconds; a flame again while it glows sets it back to full; the timer runs held, holstered or dropped (the heat is the
weapon record's, as AD's `wi_adup`), kept in saves.

- QC `vr_hotblade.qc` (after `vr_burning.qc`). The record (`vr_weaponinst.qc`) has `wi_heat` (the build-up), `wi_heat_at`
  and `wi_hot_end` (time), copied by `WeaponInst_Clone`. Ten times a second (`VR_HotBlade_Frame`, from `PlayerVRMelee`
  after `VR_Melee_Frame`) each hand's blade is looked at: its striking points as the melee tracks them (`mh_cp`, the
  layout's `VR_MPT_EDGE` points: an axe's head, a sword's blade, the crowbar's bar, the chainsaw's bar; not the hilt,
  the handle or the engine). A point in water or slime quenches it (`blade_quench.wav`, steam); a point in a fire
  (`VR_WallTorch_FireAt`, the dying torch's relight: lava, the map's fire spots, a torch on its wall, a lit torch held
  or lying, a lava ball) raises the build-up by dt / `vr_hotblade_heat_time`; out of the flame it falls off as fast after
  0.3 s (worked out when read: a holstered weapon's falls off too). At 1: incandescent for `vr_hotblade_time`
  (`blade_hot.wav`, sparks, a buzz); in a flame while hot: full again. Sizzle (`blade_sizzle.wav`) twice a second and a
  rising buzz while it heats.
- Blows: `VRMeleeDmg` adds `vr_hotblade_damage` to the base of a hot weapon's blow (so it scales with the blow's
  strength, the zone and the caps as the base does: the firing range's synthetic overhead axe chop 29.6 -> 44.4, the
  sword's slash 26.2 -> 39.3, x1.5 for base 20 -> 30). `T_Damage_VRMelee` then sets the struck thing on fire
  (`VR_Burn_Ignite`, a blow's) when it was alive and `vr_hotblade_ignite`, or a corpse (`vr_corpse`, `vr_corpse_dying`:
  a corpse keeps health to gib) and `vr_hotblade_ignite_corpses`; so does the corpse strike (`VR_Corpse_StrikeFrame`)
  and the running chainsaw's cut (`VR_Chainsaw_Cut`; the cut gets no extra damage: it ticks ten times a second).
- Lying weapons (`thrown_weapon` props): while any weapon may be hot, one in water or slime is quenched and a hot one
  sheds a spark now and then (QC: the engine draws only the hands' and holsters' glow).
- AD's flames now leave fire spots too (`VR_FireSpot_AD` in `ad_lights.qc`: its small wall torch, large and small
  flames, pyre, short and tall braziers; only lit for good: not LIGHTOFF, no targetname), so blades heat and torches light
  again at them, and they burn hands like id's (`VR_Burn_MapFrame` reads the same spots). Quoth's long wall torch: not
  (its flame's height on the model is unknown).
- The glow: QC packs each hand's glow (0-255) into `.vr_hot_hud` and the holsters' (0-15) into `.vr_hot_hud2`, sent as
  `STAT_QVR_HOTBLADE`, `STAT_QVR_HOTBLADEHOLSTER`. `vr_meleehud.cpp` eases the hands' glow and sheds embers off a hot
  hand weapon (the counter glow's, ~9 a second at full), and `hotGlow` codes the entity glow for the alias shader:
  4 + 2 x colour (16 levels a channel from `vr_hotblade_hue`) + strength (glow^0.7 x `vr_hotblade_glow`, 0..2). The
  shader (`QVR_ALIAS_FS_GLOW`) dims the lit skin by up to 30% and adds the colour times (0.1 + 0.9 x the texel's
  luminance + 0.45 rim^2), yellower on the brightest texels: the blade's metal glows most, the wood less. The force
  grab's, toolgun's and an open counter window's glow win over it. The world shader ignores codes of 4 and over.
- Settings (Advanced VR > Weapons > Incandescent Blades, linked from Combat > Melee too): `vr_hotblade` 1,
  `vr_hotblade_heat_time` 2, `vr_hotblade_time` 30, `vr_hotblade_fade` 5, `vr_hotblade_damage` 10, `vr_hotblade_ignite` 1,
  `vr_hotblade_ignite_corpses` 1, `vr_hotblade_quench` 1, `vr_hotblade_glow` 1, `vr_hotblade_hue` 18,
  `vr_hotblade_sound` 1. Debug > Tests > Incandescent Blades: `vr_hotblade_test` (TESTING.md).
- Sounds: `Misc/quakevr/make_hotblade_sounds.py` (synthesised: a frying crackle over a thin hiss; a flame's whoosh and an
  iron bar's inharmonic ring; a bubbling burst into a falling steam hiss).
- Tests: `Misc/quakevr/hotblade_test.sh` (19 cases, all PASS): a map flame (build-up 0.44 at 1 s, hot at 2 s, out of
  it 1.00 then 0.82 with 2.5 s left of a 3 s fade, full again in the flame, cold at the end), a torch on its wall
  (entity and static), the off hand's lit torch, lava, slime (held, and dropped into it), holstered (cools on, glows
  there), Mjolnir (no heat), the chainsaw, off; blows on a grunt (+10, lit), a corpse (lit), corpses off and enemies
  off (not lit); saved hot and loaded hot (29.0 -> 28.6 s left); real swings at the dummy (29.6 cold, 44.4 hot, lit).
  `eval.sh`: 168/177 (as before). e1m1 smoke clean. Screenshots (axe: cold, half built up, hot, fading) checked by eye.
- Not done: a lying weapon's model doesn't glow (the client knows only the hands' and holsters'; it sparks instead);
  a level change starts weapons cold (their records are made anew). A blade held in a brazier may set you alight too
  if your body's box reaches the flame (Combat > Burning's map flames: unchanged).
- In VR: how long 2 s in a flame feels; whether the glow reads as red-hot metal in the headset (brightness, hue 18) and
  the build-up as heating; the sizzle, ring and hiss levels; reaching into a brazier with the axe without catching fire
  yourself; dipping in water.

## The VR main menu centred up and down (menucenter, 2026-10-13)

The author's note: the VR main menu still sat towards the panel's bottom. It started at Quake's y 4 (the title) and
ran down to y 312 (twelve rows 20 apart, four group gaps), on a panel from y -220 to 420: its middle 58 below the
panel's.

- menu.c `M_Main_Layout`: in the headset (`VR_MenuCanvas`) the title, the rows and their cursor (and the flat banner's
  place) move up together (`m_main_top`, Quake's 4 before) so the title's top to the last row's bottom is centred in the
  panel; never lower than Quake's place, never above the panel's top (4 under it). The spacing may use the whole
  panel's height: at Menu Height 1.5 the rows are now 20 apart (were squeezed to fit from y 32 down). Menu Height 1 is
  unchanged (the panel is Quake's 200 rows: 13 apart, y 4..195). The laser and mouse pick rows from the same layout.
- The corner buttons and the VR banner (left of them) stand far left of the rows (x -336..-136 against 19..300), the
  version box and update notice right of them, so moving the rows up crowds nothing.
- A flat screen is unchanged (decision): its corner buttons and the top right notice stand at the title's height, the
  banner and the long rows would run into them. It stays from y 4 down (middle 130 on a 320-row canvas).
- `menu_vr pos` prints `main title and rows y a..b (middle m; the canvas's y c..d, middle n)`.

Numbers: VR y -54..254 (middle 100.0; panel -220..420, middle 100.2), was 4..312 (middle 158); rows x 19..300 as
before. Mock laser at (25,-16) picks VR Calibration, (290,94) Single Player, (30,244) Quit; on the title (160,-40) the
selection stays. Menu Height 1.5: y -47..249 on -52..252. Flat: rows 16 apart, y 4..256, as before.

VR check: the main menu's title and list in the panel's middle up and down, equal room above MAIN and below Quit; the
laser picks the row it points at; the banner and the corner buttons where they were.

## Complete stock/mission-pack PBR preservation audit (2026-10-13)

The box-only guard missed shared world material hashes. A complete installed id1/Hipnotic/Rogue census now
protects the original authored/procedural loader by 1,559 BSP source identities, including HD replacements.
The generated corpus overlaps 712 stock identities (526 learned components, 186 baked authored copies).
Alias skins stay excluded; no further model regression was reproduced. Existing stock material selection also
stays intact when AD reuses those pixels, while novel AD companions retain their generated-only strength.

Historical reproduction on every campaign map changed 1,175 procedural world comparisons (535 Quake,
360 Hipnotic, 280 Rogue): 1,175 normal RGB changes and 1,135 height changes. All 1,953 authored external
material comparisons remained intact. Protected runtime: all 73 maps, 81,770 paired world/brush/alias/frame
comparisons identical at strength 8, zero missing pairs; 1,720 additional nearest/smooth and external-off/on
controls identical. Composition 17/17, live/GPU component precedence 6/6 each, stock guard 84/84,
scanner 4/4 and pipeline 18/18 pass; build and e1m1 smoke pass. Native AD generated city5_7b still scales
3/3, authored/stock city5_3 stays 1/1. No original companion PNGs or authored height were modified.

See PBR_STOCK_AUDIT.md, pbr_stock_audit.json and pbr_stock_runtime.json for the counts and pixel-free
census/evidence; PBR_PIPELINE.md documents refreshing protection for later texture packs. The scanner follows
the engine's contiguous-PAK/Pak-before-loose precedence; generator discovery is broader and is not treated as
proof of runtime selection. New vr_pbr_audit command supports actual GPU material hashes (world-only optional).
## Two super shotguns: each hand's flick toggles only its own (worktree `ssgflick`, 2026-10-14)

- The author's note: dual wielding super shotguns, a flick of the left hand closed the right hand's gun; the flick was
  always closing the right one. Cause: the pry and the lift (vr_flick.cpp) reach the server as the OTHER hand's flick bit
  (vr_client.cpp ORs `flick::pried(main)` into the off hand's bit), so `VR_Reload_SsgFrame` read every flick bit of the
  other hand as a pry/lift of this gun. With a super shotgun in each hand a left flick was the left gun's flick and the
  right gun's pry at once; the main hand's gun is handled first, toggled, and the shared `vr_ssg_toggled` pause (0.35 s)
  then blocked the left one. Opening had the same bug.
- Fix (QC/vr_reload.qc): the other hand's bit counts as the pry/lift only with that hand on this gun (empty and
  gripping: the two-handed hold); and the pause after a toggle is per hand (`vr_ssg_toggled` the off hand's,
  `vr_ssg_toggled_main` the main's, `VR_Reload_SsgSetToggled`), so both guns flicked at once both toggle.
- Tests (reload_test.sh, section 7): two super shotguns, left/right flicks alternated, opening and closing
  (`0o 1o 0c 1c 1o 0o 1c 0c`; before the fix `1o 1c 1o 1c ...`), both flicked at once (both open, both shut); the
  two-handed pry/lift check and the ways matrix unchanged and passing. Whole reload_test.sh: 110 PASS, 0 FAIL. e1m1
  smoke clean.
- In VR: two super shotguns, flick each hand alone (opens/closes only that one), both together; then one super shotgun
  held two-handed: the pry still opens it and lifting the barrels still shuts it.
## AD pickup menu: the soldiers' guns made at the world's origin (adspawn, 2026-10-13)

His note: from the AD pickup rows (Debug > Tests > Arcane Dimensions Monsters / Weapon Pickups) only the Shadow Axe,
Widowmaker and Plasma Gun appeared; every other gun did not, and props fell somewhere far away.

- **Why**: those rows are `vr_test_spawn 312..319; impulse 241`, the firing range's dispenser path
  (`func_enemy_dispenser_use`). It ran the spawn function on a fresh entity still at the world's origin and moved it
  to the spawn point only afterwards. A monster or a pickup is placed by that later `setorigin`, but
  `func_weapon_grabbable` (the guns, WID 20..27) makes its `thrown_weapon` at its own origin inside the spawn function:
  every gun was made at (0 0 0) and fell there (the far-away clatter). The previous round's tests used
  `VR_ADWeapons_Make` (origin set first) and the toolgun (`box3d::spawnClass`, origin set first), so they missed it.
- **Fix** (buttons.qc): the dispenser sets the origin before the spawn function too, as a map's entity has it (the
  setorigin after stays). The AD pickup rows now also set `vr_test_spawn_dist 64` (the upgrades and items used
  whatever the last row left: 56 or a monster row's 128).
- **Test aid**: `vr_ad_test 86` (TESTING.md): the pickup, gun or monster nearest the spawn point, PASS when within reach
  ahead, then removed.
- **Numbers** (headless, fast): vrfiringrange 300..319: before 312..319 FAIL (each `thrown_weapon` 346 units away, the
  map's origin), after 20/20 PASS (guns 46..80 units ahead, on the floor; items and upgrades 64). ad_dm1 (`-game ad`):
  20/20 PASS. Dispenser monsters 0..11, 200..206, 210, 220, 230 in vrfiringrange: 22/22 in place at 128 ahead.
  e1m1: `vr_ad_test 81` 11 passed, 0 failed; e1m1 smoke clean.
- **In VR**: Debug > Tests > Arcane Dimensions Monsters, its Pickups: each of the eight AD guns lands about an arm's
  length ahead (vrfiringrange and an AD map); no far-off clatter.
## Incandescent blades: his tweaks (worktree `hotblade`, 2026-10-13)

The author after playing it ("very good, but they need a few more tweaks"): a dynamic light following the glow; a dropped
blade must look the same on the ground (it reverted to the plain model) and keep cooling there; the glow on the blade only,
not the handle, the light from there too; glow strength up (his 1.5); more embers, from the whole heated part (they came
off the handle), with settings; a swung blade leaves fire and sparks in the air behind its heated part; a buzz while
reheating a blade not yet cold; a thrown red-hot blade burns and hits harder too.

- Client side moved to `vr_hotblade.cpp` (out of `vr_meleehud.cpp`): the glowing blades drawn each frame are the hands'
  (`STAT_QVR_HOTBLADE`, eased), the holsters' (`STAT_QVR_HOTBLADEHOLSTER`) and up to 8 lying or flying (nearest first).
  A lying one's glow comes with the prop: QC writes `.vr_hot_glow` on every `thrown_weapon` ten times a second while any
  weapon may be hot (`VR_HotBlade_Lying`, which also quenches it in water; its old QC sparks are gone) and the server packs
  it (0-127) into bits 24-30 of `U_QVR_WEAPONUID`'s weapon id (ids stay far below 2^24; old demos read 0). Its heat is its
  record's, so it ticks down on the ground exactly as held (`drop_cools`: 1.00 with 4.4 s left, 0.82 with 2.5, cold).
- The blade: the weapon's line from the grip (the entity's origin as a hand holds it) to its muzzle anchor (the melee's tip,
  `weaponMount`'s: the report finds both within 0.00-0.02 units held still), from where the QC's striking points start
  (`VR_Melee_PartAt`): the axe and Super Axe 0.6 (the head; the Shadow Axe is the axe's model), the swords 0.2 (past the
  guard), the crowbar 0.05 (the bar past the fist), the chainsaw 0.45 (the bar). Worked out in the model's pose space (the
  model drawn unturned at the origin, the muzzle brought back through the inverse matrix), so a holstered or lying
  weapon's is the same part.
- Glow on the blade only: the alias instance has a new `HotBlade` vec4 (`vraliasinstance_t::hotblade`): a plane in the
  pose's space, 0 at the blade's root rising to 1 over 8% of the line (zero: all of it). The vertex shader's
  `out_morphpos` now carries the raw pose position always (the morph scales it in the fragment shader: no new varying,
  all 16 locations are taken); `QVR_ALIAS_FS_GLOW` multiplies the strength by smoothstep(mask). Vertices that glow
  (report): axe 10/223, sword 8/230, crowbar 280/464, chainsaw 18/285, Super Axe 295/795.
- Light: `emissive::hotBladeLight` (keys -0x5A00 - index, unshadowed) from the middle of the heated part, `vr_hotblade_light`
  (1) x the glow (build-up and fade included) with a slight flicker, reach `vr_hotblade_light_radius` (72) x (0.6 + 0.4 glow).
- Embers (`particles::hotEmber`): puffs of `vr_hotblade_embers` (4), `vr_hotblade_ember_rate` (12) a second x glow^2, each
  off a random vertex of the heated part (tried 16 times; else its line), leaving the surface and rising; hands, holsters
  and lying blades alike (48 a second held still at full glow).
- Trail (`particles::hotTrail`): a hand's blade, or a lying one moving (thrown), whose tip moves faster than
  `vr_hotblade_trail_speed` (1.5 m/s) leaves `vr_hotblade_trail` (1) x glow x the area its heated part swept / 0.8 sq units
  (eased in over the next 1.5 m/s; at most 48 a frame; none for a move over 64 units in a frame), each at a random vertex
  of the heated part between last frame's and this frame's place (its shape: an axe's head, not the handle line): 65%
  small upright tongues of flame keeping 4% of the blade's speed, 35% sparks flung on with 15-35%, lingering
  `vr_hotblade_trail_linger` (0.5 s). The mock sweep (12 frames at ~7 m/s) leaves ~85-170.
- `vr_hotblade_glow` default 1.5 (config 131 migrates a config at 1).
- Haptics: in a flame while still glowing (reheating) a steady 0.4 buzz each tick ("full again ...: reheating, a buzz");
  the first build-up's buzz starts at 0.2 (was 0.1) rising to 0.5; pulses overlap so it is continuous.
- Thrown: `VR_Thrown_Damage` adds `vr_hotblade_damage` to a red-hot `thrown_weapon`'s base (`VR_HotBlade_ThrownExtra`), and
  `forcegrabbable_touch` sets what it hurt on fire (`VR_HotBlade_ThrownStruck`, the blows' ignite settings). The axe at a
  grunt at 276 u/s: 19.8 cold, 25.4 hot (base 35 + 10), lit.
- Settings (Weapons > Incandescent Blades, Look and Sound): Light, Light Reach, Embers per Puff, Ember Puffs a Second,
  Swing Trail, Trail Lingers, Trail From. Debug > Tests > Incandescent Blades: Report What the Client Draws
  (`vr_hotblade_report`), Report the Lying Blades (`vr_hotblade_test 12`), Throw It at the Nearest Monster (13), Drop It
  Here (14).
- Tests: `hotblade_test.sh` 27 cases (new: blade_parts, superaxe, embers, embers_off, trail, trail_off, drop_cools,
  throw_cold, throw_hot; flame checks the reheat buzz, holster the client's holster glow), all PASS; `shots` writes
  `scratch/hotblade/shots_blades.png` (each blade from the side), `shots_dropped.png`, `shots_trail.png`, checked by eye.
- In VR: the light's strength and reach (72 units) on the hands and walls; the embers' amount; the trail (amount, linger,
  1.5 m/s threshold) on real swings; the reheat buzz; a red-hot axe thrown at a monster; a dropped blade cooling.
- Not done: the light is one frame behind a fast-moving hand (the view entities are placed after the queue: ~2 cm at
  7 m/s); a blade held by its hotspot/foregrip uses the same model-space blade (fine), but the force grab's and counter's
  glows still win over the heat glow (unchanged priority).
## Neck snap: only move at the head, a push-pull twists (worktree `foegrab3`, 2026-10-14)

His notes on the neck snap (section 4 of "Judo: ..."): "most of my neck snaps get detected as judo throws. If both hands
are on the head or neck, then judo throw should not be possible at all: the only eligible action should be the neck
snap." And: "If my hands are on either side of the head, just moving one hand forward and one backward would create the
twist necessary to snap the neck. At the moment I have to snap my wrists and twist them."

- **Both hands at its head: only the neck snap.** `neckCheck` returns true whenever both holds are at its head, twisting
  or not: the lift and the judo throw aren't tried (before, a hold at the head that didn't read as a twist fell through
  to them: his push-pulls read as throws). Neck Snap off, the lift and the throw are tried as before.
- **The neck.** `VR_FoeGrab_OnHead` (QC) is the head sphere grown (as before) but no lower than
  `vr_foegrab_necksnap_neck` (Neck Length, 3 units) below the head sphere's bottom. Measured on the grunt (spots on its
  model standing, up from its origin; head 23 up, radius 7.2): the hands at its head 21-24, at its neck 13.6-16.3, the
  judo suite's "high" (shoulders) 10.3-12.3. The grown sphere reached down to 10.3, so the shoulder holds were at the
  head (by 0.5 units) and with the new rule they lost their judo throw: the cut at 12.8 keeps them the judo's and the
  neck the snap's. A ragdoll (knocked down, staggering, lifted): a palm also counts within 6 units (`neckGrip`) of its
  neck's line, from its head's joint down Neck Length (`box3d::ragdollNeck`, new: its head's joint, its head's middle and
  the line between them as it lies).
- **The twist** (`neckStep`, rewritten): the neck's axis is its up standing, the neck's line for a ragdoll. The palms:
  the line between them swept round, d x (v1 - v0) / |d|^2, and about the neck the angle its shadow across the neck
  sweeps, n.(d x (v1 - v0)) / |d across n|^2 (at least half |d|: hands nearly along the neck can't show its turn). So
  one hand pushed forward and the other pulled back turns the head as far as the line between them turns, whatever
  the wrists do. The wrists: each hand's turn since the last frame, about the neck, the line between the hands and the
  third axis, the lesser of the two when they agree. Each axis takes the larger of the palms' and the wrists' (a hand
  gripping the head turns as much as the line between the hands sweeps: both measure the same turn; summed they would
  count it twice), smoothed over 30 ms. A twist: each palm's speed about their middle (the wrists' turn at 8 cm) at least
  0.75 of their mean velocity (was more than all of it: a push with the other hand still is a twist now). A stroke adds
  its turn one way while the twist is at 0.4 of Twist Speed or more; it snaps at Twist Angle turned with its fastest
  at Twist Speed (`neckSnapDue`; before, the turn only added up above Twist Speed, so a push-pull's ease in and out
  was lost).
- Defaults: Twist Speed 200 -> 100 deg/s, Twist Angle 45 -> 30 deg (configs still at the old defaults take the new:
  `vr_cfg_version` 132; 131 is the hot blades' glow). A push-pull of 20 cm each way, the hands 0.24 m apart (the tests'), turns the line 59 degrees;
  0.3 m apart, 15 cm each, 45; 0.36 apart, 15 cm, 40. Its fastest (standing, `vr_foegrab_necksnap_angle 100000` to
  measure): in 0.3 s 325, 0.5 s 210, 0.7 s 154, 0.9 s 119, 1.2 s 91 deg/s and no stroke: up to about 0.9 s snaps, 1.2 s
  does nothing.
- Menu (Holding Enemies > Neck Snap): the toggle's help (head or neck, push-pull, the only move), Twist Speed's and
  Angle's help with the numbers, new row Neck Length. The debug line (`vr_foegrab_debug 2`): `twist <deg/s> (about its
  neck: the palms, the wrists), stroke <deg>, fastest <deg/s>`. The mock's `vr_mock_hand_to <hand> ragdoll neck`: at its
  head's joint.
- Tests: `judo_test.py <agent> neck` (25 games): the 13 kept (the lifted one now lifts by the shoulders, one hold in
  the head zone, then twists) and push-pulls standing at its head forward and back (325-329 deg/s, snapped at 31-33
  degrees), wide (201), uneven 25/10 cm (305), at its neck (328), knocked down at its head both ways (391-435) and at
  its neck (516): all snapped, held limp by 2 hands, let go of with the grips; the same push-pull in 1.2 s standing and
  knocked down: nothing; both hands at its head pulled left or up: not thrown, not lifted, not snapped (replaces "a pull
  at its head is still a judo throw"). All PASS (the knocked-down cases' ragdoll grab missed with the main hand in 2 of
  about 14 runs, before any twist: reruns pass). `judo_test dir hold lift` 24 PASS, `stagger_test.sh judo` 0 FAIL, an
  e1m1 smoke clean.
- In VR: push-pulls at his pace both ways at a standing grunt's head and neck, and a lying one's; whether a shoulder
  grab still throws (judo) and a neck grab never does; whether Twist Speed 100 / Angle 30 snaps too easily when he only
  repositions his hands on a head (a slow repositioning stays below 100 deg/s; a quick one could snap).

## Optional Tracy profiling foundation (2026-10-13)

- Official Tracy v0.14.1 public client source + BSD license vendored; `QvrTracy=true` opt-in optimized MSBuild
  outputs and intermediates are separate `Release-Tracy` folders. `Misc/quakevr/build_tracy.ps1` builds it.
  Normal engine compiles out the client and bridge; its existing profiler/CSV remains functional.
- Existing C/C++ CPU and GPU profiling scopes feed Tracy independently of `vr_profile`; Box3D native zones enabled
  only in profiling builds. Main/worker thread names, host frame marks, separate named Box3D frame marks.
- On-demand, localhost only, no system tracing/sampling/callstacks or competing crash handler. Coarse GPU default
  (`vr_tracy_gpu 1`, `vr_tracy_gpu_detail 0`); detail 1 covers every existing GPU scope. CPU stacks/source cache and
  async GPU query ring are bounded; reserve ends, drop whole zones rather than stall. Recover explicit zones after
  Host_Error/Host_EndGame longjmp; query deletion before GL shutdown, new context/session timestamps on reconnect.
- Builds pass; no-viewer e1m1 smoke exits0, zero drops/GPU context0. Matching CLI tools read actual trace:
  CPU171 names/443679 events, GPU43 names/128004 events, Box3D39 names/45307 events, 1783 host frames with `vr_profile 0`.
  Both same-process reconnect traces decode after intentional Host_Error/reload and GL query recreation (contexts1–3).
  Stack bound test PASS (depth1→1, overflow0;65 deliberate drops). Normal profiler e1m1 report still works.
- `docs/vr-port/TRACY.md` describes setup, captured evidence, workload-specific overhead, limits and wrapper API for
  next-stage QC/jobs/loading/resources/memory coverage. Private `.tracy`/CSV/JSON evidence remains in scratch/profile.
## Review fixes (adpbrload, adpbrgen; worktree `fixpbr`, 2026-10-13)

From review r4 (adpbrload 1.1-1.6, 2.1, 4.1-4.3; adpbrgen 1, 2), one commit each:

- `vr_pbr 0` is exactly as before the loader: Mod_LoadTextures passes an embedded texture's `_norm`/`_bump` names
  only with `vr_pbr` on (`VR_PBREnabled`). Proof against a build of 63e8ab544 (the merge's first parent): e1m1,
  e2m3, e4m1 with the head flashlight on, `vr_pbr 0`, fixed frames: 0 differing pixels each, and the same 16121
  failed file lookups (`developer 2`), none a `_norm`/`_bump`; `next` had 2140 more on e1m1 alone. Comments fixed and
  the shared lines tagged.
- Generated-component strength is resolved once per normal map (`normalmap_generated[]` in gl_texmgr.c, set in
  TexMgr_LoadNormalMap), not by a string search per draw batch: ad_crucial, exclusive, 10 s: `world` CPU 0.260 ->
  0.068 ms, CPU busy 1.551 -> 1.469 ms; `vr_pbr_strength 3` still gives the same 158 textures 3/3.
- stock.txt and provenance.txt parse with CRLF too (a present file with no records warns); `.gitattributes` keeps
  them LF; `vr_pbr_test` parses the shipped stock.txt both ways. The indexes and each companion's verified flags are a
  GameDirChange cache, not re-read and re-hashed per map: ad_crucial's companion work 129 ms first, 100 ms when
  revisited (277 calls). Stock textures no longer probe 3 file names for a diagnostic; recipes are freed with the
  last texture that uses them.
- An external pack normal given a generated height has its green judged against that height (Quetoo's heighted
  maps judge -0.86..-0.99, turned); no AD map composes one today, so AD is unchanged.
- Generator: `entries()` uses the engine's PAK order (PAKs above loose, contiguous pak0..N); its tests assert it.
- The bundle ships only companions the loader can use (his decision): 999 of 2987 moved to scratch (712 stock.txt
  sources, 218 Quetoo comparison pictures, 69 miptex AD's own replacements always cover; 19 covered only by id1 or
  mission-pack HD replacements are kept for installs without them). 364.3 MB -> 114.7 MB, provenance 1988 records.
  The generator writes only those (`shipped()`, `--comparison-output` for the rest; `prune` for an existing bundle).
  All 283 AD maps (`vr_pbr 1`, `vr_extmaps 1`): the 13946-line `vr_pbr_audit world` census is identical before and
  after the prune, and with the final build. `pbr_loader_test.py` 6/6 + 6/6, `vr_pbr_test` 21/21 (fixtures),
  125/125 (stock, e1m1); generator tests 24 OK.
- Not changed: the stale duplicate "Generated-channel provenance emission" section above, the census JSON's absolute
  paths, the wasted decode before the complete-authored/external checks (r4 1.2), the 55 flat companions (3.1).
## Review fixes: AD soldiers' guns, sky fog, AD isolation (worktree `fixad3`, 2026-10-13)

- An AD soldier's gun carried or holstered into a level without that soldier (or with `vr_adgun_drops 0`) quit the
  engine (`SV_ModelIndex: model progs/mon_soldier.mdl#gun4 not precached`). DecodeLevelParms now precaches every carried
  AD gun and its sounds (`VR_ADGun_EnsureCarried`, one AD support call). Test: `vr_ad_test 87`, `changelevel start`,
  `vr_ad_test 88` (TESTING.md); reproduced before, PASS after.
- AD's sky fog was read from the integer stat (1.3 became 1, so the fog was 0 and the map's own was hidden). It is now
  read from `cl.statsf`. `vr_ad_skyfog_info` prints the client's value: `stat 1.3, sky fog drawn 0.3` after `vr_ad_test 66`.
- A gun model the engine can't make (another AD release, a replacement pack, a save loaded without AD's data) now loads
  Quake VR's stand-in (`v_gruntgun.mdl` or `v_enfrifle.mdl`) under the gun's name instead of a Host_Error on the
  soldier's spawn (`vr_ad_test 89`). The weapon settings slot is still the AD gun's (27-31), so the stand-in may sit
  slightly off in the hand.
- AD isolation: the gadget's AD page moved to `vr_ad_gadget.inc`, its outer page button to `vr_ad_gearlights.inc`, and
  `pageAdGuns` to `vr_menu_ad_guns.inc`. The neck snap's AD branches moved to `VR_AD_NeckSnapKilled` (vr_ad_monsters.qc;
  `vr_ad_mtest 5`: an AD rocket soldier and a mummy die by AD's Killed). OnLoadGame's upgrade loop moved into
  `AD_LoadedGame`. `VR_ADGun_Attack`, `VR_ADGun_Available` and `adguns::slotNames` are now one-line calls. The guns'
  test rows moved to Debug > Tests > Arcane Dimensions Monsters. Also fixed: the vr_api.h comments and the
  `vr_ad_test` help.
## Closer to enemies: body inset and step in (closein, 2026-10-13)

His problem: he couldn't get close enough to put his hands on an enemy's neck or head; the two boxes met first. Both
fixes, collision side only (`vr/vr_bodycontact.cpp`; world.c calls `VR_BodyContactHull`, `_LetsOut`, `_Passes`):

- `vr_body_monster_inset` (default 4, 0..12; Combat > Holding Enemies > Getting Close > Body Inset): where a player's
  move meets a monster's box, or a monster's move the player's, each box (as the hull code already narrows it:
  vr_hull_ent_width, vr_mhull_ents) comes in that much on each level side: 8 units closer face to face at 4. Only body
  moves with the mover's own box (SV_FlyMove, SV_movestep, QC traceboxes with self's box); the map, brush models, props,
  corpses, monsters against monsters, shots, missiles, precise hits and melee (distances, point traces) keep theirs.
  A box never narrower than 4 units.
- `vr_foegrab_step_in` (default 1; ... > Step In): while a hand holds an enemy (`foegrab::holds`, the engine's holds),
  his body and that enemy's don't meet at all, both ways (SV_ClipToLinks skips the pair).
- Never stuck (either option on): a player and a monster already overlapping (a hold let go of, the inset lowered while
  touching) get Quake's start-solid only for a move that goes deeper than the mover is now; one that goes no deeper
  (checked at its start, a quarter unit along and its end: a box's depth along a line is concave) passes, so
  SV_CheckStuck never lifts him and a monster's SV_movestep (traced down from its new spot) can't step in deeper.
  `bodycontact::serverFrame` (VR_ServerFrameEnd, after foegrab's) pushes each overlapping pair apart along the least
  way out, 80 units/s: the monster by a walk step (SV_movestep: no ledges, no walls), the player for what it can't.
  Not when he is over or under it (the least way out is up or down: standing on it is Quake's).
- `vr_body_contact_status` (Debug > Tests > Holding Enemies > Nearest Body Contact): the nearest live monster, its
  level distance, the pair's depth (> 0 overlapping), held. `vr_foegrab_debug 2` prints each push apart.

Numbers (`Misc/quakevr/foegrab/closein_test.sh`): walked into on vrfiringrange, origins' level distance at inset 4 / 0:
grunt 12.03 / 20.01, ogre 20.03 / 28.01, enforcer 14.03 / 22.01 (8 closer each). Inset set to 0 while touching:
5.73 deep 2 frames later, apart (depth -0.01) within 60 frames; backing off then moves ~90 units in 20 frames (not
stuck). The firing range's back wall: the same stop at inset 0 and 12 (372.0 -556.0). A held grunt (step in on)
walked into: 5.59 apart (6.4 deep), let go: 8.9 three frames later, 12.01 after 60, backs off freely; step in off: stops
at 12.03 held. Regressions: judo_test.py dir hold 0 FAIL, stagger_test.sh judo 0 FAIL, eval 168/177 (one FAIL->FAIL
timing 0.375->0.391), e1m1 smoke fine. voreshove_test.sh: 3, 4, 4 shoves in three runs (needs 4) against 4, 5, 5 with
both options off: the vore, 8 units closer, takes more of its own balls' splash and dies sooner (it dies in both); the
shoves themselves are as before.

Notes: with step in, nothing stops him walking right through the held enemy and out its far side (he stays held only
within Let Go Beyond). Moving in a little more than his box would before ends nothing else: monsters' melee ranges
are QC distances from origins, unchanged, so a grunt you stand closer to still hits you by its own range.

VR check: walk up to a grunt, an enforcer, an ogre: hands reach its head and neck without stepping through; Body Inset
0 vs 4 vs 8 (live, no restart); hold one by the shoulder and step in to bring the other hand to its head; let go while
inside: eased apart, never stuck; is Step In a net plus or minus (judo, neck snap, shoves)?
## Review fixes: foegrab3, hotblade, shovefix, magfix2, flatplay, -preparerelight (worktree `fixgame3`, 2026-10-14)

From the r5/r6 reviews of `next` (one commit each):
- **Neck snap and the stick's turn.** Each wrist's turn is measured in play space (the hand's angles less the play
  space's yaw, `move.vrYaw`), so smooth or snap turning with both hands on a head no longer reads as a twist (it
  snapped: 32 deg at 187 deg/s). `judo_test.py neck`: `smooth_turn`, `snap_turn` (none), `twist_turn` (snapped).
  The ragdoll cases (`knocked_*`, `pp_knocked_*`) are flaky run to run (a ragdoll grab missing its sphere), and
  `pp_knocked_slow` snaps at 117-145 deg/s on the base build too: pre-existing.
- **Foegrab lows.** A failed lift (or judo throw) restores the monster's velocity (no hop); a lifted enemy killed in the
  hands has its flail ended when let go of (`Holder::flailing`); this round's neck snap note says config 132.
- **Hot blades.** A hand's swing trail takes its tip's speed from the hand's own play-space velocity and turn, and its
  last frame is carried with the player's move and stick turn: walking, flying or turning with a still hot blade leaves
  none (was 109-207 particles a second; the drawn weapon lags the hand a frame, so the tip's world step alone can't tell).
  The report poses the glowing vertices only when `vr_hotblade_report` runs. Turning the option off clears the lying
  weapons' glow once. `hotblade_test.sh`: `trail_walk`, `off_lying` (29/29).
- **Config 129** migrates only the two speeds; the other eight shipped at their new values (vr_defaults.cfg), so an old
  value there is a player's and stays. `config129_test.sh` updated (PASS).
- **Release Seated Magazine** (`vr_reload_mag_release`, default 1, Weapons > Reloading; his decision on the magfix2
  finding): a held magazine leaves the hand as it seats, so moving the hand on to the foregrip before letting go never
  pulls it back out; a fresh grip pulls it out. 0: magfix2's re-pull without letting go. `reload_test.sh`: 111/111 (the
  old re-pull case runs with it off; a new case seats, moves apart to the front grip, lets go, re-grips, pulls).
- **Flatscreen play.** The arms' walking lag is back along his horizontal motion whatever way he faces (at yaw 90 it
  slid the guns ~11 units sideways), the whole capped by maxTrail. In co-op the mode is each player's: his client sets
  bit 15 of the move's `vrBits0`, the server takes it out and sets `.vr_flatscreen`; `VR_FlatPlay(player)` gates
  reloading, `E`, `1`/`2`, the grip-less catch and the body's hand touch. `flatplay_test.sh` (new, 4/4).
- **-preparerelight** implies -prepare everywhere: `COM_PrepareRun()` (common.c) for the level-start autosave check
  (host.c), the hidden window (gl_vidsdl.c) and `prepare::active()`.

Smoke (e1m1, e1m2) clean; melee eval 168/177 (the one known FAIL -> FAIL timing difference, as before).
Open: flying forward in flatscreen play the hands still sit ~33 units behind (the hands' world-space easing,
`handEase`, adds ~20 to the capped 12-unit lag): easing them in the player's frame would bound the trail by maxTrail,
but changes the feel (his call).

## Neck snap: the hand-line detector (his idea), selectable

`vr_foegrab_necksnap_method` 0 (default: the velocity strokes, unchanged) / 1 (Hand Line: the line between the tracked palms, laid across the neck, compared with each sample of the last `vr_foegrab_necksnap_window` s, default 0.3; turned Twist Angle within it snaps; the wrists rolling the same way, summed over the same window, also snaps). Standing it is measured in the enemy's yaw frame; a ragdoll's in play space (its chest turns whole with the hands: measured against the chest a 75 deg twist read 27). The stick's turn is taken out (old samples turned by the play yaw now). Window 0.3 not 0.4: a 1.2 s push-pull turns the line 33 deg in its busiest 0.4 s, 26 in 0.3 (30 deg / 0.3 s = 100 deg/s, the velocity method's Twist Speed). Menu: Neck Snap > Twist Detection, Twist Window (always shown, help says Hand Line only). `vr_foegrab_status` prints the method and the windowed angle. Tests: judo_test.py neck under `JUDO_NECK_METHOD=1` 29/29 (largest no-snap 26 deg, the 3 s 60-deg shuffle 6), method 0 29/29, dir+lift 22/22.


## Tracy stage 2: QC, jobs, loading, resources and allocation lifetimes (2026-10-14)

- Default coarse capture extends QC VM, worker/claiming-thread jobs and waits, file/model/texture/image preparation,
  GL uploads, map/save/load, physics, audio and network regions. New scopes retain caller file/line; detailed QC
  functions/builtins copy transient names/files rather than retaining unloaded program pointers. Existing profiler stays intact.
- `vr_tracy_qc_detail=0`, `vr_tracy_memory_detail=0`; GPU defaults remain 1/coarse. Debug > Reports controls and normal
  build information added, calibration paths 5 found/0 missing. Registered system capacities sampled every 60 connected
  frames, summed for shared scratch/cache labels; frame draw/triangle/particle and VR timing plots have explicit limits.
- Exact named physical Zone/Hunk/Cache block events account for baseline, realloc, frees, cache relocation, hunk reset,
  detail transitions and reconnect. Native Box3D/default allocation macros retain requested-byte accounting and a worker-safe
  live mirror, preserving survivors at late attach/reconnect. Allocator behavior and gameplay are unchanged.
- A true decoded checkpoint test caught v0.14.1's unknown-MemoryDiscard serial-clock edge. Only announced names in the
  current session are discarded; fresh sessions begin with allocation baselines. All final decoded checkpoints match the
  independent engine pool/native Box3D state, not merely pointer lifetime validity. No ignored memory faults.
- Native reader uses matching official staged server/file-reader source and test-only pinned dependencies, with no game
  dependencies or downloads. Full captured evidence, controls, build options, coverage/limits and measurements are in TRACY.md.
- Normal/default profiling builds, normal profiler smoke, stock save/load/error, simple-CSQC error/recovery, representative
  expansion/integrated native AD and two captures with surviving/growing/freed bodies passed. Callstack capture proves actual
  zone stacks; optional ETW compile passed without any elevated run. General heap/scheduler data remains explicitly scoped.
- Exclusive uncapped e1m1, 3×1200 frames/state: CPU busy normal 1.0758 ms, idle 1.0288, coarse 1.1434, details 1.5053; all zero >33 ms hitches/drops. Raw rows versioned in TRACY_STAGE2_OVERHEAD.csv; scene/headset variability explicitly documented.
- Disabled argument hooks are non-evaluating macros; an -O0 fixture proves dynamic names/resource expressions have no side effects. Resource/metadata walks remain build/connection guarded.
- Final allocator review defers snapshots during the temporary hunk-header/cache-relocation window and rejects cross-session frees; host recovery clears the guard. Existing lifecycle replay verifies decoded checkpoints after this change.

## 2026-10-14: exact gun hull reuse

Retain winning scored points/initial hulls and reuse unchanged completed-piece gaps. 278 frozen-output comparisons preserve point/piece order and non-timing report bytes, including refinement/failure and parallel cases. Narrow profiling separates decomposition from cache preparation: voxelization remains dominant; no strong whole-hitch speedup established. Normal/profiling builds, smoke and gun port/well checks pass; melee retains 168/177 baseline verdicts. Details and temporary-memory limits: GUN_HULL_REUSE.md. Author VR check: held/mirrored and dropped gun ports/wells.

## 2026-10-14: multi-scenario stacked profiling

Six validated depth-eight captures cover e1m1 AI, cold/warm weapons, wound-driven ragdolls, explicitly drive-disabled sleep, verified native holding and integrated AD. Matching AD depth-zero pair has the same state hash/counts; stacks add roughly 9-11% mean CPU in this pair. Whole 3D GPU scope is now available at coarse detail. New source-backed findings and synthetic/reference limits: MULTI_SCENARIO_PROFILE.md. Artifacts backed up outside worker trees in kit/scratch/profilemulti. No gameplay changes.

## 2026-10-14: shadow caster CPU collection (adcpuopt)

Reserve/write each surface's exact triangle fan in one batch; collect ordered brush candidates locally per shadow-render invocation, including portal cameras, retaining live per-light eligibility/transform checks. No cvar/default/budget changes. Narrow world/brush/alias/candidate CPU scopes expose their shares. Normal/profiling builds and normal smoke pass; 12,264 exact oracle collections (216,614,502 indices) preserve index bytes/caster order/ranges, and all 18 active-light before/after atlas hashes match across moving runtime brushes, aliases, light removal, water, two virtual portal lights, rebuild and map reload. One exclusive matched AD pair: action caster collection 0.09154→0.08265 ms/frame plus new scan 0.00187; full shadow CPU 0.18442→0.17140, GPU 0.05450→0.05448. Counts/settings/endhash match; steady whole-frame busy rises, so no general FPS/tail claim. Short QC-function-detail diagnostic identifies liquids/wounds frame scans as future investigation targets; generic QVR builtin time is unresolved by operation, and QC/physics/melee stay unchanged. Details/limits: AD_CASTER_CPU_OPTIMIZATION.md; private evidence: kit/scratch/adcpuopt. Author VR check: AD combat, moving items/lifts and flashlight shadows through gates at actual resolution.

## Liquid/wound frame scan CPU cost (2026-10-14)

The live findflagsdue iterator moves only the existing float deadline/nonzero-field gates from QC into native
code. Callbacks, edict order, intervals, cvar gates and spawn/remove/reuse behavior stay the same. No new
settings, persistent fields or quality changes. Three interleaved AD pairs save 29.46 µs/frame in detailed
liquid+wound scopes; coarse QC VM saves 13.93 µs steady and 12.01 µs action. Settings/counts match, every
pair improves, and whole-frame variation is not an FPS claim.

Reference scans, NaN/infinity/signed-zero/boundary cases, liquid/wound/dummy/swimmer/model-zero paths,
vanilla/AD transitions and save/load pass: 30 full entity-state checkpoints and ordered records match.
Restored normal/Tracy builds and final normal e1m1 smoke pass. See
[QC_LIQUID_WOUND_CPU_OPTIMIZATION.md](QC_LIQUID_WOUND_CPU_OPTIMIZATION.md) for methods, absolute times,
limits and private kit evidence. Author follow-up: ordinary liquid combat/wet corpses/dummies in VR.

## 2026-10-14: exact voxel optimization (voxelopt)

Reuse exact barycentric fractions and ordered chamfer neighbours, with an interior-cell fast path and 4.1 KB bounded stack storage. No geometry/settings changes. Against the current d986 baseline, 288 fixture comparisons preserve sampled points, voxel states/distances, pieces/reports and rebuilt hull mass bytes; allocation balance zero. Eleven frozen inputs, 16 alternating repetitions: voxel medians improve 22.5-37.4%, sequential decomposition 5.8-19.9%. Live captures were excluded because scene/input matching was insufficient; no live FPS gain claim. Normal/profiling builds, smoke, shotgun port/fallback and lying nailgun magazine checks pass. Details: VOXEL_OPTIMIZATION.md; private evidence backed up in kit/scratch/voxelopt. Author VR check: held/mirrored/dropped gun ports and wells.

## Peripheral hit and low-health blood feedback (bloodfx, 2026-10-14)

Two independent cosmetic blood-red vignettes are on by default, with a clear central circle and combined
opacity bounded at 0.8. VR Settings > Blood Feedback contains archived controls; Debug > Tools has directional,
neutral, armor-only and real damage-payload previews, plus quantitative reports. Defaults: hit on, duration
0.45 s, intensity 0.22, directional bias 0.75; health on, threshold 35, intensity 0.25, individual beat width
0.25 s, base rate 0.75 Hz, low-health rate scale 2, severity exponent 1. Full details: BLOOD_FEEDBACK.md.

svc_damage is unchanged, including legacy colour shifts, view kicks, hand feedback and haptics. Separate hit
world directions are reprojected per rendered eye/view; front maps to upper edge, rear to lower, vertical
hits to their corresponding edge. Missing/near/zero sources are neutral. A listen server can also recognize
world/self/owned-projectile hits by an exact protocol-quantized inflictor-centre match. Remote servers and old
demos have no ownership metadata, so their available coordinates remain directional. This is the conservative
compatibility limit. Effect clocks freeze paused; death/intermission/respawn, clear-state/map/save-load/disconnect
and reverse demo playback clear history. Health is reconstructed from local authoritative STAT_HEALTH.

Existing scene resolve/warp applies blood before colour shifts; no new GL state. Movement comfort, bullet-time,
palette modes and full-black fades retain their existing composition. Portal cameras are excluded. Legacy damage
tints remain independently enabled, so stronger combined feedback can be reduced through Screen Tints.

Validation: normal and Release-Tracy builds passed; native Quake/AD and Tracy e1m1 damage+health smoke passed.
28 production-engine quantitative cases passed. At intensity 0.6/bias 1, actual damage-payload left/right and
front/rear/vertical edge peaks were 0.5671 against 0 opposite; neutral 0.5671 all round; armor-only 0.5671.
A 180-degree head turn swaps the preferred side. At 0.5 s of a 1 s hit, opacity 0.15; halving intensity gives
0.075, then doubling duration gives 0.1687. Pause leaves the clock unchanged across 72 frames. Independent
toggles, threshold/healing, death/respawn, fresh save/load, map/disconnect all passed. Default health 34/20/10/1
gives peak 0.0071/0.1071/0.1786/0.2429 and rate 0.7929/1.3929/1.8214/2.2071 Hz; at threshold/death, zero.
Exponent 2 at health 20 lowers peak to 0.0459; rate scale 0 keeps 0.75 Hz. A recorded demo retained health 20,
reverse playback cleared hit history/clock to zero, and demo pause retained clock 0.1111 across 20 frames.

Both eye images: inactive max difference 1 byte (mean below 0.000003), hit centre max difference 0–1 byte,
intended peripheral green attenuation 2.75–3.03 bytes versus 0.09–0.10 opposite, full-black pixels exactly zero.
Flat scene inactive/centre differences <=1 byte and black scene pixels zero; pause/HUD overlays remain visible.
Native baseline/left/right/health1 comparison uses test intensity 0.6 to make bias legible. Calibration board
link rebuilt successfully (qbsp/vis/light all exit 0), menu-path check 6 found, 0 missing.
Evidence/scripts/results backed up to C:/OHWorkspace/qvr-kit/scratch/bloodfx.

Author VR review: judge mild default strength and comfort, front/rear convention, head turns, low-health beat
frequency/width, and interaction with movement vignette and black fades. No melee/gameplay changes.
The test's early artificial health-zero/health-100 sequence made a later save fail to parse; a fresh-map
save/load passed, so the permanent harness isolates saves from that artificial death state.


## Adjustable blood-vignette coverage (22:08:07 note; bloodfx_size)

The requested size adjustment follows the backlog-first triage published in 5f7ed2009. Only blood coverage
changed: archived vr_blood_hit_size and vr_blood_health_size independently control Hit Coverage and Health
Coverage on VR Settings > Blood Feedback. Both default to 0.65, range 0-1 in 0.05 menu steps; zero hides only
that effect, one gives the widest coverage. Existing intensity, direction, damage handling and pulse controls
are unchanged. Each effect now receives its own radial mask before bounded alpha composition.

Defaults move the clear/full radii from the old fixed 0.6/1.162 to 0.4125/0.8475, making the red reach much
farther into peripheral vision. The full range runs from a narrow outer rim to broad coverage, while even the
widest setting retains a clear centre of radius 0.15 (normalized screen coordinates). Values clamp to 0-1;
NaN becomes zero. New cvars acquire defaults automatically, without changing existing intensity preferences.

Normal and Release-Tracy builds passed; normal VR/flat capture smokes and Tracy smoke passed. Calibration
board text rebuilt (qbsp/vis/light exit 0); menu path check: 6 found, 0 missing. Focused production-GPU tests
covered 18 size/toggle states per view, without repeating unrelated original regression suites. At sizes
0.20/0.65/1.00, left-eye hit pixels affected by more than two bytes were 416757/810474/979310, health
425454/815569/981620. Right-eye hit counts: 416409/810607/979352; health425469/815517/981672. Flat scene:
hit121509/253294/309117, health123880/254965/309754. Every view increased monotonically, with no pointwise
coverage regression over two bytes. Changing an inactive effect's size, zeroing only its size, or disabling
its toggle changed the independent effect by at most one byte. Clear-centre differences were at most one
byte in the eyes, zero flat. Negative, above-range and invalid sizes matched their bounded reference images.
Retained scalars at the captured peak: hit0.2200, health0.2429, rate2.2071 Hz.

Off/default/wide comparisons use original default intensities and actual eye captures; desktop mirror
updates can lag uncapped mock frames. VR and flat comparisons visibly confirm the wider default and broader
maximum while preserving the clear centre. Reusable test: Misc/quakevr/test_blood_size.py, run with the bundled
image Python, optional --tracy-smoke. Evidence and scripts: C:/OHWorkspace/qvr-kit/scratch/bloodfx_size.
Author VR review: choose preferred independent coverage and confirm the new defaults are comfortably visible.

## The spawn tool's ghost, held by a hand (worktree `tghost`, 2026-10-14)

The spawn tool's ghost was moved and turned with the sticks, which took the player's walking and turning away and put
the ghost's controls on a thumbstick. It is held by the hands now, and the spawn tool takes no stick at all: the off one
walks, the gun's turns, as with any other gun. No new cvars, no new panel rows; `vr_toolgun_snap` and
`vr_toolgun_turn_axis` are the physgun's alone (their panel rows already live on its page).

`Quake/vr/vr_toolgun.cpp`: `GhostHold` (which hand moves the ghost, which turns it, and how far the holding hand has
gone), `ghostShowing()` (the toolgun held, its aim and the choice valid, not picking, the spawn tool), and
`ghostHoldFrame()`, run from `toolgun::frame()` before `sticksFrame()`. The other hand's A / X press takes the hold and
nothing else: from then on only that hand's *translation* counts, applied 1:1 in the gun's level axes (`levelAxes()`:
its forward flattened, its right, the world up) as offsets, so the ghost never jumps at the press and that hand's
rotation does nothing. The gun hand's A / X press holds its turn: the ghost's yaw only, from the change in that hand's
way (`handTurnWeight`, the physgun wrist's damping, so a hand near straight up or down does not spin it); its pitch and
twist and the offsets are untouched. A release ends the hold and keeps what it did; `reset()` (the panel's Reset,
`vr_toolgun_reset`) clears both. `toolgun::button()` swallows that hand's A / X while a ghost shows — no jump, and the
hook never sees it (`primaryHeld` is set from the key actually sent) — and with nothing chosen, picking, or another tool
in hand the button goes through exactly as before. `sticksTaken()` is unchanged: the spawn tool never takes a stick. The
gun's stick press still cycles the tools, A / X held still the one before. `vr_toolgun_status` prints the offsets, the
turn, which hand holds it, and where that hand is and how far it has followed.

`Misc/quakevr/toolgun_spawn_6dof_test.sh <agent>` (new) proves it in vrfiringrange with a grunt chosen, both hand
orientations. The A / X presses, in order, are taken by the tool, by the tool, *not* taken with no choice, by the tool,
by the tool, by the tool. A hand moved by -20 +10 +15 (the player faces west: 20 along the gun's forward, 10 to its
right, 15 up, its aim `-0.87 -0.00 -0.50`) gives offsets `20.0 10.0 15.0`; turning that hand in place changes nothing;
walking with the off stick while its A / X is held moves the player 83 units and the offsets not at all; the trigger
places the grunt at `44 -546 57` where that ghost stood (`45 -546 57`); the sticks then walk and turn the player with
the ghost untouched. The gun hand's yaw +90 turns the ghost `0.0 -> 90.0` with the offsets unchanged, its pitch swings
the aim 0.2 degrees and the ghost 0.0, its twist swings the aim -37.7 and the ghost -37.6 (the turn follows the way and
nothing else); its stick still turns the player and its press with A / X held lands on the tool before. Offsets survive
a tool change; Reset clears offsets and turn to zero; the physgun with A / X held reports `adjust 1` as before. With the
gun in the off hand (a fresh map: `impulse 189` gives it there only when the main hand does not hold one) the same holds
with the hands swapped: the main hand's A / X moves it `20.3 10.2 15.2`, the off hand's yaw turns it 90.0.
`toolgun_panel_test.sh` and `toolgun_joint_menu_test.sh` pass unchanged (70 picks, 0 wrong, both hands), the melee
canary is unchanged, e1m1 smoke clean, build 0 warnings.

Note for the author's check: turning *yourself* with the gun's stick while its A / X is held turned the ghost too (the
hold followed the hand's way in the world; changed by the review below: the play space's). Author VR check: choose a grunt, hold X in the
left hand and reach forward, aside and up; let go and place it; hold A in the gun hand and turn your wrist; then the
same with the gun in the left hand.

## Model vertex wobble: .mdl vertices dequantised at load (worktree `dequant`, 2026-10-15)

Backlog "Model vertex wobble" (1). `vr_mdl_dequant` (default 1; Graphics > Models and Effects > Steady Model Vertices):
each .mdl vertex follows the smoothest path through its animation within half a byte step of every stored value; 0 is
the old look exactly. Debug > Reports > Steady Vertices Check (`vr_mdl_dequant_check`). Design, numbers (near-still
wobble -46% to -100%, 114 ms load for 139 models), the proof for 0 and what to try in the headset: `MDL_DEQUANT.md`.

## Slow motion by accident, and get-ups that glide (worktree `flatfist`, 2026-10-15)

His notes of 2026-10-15 (BACKLOG.md): "Slow motion still turns on by accident: double-tapping the wrist gadget screen
during frantic melee/shoves" and "Active-ragdoll get-up moves the body around too much: it visibly glides on the ground
plane instead of just getting up".

### The screen tap in a melee's thick (vr_bullettime.cpp `calm()`)

His shipped tap is permissive (Double Tap, 0.2 m/s a tap, 80 degrees off straight, a 0.8 s window): two knocks of a
punching hand on the wrist in a melee are a double tap. Two guards, each checked every frame; while either holds, a tap
that lands is ignored (no click, no glitch, `bullet time: a tap at N m/s ignored (...)` with `vr_debug_bullettime 1`)
and a double tap's first waiting is dropped:

- **Quiet after blows** `vr_bullettime_tap_blow_quiet` 0.5 s (real): after a melee blow of his lands, or he shoves or
  bashes (either hand). The QC stamps the player's `.vr_blow_time` (VR_Melee_Landed, the push code in VR_Bash, and
  VR_Bash_Hit, which `vr_knockdown_test 21` calls); the engine notes the realtime it changes.
- **Calm hands** `vr_bullettime_tap_calm_speed` 2.5 m/s (real, the play space's: walking doesn't count): within 0.3 s of
  the gadget's hand going faster, or the tapping hand moving that fast across the screen against it (its motion into
  and out of the screen is the tap's own and never counts: a hard deliberate tap is not a swing). Blows, shoves and
  bashes need 3 m/s or more (`vr_melee_speed` 3.2, `vr_shove_speed` 3.5, `vr_bash_speed` 3); his held poses settle at
  1.6 at most.

Both 0: as before. Menu: VR Settings > Bullet Time > Quiet After Blows (basic page, under Screen Tap); Advanced >
Combat > Bullet Time > Screen Tap: Quiet After Blows and Calm Hands Speed.

**Test** `Misc/quakevr/gadget_tap_melee_test.sh <agent>` (vrfiringrange, shipped tap settings, 1.7 m/s taps): a calm
double tap on, again off; a double tap 0.2 s after a shove (`vr_knockdown_test 21`) ignored, 1 s after on, off; a 4 m/s
swing across the screen then at once a double tap ignored ("the tapping hand"); the gadget's arm swung 3.8 m/s then a
double tap ignored ("the gadget's hand"); the guards off, the swing-then-double-tap on as before. 11/11 PASS.

### Get-ups in place (vr_ragactive.cpp `holdLevel`, `riseFrame`)

Measured first: each rise's body (its mass centre) traced level from where it lay (`getup: N level travel: ...`, a new
line with `vr_knockdown_debug 1`; `vr_debug_ragdoll 2` adds it to the trace). The cause: the roll over (a body lying
the other way up from its get-up's first frame: grunt, enforcer, knight) turns the whole body about the edge it rolls
over, on the floor: with the floor's grip it rolls like a wheel, and the edge is its farthest part on that side (an arm
flung out), so a knight rolled 50 units along the floor; then the pin pulled its pelvis back over 0.6 s, overshooting
the spot by 13. The out-and-back is the glide.

`vr_ragdoll_active_getup_hold` 1 (default; Gibs and Corpses > Ragdoll Settings > Get Up in Place): while rolling over its
mass centre's level motion is eased back towards where it lay (`holdLevel`: towards going back there over 0.15 s, at most
1 m/s, the same change to every part: its turn and pose untouched), so it turns over on the spot. 0: as before.

Headless (vrfiringrange, seeds 7 and 11, `vr_knockdown_test 0` then 1, and a sure one-hand shove `vr_knockdown_test 21`
with Chance 100 getting up on its own; 4 rises each; farthest the body went from where it lay, units, and its level path
while rolling):

| Monster | farthest, before | after | path rolling, before | after |
|---|---|---|---|---|
| grunt | 10-41 | 4-8 | 9-35 | 3-4 |
| enforcer | 39-54 | 16-24 | 27-40 | 5-8 |
| knight | 36-71 | 4-9 | 30-61 | 3-4 |
| death knight, ogre, zombie (no roll) | 5-12 | 4-12 | 0 | 0 |

What is left is the get-up's own: the body's middle comes over its feet as it stands (the enforcer's 16-24 units with
its pelvis still on the clip's within 3; the zombie rises up its fall backwards, 10-12). Tried and dropped: moving the
get-up's spot to where the pelvis lies after the roll (its first frame's pelvis on the body's): its stand ended farther
from where it lay (grunt 1-3 -> 2-10 units, enforcer 16-24 -> 26-30), no gain while pulled in.

Checks: `stagger_test.sh flatfist backpedal steady getup taken lost judo` (N 4): 42 PASS, 1 FAIL, `taken` once (the grunt rose
without the player on its spot: it lands differently each run), then 4 of 4 PASS with the hold and 3 of 3 without;
backpedal, steady, getup (handed back, 1 and 0), lost and judo all PASS. `eval.sh` 111 hits in 111 takes; e1m1 smoke clean;
build 0 warnings.

**In the headset**: shove grunts and knights over and watch them get up: they should turn over where they lie and rise
there, not slide off and back. Melee frantically near your wrist (punches, shoves, gun bashes with the off hand's gadget
in the way): slow motion should never start; then hold the wrist still and double tap: it should start as before.

## VR Settings' sections as submenus (worktree `bloodfx`, branch `agent/vrsubmenus`, 2026-10-15)

Backlog "VR Settings menu organisation": "turn each subsection of the 'VR Settings' menu into a submenu". VR Settings
(page 0, the Standard-level first-time page) was one 107-row list of 21 sections; it is now 28 rows: one link per
section, grouped under four headers (Calibration: Height, Hand, Holster Calibration, Ammo Pouch; Moving: Controls,
Locomotion, Comfort, Teleportation, Turning; Playing: Flashlight, Weapons, Bullet Time, Body, Haptics, HUD; Sight and
Sound: Lighting, Display, Scaling, Graphics, Sound), each link with a one-line help of what is inside, then Reset All to
Defaults (kept here, as it covers every section). The sections' rows moved unchanged, in their order, to 20 new pages
`VR Settings - <section>` (pages 175-194, `menu_vr list`; home VR Settings, level Standard). Advanced VR Options is
untouched, and so is the basic/advanced split: the new pages are Standard like page 0, and every row is still also on
its topic's Advanced page. Lighting moved from between Flashlight and Weapons to Sight and Sound; nothing else reordered.

`Quake/vr/vr_menu.cpp`: `pageMain()` (the links), `pageVrs*()` (the sections), `vrSettingsParts`/`vrSettingsPage()`:
the wrappers (Turning Mode, Move Towards, the hands, the holsters) are synced on any of these pages, Turning rebuilds
itself when snap turning changes (Snap Angle or Turn Speed shown), and Reset All gathers its non-`vr_*` rows (volume,
music, run, antialiasing...) from all of them. Blood Feedback's home is now VR Settings - Comfort. The HUD/crosshair
links from HUD and Menus > Crosshair, Wrist Gadget and Status Bar now open VR Settings - HUD. Boards:
`{menu:VR Settings - Height Calibration>Set Height Now}` (vrcalibration, vrtesthall) and
`{menu:VR Settings - Turning>Turning Mode}` (vrtesthall); both maps rebuilt with `--compile`.

Verified: `menu_vr dump` before and after (`Misc/quakevr/menu_coverage.py`: PASS, no option lost; a row multiset diff,
headers and the new links left out: 4642 rows before, 4639 after, the 3 missing being the HUD links above, retargeted;
161 pages reached before, the same 161 plus the 20 new after). Navigation by keys in VR and flat: VR Settings > Enter
opens VR Settings - Height Calibration (back to 0), Escape returns to VR Settings, Escape again to Options. Turning:
Turn Speed with `vr_snap_turn 0`, Snap Angle after `vr_snap_turn 45`. Reset All (twenty downs, Enter twice): volume
0.3 -> 0.7, bgmvolume 0.2 -> 1, vr_snap_turn 90 -> 0, vr_turn_speed 7 -> 3.25. `vr_menu_path_check`: vrcalibration 6
found, vrtesthall 14 found, 0 missing. e1m1 smoke clean, build 0 warnings.

Checklist (proposed): "VR Settings: each section is its own page (Height Calibration ... Sound); open a few, change a
value, Back returns to VR Settings; Reset All still there and still resets them; nothing you used to set there missing."

## Quad Damage on you: a blue glow (worktree `adcpuopt`, branch `agent/quadglow`, 2026-10-15)

His words: "an effect on your body and arms that looks different from the electricity effect ... a 'blue glow' + blue
dynamic light coming out of the player's hands/arms/body. Make it a new (default) option, keep the current lighting
arcs as an alternative." Until now Quad drew `shock::armArcs` over the hands and forearms (vr_view.cpp `quadArcs`, body
shown only), the very arcs lightning leaves on you when it strikes (vr_shock.cpp `drawSelf`, `vr_shock_self_*`), so the
two looked the same.

- `vr_quad_look` (1; Body > Quad Damage Look, under Show Powerups): 1 Blue Glow, 0 Lightning Arcs (the old look,
  unchanged), 2 Both. `vr_quad_glow` (1, 0..2, Quad Glow) and `vr_quad_glow_light` (1, 0..2, Quad Glow Light).
  `vr_body_powerups 0` still turns all of it off. New cvars only: no config migration.
- The glow (vr_view.cpp `quadGlow`, once a frame after `setupBody`): a strength breathing 0.64..1 (about 0.8 Hz, a
  slower swell under it), x 0.6 on the hand models (empty hands too) and x 0.3 on the body (seen from inside, its edges
  are all round the view), copies in a mirror alike (`view::quadGlow`). It reaches the alias shader through
  `VR_EntityGlow` as a new code, -2 - strength (after the toolgun, force grab, counter and hot blade codes, which keep
  their ranges): `QVR_ALIAS_FS_GLOW` adds a saturated blue `(0.12, 0.35, 1)` x (rim^2.5 x 1.4 + 0.08), faded out within
  16 units of the eye (`smoothstep(6, 16)`), so nothing glows right at the eyes and the view is never washed.
- The light: two unshadowed blue dlights at the hands (`emissive::quadLight`, keys -0x5980/-0x5981, radius 110,
  strength 0.8 x pulse x `vr_quad_glow_light`; inside the hand, so it lights the world, not the hand). The body's own is
  Quake's existing blue quad light round you (cl_main.c `EF_QEX_QUADLIGHT`), so no third light. Dead, at the
  intermission, in the immersive death view or with the quad gone: strength 0, lights killed the same frame.
- The lightning-strike shock still draws its arcs whatever the look (it never went through `quadArcs`).
- Debug > Tools > Test Effects: Quad Damage On You (`impulse 190; vr_quad_info`): `vr_quad_info` prints the look, the glow
  on hand and body and the lights.

Verified (mock, e1m1 start, looking down 50 at both hands out): `scratch/quad_glow_vs_arcs.png` (none, glow, arcs, glow
+ `vr_shock_self_test 10`, quad ended). Arm region mean RGB: no quad 118/87/65, glow 121/108/168, arcs 101/81/112, glow +
struck 128/123/191 (selfshock arcs=17 drawn over the glow); the floor 3.7/3.0/2.1 without quad, back to exactly that once
it ends (`vr_quad_info`: glow 0, no lights). Cost (`--exclusive`, `vr_profile_report 5`, 5 s each): arcs GPU 1.140 ms a
frame (models 0.121, world 0.294, 176 draws), glow 1.081 (models 0.127, world 0.307, 173 draws): +0.006 ms on models,
+0.013 on the world for the two lights, within noise overall. Build 0 warnings, e1m1 smoke clean (`vr_quad_look 2`: both).

Checklist (proposed): "Quad Damage (Debug > Cheats > Quad Damage): your hands, arms and body glow a soft pulsing blue
and the hands throw blue light round you; nothing glows in your eyes; Body > Quad Damage Look: Lightning Arcs brings
back the old arcs, Both shows both; struck by lightning you still get arcs; when the quad runs out the glow and light go."
## Review: the spawn tool's ghost hold (worktree `tghost`, 2026-10-15)

Review of 17baee890 / b41b7fc33 / e50500bcb / c41eb26d3 (merged in 777a064e5, b7ee7d69a). Fixed (`vr_toolgun.cpp`):

- The gun hand's A / X held + stick press from Spawn went to Brush, but the hold was the ghost's (`ghostHold.turn`), not
  `tg.adjust`, so in Brush A / X no longer had the stick and a second press went *forward* again (Spawn <-> Brush). Now
  A / X on the gun hand is always `tg.adjust`; the ghost's turn is derived each frame (`ghostHoldFrame`: adjust held,
  the spawn tool, its ghost showing), and `adjustTakesSticks()` (sticksFrame, sticksTaken) never gives the spawn tool
  the sticks (before: with nothing chosen, A / X held still froze walking for nothing).
- The turn counted the hand's world yaw, so turning yourself with the stick while A / X was held turned the ghost by
  that much on top of the ghost already facing you (its base yaw follows the aim): the 6DOF test's own run showed
  `turn 52.4 -> -38.6` across the stick turn. Now the hand's yaw in the play space (`handRot.y - turnYaw`): `52.4 ->
  52.4`; the ghost is carried round with you, turned as it was.
- `toolgun_spawn_6dof_test.sh`: a second back press with A / X held (Joint, adjust 1) and the stick-turn check; PASS.

Left for the author (not changed): the ghost's world yaw moves by twice the hand's yaw while turning (its base faces
you along the aim, which swings with the gun hand, and the turn adds the hand's yaw too) — the test asserts the turn
equals the aim's swing, so this looks intended; and the off hand's move integrates `hands::State::vel` over game time:
in slow motion on a headset that reports velocity (not the mock, which differences positions on game time) the ghost
follows the hand at the time scale's fraction. A position delta would be exact.

## The spawn ghost stays put while you turn it (worktree `tghost`, branch `agent/tghost3`, 2026-10-15)

Voice note (10:51): with the gun hand's A / X held to turn the ghost, it followed the gun as the hand swung, so the
turn was hard to see; and the turn should be about twice as fast.

- `vr_toolgun.cpp`: `GhostHold` gains `fixed`, `point`, `dir`. The frame the gun hand's turn starts (`ghostHoldFrame`)
  it keeps the aimed point (`ghostPoint`, split out of `updateGhost`) and the aim's way; while it turns `updateGhost`
  places and faces the ghost from those (`ghostDir()`), plus the offsets and the turn. Let go (or another tool, or
  the gun put down: `ghostHold = {}`), it follows the aim again with the turn kept. The off hand's move, if held at
  the same time, goes along the fixed way (`ghostDir()`), so it stays 1:1.
- The turn is `ghostTurnRate` (2) times the hand's yaw (was 1:1). With the base fixed, 90 degrees of hand yaw turns
  the ghost 180 in the world, as before (when the aim's swing added the other 90), but now in place.
- Stick turn with A / X held: the ghost stays put in the world, its turn unchanged (was: carried round with you).
- `vr_toolgun_status` says `fixed` / `following` after the hold.
- `toolgun_spawn_6dof_test.sh`: turn = 2x the aim's swing (both hands' guns), the ghost's origin unchanged through
  yaw / pitch / twist / a 0.3 m move of the gun with A / X held, its world yaw = the press's + the turn, no jump at
  the press; let go: `following`, turn kept, ghost moved > 8 units to the aim and facing it + the turn. PASS. (The
  off hand's 1:1 move read 21.6 vs 20 on one run, 20.4 on the next: timing noise in the velocity-integrated follow,
  untouched here.)

Checklist: "Spawn tool: hold A on the gun hand and turn your wrist: the ghost stays where it was and turns twice as
fast as your hand; let go and it follows the gun again, still turned."

## Melee space: monsters kept out of your head while they attack (worktree `tghost`, 2026-10-15)

His note: knights "often get too close and go through the player" when they attack. Why: the player's body is narrow
(`vr_hull_ent_width` = `vr_hull_width` 16: 8 a side) and Body Inset takes 4 more off each side, so a melee monster's
charge (`ai_charge` -> `movetogoal`) goes on until its box all but touches his head; the knight is the worst because of
its run attack, `ai_charge_side`: a 20-unit `walkmove` aimed 30 units to the side of his origin, a flyby that brushes
past his face. Others (ogre, shambler, hell knight, rottweiler) stop with their boxes ~18 units from the head; the fiend
lands its leap on top of him.

`vr_enemy_melee_space` (default 16 units, archived; Combat > Holding Enemies > Getting Close > Melee Space; 0 off):

- QC (`fight.qc` `VR_MeleeSpace_Step`, called after the step in `ai_charge`, `ai_charge_side`; AD's `ad_ai_charge`,
  `_chargenoturn`, `_charge_side`, `_charge_front`): a melee step that would bring the monster's box nearer the player's
  head (`headpos`, level) than the space, and nearer than it was, is cut back along itself (bisected, 6 steps). Not when
  he is over or under it. Each of those (and `ai_melee`, AD's `ad_ai_melee`, `_meleesmash`) stamps `.vr_melee_time`.
- Engine (`vr_bodycontact.cpp` `meleeSpace`, every server frame): a monster attacking him (stamped within 0.25 s, its
  enemy him) that is nearer all the same (he walked or leaned in, a leap landed) is walked back (`SV_movestep`: no
  ledges, no walls) by 12/s of the shortfall, at least 40 units/s. Held enemies (Step In) are left alone.
- `vr_enemy_melee_space_status [reset]` (Debug > Tests > Holding Enemies > Closest Melee Approach, and a Clear row):
  per kind, the closest its box and its origin came to the head, overall and while attacking, and how far it was pushed.

`Misc/quakevr/melee_space_test.sh <agent>`: vrtesthall, god, standing still, each monster spawned 128 ahead and left 900
frames. Head to box gap (level, units) while attacking, off -> on 16: knight 9.5 -> 16.1 (origin 22.4 -> 32.7), MG3's
infected knight 7.0 -> 16.0, fiend 1.6 -> 6.0 (its leap; then pushed back 57 units); ogre 18.0, shambler 19.0, hell
knight 18.5, rottweiler 18.4, MG3 death knight 18.1 unchanged (bodies meet first); zombie and mummy never melee'd; AD's
monsters do not spawn in the test install (same hooks). Knights stay well inside their blows' reach (ai_melee: 60).
Walking into an attacking monster (+forward): off, it stays pressed against him (knight 37.9, fiend 16.0, ogre 20.0 apart
afterwards); on, it gives way while he presses and ends 36.4 / 32.0 / 55.7 away. Melee eval 168/177 (the known
slash_horizontal_ltr timing row only), voreshove 4/4/3/4 shoves across off/on runs (the known 3-4 flake, unrelated: the
vore never stamps), e1m1 smoke clean, toolgun 6DOF PASS.

Author VR check: let knights, a fiend and an ogre attack you standing still, then lean in and step in; try Melee Space
0 / 16 / 24.
## Hits that unbalance (worktree `voxelopt`, branch `agent/hitstagger`, 2026-10-15)

His note (BACKLOG "His notes, 2026-10-15"): "Strong prop/weapon throws and melee hits make enemies lose balance like a
shove (chance based, and/or a %hp damage threshold, prop weight, ...); maybe strong fist hits to the head too."

`QC/vr_hitstagger.qc` (new; after vr_knockdown.qc). Hooks: `T_Damage_VRMelee` (combat.qc: every hand's blow, fist or
weapon, after its push) and `VR_Prop_Flung` (vr_carry.qc: thrown props and thrown weapons, after their damage).
- **Impact**: a throw's momentum into it (its mass x its approach speed, kg m/s) over `vr_hit_stagger_throw_ref` (60: a
  6 kg crate at 10 m/s); a blow's damage (its speed, weapon and zone already in) over `vr_hit_stagger_melee_ref` (30),
  times `vr_hit_stagger_head` (1.5) at the head (the positional zone, else the top fifth of its box; fists too). Plus
  `vr_hit_stagger_damage` (1) x the damage as a share of its full health (the %hp part).
- **Chance**: 0 under `vr_hit_stagger_min` (0.5: taps), rising to sure at impact 1; times (30 / full health) ^
  `vr_hit_stagger_resist` (0.75) above 30 health: ogre 0.24, shambler 0.11 at most; times `vr_hit_stagger_chance` (1).
  Never: a dummy that stays put (Dummy Stays Put), one down or staggering, in a parry's stagger, a boss (1000+ health).
- **Cooldown**: `vr_hit_stagger_cooldown` (4 s) from an unbalancing hit; shoves and bashes ignore and don't set it.
- **Reaction, the shove's**: pushed as a shove (520 u/s and a 140 hop x `vr_bash_push`, x impact clamped 0.5..1.25 x
  `vr_hit_stagger_push`); with Knockdowns and an active stagger, the ragdoll stagger of `VR_Knockdown_Try` (now
  `VR_Knockdown_Stagger`, shared), its balance the shove's roll against its knockdown chance x min(1.5, impact): it
  recovers or goes over. Others (no rig, flying, AD's): a forced pain and 0.75 s x push without attacking, as a bash.
- Menu: Combat > Knockdowns, "Hits Unbalance" (6 rows: on, Hit Chance, Least Impact, Head Blows, Big Monsters Resist,
  Hit Cooldown); Debug > Tests: "Hit Stagger: Chances of the Nearest" (40), "... Strong Blow on the Nearest" (42).
  `vr_knockdown_test 40..46` (VR_HitStagger_Test): 40 chances, 41 heavy throw, 42 strong blow, 43 weak tap, 44 head
  punch (the stagger alone, no damage), 45 state, 46 the nearest loose prop thrown into it for real.

Verified (headless, fresh monster per trial, 20 each): grunt heavy throw 20/20, strong blow 20/20, head punch 20/20 (all
ragdoll staggers; balance 0..1, some fell), weak tap 0/20; ogre heavy throw 7/20, strong blow 3/20 (chance 0.24);
shambler heavy throw 4/20, strong blow 1/20 (chance 0.11; pushed and pained). Cooldown: a second strong blow at once -1,
1.9 s left still -1, at 0 s staggers again; a staggering/down grunt never re-triggers. Real throws (test 46, a 40 kg crate
at 5 m/s, 194-215 kg m/s): stagger through VR_Prop_Flung. Shoves unchanged (test 22: a stagger, fell). Melee eval
168/177 (1 take differs FAIL -> FAIL: a detection timing, 0.375 -> 0.391 s, outside this change). e1m1 smoke clean, build 0 warnings.

Checklist (proposed): "Hits unbalance: throw a heavy crate or a weapon hard at a grunt, or punch/axe it hard (the head
counts more): it staggers back as from a shove (sometimes falls); light taps never; an ogre seldom, a shambler rarely;
hitting it again right after doesn't re-stagger it (4 s). Combat > Knockdowns > Hits Unbalance to tune or turn off."
## Moving the menu panel; one menu's state in a MenuInstance (worktree `bloodfx`, branch `agent/multimenu`, 2026-10-15)

Stages 1 and 2 of MULTI_MENU.md ("Several VR menus open at once"; its Status and the sync list for stage 3 there).

**Stage 1, the panel moved by a grip** (vr_menuui.cpp `PanelGrab`, vr_panel.cpp `MenuPlacement`). Point at the
panel's frame (a band 8% of its height just outside it; lit in the laser's hue while the laser is on it or the panel is
held) and hold a grip: the panel is taken at once, the grip's key not sent. On the panel's face (`vr_menu_grab 2`, the
default) a grip held takes it once the hand has moved `vr_menu_grab_deadzone` cm (4) — not after a stick was pushed
during that grip or with the trigger held: a grip held still stays the sliders' fine modifier, as before. Held, the
panel keeps its place in the laser's frame (moved and turned with the hand; a long lever at the menu's distance, so
small wrist turns swing it far), kept upright (its top edge level); let go, it stays, kept in the play space (relative
to the player's box and turn: it walks and turns with you) until the menus close; opened again it is in front of the
head as before. `vr_menu_grab` 0 never, 1 the frame only, 2 frame or face (VR Settings > Menu > "Move the Panel").
Debug > Tools: "Menu Panel Place" (`vr_menu_panel`: MENUPANEL placed, held, armed, middle, from the head, facing, size)
and "Menu Panel Back in Front" (`vr_menu_panel reset`). Tests: `vr_mock_laser frame` puts the main laser on the frame.

**Stage 2, MenuInstance** (vr_menu.cpp): the page shown, the Back stack, each page's cursor/scroll/anchor/visits, the
slider and scrollbar drags, the slider hold, the drop-down, the help box and paging, the rows' left edge, the
scrollbar's x, the armed resets, the body clone latch and the noted place are one struct; the code reads `ui().<field>`
(~310 uses rewritten by a scope-aware script: locals, parameters and members of the same names untouched; none inside
a struct body). `menu::instanceCount` (1), `currentInstance()`, `selectInstance()`. vr_panel.cpp keeps a `MenuPanel`
per instance (placement, its yaw when it appeared, its canvas handle: the 2D layer's canvas today). Kept shared on
purpose: built rows and their latches, the settings, Quake's menus, the keyboard pages (MULTI_MENU.md says why).

Verified: `Misc/quakevr/menu_grab_test.py bloodfx` 13/13 PASS (frame grab held at once and the grip key taken; a 30
degree hand turn turns the panel 30; a 30 cm move moves it 9.4 units; let go it stays; the face armed, not taken within
2 cm, taken after 10 cm without a jump; a stick push keeps it the fine modifier; `vr_menu_grab 0` inert; reopened in
front of the head). Stage 2: `menu_vr dump` at Menu Detail Developer byte-identical before/after (182 pages, 4719
rows; `menu_coverage.py` PASS, `menu_rows_diff.py` 0 lost 0 added); a VR navigation log (keys down/Enter/Escape, the
laser on corner buttons and a row, right steps, stick scroll; `menu_vr pos`/`rows`) and a flat one (keys, desktop
mouse click) identical but the build hash. e1m1 smoke clean, build 0 warnings.

Checklist (proposed): "VR menus: point just outside the menu panel's edge (a frame lights up) and hold a grip: the
panel follows your hand, let go and it stays; a grip held on the panel itself and your hand moved a few cm moves it
too; holding a grip still while stepping a slider still gives fine steps; Back to Game and reopen: in front of you."
## Model vertex wobble 2: loop seams and shading flicker (worktree `dequant`, 2026-10-15)

His note: "a noticeable itch at the loop point for certain animations (e.g. grunt and knight idle)", and some
wobbling/pulsating left. Measured (`vr_mdl_dequant_check`, near-still axes: 3 steps or less over the loop):

- **The itch was ours.** QVR's soldier, knight, dog and player have no frame names (knight: "1".."97"), so the idle loop
  was merged into the next animation (knight 0..16: stand + run) and smoothed with free ends: the seam (stand9 -> stand1)
  got up to a step of kink while the inside got smooth. Seam second difference rms: knight idle 0.63 stored, 1.26 before,
  soldier idle 0.90 stored, 1.85 before.
- **Fix: the loops come from the progs.** A model's loops are the cycles of the QuakeC frame functions
  (`[$stand9, knight_stand1]` = OP_STATE) reached from the functions that name the model (monster_knight's
  `th_stand = knight_stand1`); string immediates only (not variables, locals or the parameter globals, whose live values
  lie), a loop to the models naming it at least half as often as the most, overlapping loops: the most named. 575 loops,
  built once per progs in 3-5 ms; e1m1's 139 models: every stock monster's stand/walk/run found. Smoothed cyclically.
  Knight idle seam 1.26 -> **0.22** (inside 0.44 -> 0.17; max at the seam 3.67 -> 1.00 = the inside's max), soldier idle
  1.85 -> **0.23** (inside 0.69 -> 0.13; max 4.67 -> 1.00, inside 0.81). Walk loops whose artist's seam is itself a hitch
  keep part of it (knight walk: stored seam 2.18 vs inside 0.72 -> 1.05 now): the half-step box allows no more.
- **Pulsating: the shading.** An idle knight's near-still vertices turn their stored normal 7.5 degrees rms a frame (55%
  of frames change direction: the 162-direction table, 14.5 degrees apart). Now each normal component is smoothed along
  the same runs within 0.1 of the table direction's (at most 10.2 degrees from it; the table's own error is up to 10.7):
  knight 7.47 -> 2.16 degrees, soldier 2.09 -> 0.68, zombie 6.09 -> 1.81, player 4.85 -> 1.09, ogre 11.1 -> 4.7.
- **Not the solver nor the blending.** A third-difference objective (prototype) gave the idle loops the same second
  differences (knight seam 0.240 vs 0.222, inside 0.172 vs 0.173); linear blending between the smoothed poses departs
  from a smooth curve by about d2/8 = 0.02 steps (0.007 units): nothing to fix.
- Off is still the bytes exactly: the seeded ogre scene with 0 gave the base commit's own image (md5 355b30a9...) and,
  in another run, the known 1/255 noise (429 pixels). Load: 139 models 202 ms (`--exclusive`; was 114: the normals
  double the solves); memory 8 bytes per pose and vertex (7.6 MB for those 139).
- The check prints, per model, the progs loops' seam and inside (stored, without the progs' loops, now) and the
  normals' turn; one model named: each loop's.

## Pentagram and biosuit glows, like Quad's (worktree `adcpuopt`, branch `agent/powerglow`, 2026-10-15)

His words (10:52): "make the protection pentagram give you a similar glow and also the bio suit to give you some sort
of green glow ... so that they're all recognizable during gameplay."

- The Quad glow (above) is now a power-up glow (vr_view.cpp `powerupGlow`, `view::powerupGlow`): Quad's blue, the
  pentagram's red `(1, 0.05, 0.08)` (deep, gain 1.5: Quake's own yellow pentagram flash over the view washes a lighter
  red out), the biosuit's green `(0.25, 1, 0.18)`. Same breath, same rim-over-wash shader, same hand lights
  (`emissive::powerupLight`, was `quadLight`, now with a colour; keys unchanged).
- The shader code carries the colour: `-2 - 2 x (r + 16g + 256b, 16 levels a channel) - strength (0..1)`, decoded in
  `QVR_ALIAS_FS_GLOW` as the hot blade's code is (its old `-2 - strength` was this with colour 0).
- Several at once take turns: each shows 1.6 s, crossfading into the next over the last third of its turn (colour,
  strength and light), so each stays recognizable (a blend would be a muddy purple / cyan).
- New cvars (archived, Body page under the Quad rows): `vr_pent_glow`, `vr_pent_glow_light`, `vr_suit_glow`,
  `vr_suit_glow_light` (2, 0..2: as his Quad settings). 0 glow: the old body tint (pentagram's red pulse, biosuit's green
  multiply) comes back instead; with the glow on, the tint is skipped (they doubled up). `vr_body_powerups 0` still
  turns all of it off. The ring of shadows is unchanged (translucent body, no glow).
- Debug > Test Effects: Pentagram On You (`impulse 191`), Biosuit On You (`impulse 193`); `vr_quad_info` is now
  `vr_powerup_info` (which glow, how many take turns, its strength and code on hand and body, the lights).

Verified (mock, e1m1 start, looking down at both hands): `scratch/powerup_glows.png` (none, quad, pent, pent glow 0 =
old tint, suit, suit glow 0), `scratch/powerup_glows_defaults.png` (none, quad, pent, suit at the new defaults: Quad's
look 2 draws arcs and glow). `vr_powerup_info`: pent code 287 (r15 g1 b1), suit 1012, quad 3922; with all three: quad,
quad, pent, pent, (crossfade, code 650), suit over 4 s; lights 0.77/0.23/0.15 (pent), 0.21/0.54/0.16 (suit). Build 0
warnings; e1m1 smoke clean.

## His blood, quad and combat tweaks are the defaults (worktree `adcpuopt`, 2026-10-15)

"I have tweaked settings for the blood feedback, for the quad effects, and some other damage and combat settings.
Please make those the new defaults." His config of 2026-10-15 11:06 (version 132) against this build's shipped
defaults (`resetcfg; writeconfig`): 102 archived settings differ; 33 promoted (compiled defaults, vr_cvars.inc);
config 133 moves a config still at an old value; one set by hand is kept.

| cvar | was | now |
|---|---|---|
| `vr_blood_health_intensity` / `_size` | 0.25 / 0.65 | 0.3 / 0.75 |
| `vr_blood_hit_bias` / `_duration` / `_intensity` / `_size` | 0.75 / 0.45 / 0.22 / 0.65 | 0.9 / 0.4 / 0.4 / 0.6 |
| `vr_quad_look` / `vr_quad_glow` / `vr_quad_glow_light` | 1 / 1 / 1 | 2 (glow and arcs) / 2 / 2 |
| `vr_bash_damage` | 10 | 5 |
| `vr_melee_phase_time` | 0.35 | 0.2 |
| `vr_foegrab_lift_time` | 2 | 3.5 |
| `vr_foegrab_necksnap_method` / `_speed` / `_angle` / `_window` / `_neck` / `_strength` | 0 / 100 / 30 / 0.3 / 3 / 1.2 | 1 / 210 / 80 / 0.2 / 1.5 / 1.25 |
| `vr_foegrab_throw_feet_speed` / `_push` / `_speed` / `_spin` / `_topple` / `_twist` | 30 / 240 / 2.5 / 0.55 / 200 / 140 | 20 / 220 / 3 / 0.5 / 180 / 130 |
| `vr_ragdoll_grab_force` / `vr_ragdoll_hand_stick` / `vr_ragdoll_hand_turn` | 3000 / 2 / 60 | 4000 / 4 / 70 |
| `vr_reload_port_sshot_radius` / `vr_reload_reinsert_clearance` | 3 / 4 | 3.15 / 1.5 |
| `vr_decal_max` / `vr_decal_bucket_max` | 768 / 32 | 512 / 24 (menu help's "shipped" numbers too) |
| `vr_body_bracers` | 1 | 0 (bare forearms) |
| `vr_pbr_strength` | 1 | 2.5 |

With them, the new `vr_pent_glow`, `vr_suit_glow` and their lights ship at 2, as his Quad.

**Not promoted** (look debug/test-only, personal or machine-specific):
- `vr_dummy_type` 2 (knight), `vr_dummy_gore` 0, `vr_dummy_gib` 0: the training dummy's test setup.
- `r_lerpmodels` 3: the experimental spline animation smoothing (vr_animsmooth.cpp, under evaluation with dequant).
- `vr_retrolight_model_steps` 16: the compiled default; the shipped 15 (vr_defaults.cfg, his 10-04 notes) never reached
  a config that saved 16 earlier, so likely a leftover, not a tweak.
- `vr_melee_phase_speed` 3.996, `vr_relight_strength` 1.1988: the default x 0.999 artefacts (as before).
- Personal: `vr_body_elbow_back` 0.35, `_hand` 0.5, `_lift` 8, `vr_bodycal_*`, `vr_height_calibration`.
- `vr_wofs_hs3_*_04`: the nailgun's third hotspot, type 0 (no effect).
- Window, recording, hardware, flat screen: `vr_foveated` 2, `vr_spectator_fov` 120, `_scale` 1.5, `vr_window_view` 0,
  `vr_mirror_hide_hud_text` 1, `vr_motion_*`, `vid_*`, `fov`, `gamma`, `contrast`, `scr_*`, `sensitivity`,
  `cl_alwaysrun`, `volume`, `ui_live_preview`.
- Tools and bookkeeping: `vr_toolgun_*` (brush grid 4, joint 4, panel size 9.5, scale uniform 0), `vr_relight_reload`
  0, `vr_menu_level` 2, `vr_notes` 1, `vr_menu_positions`, `vr_prop_id_*`, `vr_*_version`, `vr_tutorial_started`.

**Test:** `resetcfg; writeconfig`: all 33 at his values (and the four new glows at 2). A version 132 config with the 33
at the old values and `vr_bash_damage` 7 by hand, then `vr_migrate_config`: 32 at his values, bash damage kept at 7,
version 133. eval.sh: the same 36/45 and 168/177 as the baseline; only the bash hits' damage differs (6 -> 3, 10 -> 5).

Checklist (proposed): "Pentagram (Debug > Test Effects > Pentagram On You): your hands, arms and body glow a pulsing
red and the hands throw red light; Biosuit: the same in green; with Quad too, the glows take turns (blue, red, green);
Body > Pentagram Glow / Biosuit Glow 0 brings back the old tint." "A fresh config has his blood feedback, Quad (glow
and arcs, 2x), bash, neck snap (method 1), judo throw, lift, limb grab, reload, decal and PBR values, and bare
forearms."

## Permanent decals: the slowdown, a full atlas, on and off (worktree `permdecals`, branch `agent/permdecals3`, 2026-10-15)

His note (23:07:45): blood/decal slowdowns, exactly when marks bake (and whether only at the bucket limit), and off
leaving some marks and removing others. Details, tables and the rule: `PERM_DECALS.md` ("When a mark is baked", "On and
off", "Performance").

- **The slowdown**: the bake (`permdecals::flush`, every frame a mark baked) asked GL for the framebuffer, viewport and
  scissor; a glGet waits for the GPU. With the GPU busy (98%, as under a compositor) one flush took up to 325 ms of CPU
  and 230 of 711 flushes over 0.5 ms in a gore-2 crowd fight (vrfiringrange, 32 monsters, 16 blasts); now 0.09 ms at
  most, none over 0.5 (no glGet: it runs before `R_SetupGL` and leaves framebuffer 0, as the shadow maps do). Also the
  whole page table went up for each new page (152 MB in 6 s; now 0.43 MB, changed runs only), emptied slots were a
  scissored clear each (one draw now), the full-atlas eviction scanned every slot per page (an O(1) list now), and the
  program and texture were made at the first mark (5-10 ms; now in the load). New `vr_permdecal_bake_max` (1024 pages
  a frame; the rest stay live a frame or more).
- **When marks bake**: not at the bucket limit (`vr_decal_bucket_max` only lists the live marks): chips, scorches,
  plain blood 0.1 s after they show; spreading marks when spread; the gore's drying marks (sprays, runs, pools) after
  8 s; at once when the live ring lets them go early, and on doors and lifts.
- **Atlas full keeps marks**: a 2D array that grows by layers (`vr_permdecal_atlas_max`, 512 MB: 6 layers of 85 MB at
  4096); only full at that does a page give its slot up. A 1024 atlas at density 0.25 grew to 4 layers with none given
  up, its frame the same as one 4096 layer's (peak 1); capped at one layer it lost 2922 pages.
- **On and off, one rule**: every mark is kept, on or off; on shows them all baked; off (`vr_permdecals 0`) shows the
  newest `vr_decal_max` live, exactly as the game without the layer (frame-identical to the layer off from the start);
  marks let go while off are kept and baked once on (only those: 0.46 ms); `vr_decals 0` hides both (the frame with no
  marks), 1 brings them back. Saves keep the live marks not baked yet too.

Files: `Quake/vr/vr_permdecals.cpp/.hpp`, `vr_decals.cpp/.hpp` (`showRecord`, `pushDecal`, `bakeSettled`'s budget and
switch, `liveStamps`), `vr_glsl.h` (array sampler), `vr_gfx.hpp/_gl.cpp` (`gfx::update`), `vr_cvars.inc` (two cvars),
`vr_menu.cpp` (help texts). `Misc/quakevr/permdecal_test.sh` (7 runs: cap/doors/saves, burst budget, atlas growth, the
on/off frames; `permdecal_shots.py`) PASS under his defaults (512/24); build 0 warnings; e1m1 smoke clean. Off against
the base build: 33 pixels of 518400 differ by 1.

Checklist (proposed): "Permanent Decals: a big gore fight (e1m1 or a crowd) has no stutter as blood lands; turning
Permanent Decals off leaves the newest marks (Max Decals) and on again brings every mark back, none darker; marks made
while off are there once on; save and load keep the freshest marks too."
## Bullet-time throws: the stroke's tempo (worktree `fasttests`, branch `agent/btthrow`, 2026-10-15)

His note: small flicks and tosses should feel small in bullet time (gun tricks, tossing a prop a few centimetres up to
re-grip it), smooth slowed full-arm throws should go full strength; the round-21 nudge/flick rules made an invisible
threshold past which a throw was suddenly strong again. Before (`flick_slowmo_compare.sh`, metres at 45 degrees, bullet
time 0.3x made at real speed / made 3.3x slower, over full speed): nudges 1.00 / 0.00-0.06, lob40 1.00 / 0.10, lob50
1.72 / 0.27, lob60 1.71 / 0.46, overhand0 1.47 / 1.01: the step between lob40 and lob50 and a 10x jump in the made-slowly
column.

Now (`vr_throw_slowmo_stroke` 1, default; `vr_throw.cpp` `strokeGain`, `strokeEstimate`): in slow motion (not
Sandevistan) a throw is its controller's velocity and spin in the player's real time times a gain. The stroke (as
`strokeOf` measured it: the controller's path to the release while its speed is over a fifth of its peak) has a tempo,
path over peak speed (real seconds: grows with how slowly it was made, and unlike the duration not moved by where the
speed's edges fall). A real-speed stroke of that path has `vr_throw_slowmo_stroke_time` (0.09 s) for a full arm's
(0.5 m to the release), down to 0.6 of it for none. The gain is tempo over that, clamped to 1 .. 1/timescale: a stroke
made k times slower is raised k times, so it comes out at the speed the same stroke has at real speed, however slowly
it was made (a slow 5 cm toss is still a 5 cm toss; a slowed full throw is a full throw). No threshold: the gain is
continuous in the motion. The release's windows are taken at the same multiple (real seconds for a real-speed stroke,
the game's for one made slowly with the world). The speed and way are the controller's (the slowed hand's 8 m/s cap and
lag no longer limit a throw), raised at most to `vr_throw_slowmo_max_speed` (15 m/s; never under the real speed). The
object still leaves the hand as drawn. Both-hand throws the same. `vr_throw_slowmo_real_strength` 1 still means gain 1.
`vr_throw_slowmo_stroke 0`: the old rules (`_flick`, `_flick_arm`, `_short_travel`, `_long_travel`, `_tempo`,
`_aim`), kept for an A/B. Timescale 1: untouched (the path never runs). Menu, the throwing rows after "Slow Motion: Throws in Real Time": "Slow Motion: Throw
Strength by Stroke", "Full Stroke Tempo", "Most Raised Speed". `vr_debug_throw 1` prints "the stroke X m in Y s, peak
Z m/s real, tempo T s: gain xG".

Tests (headless, `Misc/quakevr/throw_slowmo/`): `stroke_sweep_test.sh <agent> lob|toss` (new: 3 .. 70 cm strokes in
their real-speed time, full speed and bullet time made 1, 1.5, 2, 2.5 and 3.3 times slower), `flick_slowmo_compare.sh`.
Bullet time, made at real speed, over full speed: every flick, nudge and lob up to 35 cm x1.00; lob50 x1.00-1.23, lob70
x1.35, overhand0 x1.00, overhand (with its wrist, tempo 0.118 s) x1.49. Made 3.3x slower (slowly with the world): lobs
from 35 cm and both overhands x0.88-1.01 (overhand0 0.88); shorter strokes x0.1-0.9, rising smoothly with length (they
go about as far as the same stroke at real speed allows: the gain reaches 1/timescale only for long strokes). Every
column and row is monotone in the stroke, no step. Tosses at real speed in bullet time rise as at full speed: 3 cm 1.0
cm, 5 cm 2.4 cm, 8 cm 5.7 cm, 12 cm 11 cm. The parallel runs in `flick_slowmo_compare.sh` shared one worktree's console
log (a column could come back empty or another's): it now runs them one after another.

Trade-offs: a stroke made a little slower than real speed but faster than the world (1.5 to 2.5x) comes out between, a
long one up to 1.4x the full-speed distance (it reads as made slowly, its natural tempo quicker than the synthetic
lob's); a real-speed full throw in bullet time goes up to 1.5x as far (the old rules: 1.2-1.7x). His real strokes may
have other tempos than the synthetic min-jerk ones: Full Stroke Tempo moves the balance (lower: slowed throws reach full
strength sooner, real-speed ones raised a little more).

## Mid-air catches (same worktree)

Re-catching a tossed or spinning weapon by its handle. `Misc/quakevr/throw_slowmo/toss_catch_test.sh <agent> [cm]`
(new): the axe in the main hand tossed straight up 4 or 8 cm, the grip closed 0.05 .. 0.4 game seconds after the
release, at full speed and in bullet time. Before: caught only with the grip closed 0.15-0.25 s after (8 cm: 3 of 7;
4 cm: 2 of 7): closing at once was refused (`VR_HandGrabUtil_StartedHandGrabbing` ignores a grab within 0.125 s of a
release) and in the air the fist had to touch the drawn axe exactly. Now 5 of 7 (8 cm) and 4 of 7 (4 cm), the same in
bullet time; the late ones (the axe already 7-15 cm below the hand) still miss.
- `vr_weapon_catch_handle_leniency` (6 cm; engine `vr_physics.cpp` `handOn`): a weapon in the air, the hand's point
  within `vr_weapon_grab_anywhere_min` of its handle, is caught with the fist that far off its drawn surface (not one a
  force grab pulls: QC `fg_state` 1, its catch is its own). Lying weapons keep `vr_weapon_grab_handle_leniency`.
- `vr_weapon_catch_window` (0.25 s, game time; QC `weapons.qc` `wpnthrow_midair_catch`): a weapon in the air is caught by
  a grip closed up to that long before it reaches the fist, and the hand that tossed it may close again at once: it
  catches it once it comes back down (falling), from 0.05 s after the release (not the throw's own fist closing as it
  leaves). 0: as before.
Menu: after "Handle Grab Leniency", "Mid-Air Catch Leniency" and "Mid-Air Catch Window".

## Force grabs: bodies block the view (same worktree)

`VR_Forcegrab_FindTarget` traced the line to a candidate through level geometry only, so a thing behind an enemy was
pulled through him. Now (`vr_forcegrab_bodies_block` 1; menu: "Bodies Block" after "Aim Cone") a second trace with monsters
skips a candidate whose line first meets a monster (alive, or a solid corpse) or a ragdoll (`developer 2`:
"forcegrab: <thing> behind <monster>, skipped"). `Misc/quakevr/forcegrab_block_test.sh <agent>` (new): a health box 150
units ahead on the firing range, the mock's force grab at it: nothing between, caught; a grunt 80 units ahead, skipped
(no catch); the same with the cvar 0, caught; a ragdoll there, skipped while it stands, caught once it lies down.

Checklist (proposed):
- "Bullet time: toss a gun or a prop a few cm up and re-grab it: it rises a few cm, as at full speed. Flick a weapon:
  as far as at full speed. A full throw made slowly with the world: as far as at full speed. Try throws of every size
  and speed between: the distance grows smoothly, no sudden jump. Outside bullet time throws are unchanged."
- "Toss the axe (or a gun) up and catch it by its handle as it comes down, also closing the hand straight after the
  toss: it is caught; a spinning flip too. A weapon lying about grabs as before."
- "Force grab a weapon or prop with an enemy standing in front of it: nothing comes (it does with the enemy out of the
  way)."
## Several menu panels at once (worktree `bloodfx`, branch `agent/multimenu2`, 2026-10-15)

Stages 3 and 4 of MULTI_MENU.md. His note: several VR menus open at once, each with its own state; only settings
must agree between them (they do by construction: every row reads its cvar as it draws).

**What it does.** The corner's new **New Panel** button (above Checklist; headset only, left out of the flat row)
opens another panel beside the first (the second to its right, the third to its left, turned round the head so their
edges meet), on the page the current one shows (the VR Settings when that is not a VR page or is a page for one panel
at a time). Three panels at most. Each has its own page, Back stack, selection, scroll, drop-down, drags and corner
buttons; on the panels after the first the top button reads **Close Panel** and closes that panel only; Back out of
such a panel's first page closes it too. Back to game (the first panel's button, the menu button, any way the menu
closes) closes them all. The laser tests every panel (nearest hit; frames too, so each panel can be gripped and moved
as in stage 1); the panel a laser is on is focused: its mouse follows that laser, and the keys, the sticks and Back go
to it. With two or more panels, both hands work at once: each hand's laser moves the mouse of the panel it is on (one
laser a panel; both are drawn), a trigger clicks where its hand points; one mouse button, so a second hand's click lets
go of the first hand's (a drag ends). Quake's own menus (Main Menu, Levels, Options, Load...) stay single: opened from
another panel they open on the first. Pausing is the first panel's menu's, as before.

**One panel at a time**: Weapon Offsets and its parts (the weapon/hotspot edited, Align Sights' pending result), Body
Calibration, Checklist, Retro Textures > All Categories (Apply to Checked Categories), Review Takes/Take, Search,
Console, Map Library. A second panel is refused them while one shows them: it stays where it is, its title reads
"<page>: in panel N" for 2.5 s (console: "<page>: open in panel N (one panel at a time)").

**Places.** When a second panel opens the first is kept where it is (placed), so they stay side by side. Each panel
after the first remembers where it was (from the first panel, when it closes) for the session:
`vr_menu_panel_remember 1` (default) opens it there again; 0 always beside. The first panel still opens in front of the
head each time (stage 1 unchanged; menu_grab_test check 9).

**Flat screen / the runtime's own panel**: the focused panel's menu only (the others are not shown; Back out of it
closes it and shows the next).

**How.** `menu::instanceCount` 3. `selectInstance` swaps Quake's `m_state` and the menus' mouse (`m_mousex/y`) with
the instance (`MenuInstance::quakeState`, `mouseX/Y`) and lays the menu's bounds out again; the current instance is the
focused panel. `M_Draw` draws the first panel's menu into the 2D layer's canvas whichever is focused
(`VR_MenuDrawSelect/Restore`); `VR_End2D` then draws each other panel's menu into its own canvas (`M_DrawPanel`, the
first canvas's size and mipmaps; freed when it closes), GPU scope "menu panels". `menu::tidyInstances` (each frame and
before drawing) closes the others when the menu closed, and moves a Quake menu opened from another panel to the first.
menuui: `Hit::menu`, per-instance corner-button state (`tb()`), sent mouse and highlight; `pointTo(hand)` per laser.
`vr_menu_panels [open | close <n> | focus <n>]` (MENUPANELS / MENUPANEL lines: each panel's menu, page, selected row,
its value as last drawn and how many frames ago); Debug > Tools: "Menu Panels", "Open Another Menu Panel".
`vr_mock_laser ... [panel <n>] [hand off]` (the off hand's mock laser is new).

Verified: `Misc/quakevr/menu_panels_test.py bloodfx` 17/17 PASS: New Panel opens a second panel on the first's page,
to its right; two pages at once; the laser focuses the panel it is on and moves only that panel's row; a key moves the
focused panel's row only; same page and row in both, a click on the first toggles the setting (0 -> 1) and the second
draws the new value the next frame (drawn 1 frame ago); Weapon Offsets in the first refused in the second; Back out of
the second's first page closes it only; reopened on the VR Settings (the first shows Weapon Offsets) at the same place
(remembered); both hands: the off hand's laser on the second and the main hand's on the first, each panel's row
follows its own laser; the menu button closes all, reopened one panel. `menu_grab_test.py` 13/13 PASS. One panel:
`menu_vr dump` (Menu Detail Developer) 182 pages, rows 4761 -> 4763 (the two Debug rows; `menu_coverage.py` PASS, none
lost); the VR navigation log identical but the corner column's bottom (one more button: y 72.1 -> 84.1); the flat log
identical but the build hash (New Panel left out of the flat row). Flat with two panels open (`vr_enabled 0`): the
focused one is drawn (1 frame ago; the other not for 150), Back out of it closes it, the first shown. e1m1 smoke
clean, build 0 warnings.

GPU (`--exclusive`, mock headset, World page, 3 s each): ui/hud/menu 0.129 ms (one panel) -> 0.192 (two) -> 0.273
(three); the frame's GPU 0.947 -> 1.053 -> 1.173 ms; CPU ui 0.167 -> 0.252 -> 0.331 ms. VRAM: one more canvas per panel,
the first canvas's size (with mipmaps; at most ~50 MB at the 4096 cap, a few MB at the usual `vr_menu_resolution`).
Screenshot: scratch/two_panels.png (the worktree's).

Checklist (proposed): "VR menus: press New Panel (corner): a second menu opens to the right; use one page in each (both
hands can point at once, each panel's trigger clicks there); change a setting in one with the same page open in the
other: it changes there too; Weapon Offsets open in one is refused in the other; Close Panel closes one; Back to Game
closes all; New Panel again comes back where you left it."
## AD stealth, AD soldiers' drops, Armagon's torso, SoA relief, the range's laser cannons (worktree `profilemulti`, branch `agent/adbugs`, 2026-10-15)

Backlog "Playtest voice notes, 2026-10-14": "Arcane Dimensions and expansion regressions" and "Firing-range laser
cannon spawn". All five checked headless by `Misc/quakevr/adbugs_tests.sh <agent> [stealth|guns|armagon|range|all]`.

**AD stealth (22:13:00).** Cause: AD's monsters never met the stealth AI. AD's own AI runs them (`ad_FindTarget`,
`ad_ai_stand/walk/run`), which never called it, and `VR_Stealth_ClassOf` excluded them anyway (no `th_stand`/`th_walk`:
AD's are `ad_th_*`). Before: an AD rocket soldier 300 units ahead, the player dark (light forced 10) and still: Hostile
after 1.8 s, `stl_class` 0. Now the importer patches AD's AI (`ad_import.py` `FUNC_PATCHES`, function-scoped: AD's
FindTarget ticks the stealth AI and sees a player through the suspicion meter, AD's relay and sight groups staying AD's;
FoundTarget and FoundHuntTarget are Hostile; stand and walk run Alert; run loses him) and `QC/vr_ad_stealth.qc` answers
`vr_stealth.qc` for AD's monsters (`.stl_ad`: AD's thinks, `ad_movetarget`/`ad_pausetime`, AD's FoundTarget, AD's idle
sound for the "huh?"). Excluded as Quake VR's are: AD's bosses (`ad_bossflag`), flyers and swimmers; AD's passive mode
on (`ad_passive_state`) keeps AD's rule. AD's zombies get the zombies' senses. Shared lines (AD support):
`vr_stealth.qc` (`VR_Stealth_AD`, `_Path`/`_SetPath`, `_Walks`, `_Found`), `vr_foegrab.qc` (a grabbed AD monster is
spotted), `vr_stealth_test.qc` (its reset and scene 7 work on AD's monsters). After: dark, Idle after 7.5 s (meter 0);
lit, Alert 0.4 s then Hostile 1.1 s; `vr_stealth_test 1` on that soldier on e1m1: 14 of 15 scenes pass (7, a map's
trigger, is AD's `monster_use`, which only wakes for AD's player: passes on AD's start map). The grunt's run unchanged
(0 failed). `ad_e1m1` fought through as before (46 woken, 31 killed, 14 left as with `vr_ai_enhanced 0`); woken AD
monsters that can't see him now give up (the stealth lose rule), as Quake VR's do. The importer also generates the AD
flames' fire spots now (they had been hand-edited into `QC/ad/ad_lights.qc`; `VR_FireSpot_AD` renamed
`VR_AD_FireSpot` so the renaming keeps it): a re-import gives the committed files back.

**AD soldiers' drops (22:14:26, 22:16:03).** His decision: AD's dropped enemy guns are Quake VR's weapons. New
`vr_adgun_ours` (1, VR Settings > Combat > Enemy Weapons > Arcane Dimensions' Guns, "Drop Quake VR's Weapons"): the
rocket soldier drops the rocket launcher, the grenade soldier the grenade launcher (our grenades, size, physics and
muzzle clearance: the oversized AD grenade sticking in the muzzle was AD's gun's), the defender the super shotgun, the
plasma soldier, eliminator and centurion the laser cannon (cells, energy bolts; `VR_ADGun_ForClass`), loaded as an
ogre's or enforcer's drop and refilled from his ammo. The pyro's flamethrower and the fumigator's sprayer have no kind
of ours and stay AD's (ARCANE_DIMENSIONS.md 17.6). 0: AD's own guns as before. `vr_ad_test 71` now drops by class:
8/8 pass with either setting (ours: v_rock2 4, v_rock 4, v_laserg, v_shot2 2; AD's: the `#gun` models). Killed grenade
and rocket soldiers drop a Grenade Launcher (4) and a Rocket Launcher (4).

**Armagon's torso (23:16:08).** Cause: precise hits (`vr_hit_precise`) test a monster's own model, and the Armagon is
two entities: its legs (the monster, `armalegs.mdl`) and its torso drawn by a second, non-solid entity (`armabody.mdl`),
which no shot could meet. New QC field `.vr_hit_extra` (a part drawn as an entity of its own that shots meet as the
monster's; set on the Armagon to its body) and `vr_hitmodel.cpp` `clip` tests that part too, the nearer hit counting.
Debug > Tests > "Hit Probe: Nearest Monster" (`vr_test_monsters 5`): level precise-hit shots every 8 units through its
middle. hipend, before: hits at -24 and 32..48 (legs only); after: -24 and 32..96 (the torso up to its top; Quake's box
ends at 84; with `vr_hit_precise 0`: -24..80, the box, as before).

**SoA relief (23:04:41): not a regression.** `vr_pbr_audit world` on hip1m1, hipend, hip1m2, hip1m4, hip2m1, hip2m3,
hip3m1, hip3m4, hipdm1: every texture has a normal map; the only flat ones are sky, liquids and `notexture`. hip1m1:
21 of 98 textures use the Quetoo pack's authored maps (`vrext/`, names shared with id1; authored priority kept), 28
of the pack's same-named maps are refused by `vr_extmaps_match` 0.5 (the same textures and scores as on e1m1: the
pack's repaint differs), 49 have none; those get maps made from the texture's shading, which carry no parallax
heights while Quake's 8-bit textures are drawn sharp (`vr_texture_smooth` 1, his setting; 2 gives 110 of them
heights). `vr_pbr` 0 and 1 give identical GPU normal maps on hip1m1 (161 textures, 0 differ): AD's companion loader
changes nothing there. What reads as "no relief" next to AD is that AD's textures now have generated companions with
heights (his `vr_pbr_strength` 2.5) and SoA's (and id1's, rogue's) have none: running `Misc/quakevr/pbr_generate.py`
over id1, hipnotic and rogue (PBR_PIPELINE.md) would give them the same; his call (an asset bundle, GPU time).

**The range's laser cannons (22:37:06).** Cause: placement against a random turn. A `func_weapon_grabbable` lies turned
at random (unless AS_PLACED) at its origin; the laser cannons' are 12 units over their table and the cannon is long, so
some turns started it sunk into the table and the physics threw it out (one at 236 u/s, 230 units away) or left it
wedged 37 units up. `VR_WeaponGrabbable_Make` now raises a new one until its turned drawn box clears what is under its
origin (`VR_WeaponGrabbable_Clear`); it falls and settles as before. Debug > Tests > "Weapons Lying Near"
(`vr_pickup_test 5`). Six random sequences, 12 cannons, after 5 s: before 3 wedged at z 58, the rest on the table; after
all 12 at rest on the table (z 19..30, within 42 units of their pads).

Verified: build 0 warnings; e1m1 smoke (23 woken and killed, no errors); eval.sh (archived takes, unchanged).

Checklist (proposed):
- "AD stealth: in an AD map, sneak up on soldiers and knights in the dark: they don't notice you at once; a '?' and a
  look, then '!' as Quake's monsters do; a noise or a body draws them; out of sight a while, they give up."
- "AD soldiers' drops: kill rocket, grenade, plasma soldiers and a defender: they drop your rocket launcher, grenade
  launcher, laser cannon and super shotgun; the grenades fire clean from the muzzle. Enemy Weapons > Arcane Dimensions'
  Guns > Drop Quake VR's Weapons off: their own guns again."
- "hipend: shoot the Armagon's torso (and above its waist): hits land, not only on its legs."
- "vrfiringrange: both laser cannons lie on their table at the start (load the range a few times)."
## A faster QuakeC loop (worktree `qcvm`, 2026-10-15)

BACKLOG.md's two QuakeC VM notes, steps 0-2; details and numbers in QC_VM.md.

- **Measured first:** QuakeC with its builtins is 4-9% of the frame on busy scenes (ai_crowd_64, combat_48, a 24-monster
  e1m1 fight, an AD fight), the interpreter alone ~3-7% of an uncapped frame (1.7-2.3% of a 90 Hz budget): under the
  ~10% bar, so steps 3-4 (pre-decoded statements, superinstructions) were left out.
- **New loop** `Quake/vr/vr_qcexec.c` + `vr_qcexec_loop.inc` (included by pr_exec.c; one hook at the top of
  PR_ExecuteProgramRun): pointers in locals, the statement count implicit (changed only where control jumps, profile
  totals exact), the runaway checked where it changes and before anything leaving the VM (same statement in the
  error), tracing as a second dispatch table, computed-goto dispatch. `vr_qcexec` (archived): 1 default, 2 the same
  loop with a switch, 0 the original. Debug > Profiling: "QuakeC Loop", "QuakeC Loop Bench".
- **Tools:** `vr_qcexec_bench [function] [calls] [args]` times a function in all three loops and checks they agree;
  `vr_qcexec_run [trace] <function> [args]` one call in the current loop (result, statements; a trace from its first
  fabs).
- **Same behaviour:** seeded 600-frame fights on e1m1, AD and vrfiringrange give equal state hashes and per-function
  statement counts in all three loops (and twice in the original); traces and runaway errors (mid-loop too) print the
  same. **Speed:** 1.2-1.7x on QuakeC loops (bench), the `quakec` scope 7-13% lower in game (0.02-0.045 ms a frame).

Checklist (proposed): "Play a few minutes of a busy map (e1m1, an AD map): nothing about the monsters, items or
triggers behaves differently. Debug > Profiling > QuakeC Loop: Original and back to Fast changes nothing you can see."

## Native prop think (worktree `qcvm`, branch `agent/fgnative`, 2026-10-15)

QC_VM.md's next step: `forcegrabbable_think_impl` (5-18% of all QuakeC statements) now runs in the engine; details and
numbers in QC_VM.md, "Native prop think".

- **What:** the `propthink` builtin (`Quake/vr/vr_propthink.cpp`) does the think's water, dust and settling work step
  for step as the QuakeC, kept as `forcegrabbable_think_qc` and run when the builtin declines (cvar off, fields
  missing, an angle too far out to settle natively). `vr_prop_think_native` (archived, 1): 0 the QuakeC. Debug >
  Profiling: "Native Prop Think", "Prop Think Check" (`vr_propthink_check [states]`: QuakeC and native from the same
  states on every prop in the map, compared field by field, random stream and particle message included).
- **Plumbing:** `PR_RandomNumber` (PF_random's body, pr_cmds.c), `progs::sendParticle2`, `edictindex::fieldStored`
  (a native's .flags write followed as a STOREP), `srvrandom::stream/setStream` (the check), fields
  `throwstabilize`/`throwstabilizedim` in vr_fields.inc.
- **Same behaviour:** the check identical on 81,270 states (e1m1, vrfiringrange, ad_crucial); seeded fights with props
  spawned and flung give the same state hashes at 4 points with the cvar 0 and 1 on the three maps. The force grab,
  catch and throw tests (forcegrab_block, weapon_catch, throw_spin, throw_trigger, throw_grace, toss_catch,
  pickup_mag), each run twice with the cvar 0 and twice with 1: the same verdicts and outcomes in all four; their
  numbers (a few mm, a few ms of a throw's age) vary as much between two runs of one setting as between the
  settings (the mock's real-time plays). weapon_catch's "in the air" check fails in all four (its seed; not this).
- **Gain:** QuakeC statements -5% (e1m1), -11% (vrfiringrange), -17% (AD) a frame in those fights; in time a few µs a
  frame, under the noise (no frame-time claim).

Checklist (proposed): "Throw and drop weapons and pickups into water, onto the floor and on the range: they float or
sink, puff dust and settle flat after a throw as before; Debug > Profiling > Native Prop Think off and on changes
nothing you can see."

## Impacts and manhandling (worktree `voxelopt`, branch `agent/ragimpact`, 2026-10-15)

BACKLOG "Playtest voice notes, 2026-10-14" > "Throwing, grabbing and living ragdolls": Thrown living-enemy damage,
Landing crush, Head/wall slams and live-ragdoll access, Impact dismemberment. His decision for more ways into live-ragdoll
interaction: "Grab while staggered" and "Low health".

`QC/vr_ragimpact.qc` (new, after vr_foegrab_throw.qc); engine `vr_box3d.cpp` (`noteRagImpacts`, `callRagImpacts`,
`grabRagdoll`) and `vr_foegrab.cpp` (`manhandle`).
- **Strikes** (engine): each ragdoll part's hardest hit on the level (world, doors, fixtures) per frame, from Box3D's hit
  events: its approach speed (m/s), capped at `vr_ragimpact_whip` (2) x its body's speed before the step (a limb or welded
  sword whipping on a throw is no blow) unless a hand holds the part; the mass behind it: the part's + `vr_ragimpact_body`
  (0.1) of the rest + `vr_ragimpact_held_mass` (20 kg, your arm) when held. Grunt: 80 kg, torso 23 kg, head 5 kg; behind
  a held head 33 kg, a free torso 29 kg. Not a loose weapon, not while staggering or getting up.
- **Damage** (QC `VR_RagImpact_Hit`): `vr_ragimpact_damage` (0.15) x mass x (speed - `vr_ragimpact_min_speed` (2.5), or
  `vr_ragimpact_held_speed` (1) for a held part), x `vr_ragimpact_head` (1.5) on the head; one strike per piece per 0.15 s.
  Only bodies you drive: held, or let go of/thrown within `vr_ragimpact_time` (2 s; the two-hand throw now marks
  `vr_rag_letgo`). A shove's knockdown falling over by itself takes only its fall damage, as before.
- **Pieces** (shared count `vr_limb_corpse_dmg[]`, the corpse wear's): the head (`ragdollhead`) or limb (`VR_Limb_At`)
  struck accumulates the damage; its own health = corpse health x `vr_ragimpact_piece_hp` (0.4: a grunt's head 32). The
  living: damage to health (T_Damage); a head worn past it pops and kills (`VR_Decap_Arm`, sure); a limb worn past it
  pops on the strike that kills (`VR_Limb_Arm`), else on the next strike once dead. The dead: `VR_Limb_WearInto` (the
  shared wear path; torso strikes do nothing on corpses).
- **Landing crush** (`client.qc` landing -> `VR_RagImpact_Crush`): landing at `vr_ragimpact_crush_speed` (400 u/s) or
  more on a living ragdoll: the part under your feet (`ragdollbone` within your feet's square) takes damage x
  `vr_ragimpact_crush_mass` (50 kg) x (speed - 400 u/s in m/s) x `vr_ragimpact_crush` (1), the same piece path.
- **Manhandling**: one hand taking hold of a humanoid (the lift's classes, not the dummy) staggered within
  `vr_foegrab_manhandle_stagger` (1.5 s: a shove landed, an unbalancing hit, a stagger recovered; `.vr_kd_shaken`) or at
  `vr_foegrab_manhandle_health` (0.25) of its health or less: knocked down in place, held in that hand as a lifted one
  (lift strength, flailing, breaks free after Lift Time). A staggering ragdoll a hand takes goes down
  (`ragactive::lapse`). Healthy and steady: held standing as before; lifting unchanged.
- Menu: Combat > Knockdowns, "Impacts" (7 rows); Combat > Holding Enemies after Lift: "Grab While Staggered", "Grab
  When Weak". Debug > Tests: Impacts print/state/throw at a wall/drop you onto it, Manhandle staggered now.
  `vr_knockdown_test 50..58` (`VR_RagImpact_Test`). `vr_knockdown_debug 2`: the engine's every strike.
- Test: `Misc/quakevr/foegrab/ragimpact_test.py <agent> [wall slam crush grab]`. The judo test's staggering neck snap
  case now sets `vr_foegrab_manhandle_stagger 0` (else the grab knocks it down first: snapped "knocked down").

Verified (headless): a grunt knocked down 48 units off a wall and flung at it at 7 m/s: 8 strikes, 58.6 damage, dead
(Impacts off: 0 strikes, 30 health). Ten one-hand head slams on the floor (the hand kept above it, the head 1.7-3.9 m/s):
4-6 head strikes of 3-19, dead after about 3, head popped; 3/3 runs. Dropped 256 units onto a knocked-down grunt
(20.3 m/s): 57 to its torso, dead; 96 units: nothing. One-hand grab: staggered -> knocked down, held; at a fifth of its
health -> knocked down, held; healthy -> held standing; staggering ragdoll taken -> goes down, "staggered and fell".
Regressions: melee eval 168/177 (same take differs FAIL -> FAIL as before), judo lift 6/6, judo neck + hold 31/31,
limb wear 11/11, e1m1 smoke clean, menu paths 0 missing, build 0 warnings.

Checklist (proposed): "Impacts: lift a grunt and throw it hard into a wall: it is badly hurt or killed; grab a knocked-down
grunt's head with one hand and slam it on the floor or a wall 3-4 times: it dies, and its head pops soon after; jump onto
a knocked-down one from a high ledge: the part you land on is crushed. Shove a grunt and grab it with one hand within a
second or so: it goes down in your hand; same for one badly hurt; a healthy one you just hold. Combat > Knockdowns >
Impacts and Holding Enemies > Grab While Staggered/When Weak to tune."

## AD soldiers' guns: their own again, our shots inside (worktree `profilemulti`, branch `agent/adguns2`, 2026-10-15)

**Supersedes** "AD soldiers' drops" in "AD stealth, AD soldiers' drops, Armagon's torso..." above (that section is left
as reviewed). His words: "I want the soldier's drops to be their own single-use weapons. They should look and feel
different, but also share some properties with our guns (e.g. the grenades shot by the soldier's grenade launcher should
be the same, not the old and blocky ones)."

- **Reverted:** `vr_adgun_ours` (cvar, the "Drop Quake VR's Weapons" row, adbugs_tests.sh's two variants): every armed AD
  soldier drops its own single-use gun again (`VR_ADGun_ForClass`). Kept from that round: AD stealth, Armagon's torso,
  the range's laser cannons. `vr_ad_test 71` still drops by soldier class.
- **Shared with ours** (each gun keeps its model, sound, rate of fire, rounds and single use; ARCANE_DIMENSIONS.md 17.6
  has the table): the grenade soldier's launcher fires our grenade and the rocket soldier's our rocket, through the
  launchers' own code, split out of `W_FireGrenade`/`W_FireRocket` as `VR_FireGrenadeMissile`/`VR_FireRocketMissile`
  (model, size, bounce, fuse, blast, damage settings, muzzle clearance, catchability); the plasma rifle and the
  eliminator's and centurion's guns keep AD's bolt (the Plasma Gun's) with our hit (positional damage, prop push,
  sparks); the defender's shotgun already fired our pellets (`FireBullets`); the pyro's and fumigator's streams stay AD's
  (no kind of ours; the pyro's already sets our burning). Gone with them: `vr_adgun_rocket_damage/_splash/_speed` and
  `vr_adgun_grenade_damage/_speed/_fuse` (and their six menu rows: the shots are ours, tuned by Weapon Damage);
  ammo and time between shots stay per gun.
- **The muzzle self-touch:** measured by `vr_ad_test 90` (new; Debug > AD's "Soldiers' Guns" > "Their Shots Leaving
  the Muzzle"; the QC counter `vr_owner_touches` in `VR_OwnerSpared`). A launched grenade (ours as much as AD's old one)
  is touched by its firer 1-2 times as it leaves the muzzle: the engine's body touch (`VR_TouchLinks`) meets the
  catchable grenade's box inside the player's; every such touch is spared by `VR_OwnerSpared` and does nothing.
  Rockets and plasma bolts: 0. So the touch was not the stick, and nothing in it needed changing; AD's old grenade had a
  drawn box of 19x8x8 units against ours' 6x2x2, and is no longer fired. Headless, at four hand poses (level, low,
  raised, pointed down), the AD gun's grenade is 99-181 units out and flying 0.3 s on, as ours; no stick reproduced with
  the old model either (it may have been its look). His headset check decides.

Verified: build 0 warnings; `vr_ad_test 71` 8/8 (each class its own gun, its clip); `vr_ad_test 90` 7/7 (the AD
grenade: `progs/grenade.mdl`, box 6.3x2.3x2.2, bounce, fuse 2.50, 600 u/s, GrenadeTouch/GrenadeExplode, the same as the
grenade launcher's; 179 units out 0.3 s on; rocket `progs/missile.mdl` 1000 u/s; plasma bolt with our touch);
`vr_ad_test 70` 8/8 (grenade 104 dealt, our blast); killed rocket and grenade soldiers drop their own launchers (5, 6
rounds); e1m1 smoke clean; menu paths 0 missing. Rerun: `Misc/quakevr/adbugs_tests.sh <agent> guns`.

Checklist (proposed): "AD soldiers' guns: kill a grenade soldier and fire his launcher: Quake VR's grenades leave the
muzzle cleanly, bounce and go off as your launcher's, from his own gun (6 shots, then spent); the rocket soldier's fires
your rockets; plasma soldiers' rifles fire AD's bolts that spark and push props; each still looks like his gun."


## Neck snap: strict zone, diagnostic, slow motion, crack (worktree `qcscanopt`, branch `agent/necksnap3`, 2026-10-15)

Voice notes 2026-10-14 ("Neck-snap tuning and diagnostics", "Bullet-time lift/neck-snap responsiveness") and the
leftovers "Neck snap method" / "AD humanoids". Both detectors kept (his defaults: Hand Line, 80 deg within 0.2 s).

- **Why it felt random and the settings ineffective** (found with the diagnostic and headless numbers):
  - Twist Speed does nothing under Hand Line (only the angle within the window counts): his 210 never mattered.
  - The old head zone was the head sphere grown (radius + 3.5 + margin, 12.7 units on a grunt) down to the neck's
    length under it: a grunt's shoulder tops (9-11 units out, 15-18 up) were inside it, and an enforcer's or a hell
    knight's shoulder pads rise beside the head, so shoulder holds snapped. On a ragdoll a hold whose limb was the head
    counted wherever the palm was, the palm nearest a head part counted, and a stroke under way skipped the zone check.
  - A lifted ragdoll flails: its head swings past palms held on its shoulders (measured: the holds 7-11 units out from
    its neck's axis, its arms' parts), so a twist after a lift by the shoulders snapped by chance.
  - Slow motion: the hands' velocities are given in the game's time (1/scale faster) and the settle, retry, window and
    break grace were game seconds. At 0.3: the settle after both grips took 0.5 s of the player's (a quick twist or lift
    was missed), the retry 1.7 s, a gentle 0.4 m/s pull read 1.33 m/s (over Lift Speed), a slow twist read 3.3 times as
    fast and Hand Line's 0.2 s window was 0.67 s: lenient here, deaf there.
  - On a lying ragdoll the twist counts only about its neck (along the floor): twisting about the vertical is mostly a
    tilt of it (110 deg about the vertical read 22 within the window; the diagnostic draws the neck's axis).
- **Strict zone** (vr_foegrab.cpp `HeadZone`, both methods, standing and ragdolls): the head sphere counts down to 0.35 of
  its radius under its middle and no further across its axis than 0.9 of the radius + margin; under that a column as
  narrow as the neck (0.75 of the radius + margin; a grunt: 7.4 units) down to Neck Length under the sphere. Standing:
  each hold's spot on the model standing (as before). Ragdolls: where each hold grips the body (new
  `box3d::ragdollHoldPoint`), and its limb the head's or the one the head hangs from (never an arm); the palm only without
  a hold point (+2 units). A stroke under way grows the zone 6 units (was: no check). QC `VR_FoeGrab_OnHead` became
  `VR_FoeGrab_HeadZone` (the head sphere's numbers); `cvarh_vr_foegrab_necksnap_neck` dropped. Behaviour change: a
  lift by the upper chest then a twist no longer snaps (judo_test `lifted` now expects none): take its head.
- **Diagnostic** `vr_foegrab_necksnap_debug` (Neck Snap > Show Diagnostic; Debug > Holding Enemies > Neck Snap
  Diagnostic): over the held humanoid (else the one a hand holds, else the nearest within 64 units of the hands) its zone
  (cyan, green with both hands in), each hand's spot (green head/neck, orange shoulder, red off) and a line to the palm,
  the palms' line (yellow), the neck axis (magenta), the twist's arc against Twist Angle (grey) and the turn now (orange,
  green at the angle), and a panel: the method, turn vs angle (bar, threshold tick), fastest vs Twist Speed (Velocity;
  "not used" under Hand Line), the two measures (line/palms and wrists), where each hand is, the hold's best, the last
  outcome for 4 s. Each two-hand hold's end prints `necksnap diag: ... largest turn N deg (snaps at A), fastest S deg/s:
  snapped|no snap` (also with vr_foegrab_debug). vr_foegrab_debug 1 logs where both hands are when not at the head.
- **Slow motion**: the gestures are measured on the player's clock (`gameSeconds`, `realFrame`, `realVel`): the settle,
  retry, Hand Line window and sample step, the break grace, the twist's integration and smoothing, the two-hand
  velocities (lift, judo throw, neck snap) in real units. The slowed hands still lag their controllers
  (vr_timescale_hand_speed/spin: 8 m/s and 20 rad/s of the game's time, 2.4 m/s and 344 deg/s real at 0.3).
- **Crack**: vr/necksnap.wav regenerated louder (make_necksnap_sound.py: the soft clip driven at 4 not 1.6, a bigger
  lower thump; its loudest 50 ms -9.7 -> -3.4 dBFS, 200 ms -15.2 -> -7.7: as loud as vr/headshot.wav, -3.6/-7.0), and
  three layers (QC `VR_FoeGrab_Crack`): one from each hand (CHAN_HAND: right by the head held, near the ears; 0.7 of the
  volume) and one from its head (its voice channel, cutting its death cry). Crack Volume now reaches 2 (the hands'
  layers louder).
- **AD**: `monster_centurion` and `monster_fumigator` in vr_foegrab_lift_classes (config 134). Mock: `vr_mock_hand_to
  <hand> nearest <classname> head [units]` (its head zone's middle). judo_test `adneck` (AD's data, on vrfiringrange,
  vr_ad_everywhere): each AD humanoid's head snap standing, lift, and knocked-down holds (fumigator, death knight, pyro;
  JUDO_AD_KD=1 all).
- **Hand Line vs Velocity** (for his choice): Hand Line compares the palms' line across the neck now with each sample of
  the window, relative to the enemy's yaw (a ragdoll: play space), and the wrists' common roll; it ignores speed, so slow
  shuffles never add up and only the angle within the window tunes it; a tilt is not a turn. Velocity integrates the
  palms' and wrists' rotation about the neck stroke by stroke and needs a fastest speed: it also catches push-pulls and
  strokes longer than a window, and tilts count. Both pass the same suite; Hand Line is simpler to tune (one angle, one
  window); Velocity is more forgiving of slow-but-big twists.
- **Tests** (judo_test.py; `case_neck` now pins its reference thresholds, 100 deg/s, 30 deg, 0.3 s, neck 3; new
  `neckdefaults` at the shipped defaults with 110-degree twists): neck (Velocity) 39/39 (new: shoulders, shoulders_pp,
  lifted_shoulders no snap; quick, slowmo_quick, slowmo_pp, slowmo_knocked snap; slowmo_slow none); neck (Hand Line)
  39/39 with its lying-ragdoll cases measured, not judged (the mock twists about the vertical: a lying neck read 7-35
  of 75 degrees, the hands at its neck joint once 141: a line nearly along the neck swings wildly); neckdefaults 8/8
  under each method; lift 10/10 (new: quick and slow-motion quick lifted, gentle 0.4 m/s not, slowed or not; it pins
  Breaks Free After 2, the shipped 3.5); hold 2/2; ragdoll/stagger_test.sh judo 0 FAIL; adneck: every AD humanoid's
  lift by the chest (2 hands; none flails: AD's have no driven rigs), head snaps standing for 12 of 15 (the centurion:
  killed standing, it hovers, never knocked down nor lifted); the eliminator's, the fumigator's and the zombie knight's
  heads were not taken from their head zone's sides (the zombie knight's head sits 3-5 units to its side of the zone:
  headfit 2.8 3.1 21.1; positional damage's head zones have no sideways offset); knocked-down pyro, fumigator, death
  knight held at the head and measured. Melee eval 168/177 (3 bash numbers differ from eval_baseline.csv: his
  vr_bash_damage default, not this). Runs flake now and then under load (a hold's ragdoll grab missed, a frame jump past
  a twist): rerun.

Checklist (proposed): "Neck snap: hold a grunt's or enforcer's shoulders and twist hard: no snap; hands on its head or
neck and twist: snaps with a loud crack in your hands. Turn on Neck Snap > Show Diagnostic: the zone, hand dots and the
turn arc/panel match what you feel; Twist Speed reads 'not used' under Hand Line. In bullet time grab a head and twist
right away: snaps as at full speed; a slow twist doesn't. Lift a centurion and a fumigator (AD)."

## Neck snap takes in the motion recorder (worktree `qcscanopt`, 2026-10-15)

His idea: "add the neck snap expected/unexpected to the motion recorder too". Two new categories (MOTIONS.md, "Neck
snap takes"), so his real twists become a regression set for both twist detectors:

- **Recording**: Motion Recorder > Category **Neck Snap Expected** (`neck_snap`; Detail head, neck, push_pull, turn,
  tilt) or **Neck Snap Not Expected** (`no_neck_snap`; shoulders, slow, small, pull, lift, adjusting). Empty hands, the
  training dummy in the melee recording room: start the take, then grip its head or neck (or shoulders) with both hands,
  twist, let go, end the take. Grip after starting (a grip closed in the replay's setup doesn't take hold). Its neck
  "snaps" without killing it (`the training dummy (it stands)`); it stays holdable for the next take. The takes are the
  usual files (both hands' tracking, grips, the dummy's place); the header now also lists the `vr_foegrab_necksnap*`
  settings, and the game's snap is a `necksnap` event (sub `velocity`/`handline`, detail `83 deg at 863 deg/s standing`;
  `vr_foegrab.cpp` `neckSnap`).
- **Verdicts** (`expect.cfg`): `neck_snap` needs a `necksnap`, `no_neck_snap` must have none; blows on the way in allowed.
- **Replay per detector**: `vr_motion_eval <takes> necksnap 0|1` sets `vr_foegrab_necksnap_method` for the replays (put
  back after; the other neck snap settings as set; `eval_status.csv` unchanged).
- **eval.sh** (kit change, staged as `qcscanopt/scratch/eval_ns.sh` for the coordinator to copy over `kit/eval.sh`):
  the `neck_snap_*`/`no_neck_snap_*` takes are kept out of the melee sets (the melee count and baseline unchanged), then
  replayed under each detector in two copies: `method 0 (Velocity): expected snaps caught a/b, missed c, false snaps
  d/e`, the same for method 1 (Hand Line), each miss and false snap by name. Reported only (no baseline yet).
- **Synthetic takes** (`motion_synth.py necksnap`, `source: synthetic`, written to `scratch/`, never his folder):
  `neck_snap_turn` (fists at the head, 90 degrees in 0.2 s), `no_neck_snap_shoulders` (the same at the shoulders),
  `no_neck_snap_small` (20 degrees). Through the new section at the shipped settings: Velocity caught 1/1, 0 false of 2;
  Hand Line caught 1/1, 0 false of 2. By name only, `neck_snap_push_pull` (25 cm push-pull, the wrists turned 100
  degrees about the vertical, 0.3 s): Velocity snaps it (its measure adds the wrists' turn), Hand Line misses it (the
  palms' line turned 61 degrees in the 0.2 s window, under Twist Angle 80; the wrists' turn as the mock makes it didn't
  add): worth a look with his own push-pull takes.
- Melee eval 168/177 (unchanged); e1m1 smoke clean; build 0 warnings.

Checklist (proposed): "Motion Recorder: record a few Neck Snap Expected takes (grip the dummy's head after starting the
take, twist hard) and Neck Snap Not Expected ones (twist at the shoulders, a slow or small turn, a pull): each saves,
the dummy stays standing and holdable, and Review Takes lists them under their categories."
## Lift and judo throw takes in the motion recorder (worktree `qcscanopt`, 2026-10-15)

His request: "add expected/unexpected lifts and judo throws to the motion recorder, so that we can tweak those too".
The neck snap pattern, four categories (MOTIONS.md, "Lift and judo throw takes"):

- **Recording**: Motion Recorder > Category **Lift Expected** (`lift`), **Lift Not Expected** (`no_lift`), **Judo Throw
  Expected** (`judo_throw`), **Judo Throw Not Expected** (`no_judo_throw`), with Detail tags. Empty hands, the training
  dummy: start the take, grip it with both hands, pull it up or turn it down, let go, end the take. The dummy only
  reports "would be lifted/thrown (it stands)": it stays standing and holdable. The game's events: `lift` and
  `judothrow` (`vr_foegrab.cpp`); the header lists `vr_foegrab_lift*` and `vr_foegrab_throw*`.
- **Verdicts** (`expect.cfg`): `lift` needs a `lift` event, `no_lift` none, the same for `judo_throw`/`judothrow`.
- **eval.sh** (kit, staged as `qcscanopt/scratch/eval_lift.sh`, based on the current kit eval.sh, with the neck snap
  section kept): the four kinds are out of the melee sets, then replayed once from a take list; per kind `expected
  caught a/b, missed c, false d/e`, each take by name. Report only.
- **Synthetic takes** (`motion_synth.py liftjudo`, `source: synthetic`, in `scratch/`): `lift_straight`, `lift_fast`,
  `no_lift_slow`, `no_lift_sideways`, `judo_throw_left/right/towards`, `no_judo_throw_slow/small`. All nine give the
  expected verdict at the shipped settings: lifts caught 2/2 (1.6 m/s up, standing), throws caught 3/3 (580-1156 deg/s at
  3.2-3.5 m/s), 0 false of 4. (No `too_far` synthetic: the hands can't reach there.)
- Melee eval 168/177 (unchanged); build 0 warnings.

Checklist (proposed): "Motion Recorder: record Lift Expected takes (grip the dummy with both hands after starting, pull
it up hard) and Judo Throw Expected ones (turn it down), plus Not Expected ones (a slow pull, a slow or small turn): each
saves, the dummy stays standing and holdable, and Review Takes lists them under their categories."

## Faster test suites: batched cases shipped (worktree `fastship`, branch `agent/fasttests`, 2026-10-15)

The batched test cases from 2026-10-14 (`Misc/quakevr/testbatch/`: many cases in one game between `vr_test_snapshot`
and `vr_test_reset`, games in parallel, unpaced frames; `vr_waituntil`, `vr_waitprint`; TESTING.md "Batched test
cases") merged with next and proven again: the 28 converted suites (the reload family, the ragdoll suites, limbs,
prop_climb, gib_2h_throw, ad_monsters, collectfx, weapon_catch, voreshove, judo) each run alone (`TB_SLOW=1`) and
batched. 9361 s alone -> 1646 s batched (5.7x; reload_test.sh 1156 s -> 80-157 s, judo 608 s -> 14 s), the same
verdicts but for cases that flip between two runs of the same kind (ragtest's crawl and catch, climb's per-monster
lines; reload_test alone under load failed 2, then 9: wall-clock frames; batched 112 of 112 both times). Table and
notes: TESTING.md "The batched suites, proven again on next".

- next's newer cases kept: reload_test's two Release Seated Magazine cases batched; judo's slow shuffle and
  `JUDO_NECK_METHOD` cases batched through its `run()`.
- Found by collectfx batched: `load` right after `save` read the save half written (Ironwail saves in a background
  thread). `load` now waits for it (`Host_WaitForSaveThread`, host_cmd.c): a quick save then a quick load could hit it.
- Not changed: guard and decap batched with frames drawn, voreshove with real-time frames; brush_test.py not batched;
  the 19 release-suite conversions left uncommitted in the old worktree are lost (BACKLOG.md).

Nothing to test in VR (test harness), but quick save then at once quick load: the game loads (no error).

## Small fixes (worktree `fastship`, branch `agent/smallfix`, 2026-10-15)

- judo_test `hold`: the drag bar is 2 units (the 0.5 m drag moved the pelvis 8-13 here, 4-12 in the flaky runs): still
  proves it is pulled nearer. hotblade `throw_cold/throw_hot`: 5 runs, all pass (19.7/25.4 damage at 8.8 m/s); the
  under-6.3 m/s bounce did not reproduce alone, only batched: not widened blind.
- voreshove_test: the walk is 2500 frames (was 1500) for the same 4 shoves (3-4 came in 1500 at Body Inset 4).
- Blades keep their glow across `changelevel` (`WeaponInst_PackHot`, parm65..72: time left in tenths per hand/holster
  record); hotblade_test `levelchange`. A hot blade's outline now comes before force-grab/counter/toolgun outlines
  (`VR_EntityGlow`, vr_fgfx.cpp). Quoth's `light_torch_long_walltorch` is still no fire spot: its flame's place in
  `misc_longtrch.mdl` is unknown (needs the model's flame vertex or a measured offset).
- Walking nudges: `nudgeWalkedRagdolls` already existed in vr_box3d.cpp (untested). Added `vr_box3d_walk_nudge` (1;
  Feet Nudge Ragdolls in the corpse menu; 0 disables) and dropped its `corpseDynamic` requirement. A probe (walk a
  stick from y -288 to 32 through a dead grunt at y -126, Walk Through) still moves the pelvis 0.0: the nudge does not
  fire yet (overlap test or ragdollOf not matching); the probe is scratch/walknudge_probe.sh, not committed. Open.
- Not investigated: bullet-time gib_2h bursts, weapon_catch "in the air", liquid_active knight batch FAIL.
## AD flamethrowers, poison and electric bolts: our effects (worktree `profilemulti`, branch `agent/adfx`, 2026-10-15)

His voice notes of 2026-10-15 13:28-13:31, and his clarification: the same for the AD soldiers firing them as for the
player with their dropped guns, one path for both (`vr_ad_enemyguns.qc`; the AD files call it, `// Quake VR` lines).

- **Flamethrower and poison sprayer** (the pyro's, the fumigator's): AD's flame and poison sprites see-through
  (`vr_adgun_stream_alpha`, 0.5; 1: AD's), drawn in the soft-sprite pass with the entity's alpha (r_sprite.c: a batch
  per alpha, no depth written when see-through; Ironwail drew sprites opaque whatever their alpha), and our torches'
  fire tongues (CellFire) at random angles carried along with each (`vr_adgun_stream_fire`, 2 a tenth of a second; green
  for poison; presets `AdFlame`, `AdPoison`). The muzzle licks a tongue a flame; the pyro's smokes (gun smoke, 3 in 10),
  the fumigator's does not. Not as fire under water (AD's smoke sprite there).
- **Poisoned bodies** puff green smoke (additive, faintly glowing: `PoisonPuff`, 2 every quarter second over their box)
  until the poison ends (death or its time); the damage stays every half second. A fumigator's spray on a monster
  (infighting) now poisons it too, as the player's sprayer does (`vr_adgun_fumigator_poison`/`_poisontime`), on top of
  AD's own hit; AD's poison on the player is unchanged (no puffs over your own view). `vr_adgun_stream_fire 0`: AD's
  old motes.
- **Electric bolts** (the plasma soldiers', eliminators' and centurions', `progs/proj_plasma.mdl`, theirs and the
  player's): AD's grenade-smoke trail replaced by a faint smoke, crackling arcs and blue-white sparks
  (vr_particles.cpp `electricTrail`, from `VR_EntityTrail`); the muzzle and the hit crackle (`ElectricSparks`, from
  `ad_launch_plasma` and both touches); `vr_adgun_bolt_fx` 1 (0: AD's). The poisonous green bolts keep AD's look.
- **Electrocution**: what a bolt strikes, alive or dead, gets the lightning gun's arcs and lasting shock
  (`bodyshock` + `VR_Shock_Hit`: convulsing ragdoll, crackle, burns; `vr_shock_*` settings). The bolts now move as
  missiles (FLYMISSILE), so they strike bodies (only missiles touch corpses): monsters' bolts also hit monsters a little
  more easily (Quake's missile box against monsters); the player is hit as before.
- Debug > Tests > Arcane Dimensions Monsters: "Make the Nearest Shooter Fire" (`vr_ad_test 92`: the nearest AD pyro,
  fumigator or plasma soldier fires once at the monster or body nearest it; a standing target hops, as flames are
  triggers that touch only what moves into them). Settings > AD guns: "Flamethrowers' Look" (the three settings).

Checked (headless, scratch/): `adfx_pyro_player.png` (your flamethrower: AD's, ours, ours with the sprites at 0.1 to
show the tongues), `adfx_enemy_pyro.png` and `adfx_enemy_fumigator.png` (a pyro and a fumigator spraying a monster,
before/after; the fumigator's third: the poisoned pyro puffing), `adfx_enemy_bolt_corpse.png` (an eliminator's bolt
before/after, then the struck body's arcs). Lines: `adgun bolt shock: monster_army alive before 1 ... shock left 3`
(an eliminator's bolt, and the player's with the dropped gun, on a living grunt) and `... alive before 0, corpse 1,
shock left 3` (an eliminator's bolt on the body); `adpoison: monster_pyro's poison ends (puffs 11)`. `vr_ad_test 70`
8/8 PASS; e1m1 smoke clean; build 0 warnings. Not managed headless: the player's own bolt into a body (the mock hand
could not be aimed into a lying body; the same touch and movement as the monsters' bolts, which did).
Frame cost, a sustained spray from your flamethrower (3 s, real time, exclusive): frame 4.47 -> 4.42 ms (noise),
particle sim 0.015 -> 0.020 ms, particles drawn 0.057 -> 0.052 ms CPU, 0.10 -> 0.16 ms GPU.

To test in VR: AD map with pyros and fumigators (and drop their guns): the streams see-through with fire (green)
tongues, the muzzle's fire and the pyro's smoke; poisoned monsters puffing green until it wears off; an eliminator's or
plasma soldier's bolts: thin crackling trail, sparks at the muzzle and the hit, the struck monster or body arcing and
convulsing.
  fire yet (overlap test or ragdollOf not matching); the probe is scratch/walknudge_probe.sh, not committed. Fixed
  below ("Walking nudges, and three test oddities").
- Not investigated: bullet-time gib_2h bursts, weapon_catch "in the air", liquid_active knight batch FAIL (below).

## Walking nudges, and three test oddities (worktree `fastship`, branch `agent/walknudge`, 2026-10-15)

- **Walking nudges** (his note r2m1 22:48:15): `nudgeWalkedRagdolls` did fire (the overlap and `ragdollOf` were fine),
  but pushed at most 3 m/s^2, under the floor's friction on a lying limb (ragdoll 0.8 with the floor's 0.6: ~7 m/s^2),
  so nothing ever slid. Now each limb in the lower half of his box (his legs) is pushed along his pace towards 0.3 of
  his speed, at most 0.6 m/s, with at most 3 m/s^2 more than its friction holds it (`walkNudgeShare/Speed/Accel`,
  vr_box3d.cpp). The player is never touched: his walk is unchanged. `vr_debug_box3d 2` prints each nudge.
  `Misc/quakevr/ragdoll/walknudge_test.sh` now asserts: a dead grunt walked through (Walk Through): the pelvis moves
  1.6-2.3 units with the nudge (0.5-12 allowed), 0.0 without; he ends at y 31.8 vs 31.9 either way. PASS.
- **weapon_catch "in the air"** (both prop-think paths): not a regression of the catch. Since the mid-air catches
  (0660bdf2f) a weapon in the air is taken at its handle with `vr_weapon_catch_handle_leniency` (6 cm) more; the test
  lowers the hand onto the crowbar's middle, within `vr_weapon_grab_anywhere_min` of its handle, and it was taken 5.5
  cm off (allowed 6). The check now sets the leniency to 0 (it tests the fist's own rule): 6 of 6 PASS.
- **liquid_active `knight water` "FAIL energy 83 J"**: not batched only (alone too, on next). The 83 J is the
  armoured knight going limp near the top and sinking at ~1.3 m/s; it reached the pool's bottom 0.06 s after the
  trace's quiet window began. `liquid_trace.py` now leaves out the frames whose pelvis still sinks (over 0.5 units a
  traced frame) from the energy after the drive: water, knight, ogre, enforcer, hknight PASS (knight 14 J).
- **bullet-time gib_2h bursts**: small gibs (`vr_smallgibs_test 9`) only. A small gib is owned by the body it came
  from for `vr_smallgibs_grace`; at its end `VR_SmallGib_Think` set the owner to world whoever it was. At full speed
  the grace ended while it was held; in bullet time (game time 0.3x for the same frames) it ended 13 ms after the
  throw, taking the thrower's ownership away: the gib struck him at once ("gib: struck player at 422") and burst.
  Now the grace's end leaves a holder's or thrower's ownership (vr_smallgibs.qc). push/hard x stagger 0/1 x gib/small
  in bullet time: 0 bursts batched (was 2), alone 1 (a world hit 200 units off, as full speed has: 3).
  Separately, many small-gib runs are thrown one-handed ("the other hand is not within reach"): the test's hands are
  14 cm apart (HALF 0.07); its header says HALF=0.03 for a small gib.

**In the headset:** Corpses = Walk Through, walk over a dead grunt: its legs and arms slide a little ahead of your
feet, a few units, never flung; your walk does not slow. Feet Nudge Ragdolls off (under Corpse Collision, beside
Corpses): they stay put.
In bullet time, take a small gib in both hands and throw it: it flies, it doesn't burst in your face.

## Main menu layout (worktree flatfist, branch agent/mainmenu, 2026-10-15)

The flat (desktop) main menu laid its rows down to the screen's bottom (menu y 260), under the version/notice box and
the Recording button: bottom-heavy. M_Main_Layout (menu.c) now limits a flat screen's rows to Quake's 200 (the title at 4,
Quit's row ends 199): the space above the title (64) and below Quit (~60) are even, and the notice box (from y 227) and
the corner buttons stay clear. The headset is unchanged: it centres in its panel, and with a Continue row (a save
present) the check shows the block centred (about 115 px above, 110 below at 960x540), its save lines clear of the box.
Screenshots (scratch/): mm_flat_before.png, mm_flat_after.png (run with -ExtraArgs "-flatscreenmode"),
mm_vr_nocontinue.png, mm_vr_continue.png. Not tested in a real headset.
Follow-up (branch agent/mainmenu2): on the flat screen the title now starts at menu y -20 (the canvas reaches -60; the
corner buttons end about -29), so the rows get 24 more units above the unchanged bottom limit 200. With the Continue row
(14 rows) that is steps 13 and gaps 1 (before: none); fewer rows give wider gaps. The headset path is untouched.
Screenshots: scratch/mm2_flat_after.png, mm2_vr.png.
## The small crates' lips: not a bug (2026-10-15, worktree `fastship`, branch `agent/climblip`)

The note in "Review fixes" (`vr_climb_try` finds no hold at a small crate's lip, spawns 123/124) was a wrong probe: the
hand was put where the large crate's hold is, but a small crate's front is 4 units further back (y -76, not -80) and its
top lower. At its own lip: a small crate on a large one (123, 80 over the feet) is an exact hold (hand at -300 -74 80:
the hold 1.1 units off, top 80.1; from 4 units out, a lenient one); a small crate alone (124, 32 over the feet) is none
from the ground, turned down as `low`: under `vr_climb_prop_min_height` 36, as the rule means (the grip carries it); at 30
the same hand holds (top 32.1). Exact holds always report "0 holds seen" (that count is the lenient search's), which
read as a miss. No engine change. `prop_climb_test.sh` has a new `lip` case (those three tries) and drops its "Not
covered" line: 12 PASS. e1m1 smoke clean. Still untested: a real grip and pull on a small crate on a stack.
## Beams: the broadcast's message boundaries count the beam id (2026-10-15, worktree `backlogclean`, branch `agent/beamfix`)

The non-AD part of AD P1b (6267ce7d2): `tempEntityLength` (vr_server.cpp) counts Quake VR's beam id byte in a
TE_LIGHTNING1-3/TE_BEAM when the progs are ours (PRFL_QUAKEVR_PROGS). It was one short, so a beam never ended on a
boundary and the QuakeC temp entities right after it had none either: a full broadcast dropped more than it had to
(never garbled). Lightning gun held 5 s in e1m1 with `vr_debug_net 2` (220-240 broadcast bytes a second, no errors);
e1m1 smoke clean.

## Neck snap: the Hand Line only; lift and judo throw diagnostics (2026-10-15, worktree `backlogclean`, branch `agent/foediag`)

His notes: "Please only keep the handline technique, get rid of everything else that's related to the other technique
and update the values with my new ones", and "add similar diagnostic views for the other interactions such as the lift
and the judo throw".

- **Hand Line only.** His `vr_foegrab_necksnap_method 1` is Hand Line (checked: the cycle row's 1). Velocity is gone:
  `vr_foegrab_necksnap_method` and `vr_foegrab_necksnap_speed` (Velocity's alone: the hand line never read a speed),
  the menu's Twist Detection and Twist Speed rows, the stroke tracking (vr_foegrab.cpp `neckSpin`, `neckAxis`,
  `neckPeak`, `neckStroke`, `neckSmooth`, `neckRollArm`, `neckFloor`, `neckPullShare`). `neckStep` is now the hand line
  itself (the old `neckWindowStep`; the wrists' roll about the neck reads as before: the lesser of the two when they turn
  the same way). Logs and the motion event say "deg within <window> s" (`necksnap/handline`; older takes say
  `velocity`). `vr_motion_eval`'s `necksnap <method>` option is gone.
- **His values the defaults** (from his config): Twist Angle 75 (was 80), Twist Window 0.15 s (0.2), Head Grip Margin 0
  (2), Neck Length 0 (1.5); his speed 210 dropped with Velocity; hertz 24, strength 1.25, volume 1 were the defaults
  already. `configVersion` 137 migrates a config at the old defaults.
- **Lift and judo throw diagnostics** (Holding Enemies > Lift / Two-Hand Throw > Show Diagnostic, and Debug > Tests >
  Holding Enemies: Lift Diagnostic, Judo Throw Diagnostic; `vr_foegrab_lift_debug`, `vr_foegrab_throw_debug`, not saved),
  like the neck snap's. Over the enemy both hands hold (else the one a hand holds, else the monster nearest the palms;
  the one last tried stays 4 s, lifted or thrown). The lift: the hands' pull, an arrow from between the palms (12 units a
  m/s; green once at Lift Speed and within 45 degrees of straight up), the cone of both (grey), the Lift Reach circle
  round its feet (green with you inside); its panel: up vs Lift Speed, the pull's angle vs 45, your distance vs Lift
  Reach (bars with the threshold mid-track), hands on it, humanoid or not, `now:` what stops it this frame (pull up
  faster, pull straighter up, one hand, settling the grip, at its head: the neck snap's, not a humanoid, too far, just
  tried), its best pull and the last try's result. The throw: the turn's way, an arrow from its top (0.08 units a
  deg/s), the Turn to Throw circle; its panel: turn vs Turn to Throw, the hands' speed vs Hands' Speed, the way (left,
  right, towards, away), `now:` (turn it over faster, hands too slow, pulling up: the lift's, at its head, away from you:
  a shove's, its kind, not hurt enough) and the best and last. Each try prints `lift diag:` / `throw diag:` with the
  numbers against the thresholds; a two-hand hold that ends untried prints its best (`... no lift`, `... no throw`).
  The reasons come from `twoHandCheck`, `liftCheck` and `throwCheck` themselves (the measuring moved to `twoHandsOf`,
  shared with the frames they don't measure), so they can't drift from the real rules.
- **Tests.** Build clean. `judo_test.py backlogclean neck neckdefaults lift hold dir`: 71/73, the two passing on a rerun
  (`pp_knocked_fwd`: a ragdoll grab missed, the known flake; `defaults_stagger`: at his values a staggering grunt's 110
  degrees in 0.2 s read 66 within 0.15 s, under 75: the case now twists 140, snapped at 82). `case_neck` pins its
  reference (30 deg, 0.3 s, neck 3, now margin 2 too); `JUDO_NECK_METHOD` gone. Melee eval 168/177 before and after (the
  same 3 bash numbers off the baseline: his `vr_bash_damage`). Neck snap takes (eval.sh, now Hand Line only; the kit's
  copy backed up to `qvr-kit/scratch/eval.sh.bak_backlogclean`): 6/6 caught, 0/6 false (before, at the old defaults: 6/6,
  1/6 false). Headless screenshots: `scratch/diag_lift.png` (held still: "pull up faster"; then LIFTED at 2 m/s, 36 of
  48 units), `scratch/diag_throw.png` (a slow pull left: turn 160 of 130 deg/s, hands 0.6 of 3 m/s, "hands too slow";
  then thrown right at 840 deg/s). `vr_menu_path_check maps/vrcalibration.map`: 0 missing. e1m1 smoke clean.

Checklist (in checklist.txt, replacing the method comparison): the lift and judo throw diagnostics against what you
feel.
## Bullet-time throws: the stroke's tempo only (2026-10-15, worktree `fastship`, branch `agent/throwlean`)

His note: the new slow-motion throwing feels mostly good; keep only it and drop the rest, leaner settings. The
stroke-tempo throw ("Bullet-time throws: the stroke's tempo" above) is now the only one: `vr_throw.cpp` `estimateAt` and
`estimateBothAt` always go through `strokeEstimate` (at timescale 1 the hand's and controller's samples are the same, the
gain 1: exactly `releasePeak` as before). Gone with the old path: `motionRate`, `withOwnAim`, `Tempo`, `armVelAt`,
`armSpeed`, the free `valueAt`, and `Estimate`'s `flickRate`, `flickShare`, `nudge` (and `vr_debug_throw`'s old-path
lines). About 250 lines less.

Removed cvars (listed in `retiredCvars`: an old config's lines are dropped quietly; config version 136, nothing to
migrate) and their Throwing-page rows:
- `vr_throw_slowmo_stroke` (the A/B switch; row "Slow Motion: Throw Strength by Stroke")
- `vr_throw_slowmo_real_time` (1 was required for the stroke path; row "Slow Motion: Throws in Real Time"): the
  release's windows and the release speed threshold are always in the player's real time in slow motion
- `vr_throw_slowmo_tempo` ("Slow Throws Match"), `vr_throw_slowmo_aim` ("Throw Where You Aim"), `vr_throw_slowmo_flick`
  ("Real-Time Flicks"), `vr_throw_slowmo_flick_arm` ("Wrist Flick Below"), `vr_throw_slowmo_short_travel` ("Nudge
  Below"), `vr_throw_slowmo_long_travel` ("Arm Throw From"); with them version 111's three default-change rows

Kept (the stroke path reads them): `vr_throw_slowmo_stroke_time` (0.09 s, "Full Stroke Tempo", its help now explains the
mechanic), `vr_throw_slowmo_max_speed` (15 m/s), `vr_throw_slowmo_real_strength` (0, "Throws As In Real Time"). The
bullet-time tap and trigger settings are untouched.

One difference: while the slowed hands are still catching up right after bullet time ends (timescale 1, hand behind its
controller), a throw had the controller's way at the lagging hand's speed (`withOwnAim`); now the controller's way and
speed (the stroke path at gain 1), as at full speed. `vr_debug_throw 1` prints its stroke line then too.

Tests: `flick_slowmo_compare.sh` identical to before the change (all 17 rows, every column: lob50 bullet x1.23,
overhand x1.49, overhand0 made slowly x0.88); `stroke_sweep_test.sh lob` as in the section above (lob50 x1.23, lob70
x1.35; made 3.3x slower x1.01, gain x3.33); `throw_slowmo_test.sh` in bullet time runs with its stroke lines; the
removed cvars set from a script are silent; e1m1 smoke clean.
## VR Settings submenus merged, biosuit glow defaults

The 20 VR Settings submenus (rows: Height 4, Hands 8, Holsters 11, Ammo Pouch 5, Controls 1, Locomotion 6, Comfort 5,
Teleportation 2, Turning 2, Flashlight 2, Lighting 2, Weapons 4, Bullet Time 3, Body 4, Haptics 1, HUD 3, Sound 3,
Display 2, Scaling 4, Graphics 10) are now 9, each part under a small header, every row kept (vr_menu.cpp, addVrsPart):
Height and Hand Calibration; Holster Calibration; Ammo Pouch; Locomotion and Controls; Comfort, Teleport and Turning;
Weapons and Bullet Time; Body and Flashlight; HUD, Haptics and Sound; Display and Graphics (Display, Lighting, Scaling,
Graphics). The boards' menu paths were renamed (make_vrcalibration_map.py, make_vrtesthall_map.py; both maps rebuilt;
vr_menu_path_check: 0 missing). Biosuit glow defaults from his config: vr_suit_glow 1 and vr_suit_glow_light 1 (were 2);
configVersion 135. The pentagram (2, 2) and quad values already match his config.

## Menu panels kept when the menu closes (worktree `tghost`, branch `agent/panelkeep`, 2026-10-15)

His note: the extra menu panels must not close when he returns to the game. Now leaving the menu by any way (Back to
game, Esc, the menu button, the dashboard) HIDES them: `MenuInstance::open` stays true, `menu::instanceOpen` is false
while the menu is not shown, `tidyInstances` only makes the first panel current again (it no longer closes the rest).
Opening the menu shows every panel with its page/submenu, scroll and selection as left; map changes and loads keep
them too (Quickload no longer closes them). Only Close Panel (or Back out of a panel's first page) removes one.
Placement: `vr_panel.cpp drawInEye` no longer unplaces panels when the menu reappears, so a panel placed by grip (or
by New Panel) stays put (stored in play-space, so it follows the player across maps); a main panel never moved is
still re-centred in front of the head. Back to game reopens on the FIRST panel's page, whichever panel it was pressed
on. Not done: a page that becomes invalid after a map change falling back to the panel's top page (the pages are
static, none is map-dependent today). Test: `Misc/quakevr/menu_panels_test.py` steps 10-12 (hide, reopen, map change,
Close Panel) pass.
## Carried into water: no snap back; a snapped neck flops (2026-10-15, worktree `qcscanopt`, branch `agent/ragneck`)

His notes: a lifted or shoved grunt carried alive into water, when it gets up, "almost snaps back to the ground that it
was shoved onto"; and a snapped neck should be a much looser joint, "as if the vertebrae snapped".

**Carried into water: no snap back** (`vr_box3d.cpp` `standSpot`, `ragdollGetUp`, `driveActive`). The stale place was
the get-up's liquid search: lying in a liquid it looked for a floor with its head out up to `vr_knockdown_water_search`
(256 units) away, at any height up to 96 over the body, and blended the body there. Carried 190 units out into
vrtesthall's pool and held, it stood up on the pool's steps 188 units off (blended in 0.35 s); a body carried along the
pool's south side stood on that bank 47 across and 120 up, from 96 under the surface. Now:
- A get-up's liquid search goes out to its own range (Room Search, `vr_knockdown_search`, 48), and the floor may be at
  most `vr_knockdown_water_climb` (new, 48 units; Knockdowns > Climb Out of Water) over its pelvis. The swimmer's goal
  (where it paddles to, `livingSwim`) still searches `vr_knockdown_water_search` (its help reworded). None near: it
  paddles on, and after Give Up in Water it gets up where it floats (as before).
- Held in a liquid by a hand, it stays down (`ragdollgetup` -1: asked again; the QC's give-up timer starts when it is
  let go of). Then it swims as one shoved in (swim > the rest), and gets up near where it floats or at the bank it
  reached.
- A get-up rising on land lapses when its pelvis is carried into a liquid, or a hand holds it more than 64 units (level)
  from its spot: it was pinned back towards that spot while carried away.
- The "anywhere" get-up (given up in a liquid) lifts its box up to 32 units: a body on the pool's bottom never found room
  (its box about the pelvis was in the floor) and stayed down for good.
- `handback` debug lines (vr_knockdown_debug) print the pelvis and where it stands, and the distance.

Measured (`Misc/quakevr/ragdoll/carry_water_test.sh <agent> [release]`: knocked down beside the pool, the main hand at its head, walked 250 units into the
pool): held throughout, before: up on the steps 188 units off; now: stays down while held, then up where it floats, 5
units off. Let go of in the water: it paddled from (371 -354 -98) to the surface by the south bank and stood on it 51
units across (before: the bank or the steps, from the bottom). Its rise begun on land while held: "carried off its
get-up (into a liquid): it slumps". `liquid_active_test.sh kdswim kddie drownkd`: PASS (kdswim now gets up 40 units from
where it lay, 80 before).

**A snapped neck flops** (`vr_foegrab.cpp` `neckSnap` -> `box3d::ragdollNeckSnapped`; `ragactive::Parts::loose`). After
a snap (standing, knocked down, staggering or lifted) its ragdoll's neck joints (its head's, and one more up to its
chest if the rig has a neck bone) are set loose: swing `vr_foegrab_necksnap_cone` 120 deg (the grunt's living neck 45),
twist `vr_foegrab_necksnap_twist` 140 (50), no spring, friction `vr_foegrab_necksnap_friction` 0.1 of the body's; the
active drive never drives them again (a death's collapse, a swim). `vr_foegrab_necksnap_loose` 1 (Holding Enemies >
Neck Snap > Broken Neck Flops, with Swing, Twist and Friction rows). Other ragdolls unchanged. Past about 120 the head
sinks into the chest (it never meets its jointed part): 150 reached 149.
Debug > Tests > Holding Enemies > Snap the Nearest Neck (`vr_foegrab_necksnap_test`: as both hands would, none
holding); `vr_foegrab_neck_angle [edict]` prints NECKANGLE (head against chest now). `Misc/quakevr/foegrab/neck_loose_test.sh <agent> <loose> [kd]` (summed by `neck_loose_sum.py`), a grunt
snapped, 40 samples over 3.3 s:

| case | swing range (deg) | twist range (deg) |
|---|---|---|
| standing, stiff (before) | 26..50 (24) | -50..14 (64) |
| standing, loose | 7..119 (112) | -3..141 (144) |
| knocked flying then snapped, stiff | 0..52 (52) | -50..50 (100) |
| knocked flying then snapped, loose | 1..118 (117) | -113..28 (141) |

`judo_test.py neck` (grunt_cw, knocked_cw, stagger, lifted): PASS. e1m1 smoke clean. To try in VR: snap a grunt's neck
standing and lying, and carry its body by the head: the head should loll and roll; carry a knocked-down grunt into deep
water and hold it, then let go: it swims, then gets up where it floats or at the bank beside it. Not done: a loaded
game's ragdoll made again has a stiff neck.

## Gloves; Player Appearance (2026-10-15, worktree `fastship`, branch `agent/playerlook`)

His words: "Glove options: fingerless or full gloves, with hue, saturation and lightness controls ... Collect all the
player customization options into a new submenu under 'VR Settings' ... Ideally, all of these things should be
arbitrarily combinable."

**Gloves** (`vr_gloves` 0 none / 1 fingerless / 2 full; `vr_glove_hue` 25, `vr_glove_saturation` 0.45,
`vr_glove_lightness` 0.2: a dark brown leather; all archived, config 138, new settings only). Drawn the way the skin
tone is, no painted art: the skin masks (`progs/hand_rig_NN_00_skinmask.tga`, `vrbody_NN_00_skinmask.tga`,
`Misc/quakevr/make_skin_masks.py`) gained three channels beside R (the skin, unchanged bit for bit): **G** the
fingerless glove (the palm island; each finger's first segment to 55% of its length; the thumb's long base and half of
its second segment), **B** the full glove (the whole hand, nails too), both leaving the static blood of the damaged
skins out and both 0.55 at the glove's edge rising to 1 a 3 mm hem inside it (made from the finger coordinates in 3D,
so the islands' UV seams get no hem); **A** the skin's own brightness over the clean skin's mean, halved (since
replaced by the glove's own seams: "Gloves: leather, not painted skin" below). The alias
shader (`vr_glsl.h` QVR_ALIAS_FS_LIGHT) puts the leather (the HSL colour, `vr_view.cpp gloveLook`, times the skin's
own shading from A: creases, knuckles and grain kept; the hem 40% darker at the edge) over the covered texels and
tones only the skin the glove leaves bare, so every skin tone x glove x colour works, and dynamic wounds, burns and
water still show on the glove (the same skin share as the tone's). The mask is now loaded with alpha
(`TEXPREF_ALPHA`) and bound whenever a tone or a glove is on (`view::skinMaskWanted`). **Bare forearms**
(`vr_body_bracers 0`): the body's forearm gets a cuff from 4.5 cm behind the wrist to the lip over the hand, with
its hem, drawn from the arms' skin block rows (`GloveCuff`, from vr_bodybare.inc's numbers: crisp at any texture
size, no mask needed); with the bracers the bracer's lip covers the join and nothing of the glove is on the body. Only
the jointed hand (`vr_hand_rig 1`) gets gloves, as only it gets the skin tone. New instance fields: `Glove`,
`GloveCuff` (gl_shaders.h, vr_api_render.h).

**Player Appearance** (VR Settings, after Body and Flashlight; page 184 "VR Settings - Player Appearance"): Body
(Build, Skin Tone, Bracers), Gloves (Gloves, Hue, Saturation, Lightness), Pauldrons (on, Style, Size). Their one home:
Build left Body and Display, Bracers and Skin Tone left Body, the pauldrons' on/style/size left Body - Arms and
Pauldrons (their fit stays there); each old page links here. The calibration room's board line "Skin: Body > Skin
Tone" is now "Look: VR Settings > Player Appearance" (`make_vrcalibration_map.py`, map rebuilt;
`vr_menu_path_check` 0 missing on vrcalibration and vrtesthall).

Tested (headless, vrfiringrange, eye captures): bare; fingerless dark leather; full bright blue; bracers + full red +
skin tone -0.8; fingerless green + skin tone 0.7; bracers + fingerless white + tone -0.4: each as set, fingertips
toned, cuffs and bracers right; e1m1 through body modes 0/2/3 and `vr_hand_rig 0/1` with gloves on: no errors. For
him in VR: the leather's look up close (its shading strength: the shader's 0.85 of A), the fingerless edge's place,
the cuff's length (4.5 cm), and the default colour.
## Menu corner: Toolgun shortcut (worktree `tghost`, 2026-10-15)

A "Toolgun" button joins the top-left corner shortcuts (laser, stick focus, flat-screen icon row), between New Panel
and Checklist, shown only at Menu Detail: Developer (`vr_menu_level 2`, as the Checklist). Pressing it closes the menu
like Quicksave and runs the Debug menu's own path: `impulse 169` (toolgun in the main hand), `impulse 189` (off hand)
when the main hand holds a weapon; if a hand already holds a toolgun it only says so. It prints
`Toolgun: in the main|off hand.`; unavailable (dim, reason printed) outside a local single-player game or while dead.
The column's fit-above-Recording scaling counts the shown buttons, so the smallest panel adapts. The Checklist was
already Developer-only (corner button and Debug > Checklist); unchanged. `vr_mock_laser toolgun`.
Verified: build 0 warnings; dev vs standard corner screenshots (`scratch/tg_dev.png`, `scratch/tg_std.png`: Checklist
and Toolgun only in dev); laser click gives hand 1 (main) or, after `impulse 152`, hand 0 (off) in `vr_toolgun_status`;
`vr_menu_path_check`: 6 found, 0 missing; e1m1 runs. In VR: press it with a weapon in the main hand.

## Torches and fires from the toolgun (worktree `permdecals`, branch `agent/torches`, 2026-10-15)

His note: spawn torches, fires and brazier flames with the tool gun, as a prop or as a wall torch. His decision: one
group; free-standing ones grabbable props, wall torches snapped to the wall aimed at, facing out; each keeps its glow and
sound as the map entity has them.

- **The Fire tab** (the toolgun panel's fifth tab; `vr_toolgun.cpp` spawnables, entries 46-55): Wall Torch
  (`light_torch_small_walltorch`), Large Flame (`light_flame_large_yellow`), Small Flame, White Flame
  (`light_flame_small_*`), Rogue's Candle and Lantern (only while their models are there), and AD's Short and Tall
  Brazier, Flame Pyre and Long Wall Torch (only in an AD map: outside AD's rules `box3d::spawnClass` makes no `ad_`
  light). Unavailable entries show greyed as AD's do. Pick works too: a picked wall torch hangs on a wall again.
- **Live spawns** (`vr_spawn_live`, a nosave QC global the engine sets round the spawn function in `box3d::spawnClass`):
  what the map makes static (`makestatic`, `ambientsound`: only the level's signon, which a client in the game never
  reads again) stays an entity. The id spawn functions (`misc.qc`), Rogue's (`rogue_newmisc.qc`) and AD's
  (`ad_setup_light_static`, `ad_setup_light_sound`) check it.
- **Wall torches**: the ghost snaps to a wall (the world's or a brush entity's such as vrfiringrange's prop wall; not a
  floor, a ceiling or a thing before it) 10 units out (`wallStandoff`: id's maps hang theirs 6.5-12.5 off, e1m2's) and
  faces out of it (its yaw the wall's normal; the turn still applies). With `vr_walltorch` on it is the takeable torch
  (`VR_WallTorch_Setup`: grip, force grab, shot off, relit...); off, an entity with id's model.
- **Flames as props** (`VR_FlameProp_Setup`, `vr_walltorch.qc`): `.vr_flameprop` 1 small, 2 large, 3 candle, 0 a
  lantern (glass: burns nothing). Each hangs where placed, upright (MOVETYPE_NONE: on a map's brazier, a ledge, the
  floor) until a hand or a force grab takes it (`VR_Carry`); let go of, it falls and tumbles as a box. Its fire heats
  blades and relights torches (`VR_WallTorch_FireAt`, by `findflags`); resting, the player meets it as a map flame of
  its size (`VR_Burn_MapFrame`); carried or thrown it burns what it touches as a torch's head
  (`VR_Burn_TorchTouch`) and sets on fire what a throw of it hits.
- **Glow and sound**: the light is the engine's torch light (`vr_emissive.cpp` finds flame.mdl/flame2.mdl/candle/lantern
  entities wherever they are; there is no baked light for a spawned one, so it is the same small flicker a map torch
  adds on top of its lightmap). The crackle is fire1.wav on the entity, started from its first think: a sound started
  in the spawn itself (the toolgun's input, before the server frame) was cleared with that frame's datagram unsent.
  The engine moves a flame prop's crackle with it (`vr_walltorch.cpp crackles`). The toolgun precaches the torch
  models and the taken torch's sounds in every map (`VR_Toolgun_Precache`): a sound precached late never reaches the
  client.
- **Debug**: `vr_torch_lights_list` (Debug > Reports > Torch Lights): every torch light lit now, static or entity.

Test: `Misc/quakevr/toolgun_fire_test.sh <agent>` (PASS): the wall torch hung at x -590 (the wall -600), yaw 0, its
crackle and light; the large, small, white flames and the lantern props, each with its torch light and (but the
lantern) its crackle on its own entity (`snd_show 2`); the large flame taken by the off hand's grip and dropped (it
falls); the wall torch taken by the grip (`vr_test_walltorch_shot 24`). Screenshots: `scratch/toolgun_fire.png`.
`toolgun_panel_test.sh` passes with five tabs (its Props click still lands). e1m1 smoke clean.

Not done / to check in VR: AD's entries are untested (no AD data here) and stay fixed where placed (AD's particle
emitter does not follow a carried prop); a loaded save loses a spawned flame's crackle (sounds are not saved); a flame
prop let go of tumbles like any prop (its flame drawn tilted); a hand reaching a spawned wall torch is the same reach as
a map torch's (`walltorch_grab_test.sh` is a pre-existing failure: its "gripped on its wall" message no longer exists).
## Shoves and lifts after a reload: not reproduced (2026-10-15, worktree `flatfist`, branch `agent/ragreload`)

His e1m2 note: after an autoload or a respawn, shoves and lifts no longer made active ragdolls. Not reproduced headless;
no fix. `python Misc/quakevr/ragdoll/reload_test.py <agent> [modes]` shoves a grunt with the mock hands (motion_synth's
palm_shove_2h, Chance 100) and lifts one (both hands at its shoulders, pulled up), reloads, and does both again; PASS
when each becomes a ragdoll every time. Modes: restart, load, quickload (game time back about a minute), death (kill,
restart), autoload (kill, restart: sv_autoload loads the save), map, the pool full first (ten corpses and one knocked
down: crowdload/auto/death), saved with a grunt held up in a lift or mid-stagger (midlift*/midshove*). All pass; also on
e1m2 with his whole ironwail.cfg (death, load), in real time (`-RealTime`: the loading screen's deferral), and a save
loaded first thing in a fresh process. Ruled out by reading: foegrab's holders (reset on load and on a time jump),
box3d's world and its ragdoll pool, slots and roomRefused (all per world: resetServerWorld), the drawn ragdolls (a
MapChange cache), the cvar handles (made again in the same order on load: a saved `vr_kd_chance_h` stays valid), stamina
and the shove's QC state (saved with the game time). Still possible, untested: a rig marked failed once
(`rigCache.failed`, kept until a game dir change: `canragdoll` 0 for that model from then on), or something only the
real headset does. For the next time: Debug > Combat, "Why Can't I Knock It Down?" (`vr_knockdown_test 10;
vr_ragdoll_list`) prints each gate for the nearest living monster (its chance, canragdoll, can stagger, stamina), the
ragdoll pool as VR_Knockdown_MakeRoom counts it (each ragdoll and whether it can be freed; "NO ROOM" when none can), then
the engine's ragdolls.

## Model vertex wobble 3: rigged monsters rebuilt from the skeleton (worktree `dequant`, branch `agent/rigrebuild`, 2026-10-15)

His words: "there still is a bit of wobble. Ideally this should be implemented in an efficient way and be toggleable."
Backlog "Model vertex wobble" (2). `vr_anim_smooth_rebuild` (archived, **default 1**: the numbers are clean),
Graphics > Models and Effects > Rebuild From Skeleton (after Swing Limbs); Debug > Reports: Rebuild Fit Limit
(`vr_anim_smooth_rebuild_error`, 0.5 units). Design and numbers: MDL_DEQUANT.md, "Item (2)"; vr_animsmooth.cpp, "C".

- A rigged monster (soldier, knight, hell knight, dog, enforcer, fiend, ogre; not the shambler: under half its vertices
  fit) is drawn every frame, with any Model Animation and at the poses too, as each vertex's fixed place on its bone moved
  by the bone; a vertex the skeleton fits worse (round the joints) keeps the frames' place (faded between 0.5 and 1 unit
  rms). Normals turn with the bone.
- The stored bone fits carry the rounding; rebuilding on them was worse than off. The bones are refitted at load to the
  dequantised poses (two rounds of places and Gauss-Newton fits), so the loop seams and the motion are the dequantised
  ones, the per-vertex jitter gone.
- Cost: precomputed per model (with the rig's data); per instance 9 rows a bone (memoised per view as before), no
  per-frame allocations; +0.07 ms a frame for 60 idle grunts (about 1.1 us each), on the GPU 2 more reads a vertex.
- Numbers (idle grunt, knight; worst vertex's second difference, p95, units): 10 Hz grunt 0.456 -> 0.314 (0.272 with
  Smooth), knight 1.071 -> 0.865; 90 Hz grunt worst 0.059 -> 0.032. Off draws as before (Smooth keeps its rows and
  shader path; the trace now samples the dequantised poses as drawn).
- `vr_anim_smooth_info`: each model's rebuild counts and fit rms, instances rebuilt and VR_AnimSmooth's time.
- Found, not fixed: with Model Animation Smooth an idle knight jolts (`vr_anim_smooth_trace` at 90 Hz: one vertex up to
  2.5 units a frame, p95 0.46; Quake's blend 0.10), rebuild or not: the spline itself (a mispredicted next frame or
  span on the unnamed knight frames?).
- To try: an idle grunt and knight, Rebuild From Skeleton on and off, Quake's Blend and Smooth: the breathing frames
  without the surface's wobble, the same shape (elbows, knees, the gun).
## Step In Depth: a cap on walking into a held enemy (worktree `tghost`, branch `agent/stepin`, 2026-10-15)

His go on the BACKLOG item ("Closer to enemies" Notes: with Step In he walked right through a held enemy). New
`vr_foegrab_step_in_depth` (default 8 units, 0..32, 0 = no cap; Combat > Holding Enemies > Getting Close > Step In
Depth, under Step In). With Step In on and the cap set, the player's own body moves against the enemy his hand holds
no longer pass: they meet the pair's hull (as `vr_body_monster_inset` makes it) with its level sides that much further
in, so his origin stops that deep past where the two would meet and Quake's clipping slides him along it, a solid
body. Stepping back or aside is free. Unchanged: the held enemy's own moves (Follow the Hand, Pull, a throw: they still
pass, so nothing fights foegrab), corpses and ragdolls (not pairs), monsters not held, Step In off. Never trapped: if he
is deeper than the cap already (the enemy moved into him, the cap lowered), `VR_BodyContactLetsOut` passes every move
that goes no deeper, as for any overlap. The cap never narrows the hull below 1 unit a side. `vr_body_contact_status`
prints the cap.

Why 8: at inset 4 a grunt's pair hull is 12 deep from its edge to its origin; the step-in walk in the "Closer to
enemies" numbers stopped 6.4 deep (5.59 apart), the benefit he asked for, so 8 keeps all of it (origins 4 units apart
face to face, head against head) while the far side stays out of reach. An ogre (hull 20) stops 12 from its origin.

Numbers (`closein_test.sh stepcap`, held grunt on vrfiringrange, 50 frames walking in): cap 0: 10.66 deep after 10
frames, then through and out (held 0, another grunt nearest at 150); cap 8: 7.97 deep (dist 4.03) after 10 frames and
still 7.97 after 40 more; slid aside 47 units, backed off to 97, still held. `stepin` case as before (6.29 deep at cap
8: under it; step in 0 stops at 12.03). `judo_test.py dir`: FAILS 0. e1m1 smoke fine.

VR check: hold a grunt by the shoulder and walk on into it: you stop with your face at its face, can't come out its
back; strafe and step back freely; Step In Depth 0 walks through as before; is 8 the right depth (try 4..12)?
## Impacts: settings, and crushing standing monsters (2026-10-15, worktree `flatfist`, branch `agent/impactdmg`)

His notes: thrown or slammed living enemies take real impact damage (with settings), impact dismemberment (check,
settings), landing crush. **Already there** ("Impacts and manhandling" above, `agent/ragimpact`): held/thrown living
ragdolls hurt per part on walls and floors (`VR_RagImpact_Hit`), heads and limbs worn until they pop (the shared
`vr_limb_corpse_dmg[]` count; corpses through `VR_Limb_WearInto`, the one corpse wear path), and a crush of knocked-down
ragdolls. Added here (defaults keep the old behaviour; Combat > Knockdowns > Impacts):

- **Damage curve**: `vr_ragimpact_curve` (1: linear; 2 quadratic, 0.5 square root) pivoting at `vr_ragimpact_curve_ref`
  (3 m/s past the least speed: every curve deals the same there); `vr_ragimpact_max` (0: no cap) per strike or crush,
  the head's and limbs' multipliers in; `vr_ragimpact_limb` (1) on arms and legs as `vr_ragimpact_head` on heads. Rows:
  Damage Curve, Curve Pivot, Most per Strike, Limb Strikes (Least Speed, Impact Damage, Head Strikes and Head and Limb
  Health were already rows). One shared `VR_RagImpact_Amount`/`_Scale` for strikes and crushes.
- **Dismemberment**: threshold `vr_ragimpact_piece_hp` (as before, head and limbs alike) and new `vr_ragimpact_wear`
  (1): the share of a strike's damage worn into its piece (0: impacts hurt but never pop pieces), on the living and on
  corpses (passed to `VR_Limb_WearInto`). Row: Head and Limb Wear.
- **Landing crush** now also on standing monsters (`vr_ragimpact_crush_standing` 1) and staggering active ragdolls (any
  living ragdoll, not only knocked down). Quake never sets FL_ONGROUND on a monster's top, so client.qc also calls the
  crush when a fast fall (`jump_flag` under -300) stops short in the air (`velocity_z` above half of it). A standing
  one's part: its model met straight down from your feet (`hitmodel_segment`; else halfway to its middle), the head by
  `VR_Decap_OnHead` (positional damage's zone), a limb by `VR_Limb_At`, else the torso; then the same
  `VR_RagImpact_Deal` (a head crushed past its health pops). The crush no longer needs Impacts Hurt on (its own 0: off).
  **Fall damage**: a landing that crushed something deals none (`vr_ragimpact_crush_cushion` 1; 0: Quake's 5 as ever);
  AD maps keep AD's fall rules. Landing on a standing one's top never dealt fall damage (no FL_ONGROUND). Rows: Crush
  Least Speed (`vr_ragimpact_crush_speed` 400 u/s), Crush Standing Ones, Crush Breaks Your Fall. Debug > Tests:
  "Impacts: Drop You onto the Nearest Standing One" (`vr_knockdown_test 59`, `vr_test_drop_height` 256; test 52 now
  uses it too).

Verified (headless, `python Misc/quakevr/foegrab/ragimpact_test.py <agent> wall slam crush`, 0 FAILS): a knocked-down
grunt flung at a wall at 5/7/9 m/s: 27.1/51.5/78.3 damage (first strikes 8.0/13.9/19.7 at 5.3/7.4/9.4 m/s, 12.7 kg);
Impacts off: 0. Curve 2 + Most 12 at 7 m/s: every strike capped at 12.0, a 5.1 m/s excess 11.4 (linear 6.7). Ten head
slams: dead, head popped (worn 50.2 of 32). Dropped onto a standing grunt's head: 128 units 13.2 at 13.9 m/s (alive),
256 units 81.7 at 19.9 m/s (its head pops); 64 units nothing; onto a knocked-down one 256 units 56.7 to the torso, 96
nothing. 400 units onto a down one without god: your health 100 cushioned, 95 not. Melee eval 168/177 (baseline), e1m1
smoke clean, build 0 warnings.

Checklist (proposed): "Impacts: jump off a ledge onto a standing grunt's head: it is hurt (from 2 storeys its head
pops) and you take no fall damage; Combat > Knockdowns > Impacts: Damage Curve 2 makes hard throws much worse than soft
ones, Most per Strike caps one strike, Head and Limb Wear pops heads sooner."


## Permanent decals leftovers: retro filtering, late marks on doors, boxes (2026-10-15, worktree `fastship`, branch `agent/permdecals4`)

His go: the missing permanent decal features (PERM_DECALS.md, BACKLOG "Permanent decals leftovers").

- Retro (blocky) textures: the layer is now read at the middle of the texture's block or texel under the pixel
  (`PermDecalsLook`, vr_glsl.h), so a baked mark has the surface's blocks: retro textures' (the pixel's set: snapping,
  Block, soft edges, fade far off) or, with `gl_texturemode GL_NEAREST*`, the texture's own texels (Quake's own textures;
  every texture with `vr_texture_smooth 0`: `TexMgr_NearestKind`, sent in `PermDecals.x`). The layer's texel at the
  block's middle comes from the screen derivatives of both coordinates (both affine on the face; parallax's shift kept).
  Smooth as before with linear textures, where no page is under the pixel (most of the world: one read, as before), and
  where a pixel spans a block or more. The palette (Retro Palette) is not applied to the layer (it multiplies the
  texture, which is in the palette already).
- Gore's late marks on doors and lifts: a mark that shows later (the flying blood's `delay`) on a brush model now waits,
  laid in the model's space as it is when made, and is baked when it shows (`permdecals::bakeEntities(stamp, due)`,
  `bakeDue` once a frame). Waiting ones go into the saves (baked at once after the load) and show in
  `vr_permdecal_info` ("N marks on brush models waiting to show"). Test aid: `vr_decal_at ... [size] [delay]`.
- Marks on the brush tool's boxes: kept live, by decision (PERM_DECALS.md, Decisions): a box is made again on every
  edit (each drag step), its faces are not in the map's page table (laid out at the map's load), its id is not stable
  across saves, and its marks go with each change by design; a per-box layer would need page ranges allocated and
  given back on every rebuild and a save key for the box. The test checks the behaviour: live past every bake delay,
  not baked, gone when the box changes.
- Small leftover fixed: a mark whose pages' slots were all dropped (the atlas full at `vr_permdecal_atlas_max`, every
  slot used in one frame) now keeps its stamp (for the saves) instead of staying live untracked.

Tested (headless): `permdecal_test.sh` PASS 3 runs of 4 (one failed its off-vs-on frame check: the shot after the
switch matched the one before it; not seen again in two reruns: a flake of the shot timing, the switch untouched here)
with its new part 5 (a 3 s late mark on vrclimb's lift: 0 stamps and 1
waiting after 5 frames, baked after the delay; a box's mark live after 10 s, the world's mark beside it baked, the box's
gone after `vr_brush_set`). Shots (worktree scratch/): `pd4_door_before/after.png` (vrclimb lift, two 3 s marks and one
at once: before all three right after the hit; after only the one, the other two after the delay);
`pd4_retro_before/after.png` (lift up close: retro blocks 2, nearest, linear: before the marks smooth on blocky
textures, after blocky with them; linear unchanged). Frame time, `bench.sh --scenarios combat_48` (48 monsters
fighting, exclusive, 600 frames): frame p50 11.110 ms both (paced); GPU 3D 3.84-3.87 before, 3.85-3.92 after (one
outlier run 4.31 with every pass high); world+brush pass 1.244-1.247 before, 1.254-1.286 after (noise: the alias
pass moved as much). e1m1 smoke: no errors.

For him in VR: blood on a wall with retro textures on (Graphics > Retro Textures) and with nearest texture filtering:
the baked marks should have the wall's blocks; gibbing a monster against a door: its blood lands on the door when the
flying blood arrives, not at the hit.

## Smooth's idle knight jolt: the spline's next frame (worktree `tghost`, branch `agent/knightjolt`, 2026-10-15)

His go on "Model vertex wobble 3"'s finding (Smooth: an idle knight jolting up to 2.5 units a frame).

Cause (logged: `vr_anim_smooth_trace <frames> <model> <jolt>` now prints each frame whose worst vertex passes `<jolt>`
with the spline's four keys, weights and times, and the two frames before): the knight's frames are named by numbers
alone ("1" .. "97"), so the names chained the whole file as one animation and were trusted as such: stand9 (frame 8)
went on to run1 (frame 9) by name, over the learnt stand1. Each loop the spline into stand9 aimed at run1, the next
lerp (stand9 to stand1) set off along the right curve: the tangent jumped at the seam, 1.6-2.5 units on one vertex.
The ogre had a second cause in the dequantiser: AD's Hunter Ogre's spawn precaches `progs/ogre.mdl`, so its 8-frame
stand loop (0..7) and another (8..19) were given to the original ogre over its own 9-frame one, and the dequantised
seam was smoothed in the wrong place (stand8 into stand1).

Fix:
- `vr_animsmooth.cpp` (modelData): names that are numbers alone are a guess (as unnumbered names were: the shambler's
  "From ASE"), so a learnt successor wins; the frame expected after now also comes from `vr_mdldequant.cpp`'s runs
  (`mdldequant::poseSuccessors`): a progs loop's last frame goes back to its first (sure, over the names: knight_stand9
  to knight_stand1 from the first loop, nothing to learn), and where the names are only a guess the runs' splits at
  jumps (unknown after: learnt once seen) and their loops that come back to the first pose. A named animation's end
  stays unknown until seen, as before (an attack's last frame back to its first would be a wrong guess: tried, the
  death knight's combat p95 went up 3%).
- `vr_mdldequant.cpp`: progs loops that overlap on a model are no longer dropped when the progs is read; per model the
  most named loop that fits its frames is chosen (`qcLoopsOf`): a loop doesn't fit where the names run on into its
  first frame or out of its last (stand8 into stand9). The ogre now gets 0..8 (by its pose closing the loop: its own
  progs loop is named in `monster_ogre` and set in `monster_ogre_real`, so the finder never links the two).

Numbers (`scratch/mon.sh`: e1m1, 90 Hz fixed frames, Smooth, rebuild off; one vertex's second difference a frame, p95 /
most, units; idle: notarget, 360 frames; awake: hunting you, god on, the same 360 frames in each build):

| monster | idle before | idle after | awake before | awake after | Quake's Blend idle; awake |
|---|---|---|---|---|---|
| knight | 0.308 / 2.506 | 0.037 / 0.072 | 2.765 / 9.161 | 2.688 / 9.161 | 0.050 / 0.119; 3.877 / 14.506 |
| ogre | 0.150 / 0.410 | 0.135 / 0.280 | 3.202 / 48.226 | 3.184 / 48.226 | 0.253 / 0.514; 4.407 / 48.226 |
| shambler | 0.272 / 2.012 | 0.199 / 0.355 | 5.632 / 123.21 | 5.632 / 123.21 | 0.277 / 0.501; 7.658 / 123.21 |
| grunt | 0.020 / 0.034 | 0.020 / 0.034 | 1.686 / 11.288 | 1.684 / 11.288 | 0.024 / 0.059; 2.711 / 11.288 |
| death knight | 0.034 / 0.104 | same | 2.796 / 7.570 | same | 0.060 / 0.201; 5.339 / 13.993 |
| dog | 0.051 / 0.075 | same | 2.246 / 11.612 | same | 0.063 / 0.122; 4.160 / 12.068 |
| enforcer | 0.040 / 0.065 | same | 2.228 / 47.781 | 2.265 / 47.781 | 0.066 / 0.168; 3.639 / 46.788 |
| fiend | 0.107 / 0.192 | same | 3.718 / 19.085 | 3.758 / 18.840 | 0.157 / 0.450; 5.431 / 18.423 |

Smooth is now smoother than Quake's Blend idle on every monster. The awake maxima (11-123 units) are the game's own pose
switches (an attack or pain starting at once), the same in Quake's Blend. Rebuild From Skeleton on: knight idle 0.035 /
0.062, grunt 0.014 / 0.020, ogre 0.136 / 0.276. AD humanoids not measured. e1m1 smoke fine.

VR check: Model Animation Smooth, an idle knight and ogre close up (the breathing loop's seam: no twitch each second), a
shambler idle; knights and ogres running at you look as smooth as before.
## Weapon damage credit from the damage dealer (2026-10-15, worktree `backlogclean`, branch `agent/dmgcredit`)

His words: "Please fix the missile weapon credit in a robust non-hacky way." Honey's `FL_SPECIFICDAMAGE` targets (a
button given `.items`: "shoot it with the rocket launcher") take damage only from those weapons. T_Damage read the
*attacker's* `.damage_weapon`, which only some fire functions set on the player, some after the shot: a nail, laser,
plasma bolt, lava nail, bomblet or thrown weapon was credited with whatever the player had fired or swung last, and even
the shotguns' own pellets with the weapon before them.

Now (`QC/vr_dmgcredit.qc`): whatever deals the damage carries the credit, stamped once where it is made, and T_Damage
reads its *inflictor's* (`VR_DamageCredit_Accepts`; the shotguns' hit test and a grenade's bounce-or-blow too).
- `WeaponIdToDamageCredit`: one table from weapon id to Honey's IT_* bit; variants as the weapon they stand for (lava
  nails, multi grenades/rockets, the plasma guns, AD's soldiers' guns, the proximity gun as the grenade launcher, the
  grunt's gun as the shotgun, the enforcer's rifle as the laser cannon, the melee weapons as the axe); fist, grapple,
  toolgun and AD's flamethrowers credit nothing.
- A player's projectile: `VR_ShotLaunched(missile, hand)` (the launch hook every player projectile already went
  through as `VR_ShotPlace`, renamed at the call sites: base, Hipnotic, Rogue, enemy and AD guns) credits it with the
  hand's weapon. AD's player plasma and projectile pellets and the flamethrowers' flames, which don't go through it,
  call `VR_DamageCredit_FromHand`.
- Hitscan (the player is the inflictor): `FireBullets` (every player shotgun, the grunt's gun, AD's defender) and
  `W_FireLightning` credit the player from the hand before the shot; the underwater discharges and the lightning-in-water
  shock too. The old after-the-shot stamps are gone, and so is `LightningDamage`'s (a bolt's credit is its `from`'s).
- Thrown weapons: `MakeThrown` credits the weapon thrown. Props credit nothing.
- What a dealer makes inherits its credit (`VR_DamageCredit_Inherit`): explosions are their missile (the same entity),
  the multi grenade's bomblets theirs, Mjolnir's (and the Super Axe burst's) lightning chain its base, which takes the
  hand's weapon (`HIP_SpawnMjolnirBase(xHand)`), AD's poison its spray (`VR_ADGun_Poison` takes it).
- Monsters' shots stay unstamped (their own credit: the grunt's Honey `IT_SHOTGUN`); traps' and monsters' lightning
  bolts no longer borrow `IT_LIGHTNING` from `LightningDamage`.

Test: `vr_dmgcredit_test 1` (Debug > Tests > Damage Credit; `100+i` one weapon): each weapon fired from the main hand
at a target taking only its own bit, then only another weapon's bit (the player's last credit set to that one first).
e1m1, hand aimed level (`vr_mock_hand main 0.2 1.0 -0.3 65 0 0`): before 3 passed, 15 failed (only grenade, rocket and
lightning right; the shotguns, nails, laser cannon, proximity grenade, lava nails, multi grenades/rockets, plasma, the
enemy guns and both thrown weapons credited with the previous weapon); after 18 passed, 0 failed. AD's guns are not in
the test base (skipped). To try in VR: a Honey weapon-specific button with the right weapon and a wrong one, after
firing the right one first.

## Every expansion's entities in the toolgun's spawn panel (worktree `permdecals`, branch `agent/spawnall`, 2026-10-15)

His note: the spawn panel should have every expansion's entities too (the Machine packs' armour shards, Armagon's
wetsuit...); be thorough. The list was built from every QC spawn function and `quakevr.fgd` (367 classes) against the
panel; the diff, with what was skipped and why: `qvr-kit/scratch/spawnall_diff.txt`.

- **75 new entries** (`Quake/vr/vr_toolgun_packs.inc`, appended after the old ones: `vr_toolgun_select` numbers stay;
  195 entries in all). Monsters: id's rotfish and ogre marksman, Quake VR's training dummy; Armagon's gremlin,
  centroid, spike mine, Armagon; Eternity's mummy, wrath, overlord, phantom swordsman, eel, the three guardians (its
  spawnflag picks the look: without one `monster_morph` is an objerror), Hephaestus, the dragon; Dawn of the Machine's
  four infected, demo dog, rocket ogre, ranged knight, slime, shub zombie, lava man, blood shambler, orb, ghost; Honey's
  spectral knight. Weapons (grabbable): Mjolnir, the proximity gun, the laser cannon, the Super Axe; the bloody
  shotguns. Items: small health, the large ammo boxes, both keys, the four runes; wetsuit, empathy shields, horn of
  conjuring; lava nails, multi rockets, plasma, power shield, anti-grav belt; armour shard, lava suit, the five
  upgrades. Props: eight placed corpses (`misc_corpse` styles), the buzzsaw, Honey's tree. Fire: the fireball spout,
  the Machine packs' gas flare.
- **Availability** (`entryAvailable`): `needs` 1 the model (or `probe`: the file the spawn function checks, e.g.
  Honey's spear: precaching it missing is a Host_Error) on the search path, owned packs' read in place
  (`owned/mg3/...`); 3 Dawn of the Machine's campaign (its once-a-map upgrades, QC `MG3_UpgradeSpawn`). Unavailable
  entries are dimmed as AD's are.
- **The panel**: each tab lists id's and Quake VR's first, then each pack as a group from a fresh page in the list's
  order (`toolgun::spawnablePack`; Arcane Dimensions' last, as before), labels tagged SoA / DoE / MG3 / MG / Honey;
  `>>` (beside `<` `>`) jumps to the next group. Monsters now has 14 pages, Items 8.
- **QC**: `item_armor_shard` reads MG3's model in place in any map when the pack is owned (MG3.md, "Decisions" 5: as
  the Super Axe), `item_artifact_lavasuit` MG3's model before the biosuit stand-in; a live spawn (`vr_spawn_live`) of
  `misc_corpse` works outside the Machine world, of a `buzzsaw` snaps its yaw to 90 degrees (else an objerror), of a
  bloody shotgun in Dawn of the Machine stays outside Bloody Nightmare.
- **Debug > Toolgun**: Spawn Every Toolgun Entry (`vr_toolgun_spawn_test [<first> [<last>]]`: each entry made once on
  a ring around the player, spawned / failed / unavailable) and Check and Clear the Spawn Test (`... check`: each
  still there or gone, then all removed).

Tests: `Misc/quakevr/toolgun_spawnall_test.sh <agent>` (vrfiringrange) PASS: 119 spawned and alive, 0 failed, 76
unavailable (AD's 64 with no AD here; Honey's 2; the upgrades and the gas flare outside Dawn of the Machine).
`MAPCMD="vr_campaign_native mg3;map map1" ... 120 194`: the upgrades, the gas flare, shard, lava suit all alive (some
others "gone" there only because map1's ring spots are in walls: each alone is alive). `toolgun_panel_test.sh` PASS;
e1m1 smoke clean. Screenshots of the new groups: `scratch/toolgun_spawnall.png` (`scratch/spawnall_shots.sh`).

Not done / to check in VR: AD's other monsters and items (`ad_monsters_gen.py`'s list is the supported set; no AD data
here to test them), Quoth (no support in the QC), the scripted bosses and map logic (the diff's SKIPPED). Check the new
ghosts' heights (each a fixed lift: the dragon, Armagon and the guardians are big), Honey's entries with Honey
installed, and that 14 pages of monsters with `>>` read well on the panel.
## The vore's two-handed shove (worktree `flatfist`, branch `agent/voreshove2`, 2026-10-15)

His go (BACKLOG "Later", 2026-10-13): "A two-handed shove animation for the Vore." Its shove (10 damage, 256 units
pushed at 64: "The vore shoves you") played the end of its ball throw without the ball, one arm's swing.

- **Seven frames of its own:** `Misc/quakevr/make_voreshove.py` appends `shove1..shove7` to Quake VR's own
  `quakevr/progs/shalrath.mdl` (frames 36..42; its 36 untouched, byte for byte; genguard'ed, re-runnable: it replaces
  the shove frames it wrote). The model has no skeleton, so the frames are posed by its ragdoll rig's bones
  (`vr_ragdoll.cpp` shalrathSeeds' pivots; each vertex's bone as `rig.py shalrath shalrath_bones.json` labels it):
  the chest leans about its pelvis joint, the head nods on it, each upper arm turns about its shoulder and each forearm
  about its elbow to the key's directions, blended across a joint near it (22 vertices) so the seams stretch smoothly;
  pelvis and legs stay in the rest pose. Normals turn with their bone.
- **The motion** (`orig_mon_shalrath.qc` `shal_shove1..7`, 0.1 s each as before): 1-3 the wind-up (leaning back 11
  degrees, head pulled back, both arms drawn back, elbows low and behind, hands up by his chest: the tell), 4 the
  thrust, 5 the full reach (both arms straight out at you, the hands together at your chest's height, leaning in 12
  degrees), 6-7 back towards its rest; then `shal_run1`.
- **The blow on the peak:** `VR_EnemyShove_Hit` stays on `shal_shove5` (`$shove5`, the arms' full reach), 0.4 s in;
  the whole shove 0.7 s: the timing is unchanged. (Drawn with the frame lerp, the arms reach shove5 0.1 s after the
  blow: shove4 is already 70 percent out when it lands.) The hit's developer line now ends with the monster's frame
  (`(frame 40)`), and `voreshove_test.sh` fails a blow on any other frame.

Rig and smoothing (`vr_anim_smooth_info shalrath 1`, new: it lists each bone's unfit poses): every bone fits all
seven new poses. The engine's motion clusters shift with the 7 frames (clusters and bones 1.18 rms, were 1.20/1.28):
vertices fully rebuilt 299 -> 329, fit median 0.480 -> 0.457; the head is now unfit in 12 more of its old attack and
pain poses (2-9, 13, 16-22: drawn by the plain lerp there, as before the smoothing), the chest in 2 fewer.

Tests: `voreshove_test.sh` 4 shoves, every blow on frame 40 (2 of 3 runs; the third 3 shoves, the vore hurting itself
with its own balls' splash after its 3rd: the known 3-4 flake, `smallgib: monster_shalrath hit ... explosions`);
screenshots from the player's eyes (vrtesthall, the vore 90 ahead, push 0): wind-up, peak and recovery; e1m1 and e1m6
smoke with a vore spawned: no errors.

Not done: a fallback for a replaced vore model without these frames (as the grunt's and the enforcer's shoves: Quake
VR's own model only).

In VR: walk into a vore (Debug > Tests spawn, or e1m6 on Hard): it leans back with both arms drawn back, then thrusts
both hands at your chest and the push comes as they reach you; check that it reads at arm's length and that the push
doesn't feel early.

## The rest of AD's monsters and pickups in the spawn panel (worktree `permdecals`, branch `agent/adspawn`, 2026-10-15)

Follow-up to the section above, now that the kit has AD 1.80p1's data. Every `ad_monster_*` / `ad_item_*` /
`ad_weapon_*` spawn function in `QC/ad/` was checked against `ad_monsters_gen.py` and AD's paks.

- **Added** (`Misc/quakevr/ad_monsters_gen.py`, which writes `vr_ad_spawns.inc`, `vr_ad_spawnitems.inc`, the Debug
  menu's rows and the QC spawner; 15 entries, spawner numbers 251-260 and 320-324): monsters Gug (AD's strong Bog
  Lord), Polyp (the poison gaunt), Nail Ogre (`monster_ogre_flak`), Floyd Automaton, The Deflector, Dark Prince,
  Vermis (Nour's boss form, no intro: a boss, check in VR); AD's versions of Quoth's Death Guard
  (`monster_death_guard`, its own `mon_dguardq.mdl`), Death Lord, Sentinel. Items: AD's ammo backpack (spawnflags 15:
  random shells, nails, rockets, cells) and the armour backpack as shards, green, yellow and red.
- **AD maps only** (`QVR_AD_SPAWN_MAP`: `needs` 2, not in `vr_ad.cpp`'s `everywhereClasses`, the QC spawner removes
  them outside AD's rules): the three Quoth classnames (so a Quoth map's own still spawn nothing, as before) and the
  backpacks (Quake's backpack rules outside AD's maps, ARCANE_DIMENSIONS.md 16.1). The other new monsters follow the
  older AD entries' rule (AD maps, or any map with Additional Content Everywhere).
- **Skipped**: aliases of entries already there (`monster_baron` = Justice, `death_knight` = Death Knight, `edie` =
  Freddie, `juggernaut` = Seeker, `bob` = Jim, `ogre_marksman` = AD's ogre; `item_artifact_cross` = Blast Belt,
  `trinity` = Nail Piercer, `wetsuit` = Air Tank; `item_plasma` = cells; `weapon_bomb`/`hammer`/`plasmagun` remove or
  redirect), the `*minion` classes (precache helpers for bosses' summons: `judgeminion`, `minotaurminion`,
  `nourminion`, `shalrathminion`, `skullwizminion`, `wraithminion`, `wraithminion2`), the ghost props with no pain or
  death (`dknightghost`, `ogre_fishing_ghost`), `hell_pointknight` and `item_keyx` (need a map's target / model), the
  bosses that wait for their map's trigger (Chthon, Eidolon, Mammoth, Fire Top, Shub, Santa), and the ones whose
  models are not in AD 1.80 (`monster_sweeper`, `turretbal`, `sword`, `xmas_elf`, `xmas_raindeer`, `xmas_snowman`:
  only their `.qc` in `my_progs/`).

Tests: `MAPCMD="vr_campaign_native ad;wait30;map ad_crucial" Misc/quakevr/toolgun_spawnall_test.sh <agent> 0 239`:
210 entries, 202 spawned, 0 failed, 8 unavailable (Honey's, Dawn of the Machine's campaign-only); every new entry
spawned and alive except the Yellow Armour Backpack, which with 23 other items (id's, AD's, the packs') was "gone"
because ad_crucial's ring spots are off ledges or in walls: each of those 24 made alone ahead of the player is alive.
Default run (vrfiringrange) PASS: 190 spawned, 0 failed, 20 unavailable (the 8 AD-maps-only among them).
`toolgun_panel_test.sh` PASS; e1m1 smoke clean (`vr_toolgun_spawnlist`: 210 entries, 79 AD, 8 disabled there). The
packs' entries moved up 15 (the MG3 range above is now 135-209).

To check in VR: the new monsters' ghost heights and their fights (no VR rigging for them: they ragdoll as AD's
generic ones), Vermis spawned in an open space, the backpacks' pickup.
## Stomp: landing on a standing monster knocks it down (2026-10-15, worktree `fastship`, branch `agent/stompkd`)

The author: jumping on top of a fully alive enemy should damage it and knock it down, or kill it from high enough.

- **What**: `VR_RagImpact_Crush` (the landing crush, unchanged) now hands a standing monster that lives through the crush
  to `VR_RagImpact_Stomp` (vr_ragimpact.qc): landing fast enough, it is knocked down as a shove's knockdown
  (`VR_Knockdown_Start` + `ragdollshovetopple`, the shove's gates: `vr_knockdown`, a knockdown chance handle, no fliers or
  swimmers, `canragdoll`, an anchored dummy never, and the ragdoll pool's room via `VR_Knockdown_MakeRoom`), toppled
  along your way (standing still: away from you; straight on top: backwards), and gets up after the usual
  `vr_knockdown_time_*`. You bounce up off it (`vr_ragimpact_stomp_bounce` 200 u/s) instead of sinking through as it
  falls; the crush cushion (no fall damage) applies to a stomp too. A lethal landing still kills through the crush
  (Quake's death, a ragdoll corpse).
- **Settings** (Combat > Knockdowns, after the crush rows; all archived): `vr_ragimpact_stomp` 1 (Stomp Knocks Down),
  `vr_ragimpact_stomp_speed` 420 u/s (Stomp Least Speed: about a 115-unit drop; a 128-unit drop lands at 437),
  `vr_ragimpact_stomp_heavy` 130 kg (Stomp Heavy Mass: heavier monsters need the least speed times the cube root of
  mass/130, masses from `VR_Grapple_MonsterMass`: ogre, fiend, vore 250 kg 522 u/s, about a 180-unit drop; shambler 600
  kg 699 u/s, about 315 units; super shambler 800 kg about 375 units; 0: all alike), `vr_ragimpact_stomp_bounce` 200
  (Stomp Bounce). The stomp works with the crush off (`vr_ragimpact_crush 0`) or with Crush Standing Ones off.
- **Why a cube root**: a square root put the shambler at ~430 units, past the 352-unit drop that already pops its head
  (the crush), so it could never be stomped down; the cube root leaves it a window (~315-345 units).
- **Prints**: `vr_knockdown_debug 1` (or the tests' `vr_knockdown_test 56`) prints `ragimpact stomp:` lines (knocked
  down, stands under the least speed, or cannot be knocked down and why); `vr_knockdown_test 50` adds a
  `ragimpact doll:` line (ragdoll, knockdown, health).

Measured (headless, vrfiringrange, `vr_knockdown_test 59` drops onto the head, knockdown time 3 s, state 150 frames and
1200 frames later; scratch/stomp_test.py):

| monster | drop | landing | crush | knocked down | dead | later |
|---|---|---|---|---|---|---|
| grunt | 64 | under 400 | none | no | no | standing, 30 hp |
| grunt | 128 | 437 u/s | 13 to the head | yes, bounced 200 | no | up again, 13 hp |
| grunt | 192 | 17.4 m/s | 53 | - | yes, ragdoll | - |
| grunt | 256 | 19.9 m/s | 81, head pops | - | yes, ragdoll | - |
| grunt | 384 | 24.5 m/s | 133, head pops | - | yes, ragdoll | - |
| ogre | 128 | 436 | 13 | no (under 522) | no | standing |
| ogre | 192 | 538 | 49 | yes | no | up again, 138 hp |
| ogre | 256 | 20 m/s | head pops | - | yes, ragdoll | - |
| shambler | 256 | 628 | 81 | no (under 699) | no | standing |
| shambler | 320 | 706 | 109 | yes | no | up again, 463 hp |
| shambler | 352 | 23.5 m/s | head pops | - | yes, ragdoll | - |

`ragimpact_test.py crush`: all pass, with a new check (the 128-unit drop onto a standing grunt knocks it down). Melee
eval 168/177 (as before). e1m1 smoke: no errors.

For him in VR: jump off a ledge or a crate onto a grunt's head (about a storey): it should go down under you and you
bounce up off it, then it gets up; from two storeys it dies. An ogre takes a higher jump; a shambler about two storeys,
and higher pops its head. Off-centre on the shoulders: the crush strikes the torso or an arm, the knockdown the same.
Open: a big monster's head crush kills from fairly low (ogre 256, shambler 352 units: the crush's head wear, not the
stomp); if that feels too easy, `vr_ragimpact_piece_hp` or the crush's mass is the knob.

## Active rigs for AD's humanoids (worktree `dequant`, branch `agent/adrigs`, 2026-10-15)

His go: "Please work on adding active ragdoll rigs for AD humanoids" (BACKLOG, "Active humanoid coverage for AD's
soldiers"). AD's monsters already had ragdoll rigs (`vr_ad_rigs.inc`), knockdowns and get-up frames
(`vr_ad_monsters.qc`); what they lacked was the drive: the engine drives a living or dying ragdoll only for the classes
in `activeClasses` (vr_box3d.cpp), and none of AD's own classnames were there.

- **Rig semantics checked** per model: every one below has pelvis, chest, head and two-bone arms
  (`upperarm_*`/`forearm_*`, what the reaches and the brace look for) and legs (`thigh_*`/`shin_*`); extra bones are
  harmless (the crossbow knight's and death guard's `hand_r`, the death knight's `foot_l`, the sergeant's `flail`).
  Get-ups: each one's deaths played backwards (`VR_Knockdown_SetupReverse`, the rig's `deathFirst/Last`), the zombie
  knight's and the mummy's their own falls (paine), as id's zombie. Attack origins and locomotion are untouched: the
  body is handed back to AD's own animated monster.
- **Added** to `activeClasses`: `monster_army_grenade`, `_plasma`, `_rocket`, `monster_defender`, `monster_eliminator`,
  `monster_pyro`, `monster_fumigator`, `monster_dcrossbow`, `monster_dfury`, `monster_dguard`, `monster_dknight`,
  `monster_dsergeant` (may crawl), `monster_zombiek`, `monster_mummy` (no crawl; Rogue's mummy too). AD's own grunt,
  enforcer, point knight and zombie use Quake VR's classnames and already were. So: a shove's pinned stagger and fall,
  the get-up through the body, the lift's flail, swimming and drowning, active deaths (stumble, wound hold, crawl),
  limb-cut reactions.
- **Not added**: the centurion (on its hover board: never knocked down nor lifted; its right leg has no shin) and the
  skull wizard (a robe: one leg bone, nothing to stand or stagger on; it teleports). MG3's `monster_ranged_knight` is in
  the lift list but not AD's: left as it was.
- **Bug found and fixed**: an AD monster handed back standing by the engine (a stagger recovered, a body risen:
  `VR_Knockdown_Afoot`) got `self.th_run`, which AD's monsters don't use (theirs is `ad_th_run`): it would have stood
  frozen. Now `VR_AD_RunNow` as the QC get-up path already did.
- **Zombie knight's head zone**: its head hangs to its left (headfit 2.8 3.1 21.1, r 7.0), the zone was on its middle
  line (4, 22, 6.5): 3-5 units off. Head zones can now sit to the side: `xHeadLeft` (weapons.qc; `VR_HeadSide(facing)`
  in every head-middle sum: headshots, positional melee, decapitation zones, the dummy, the tests), AD's
  `vr_ad_headl`, and the engine's hands' zone (`VR_FoeGrab_HeadLeft`, vr_foegrab.cpp `headZoneOf`). The zombie knight:
  3 forward, 3 left, 21.5 up, radius 7.
- **Test**: `Misc/quakevr/ragdoll/ad_active_test.py <agent> [ids]` (one game each on vrfiringrange through AD): neck
  snapped standing (loose; its head's swing from its chest past the living 45-degree cone), then a second one shoved
  (sure) -> staggers as a pinned ragdoll and falls -> rises through its body. 42/42:

  | class | shove -> down | get-up frames | neck swing (deg) | lift flails |
  |---|---|---|---|---|
  | army_grenade, _plasma, _rocket | yes | 17-8 | 32-120 | yes |
  | defender, eliminator | yes | 65-55 | 59-124 | yes |
  | pyro, fumigator | yes | 65-55 | 43-119 | yes |
  | dcrossbow | yes | 255-236 | 66-84 | yes |
  | dfury | yes | 254-241 | 22-120 | yes |
  | dguard | yes | 226-207 | 78-120 | yes |
  | dknight | yes | 243-224 | 35-107 | yes |
  | dsergeant | yes | 254-235 | 22-68 | yes |
  | zombiek | yes | 132-121 | 24-117 | yes |
  | mummy | yes | 173-162 | 25-120 | yes |

  The lift: `judo_test.py adneck` (its lift verdict now needs the flail, but for the skull wizard). Its case turned to
  face the monster with `vr_mock_turn`, which doesn't exist (`vr_mock_turn_to`): the player sometimes faced away and
  his hands closed on nothing; fixed. Three runs (batched, unbatched, unbatched with frames drawn): every driven class
  lifted flails (the zombie knight lifted in 1 of 3, its hands closing on nothing in the others); the standing snaps
  pass for 11-12 of 16 a run, the failing set varying (the player ends 3 units from its origin, inside it, and the
  hands find no hit sphere within 4 m: the harness, the centurion too, which isn't driven) but for two every time: the
  zombie knight (one hand read at its shoulder, even with the zone on its head) and the skull wizard (both at its
  shoulders; not driven, its zone unchanged). No baseline run: restoring the old files for one was refused.
- Melee eval 168/177 (as before; the same 3 differ from the baseline). e1m1 smoke and AD's `start` (a death sergeant
  shoved) clean.
- To try in VR (AD): shove a rocket grunt, a fumigator, a death knight and a zombie knight hard: they stagger, fall,
  get up with their bodies and fight on (not frozen); lift a defender and a death guard: they flail; snap a dfury's
  neck: the head flops; behead or headshot a zombie knight: the head zone sits on its leaning head.
## Grip fit: the hand out of what it grips (worktree `flatfist`, branch `agent/gripfit`, 2026-10-15)

His words: "Sometimes when I grab a prop in a weird way the hand IK overlaps the prop geometry too much, making it look
like my fingers are phasing through the prop. Would it be possible to detect when an overlap is too large and adjust the
grab position automatically to be a bit more on the edge of the prop? [...] customizable [...] retry steps should be
bounded. This should not cause any slowdown or hitch."

- **Once per grab** (`vr_gripfit.cpp`, called from `grip::serverTake`: every one-hand grab, a force grab's catch, a
  wall torch, a pouch's grenade): the hand is its grasp spheres (93: the fingers' segments and the palm), open and in
  the full fist, in its frame (the open set is the one `vr_view.cpp` already measures every frame for the palm's skin,
  now written straight into the grip fit's store: no copy, no new per-frame work; the fist is `held::fist`). A sphere
  counts as sunk by the less of its two poses' depths: a finger in the prop open but clear of it closed is one the
  grasp solve closes clear of it; in it both ways, it phases through however it curls (as does a palm sphere in it).
- **The shape:** the prop's Box3D hulls (`box3d::hullPlanes`: each face's plane; a sphere's depth in a convex hull is
  its radius less its largest face distance, exact inside), or its drawn box without a body (a prop just spawned).
- **The way out:** the sunk spheres' nearest faces' outward normals, weighted by depth. How far along it each sphere
  must go to be no deeper than allowed is solved directly (per face: its distance grows by the normal's share of the
  move), so the fit is three passes however many steps: the depth, the distance out, the depth there. The move is a
  whole number of steps (`vr_grip_fit_step`), at most `vr_grip_fit_steps`, at most `vr_grip_fit_max`; if it can't
  fit, the furthest allowed (the best along the way: never deeper than before, else not moved). The prop is moved the
  other way in the hand, and the move kept with its grip (`Held::fit`, added again when its settings re-place it;
  cleared at a regrip, which keeps where it is, and for a pouch's grenade or round, whose place is its own).
- **Two hands:** the hand that just took hold of what the other holds (`carry2h::serverPlace` grab): its grip on the
  prop moved out of it (the drawn hand sits on its grip), the grips' line with it.
- **Not moved:** Fixed and Along the Handle grips (authored places; a torch's handle is meant to be in the fist),
  weapons (their own grips, not this path), another player's or a dedicated server's hands (not known).

Settings (archived; the Carrying settings page, after Fit Gap): `vr_grip_fit 1` (Keep Fingers Out of Props),
`vr_grip_fit_depth 2.5` cm allowed, `vr_grip_fit_step 1` cm, `vr_grip_fit_steps 12` (1..32), `vr_grip_fit_max 10` cm.
Debug > Profiling and Memory: Grip Fit: Last (`vr_grip_fit_last`: the last fit, then its inputs at every step, timed 1000 times).
With `developer 1` or `vr_debug_carry 1` each fit prints a `gripfit:` line.

Tests (headless, e1m1; `scratch/gf_test.sh`, `gf_2h.sh`; depth in units, 0.79 allowed; off = `vr_grip_fit 0`, whose
held offsets match the measure-only `vr_grip_fit_steps 0` exactly):

| grab | off (depth) | on | steps | moved |
| --- | --- | --- | --- | --- |
| small crate, the hand 13 over its middle (by the top) | 3.59 | 0.76 | 9 | 2.83 |
| small crate, 6 over its middle (deep) | 10.59 | 7.44 (can't fit: the cap) | 12 | 3.15 |
| barrel, 12 over its middle (the rim) | 4.02 | 0.87 (the cap) | 11 | 3.15 |
| health box beside the fist (surface fit off) | 1.45 | 0.52 | 4 | 1.26 |
| health box on the palm (surface fit on) | 0.27 | 0.27 | 0 | 0 |
| explosive box, 10 over its corner (deep) | 9.63 | 6.48 (the cap) | 12 | 3.15 |
| second hand into a held health box (4 cm in) | 1.63 | 0.58 | 5 | 1.57 |

Screenshots (`scratch/gf_before.png`, `gf_after.png`, the crate by the top): before, only a fingertip shows over the lid;
after, the fingers show over it. Timing (`--exclusive`, `vr_grip_fit_last`): the worst case (every sphere sunk, all 32
steps, the barrel's 66 faces) 19.9 us mean, 18.2 us least (1 percent of runs over 50 us: the thread put off); a live
barrel grab 29.7 us, the crate 8-28 us, a clear grab about 2 us. No allocation (fixed arrays), nothing between grabs.
A pass per step (the first version) was 160 us for the barrel at 32 steps: replaced by the solved distance.

Regression: `grip_gap_test.py both` 13 checks, 0 failed (two hands, force grab catches); `twohand_regrip_test.sh` as
before (the pouch grenade's "not within reach" happens with the fit off too); e1m1 smoke clean.

In VR: grab a crate or barrel with the hand pushed well into it (or by an edge at an odd angle): the prop should jump
out of the hand a little, so the fingers sit at its surface, not through it; a normal grab should not move it. Try the
Allowed Depth and Most Move sliders if it moves too much or too little.

### Second pass: the drawn hand on the surface (worktree `flatfist`, branch `agent/gripfit2`, 2026-10-15)

His verdict on the first pass's pictures: "In both your screenshot the hand is almost completely embedded inside the
crate. Both the hand and finger overlap should be minimized." (The first pass's settings above are replaced.)

What was wrong: the hand was 93 grasp spheres: the fingers and one thin sheet at the palm's skin (radius 0.5 cm), so
the back of the hand, the wrist and the ball of the thumb were never measured; a sphere counted as sunk only if it
was in both open and in the fist (a finger could pass with its base clear closed and its tip clear open, in no one
pose); the escape was one fixed direction, the move capped at 10 cm with 2.5 cm allowed. New measuring tool,
`vr_grip_fit_drawn` (Debug > Profiling and Memory > Grip Fit: Drawn Hand): every vertex of the hand as drawn now
(455) in the held prop's shape, the hand and the fingers apart (cm).

- **The hand is the drawn mesh** (`vr_gripfit.cpp`): its vertices posed as the hand draws them, through the rig
  frame `vr_view.cpp` keeps every frame (a matrix and the fingers' shifts; the open-sphere store is gone). The palm
  joint's vertices (palm, back of the hand, wrist: 40 points) and each finger's and the thumb's with its ball (4
  points each) at three curls of the closing path (0, 2, 4), each part thinned once per rig by farthest-point
  sampling: 100 points.
- **A finger as the grasp draws it:** closed from open until it touches, or sunk open, from the tightest curl it is
  clear at; so a finger counts as in only if it is in at every curl, by its least sunk curl (all its points in one
  pose). Fingers may press `vr_grip_fit_finger_depth` (0.4 cm) in, the palm, back and wrist `vr_grip_fit_palm_depth`
  (0.2 cm). The overlap is every point's depth past its allowance, summed.
- **The shape:** the Box3D hulls, each face pushed out to the drawn model's furthest vertex along it (at most 6 cm:
  the barrel's hoops stand 4 cm out of its hull; the crates' hulls are their drawn boxes exactly).
- **The move:** the ways out are the faces nearest the sunk points (the 3 most weighted by depth, the pair of the
  first two, the depth-weighted sum); along each, the distance that clears every point is solved at once from the
  face distances; tried in order of that distance (the least move first), capped at `vr_grip_fit_reach` hand lengths
  (0.75, 18 cm), the first 2 measured there: the first that clears is kept, else the one in least (never deeper than
  before). Menu (Carrying): Finger Contact, Palm Contact, Most Move (hands); the step rows are gone (no steps).

Tests (headless, e1m1; `scratch/gf2_test.sh`, `gf2_2h.sh`, `gf2_place.sh`): the drawn hand 40 frames after the grab,
`vr_grip_fit_drawn` (points in of 172 hand / 283 fingers, deepest cm), off = `vr_grip_fit 0`:

| grab | off: hand | off: fingers | on: hand | on: fingers | moved |
| --- | --- | --- | --- | --- | --- |
| small crate, hand 13 over its middle (by the top) | 106, 6.2 | 247, 7.2 | 0 | 0 | 10.1 cm |
| small crate, 6 over its middle (whole hand in) | 172, 28.2 | 283, 29.2 | 130, 11.9 | 272, 11.5 | 18 (the cap) |
| barrel, 12 over its middle | 112, 7.7 | 256, 8.9 | 0 | 0 | 13.5 |
| health box beside the fist | 0 | 48, 0.67 | 0 | 48, 0.67 | 0 (clear) |
| explosive box, 10 over its corner (whole hand in) | 172, 29.5 | 283, 27.5 | 172, 11.2 | 283, 9.2 | 18 (the cap) |
| health box on the palm (surface fit off) | 37, 3.8 | 117, 4.7 | 0 | 27, 1.7 | 4.5 |
| second hand into a held health box (4 cm in) | 60, 4.1 | 175, 5.1 | 0 | 9, 1.2 | 5.1 |

The fingers left in (0.5-1.7 cm) are the grasp solve's own snug contact as it wraps (the health box held clear shows
the same 0.67 cm with the fit off). Pictures (`scratch/gf2_pair.png`, side views; the hand placed in reach so the arm
is drawn: `gf2_place.sh`, the crate taken by its lid's near edge with the hand 6 units under the lid, the barrel at its
rim): off, the hand is inside the crate (only the forearm shows) and inside the barrel's wall; on, it lies on the lid
and holds the barrel's outside at the rim (drawn: 7 and 0 hand points in, under 0.5 cm). Note: in the first pass's
`nearest` cases the hand is beyond arm's reach, so no arm is drawn and the fist on the lid reads as a fingertip.

Timing (`--exclusive`, `vr_grip_fit_last`, the barrel's 66 faces): the solver's worst case (every point in every
hull, all ways, 2 measured) 31.4 us mean; a live barrel grab 70-76 us with cold caches (the shape 14, posing the hand
7), a crate 25-39 us, a clear grab 13 us. No allocation at a grab (fixed arrays; the rig's points are chosen when the
rig is first used). Regression: `grip_gap_test.py` cycle 7 checks and forcegrab 6, 0 failed; e1m1 smoke clean.

In VR: grab a crate by its lid or a barrel by its rim with your hand pushed well in: the prop should drop out of the
hand so the hand lies on the lid or round the rim; a grab with the hand already on the surface should not move it.
A hand pushed in more than about 18 cm (Most Move 0.75) stays partly in.

## Gloves: leather, not painted skin (worktree `flatfist`, branch `agent/glovetex`, 2026-10-16)

His note: the gloves felt like the hand painted in a glove's shape: the full glove showed the nails and every crease
through it. Now where a glove covers, the skin's colour **and** its painted detail are gone; the leather has its own:

- **The mask's A channel** (`Misc/quakevr/make_skin_masks.py`, the hand's four `hand_rig_NN_00_skinmask.tga`; R, G, B
  unchanged bit for bit) is no longer the skin's shading but the glove's relief, made from the fingers' 3D coordinates:
  0.5 flat; seams down each finger's sides (fourchettes, a little towards the palm, ending before the tip), across the
  fingers' palm-side base and round the thumb's base, each a groove (to 0.15) with a row of raised stitches (0.8, 3 mm
  apart) either side. The bodies' A is flat 0.5 (the forearm cuff is plain leather).
- **The shader** (`vr_glsl.h` `GloveLeather`, `GloveSheen`; QVR_ALIAS_FS_LIGHT): the leather = the HSL colour (hue,
  saturation, lightness as before) x the seams' shade (grooves dark, stitches light) x a procedural pebbled grain (a
  3x3 cellular pattern in the skin's texel space, ~1.2 mm pebbles, each its own slight tone, crevices darker; faded to
  its mean where a pebble is under ~2 pixels, so no shimmer) x the hem (still 30% darker at the edge, and now rolled
  over: a ridge). Its normal: the skin's normal map faded out under the glove (no creases or nails in the light either)
  and the seams', hem's and grain's slope put on instead (`WoundBump`). A soft broad sheen (exponent 18, a little
  Fresnel), duller in the crevices and seams. The hand's form (its normals, the lighting, AO) stays. Fingerless gloves'
  bare finger ends, and bare hands, are skin as before.

GPU cost: only glove-covered fragments of the hand and cuff pay: 2 more taps of the mask, a 9-cell hash loop (~120 ALU)
and one pow. Measured (`--exclusive -RealTime`, hands 13 cm from the eyes, 1024x1024 eyes, `vr_profile_report`): frame
GPU 1.117 / 1.113 / 1.137 / 1.250 ms for gloves off / on / off / on: within the run's noise.

Tested (headless, vrfiringrange, `scratch/glove_close.py`: backs of the hands and palms; before/after in
`scratch/gloves_leather_before_after.png`): full brown, full red, fingerless brown, fingerless blue: no nails, no
creases, seams and grain on the leather, finger ends still skin; bare hands: the new shader against the old with the
same masks, 9 pixels differ (max 17, an edge). e1m1 with gloves 2/1, bracers 0, body modes 3/0, `vr_hand_rig` 1/0: no
errors. For him in VR: the grain's size and depth (`GloveLeather`: 0.55 cells a texel, depth 0.14 texel), the seams'
places, the sheen's strength.
## Crushing pops heads and limbs: down ones, corpses, same-level jumps (worktree `dequant`, branch `agent/crushpop`, 2026-10-16)

His note (dm1): landing on a ragdoll, alive or dead, never popped its head or limbs, from any height; repeated jumps
onto a lying head should pop it after a few, two or three heights at once; ogres and shamblers hurt only.

Why it never popped in play (the headless tests landed on the torso, so they passed):
- **The part under your feet was never the head.** `ragdollbone(m, feet)` is the part whose surface is nearest the
  feet, and your feet go through a lying body to the floor: under the head that is the chest or a shoulder. The test
  dropped you over the box's middle, the torso anyway.
- **Corpses were skipped** (`health <= 0`), so a dead ragdoll never went through `VR_Limb_WearInto`.
- **Same-level jumps never counted**: a jump lands at ~270 u/s, under Quake's landing gate (-300) and the crush's
  least speed (400).
- Wear from a crush was its damage, small at a jump's speed; a living one's limb only pops on the killing blow.

Now (QC `vr_ragimpact.qc` `VR_RagImpact_Crush`, `VR_RagImpact_Underfoot`; `client.qc`):
- **What your foot touches** (`VR_RagImpact_Underfoot`): a 3x3 column of points 6 units apart round your feet's middle,
  from 6 under your feet to 26 over them, each asking `ragdollbone(m, p, 2)` (new optional `within`: -1 unless a part's
  surface is within 2 units; engine `box3d::ragdollBone`); the head before a limb before the torso, then nearest the
  middle. Living ones down or staggering and **corpses** (ragdolls, not gibbed) alike.
- **Same-level jumps count**: the crush's landing gate is the lower of Crush Least Speed and Stomp Least Speed (100 at
  least), not Quake's 300; a soft landing's `jump_flag` is zeroed once you are down so it counts once. Crush Least Speed
  `vr_ragimpact_crush_speed` 400 -> **200** u/s (a same-level jump: ~260 here).
- **Per-landing wear** (`vr_ragimpact_crush_wear` 0.3, row Crush Wear): a landing wears that share of the head's or
  limb's own health (`vr_ragimpact_piece_hp` x corpse health: a grunt's 32) at the least speed, rising with the square of
  the way to **Crush Pop Speed** (`vr_ragimpact_crush_pop` 380 u/s, about a 90-unit fall to the floor: two heights onto a
  body lying down), where it is all of it: popped at once. A same-level jump wears ~0.35: the third pops the head. The
  wear is the shared `vr_limb_corpse_dmg`, so a living one's wear carries over to its corpse. Health damage is the
  crush's as before (`vr_ragimpact_damage` x 50 kg x the excess speed; head 1.5x): a jump ~11 to the torso, ~17 to the
  head.
- **Big ones hurt only** (`vr_ragimpact_crush_big` 200 kg, row Crush Big Mass: ogre, fiend, vore, shambler, AD's
  bigger ones): no head or limb is worn, and one landing takes at most `vr_ragimpact_crush_big_max` 0.25 of its full
  health (row Crush Big Most); the stomp's knockdown is unchanged.
- A living one's limb still pops only on the killing blow (as every limb pop; worn through, the next blow that kills it,
  or a crush on its corpse, pops it).
- The stomp knockdown is unchanged, but a humanoid landed on from the stomp's height (420 u/s) now dies of the crush
  (head popped, or ~75 damage), so the knockdown is mostly for big and tough ones (an ogre from 256 units: 50 damage,
  knocked down).
- Debug > Tests: "Impacts: Jump onto the Nearest Ragdoll's Head" (`vr_test_drop_height 46; vr_knockdown_test 60`) and
  "Drop onto the Nearest Ragdoll's Head" (112). Tests 60/61 put you over the nearest ragdoll's head / left shin, your
  feet `vr_test_drop_height` over the floor; 62 kills the nearest living one.

Verified (headless, `python Misc/quakevr/foegrab/ragimpact_test.py dequant crush stompwear`, 0 FAILS; new case
`stompwear`): same-level jumps (46 units, 8.2 m/s) on a down grunt's head: 0.35 a landing, dead at the second, head
popped at the third (as its corpse). 112 units on a down grunt's head: popped in one landing. A corpse: 112 units on its
head, then its shin: both pop. Standing grunt, knight, enforcer, 112 units over their top: head popped (76-81 damage).
Ogre, shambler: 50/51 damage, alive, nothing popped. Crush case: 256 onto a down one 104 to the torso; 16 units over the
floor nothing; standing grunt 64/128 units 40.6/84.6 to the head (128 pops it); 16 nothing; ogre 256 knocked down by the
stomp (alive); cushion 100 vs 95. wall, grab pass; slam is flaky (head slams' speeds vary run to run: 1 fail, 1 pass on
this branch, a pass on the base). Melee eval 168/177 (baseline). e1m1 smoke clean.

In VR: knock a grunt down and jump on its head from beside it: the third jump pops it. Kill one, jump off a ledge
about twice your height onto its head, then onto a leg: each pops. Do the same onto a standing grunt, knight or
enforcer: its head pops. Onto an ogre or a shambler: it is hurt, never killed outright, nothing pops.
## Melee arc: monster blows only ahead of the monster (worktree `tghost`, branch `agent/meleearc`, 2026-10-16)

His note (AD's start map): a knight's sword hits him while it still faces away (he snuck up behind it) and when he
steps to the side of its body as it charges. Cause, the same in every monster's melee: Quake's blows check reach only
(`ai_melee`: origins within 60 units; a bite or claw 100; AD's `ad_ai_checkmelee`: level distance and height), never
the facing; the generic `CheckAttack` (knight, hell knight, fish; AD's generic and most of AD's own checks) starts the
attack the frame the player is in melee range, whatever the facing; and while swinging the monster turns after him at
full speed, twice a frame (`ai_charge` = `ai_face` plus `movetogoal`'s step, each a `ChangeYaw` at `yaw_speed` 20),
so a step aside is caught within a frame or two.

What changed (`QC/fight.qc`, one helper family used by every path):

- `VR_MeleeArc_Hits(wide)` on each blow's hit frame, after its reach check: against a player the blow lands only if he
  is within `vr_enemy_melee_arc_angle` (default 65) of the monster's current facing (level, to `VR_AI_Origin`), plus
  `vr_enemy_melee_arc_wide` (20) for sweeping blows. Monsters fighting each other keep Quake's blows.
- `VR_MeleeArc_Launch()` where an attack starts at once (`CheckAttack`; AD's `ad_*CheckAttack`, 21 places): facing
  away (outside the arc) the attack is not started; `attack_state = AS_MELEE` instead, so `ai_run_melee` /
  `ad_ai_run_melee` turns it first and starts it when facing (`FacingIdeal`, 45 degrees). `ai_run_melee` now picks the
  knight's run or standing swing (`knight_attack`) as `CheckAttack` did.
- `VR_MeleeArc_TurnBegin/End` around a swing frame's turns (`ai_charge`, `ai_charge_side`, the fiend's, gremlin's and
  scourge's `ai_face`; AD's `ad_ai_charge`, `_chargenoturn`, `_charge_side`, `_charge_front`, the fiend's, drole's,
  minotaur's and seeker's claws): all of a frame's turns together at most `vr_enemy_melee_turn` (4) degrees.
- Cvars (archived): `vr_enemy_melee_arc` 1, `vr_enemy_melee_arc_angle` 65, `vr_enemy_melee_arc_wide` 20,
  `vr_enemy_melee_turn` 4; Combat > Holding Enemies > Melee Arc, Melee Arc Angle, Wide Swings, Swing Turn. Debug >
  Tests > Holding Enemies: Log Melee Arc (`vr_enemy_melee_arc_debug`: each blow's angle and verdict) and three test
  rows (`vr_test_meleearc` 1 faces you, 2 faces away, 3 you step aside; `QC/vr_meleearc_test.qc`).
- AD's files edited in place (as melee space's), each line marked `// AD support`.

Audit (how each blow decides a hit; arc = now also the arc, wide = with the wide margin):

| Monster (file) | Hit test before | Now |
|---|---|---|
| Knight standing swing, hell knight all swings and charges, the expansions' knights on these frames (`ai_melee`, fight.qc) | origins within 60 | arc; launch gated |
| Knight run swing (`ai_melee_side`: 5 hit frames while running past him) | 60 + CanDamage | arc (not wide); launch gated |
| Rottweiler bite (`dog_bite`) | 100 + CanDamage | arc |
| Rottweiler, fiend, spawn leaps (`*_JumpTouch`) | touch at speed | unchanged: the body must touch him |
| Fiend claw (`Demon_Melee`) | 100 + CanDamage | arc |
| Ogre chainsaw (`chainsaw`: swing turns the ogre up to 25 degrees a frame, smash) | 100 + CanDamage | wide |
| Shambler smash / claws (`sham_smash10`, `ShamClaw`) | 100 | arc / wide |
| Rotfish bite (`fish_melee`) | 60 | arc; launch gated (generic CheckAttack) |
| Zombie, vore, grunts, enforcers | no melee (thrown flesh, missiles, our shoves) | - |
| Gremlin claw (`Gremlin_Melee`), scourge tail (`Attack_With_Tail`) | 100 | arc |
| Mummy (rogue) | zombie's thrown flesh | - |
| Dragon tail (rogue) | 150, meant for behind it | unchanged |
| MG3 super shambler smash / claws | 100 + CanDamage | arc / wide |
| AD `ad_ai_melee`, `ad_ai_meleesmash`, `ad_ai_melee_side` (knight, death knight, dguard, dsergeant, dlord, dprince, fury, crossbow knight, zombie knight, judicator, ...) | `ad_ai_checkmelee` 70/meleerange/60 + z | arc; launch gated |
| AD fiend, dog, fish, spider, steelclaw, swampling, voreling, scorpion, seeker, lost soul, dguard quoth, xmas four | `ad_ai_checkmelee` | arc |
| AD drole, minotaur, freddie, shambler (side blows) | `ad_ai_checkmelee` | wide for side blows |
| AD ogres (chainsaw) | `ad_ai_checkmelee` | wide |

Tests (`Misc/quakevr/melee_arc_test.sh <agent> id`, vrtesthall, monster 56 units away, 4 s; arc off -> on; hits in
4 s, first hit (s); step: he is moved to its right, 100 degrees off, as its attack starts, hits in the next second):

| Monster | facing him: hits, first | its back to him: first, angle then | step aside: hits after |
|---|---|---|---|
| knight standing | 12, 0.6 -> 12, 0.6 | 0.6 -> 1.3 (17) | 3 -> 0 |
| knight run swing (100 units) | 14, 0.5 -> 11, 0.5 | - | 5 -> 0 |
| hell knight | 18, 0.5 -> 12, 0.5 | 0.5 -> 1.2 (21) | 5 -> 0 |
| ogre | 17, 0.7 -> 16, 0.6 | 1.3 -> 1.2 (21) | 6 -> 1 (the sweep) |
| fiend | 5, 0.6 -> 5, 0.6 | 1.2 -> 1.2 (17) | 2 -> 0 |
| rottweiler | 4, 0.5 -> 4, 0.5 | 1.1 -> 1.1 (25) | 1 -> 0 |

With the arc off a step-aside hit lands at 90 degrees (knight run swing). The fiend, ogre and dog already turned first
(their own checks use `AS_MELEE`), so only their angle at the first hit changes. AD's monsters could not be run here
(no AD data in the kit's bases): built and reviewed only. Melee eval 168/177 (unchanged); e1m1 smoke (all 23 awake)
clean.

In VR: let a knight charge and step to its side as it swings (it should miss, then turn and come again); sneak behind
one and wake it: it should turn to you before the first blow lands. Standing in front, blows land as before. Too easy
to dodge: raise Swing Turn (the old tracking was up to 40 a frame); too strict: widen Melee Arc Angle.
## Gib props: every monster's head and gibs (worktree `backlogclean`, branch `agent/gibprops`, 2026-10-16)

His notes: an AD crossbow knight's head could not be picked up and ignored physics; the hammer hunter ogre's head was a
physics object but never burst; every enemy's gibs should be proper props.

How id's do it: `ThrowHead` (player.qc) makes the monster itself its head and `ThrowGib` spawns each gib; both call
`VR_MakeGibGrabbable` (the model's box, touchable, force-grabbable, carried; a Box3D rigid body from its first landing,
VR_Gib_Touch, or when taken) and `VR_Gib_MakeDestroyable` (vr_gib_health times 1, a head 1.5; armed after 0.2 s; damage
goes to `VR_Gib_Damage`, which bursts it: blood mist, small gibs, a head's brain). The props table (vr_props.inc) only
tunes masses: a model with no slot (every AD head) is a prop all the same, its mass estimated (an AD head: its share of
its ragdoll's, `VR_Limb_SetMass`, as a beheading's). Every id1, Scourge of Armagon, Dissolution of Eternity, Honey
and Dimension/Dawn of the Machine monster goes through these two (the gremlin's `Gremlin_ThrowHead` and MG3's own deaths
too): nothing to fix there.

Why AD's were not: two ways.
- AD's own gibbing (a live one blown apart: AD's `monster_ThrowGib`, `ThrowGib`) spawns AD's `item_gib` entities, head
  (kind 25) and gibs alike: bouncing, fading after 10-20 s, never ours. Now two `ad_import.py` hooks (FUNC_PATCHES:
  `ai_gibs.qc`) call `VR_AD_Gibbed` (our gore, small gibs and limbs, as ThrowHead's) and `VR_AD_GibThrown` for each
  gib (vr_ad_monsters.qc): the head made as `VR_Decap_ThrowHead` makes one (the monster's class, HeadThink, grabbable,
  destroyable 1.5, its ragdoll's head mass), each flesh gib as ThrowGib's. Kept AD's: stone/metal/wood monsters'
  gibs (AD's gibtype; AD switches it to metal before a machine's parts), weapons and armour among the custom gibs (an
  impact sound of wood/metal/chain, or a `progs/w_` model: crossbows, swords, grenade launchers), contact-damage gibs
  (gib1dmg), the robot floyd's and the snowman's heads (no flesh). With vr_gib_limbs 2 AD's meat gibs give way to the
  limbs as id's do; a head already cut off is not thrown again.
- A corpse gibbed is Quake VR's (`VR_Corpse_Parts`: ThrowHead on the AD monster, AD's head model): a prop, but it kept AD's
  class, so `AD_Run` sent every hit on it to AD's T_Damage, which knows nothing of VR_Gib_Die: the hammer ogre's
  unbreakable head. `AD_Run` now skips our gibs and heads (`.vr_gib`, `th_die == VR_Gib_Die`), AD's gibs made ours too.

Test: `vr_gibprops_test` (vr_gibprops_test.qc; Debug > Gore Tests > Gib Props rows), run by
`Misc/quakevr/gibprops_test.sh <agent> <map> <alive|corpse> <spawner choices...>` (AD: `PRE="vr_campaign_native ad"`,
map start), which prints the table (`gibprops_table.py`): per monster the debug spawner makes, gibbed alive (or killed
whole and its corpse gibbed), its head grabbable and destroyable, a Box3D body once landed (physicsdamp), taken by the
main hand's touch with the mock's grip held, carried with a held body (heldbox), loose again when let go, then shot
(armed, burst, the small gibs made); its gibs grabbable and destroyable. Every head below: grabbable 1, Box3D 1 (a few
still in the air at 150 frames: enforcer, boglord, hell knight, polyp, flak ogre, deflector, prince; all Box3D once let
go), taken/held/loose 1/1/1, destroyable 1, burst with 22 small gibs; every gib of ours grabbable and destroyable.

| Game | Monster (head model) | Head: prop / destroyable / bursts before | after |
|---|---|---|---|
| id1 | grunt h_guard, ogre h_ogre, zombie h_zombie, shambler h_shams, scrag h_wizard, knight h_knight, death knight h_hellkn, rottweiler h_dog, enforcer h_mega, fiend h_demon, vore h_shal; corpses gibbed too | yes / yes / yes | same |
| id1 | spawn, rotfish, Chthon, Shub (no head) | - | - |
| hipnotic | gremlin h_grem, centroid h_scourg | yes / yes / yes | same |
| rogue | mummy h_zombie, eel eelgib (ThrowHead); wrath, dragon: gibs at the death's end, all props (3/3); swordsman, overlord, Guardian, lava man: no gibs | yes / yes / yes | same |
| Honey | marksman ogre h_ogre, sknight h_hellkn | yes / yes / yes | same |
| MG3 | rocket ogre h_ogre, demo dog h_dog, ranged knight h_hellkn, super shambler h_shams; infected: its grunt head thrown as a gib (by design), its zombie h_zombie | yes / yes / yes | same |
| AD, gibbed alive | death knight h_dknight, zombie knight h_zombiek, crossbow knight h_dcrossbow, hunter ogres h_hogre / h_hogreham (mace h_hogre), death guard h_dguard, gaunt h_gaunt, soldiers h_soldier, defender/eliminator h_enforcer, gargoyle h_gargoyle, minotaur h_minotaur, death fury h_dfury, centurion h_centurion, death sergeant h_dknight, drole h_drole, boglord h_boglord, gug h_boglordb, scorpion h_scorpion, justice h_justice, pyro/fumigator h_pyro, eel h_eel, freddie h_ogre, AD's zombies h_zombie, scrag h_wizard, hell knight h_hellkn, polyp h_gaunt, flak ogre h_ogre, deflector h_deflector, prince h_dprince, Quoth death guard h_dguardq | no / no / no (AD's item_gib) | yes / yes / yes |
| AD, corpse gibbed | the same heads (crossbow knight, hammer ogre, hunter ogre, soldier, fury, drole, scorpion, death guard tested) | yes / no / no (AD's T_Damage) | yes / yes / yes |
| AD | spiders and vorelings (AD throws more gibs instead), skull wizard, lost soul, boil, jim, swampling, wraith, death lord, steel claw, sentinel, fishing ogre (no head); golem, seeker, floyd, snowman (stone/metal: AD's); Nour, ice golem, judge (never gibbed by damage) | - | gibs: flesh ones props (spiders 6/6, boil 12/12, wraith 6/6...) |

Regression: e1m1 fight (vr_test_monsters 3) and ad_tfuma fight (163 woken, 77 killed; pyros gibbed by AD's code) clean.

In VR: blow apart an AD crossbow knight or a hunter ogre (rocket, quad shotgun): its head should land, be picked up and
thrown like a grunt's, and burst into small gibs and a brain when shot or thrown hard; the same for the hammer ogre's
head off its gibbed corpse. Its crossbow, sword or grenade launcher lying there stays AD's (not grabbable).

## Magnetic holsters (worktree `permdecals`, branch `agent/maghol`, 2026-10-16)

His request: RE4VR's magnetic holsters. A hand at a holster with a weapon in it presses the bind button: the weapon
is bound to that holster; thrown or dropped, it comes back on its own after a while or past a distance; a chain shows
the binding (the flashlight's). His decisions: the chain's look a setting (lanyard, tether, both), its size and
opacity too; the return a flight through the air (and walls) over ~0.4 s that snaps in with a click.

- **Binding** (QC `vr_magholster.qc`): the press is taken by the engine before the game gets its key
  (`vr_magholster.cpp` `button`, in `vr_input.cpp` before bullet time's stick) only while the pressing hand is at a
  holster that has a weapon in it or a binding, Weapon Mode Immersive; elsewhere the stick click does what it did. The
  QC reads it with the `magholsterpoll` builtin (the local player's; the hand's hotspot says which holster) and binds
  the holster's weapon (its record's id, `vr_weaponinst.qc`, in `.vr_magbind0..5`), or unbinds what is bound there:
  one weapon a holster, one holster a weapon (binding it at another holster moves the binding). Feedback: the
  flashlight's clip-on sound (bind) or clip-off (unbind), a double tick (bind) or a single one, the chain appearing.
  An empty unbound holster: the holster's "not empty" buzz, nothing else.
- **Return**: each frame (`VR_MagHolster_Frame`, end of `W_Frame`) each bound weapon is looked for: held, or in any
  holster (its own or another): it stays. Lying about (a `thrown_weapon` with its record): its clock starts when first
  seen out of the hands; when that clock passes `vr_magholster_return_time` (default 3 s) or it lies
  `vr_magholster_return_distance` (default 4 m) from its holster (either; 0 turns one off), it flies home: no longer a
  rigid body, `MOVETYPE_NOCLIP`, steered each frame by its velocity along an eased path with a low arc (a quarter of
  the way up, at most half a metre) to the holster's place as your body has it (the `holsterpos` builtin; QC's estimate
  for others) over `vr_magholster_flight_time` (0.4 s), its touch off on the way (it hurt the player it flew into
  in the first run: 27 damage), then into the holster with its record (magazine, id, blood, heat) and the holster
  click. Stuck in a wall or a monster: pulled out first. A force grab pulling it: it waits.
- **Grabbed on the way**: it is still a thrown weapon with its handtouch; a hand closing on it takes it as a caught
  throw (its velocity is real, so the mid-air catch window works) and the flight is over ("caught on its way home").
- **The holster taken** (another weapon put in it) when it would return: it waits where it lies and comes back the
  moment the holster is empty again; taken during the flight, it drops where its flight got to and waits likewise.
- **Saves and levels**: the bindings are player fields (saved). A level change brings the bound weapons lying about
  home first (holster empty), then carries each binding as its weapon's place among the carried ones (`parm73`, 4 bits
  a holster: 1..6 a holster, 7, 8 the hands; `lastExtSpawnParm` 73). A weapon gone (used up, destroyed, faded):
  unbound.
- **Chains** (`vr_magholster.cpp`, the flashlight's `coil::Cord` chain; drawn after the cell cords): `vr_magholster_style`
  0 Lanyard (default: an 11 cm chain hanging off the weapon's bottom, `cellcord::weaponBottom`, wherever it is drawn:
  holstered, held, lying, flying; a 4.5 cm stub of it hanging at the holster while the weapon is away), 1 Tether (a
  chain from the holster to the weapon while it lies about or flies home, 4% slack, its links grown past 8 m so they
  reach), 2 Both, 3 None. `vr_magholster_chain_size` (0.6: links times the flashlight's 1.6 cm), `vr_magholster_chain_alpha`
  (below 1: `gfx::drawTube`'s new `alpha`, blended, no depth write). `coil::Style` gained `stiffness` and `airDamping`
  (defaults as before: the flashlight and the cell cords unchanged): the chains use 25 and 5, else a loose chain
  hanging off one end stretched under its own weight (a 0.11 m lanyard drew 0.27 m) and whirled out straight when the
  hand jumped; `maxLinks` 192 -> 640 for the tether.
- **Menu**: Immersion > Weapons and Holsters > Magnetic Holsters: Magnetic Holsters (`vr_magholster` 1), Bind Button
  (`vr_magholster_button`: 0 either stick click, 1 the off hand's, 2 the main hand's, 3 the lower face button A/X),
  Return After, Return Beyond, Flight Time, Chain, Chain Size, Chain Opacity. Debug > (flashlight's page): Magnetic
  Holsters Info (`vr_magholster_info`) and Magnetic Holsters: All Home (`vr_magholster_return`).

Tests: `Misc/quakevr/magholster_test.sh <agent> [time walk catch save busy shots]`, all as described: time (home 2.00 s
after the toss, 0.40 s flight, no hit), walk (flies home at 2.07 m, 0.39 s after the drop; unbound: no flight), catch
(caught by the main hand 1.2 m from home), save (bound after save, unbind, load; a level change with the weapon away:
home first, bound in e1m2), busy (waits 2.4 s with the axe in its holster, flies as the axe leaves), shots
(`scratch/magholster_shots.png`: the lanyard on the holstered and held shotgun, the stub, the tether, the flight). e1m1
smoke clean.

Decisions: the bind button's choices are named by hand (off, main), not left and right (they follow Handedness); the
tether shows only while the weapon is away from the body (lying about or flying), not while held; a weapon put in
another holster stays there; Quick Slots mode: no binding or return (its holsters keep copies). Only the local player
(single player, a listen server's host) can bind: the press reaches the QC through a local builtin.

In VR: bind the hip shotgun (a hand at the holster, click the stick), throw it: it should fly back after 3 s; walk away
from a dropped one; grab one on its way back; try the shoulders and the chest holsters, the off hand, the axe and an
expansion weapon; look at the lanyard on the gun in the hand and at the holster (Chain Size, Opacity), and the tether
across a room.

### Magnetic holster chains fixed (worktree `permdecals`, branch `agent/magchain`, 2026-10-16)

His notes (vrfiringrange): the lanyard a small chain spazzing out at the grip, bunching up as if colliding with
itself; the tether not drawn at all (nothing from the holster to the gun), only a bit of chain spazzing at the holster.

- **Why**: the chains were the flashlight's mass-spring cord with `stiffness` 25 (to hold a short chain's length:
  its stretch is its weight over the stiffness, 1.2 cm at 25, whatever its length). At the 1/300 s substep that is
  past the springs' stable step (the stiffest mode, squeezed: about 950 rad/s, x h = 3.2): at 90 Hz (4 substeps of
  2.8 ms) every link blew up each frame; the headless runs at 250 fps (2 ms substeps) only just held, so the earlier
  tests looked fine. The chains were also carried along by the player's origin, which steps against the body as drawn
  (a holstered gun's chain jittered 10-25 mm a frame while walking). The tether was drawn only while the weapon lay
  about or flew home (the round's decision), never in the hand.
- **Fix** (`vr_coil.cpp`): `coil::Style::rigid` (the magnetic holsters' chains; the flashlight's and the cell cords
  unchanged): the nodes kept their relaxed spacing by position-based constraints (7 sweeps a substep, alternating, the
  last from the held end, at 1/600 s substeps) instead of springs: unconditionally stable at any frame rate, never
  stretched; its spacing eased across the substeps as a tether pays out or reels in. Air damping 3 a second.
- **Magnetic holsters** (`vr_magholster.cpp`): the lanyard, stub and tether carried by the holster's point (the body
  as drawn, as the cell cords' pouch) instead of the player's origin. The tether shows whenever the weapon is seen
  out of its own holster (in a hand too, another holster, lying, flying): its length the distance + 1% + 12 cm close
  by (that slack gone by 2 m: nearly taut far off); faded out over 0.5 m past its reach (the return distance, at
  most 6 m) and not drawn beyond. A jump of the weapon about the holster (over 15 cm a frame) restarts the lanyard
  and the tether; a change of where the weapon is restarts only the lanyard.
- **Debug**: `vr_magholster_chain_log 1` (Debug > Magnetic Holster Chain Log): every frame each chain's worst link
  stretch (%), most movement and jerk (second difference) of its nodes off its anchor (mm; the tether's off the line
  between its ends), and the jumps that restart one.

Test: `Misc/quakevr/magchain_test.sh <agent> [label]` (90 Hz frames: `vr_fixed_frames_rate`, `host_maxfps`;
vrfiringrange, the shotgun bound to the right hip, style Both): the main hand draws it and glides round a loop slowly
(1 s a leg), then fast (0.12 s a leg: about 5 m/s, started and stopped at once), holds still, holsters it and the
player walks; `magchain_summary.py` per phase and chain. Before (the old springs) and after, at 90 Hz:

| phase, chain | link error max / mean % | jerk p95 / max mm | frames jerk > 20 mm |
|---|---|---|---|
| slow, lanyard | 380 / 148 -> 0.7 / 0.3 | 102 / 130 -> 8.0 / 23 | 498 of 500 -> 1 |
| slow, tether (held) | 375 / 108 -> 0.1 / 0.1 | 311 / 504 -> 2.0 / 5.0 | 498 -> 0 |
| still, lanyard | 404 / 175 -> 7.4 / 0.8 | 108 / 140 -> 25 / 75 | 75 of 75 -> 7 (the swing after the fast stop) |
| walk, lanyard (holstered) | 557 / 177 -> 0.5 / 0.3 | 128 / 288 -> 1.2 / 2.7 | 218 of 221 -> 0 |

(The tether before was shown in the hand only for this measurement.) The fast legs swing the chains hard (an instant
5 m/s start and stop; jerk p95 70 mm, a whip, not a jitter: smooth rise and fall frame to frame) and twice the mock's
hand jumps 0.6 m in a frame (the chains restart: the 16.7% error rows are those restarts' first frames). Screenshots
(`scratch/magchain_shots.png`): the lanyard on the holstered and the held gun hanging straight down 11 cm; the tether
in the hand sagging from the grip to the hip holster; the tether from the hip down to the gun dropped 1.06 m away; a
gun thrown 18.6 m: no tether (past 6 m). e1m1 smoke clean.

Decisions: the tether now shows in the hand too (he expected it); beyond its reach it fades and is gone rather than
stretching (the weapon flies home past the return distance anyway).

In VR: Chain Lanyard: the chain at the grip should hang calm and swing with the hand, never bunch; the stub at the
empty holster still. Chain Tether: a chain from the hip holster to the gun in your hand (slack), then throw the gun a
few metres (taut-ish, sagging) and walk away from it with Return Beyond 0.
## The toolgun's spawns: loaded, placed and drawn at once (worktree `fastship`, branch `agent/tgspawnfix`, 2026-10-16)

His notes: a megahealth (or others) spawned in a map without one is invisible; health boxes, the megahealth and ammo
spawn big for a moment and then shrink, and their ghost shows the big one in the wrong place; AD's and MG3's ammo and
armour backpacks spawnable in any map (his decision; maps keep AD's rule, ARCANE_DIMENSIONS.md 16.1); the ghosts of
some AD monsters (the Death Knight...) look corrupted, "their bones all messed up".

- **Invisible spawns**: the megahealth is a brush model (`maps/b_bh100.bsp`); a brush model's lightmaps and vertices are
  built with the map (R_NewMap), so one loaded later reached the client but was never drawn. QC `VR_Toolgun_Precache`
  now precaches every brush-model entry with each map (id's health and ammo boxes small and big, the explosive boxes,
  Rogue's lava nails, multi rockets and plasma boxes where its pak is, AD's golems' rubble `maps/ad_brk/rock01/02.bsp`):
  13-21 small files. The client prints `late brush model ... not drawn` (developer 1) for any other
  (`client::lateBrushModel`).
- **Late precaches reach the client before their use**: a late model or sound precache was sent only in the reliable
  message, which goes after the frame's datagram (and waits for the last one's ack), so the entity made with it or a
  sound started on it came first: the model drawn as nothing until a later update, the sound lost. Each late precache is
  now also written at the head of every client's datagram for half a second (`VR_WriteRecentPrecaches`, sv_main.c
  SV_SendClientDatagram after svc_time); the client skips one it knows. Everything else an entry needs (models, sounds)
  is precached late, when it is chosen (the ghost's probe, below) or made.
- **Final size from the spawn itself**: `box3d::spawnClass` (the toolgun, `vr_physics_spawn`, the brush tool) runs QC
  `VR_SpawnLive_Finish` right after the spawn function: Dawn of the Machine's upgrade started (its 0.5 s), AD's item
  start (`AD_SpawnLive_Finish`: a backpack's setup and AD's 0.1-0.6 s spread), the item placed (`VRPlaceItemNow`:
  dropped, a box shrunk to its object, a pickup scaled and floated: StartItem's 0.2 s) and made an object
  (`VR_PickupObj_Place`: the 0.3 s after). The first frame the client sees is the final one.
- **The ghost as the placed item**: an Items entry (or a picked `item_`) is probed once when chosen and aimed at a floor:
  made where the ghost stands (`spawnClass` probe: QC `vr_spawn_live` 2, no ammo box's weapon drop) and removed at once;
  its model, skin, frame, scale and offsets and its origin's move are kept (`GhostLook`) and the ghost drawn with them
  (`toolgun::ghostVr`, render::networkData). The probe also precaches what the item needs before it is placed.
- **Backpacks anywhere**: AD's ammo and armour backpacks (`ad_item_backpack`, `_armour`) through the toolgun in any map
  while AD's data is mounted (`VR_AD_ToolgunSpawnsOutside`; the entries no longer AD-maps-only); MG3's four ammo
  upgrades (shells, nails, rockets, cells) in any map as ammo backpacks: outside the campaign the model is read from the
  owned pack and the map has no upgrade bit, so each fills that ammo to the usual most, every time ("Ammo backpack:
  filled to N"). The health upgrade stays campaign-only.
- **AD ghosts**: Rebuild From Skeleton keyed each entity's cached bone rows by its poses and settings, not its model; the
  ghost is a temp entity whose slot draws whatever is chosen, so a new choice on the same poses (frame 0) took the last
  one's bones. The key now includes the model (`vr_animsmooth.cpp` Key; the same would have hit an edict reused by
  another model). Screenshots, before/after (Death Knight, Crossbow Knight, Hunter Ogre, Centurion):
  `scratch/ghost_before.png`, `scratch/ghost_after.png` (`scratch/ghost_shots.sh`).
- **Debug > Toolgun**: Trace the Chosen Spawn (`vr_toolgun_spawn_trace [entry] [frames]`: its server model, scale, box,
  place and its client model and scale each frame); `vr_toolgun_spawn_test check` prints "invisible" for an alive one
  the client doesn't draw and the precaches the client lacks; `vr_toolgun_status` the ghost as drawn; with developer 1
  each spawn prints where it was placed, scale and offset.

Tests: `toolgun_spawnall_test.sh fastship 0 239` (vrfiringrange) PASS: 199 spawned and alive, 0 failed, 0 invisible,
11 unavailable; precaches 457 models / 834 sounds, 0 not on the client; the five AD backpacks and four MG3 ammo
backpacks alive there. `vr_toolgun_spawn_trace` on every entry (`scratch/trace_all.sh`): every item's server scale and
box the same from the frame it is made, its client scale final from the first frame it is drawn (id's boxes 0.2,
armour 0.5, powerups 0.6, backpacks 0.75); the megahealth drawn (was "no model"); AD's items drawn from frame 1-3 (were
frame 37-95). Ghost vs placed (`scratch/ghost_numbers.sh`: health, megahealth, green armour, shells, quad, AD health
vial, AD ammo backpack, MG3 shells backpack): model, origin, scale and offset identical. `toolgun_panel_test.sh` PASS;
e1m1 smoke (a megahealth and an AD backpack spawned) clean.

Not done: AD's monsters still appear after AD's own spawn delay (0.3-1.2 s, as in AD's maps). Weapons' entries are a
spawner that makes the weapon (unchanged). The Debug menu's QC spawner keeps the backpacks AD-maps-only.

In VR: spawn a megahealth, health, ammo and an armour in vrfiringrange: each appears small at once where its ghost was,
with no big flash; the ghost shows the small box on the floor. Choose AD's Death Knight after another AD monster: its
ghost should look like the monster. Spawn AD's and MG3's backpacks in an id map and take them.
## Flesh dents (worktree `goreflesh`, branch `agent/goreflesh`, 2026-10-16)

BACKLOG "More gore" (his notes, 2026-10-15): "real-time vertex deformation showing flesh chunks torn away". His
decision: dent + wound, persisting on the living, corpses and ragdolls, no see-through holes.

- **Where a dent lives** (`vr_dents.cpp`): each body (the entity; its `.mdl`, a ragdoll's source) keeps up to
  `vr_gore_dents_max` (8, at most 16) dents in its model's **rest pose** (pose 0, model units): centre, radius, the
  struck surface's rest normal, depth, and a blade's long axis with its stretch. The wound system already finds where
  a blow meets the drawn triangles (`vr_wounds.cpp` `strike`); the dent takes the nearest facing drawn triangle to that
  point and carries the point to the same triangle in pose 0 by its barycentrics (a blade's swing too, through the
  triangle's frame). A shot, pellet or nail makes a small dent (r 2-3.6 units, 1-1.6 deep), a blade or blunt blow a
  longer one (r 3-5, drawn out 2.2-3x along the swing), a blast one crater on its nearest facing point (r 3.8-5.5,
  2-3 deep) and two lesser ones where its bleeds land. Burns, lightning and liquids make none.
- **Drawn** (`vr_glsl.h` `QVR_DENT_FUNCTIONS`, `QVR_ALIAS_VS_DENTS`, `DentsAt`): the alias vertex shader reads the
  vertex's rest place (a `.mdl`'s pose 0; a ragdoll's skinned model's bind pose, which is that same pose 0) and pushes
  it in along its normal as drawn, after the poses, the smoothing, its rebuild from the skeleton and a hit reaction's
  kick, so a dent goes with every animation and the ragdoll. The bowl is (1 - t^2)^2 (t the distance in radii, squashed
  along a blade's axis and flattened 2x along the dent's normal so a gun held before the chest is left out), only where
  the surface faces the dent's (an arm resting on the chest in pose 0 isn't dented with it); the deepest counts. Quake's
  meshes are coarse (a small dent may move no vertex), so the fragment shader measures each pixel in the rest pose too:
  raw flesh inside a torn (noise) edge, a pale lip, a dark bloody core, a dark blood rim out to 1.35 radii, wet sheen,
  and the bowl's slope bent into the normal (the rest pose's screen derivatives; Mikkelsen's surface gradient) and
  shading the model's own light (its far wall lit, its near one dark) whatever the bumps' setting.
- **No holes:** the depth is held to 0.6 of the radius and to 0.4 of the body's thickness under the point (a ray
  through the drawn triangles: an arm, a tail, a head's side), whatever the depth and size scales.
- **Not flesh:** a triangle on a rig's loose piece (`vr_ragdoll.cpp`: the grunt's gun) gets no dent, and a blast's
  crater skips it.
- **Many:** a dent landing mostly on another merges into it (wider by 0.3 of the smaller, up to 1.25x and 7 units,
  a little deeper); a full body merges its oldest into the dent nearest it (or lets it go: its painted wound stays).
- **Rows:** one GL buffer (SSBO 12) of 128 bodies x 16 dents x 3 vec4s (96 KB), a block per body, written only when a
  body's dents change (or the scales do); the instance's `Dent`/`DentScale` say which rows and the drawn model's
  pose-to-units scale. Past 128 bodies the one hit longest ago gives its dents up. Map changes and `wounds::clear` forget
  them, a removed entity (`QVR_SVC_WOUNDCLEAR`) forgets its own.
- **The flesh chunk** is the existing small gibs' (`vr_smallgibs.qc`): already torn out where a hit lands, by its damage
  (not added again here).

Settings (Gore > Wounds on Models, archived): Flesh Dents `vr_gore_dents` (1), Dent Depth `vr_gore_dents_depth` (1),
Dent Size `vr_gore_dents_size` (1), Dents per Body `vr_gore_dents_max` (8). They ride on the wound events: with Dynamic
Wounds, Burns and Wet from Liquids all off, no dents. Debug: `vr_gore_dents_info` (Debug > the info commands: Flesh
Dents), `vr_wounds_test nearest <kind> [amount] [right] [up] [pellets]` (new `nearest`: the monster model nearest the
view) and Debug rows Dent the Nearest Monster: Nail / Blade / Blast; `vr_wounds_debug 1` prints each dent (radius,
depth, thickness under it).

Tests (headless, vrtesthall, `scratch/dentshots.sh <monster> <distance> <eye height> <prefix> "<cvars>"`: nails, six
pellets, a blade, then a blast; each with dents on and off; then `vr_test_kill 1` and the ragdoll from above; the
painted blood off, `vr_wounds 0`, to see the dents alone): the grunt's shoulder, chest and forehead (the blade's,
upright) show dark torn craters with a pale lip that are not there with the dents off; the blast's crater sits on the
nearest facing flesh (the gun skipped); the ogre's chest crater from the pellets reads as a bloody hole; both ragdolls
keep their dents (gone with `vr_gore_dents 0`). Pictures: `scratch/g_alive.png`, `g_rag.png`, `o_alive.png`,
`o_rag.png`.

Cost (`scratch/dentperf.sh`, `--exclusive`, 1440 mock eyes, 20 grunts in a fan in view, 32 bodies dented by
`vr_wounds_test all` (222 dents)): the models' GPU time 0.533 ms a frame with the dents, 0.485 without (+0.05 ms, both
eyes), the frame's GPU 1.643 vs 1.596; the models' CPU 0.070 vs 0.069 ms (a lookup per instance); placing a dent 9 us
(its triangle search, the thickness ray); no allocation per frame (the profiler's count 0).

In VR: shoot a grunt with the nailgun and the shotgun, cut one with a blade, rocket an ogre: each hit should leave a
small dark crater of raw flesh with a bloody rim that stays as he moves, dies and falls as a ragdoll; never a hole
through an arm. Too subtle or too much: Dent Depth and Dent Size.

### Weapons aren't flesh; small craters on heads (worktree `goreflesh`, branch `agent/dentfix`, 2026-10-16)

His question: "since the grunt's weapon is part of its model, does it also get dented and looks like flesh?" It could:
placement skipped the gun's triangles, but the shader dented and painted every vertex by its rest-pose distance, so a
dent on the hand or chest beside the gun reached into the gun. And a rocket's crater on a grunt's face covered all of it.

- **A flesh table per model** (`vr_dents.cpp` `fleshOf`, made with its first dent): each vertex's flesh, 1 or 0, fading
  from 0 to 1 over 1.5 units (rest pose) from a non-flesh piece. Not flesh: (1) the rig's loose pieces; (2) any piece
  of the mesh (its triangles joined by their corners; a triangle with a repeated corner joins nothing) shrunk to a
  speck (under 0.5 units) in some frame: a weapon dropped in a death, a muzzle flash, on any model, rig or none (AD's);
  (3) a short list of rig bones that are weapons held to the end (`notFleshBones`). A piece tucked away at rest (the
  zombie's and mummy's thrown flesh) is marked but nothing fades near it. (1) and (2) agree vertex for vertex on id's
  monsters, so (2) is what covers rigless models.
- **Drawn:** the table (4 vertices a row) after the bodies' rows in the dents' buffer (16384 rows: 65536 vertices;
  past it a model is all flesh); a header row before each body's dents gives its model's table, read by `gl_VertexID`
  (a `.mdl`'s drawn vertex, and the same index in a ragdoll's skinned model). The vertex shader multiplies the push by
  it (and skips the dents on a weapon's vertices) and hands it on in `out_restnor.w`; `DentsAt` scales the crater,
  rim, depth and slope by it (smoothstep 0.15-0.85). A triangle with a corner of no flesh still gets no dent.
- **Per model** (`vr_gore_dents_info` lists each model's counts): grunt 86 of 555 vertices (the shotgun), enforcer 59
  of 479 (the rifle and its flash), ogre 119 of 497 (the chainsaw's four pieces, and his left hand: the grenade
  launcher, by bone), knight 61 of 655 (the sword), hell knight 59 of 538 (blade and guard), Rogue's gremlin 38 of 123
  (the gun he holds); zombie 20 and mummy 12 (the thrown flesh, inside them). By bone label, AD's weapon hands (each
  bone the weapon with the hand round its grip): the crossbow knight, the three hunter ogres, the death guard, the
  justice and the judicator knights, the death sergeant's hand and flail; AD's enforcer, pyro and centurion guns (hidden
  on death) are found as dropped pieces. AD isn't installed here: its rows are untested. Not covered (left as flesh):
  armour (the knights are armour all over: none would be dented), the death knight's and the fury's swords (their bones
  carry the forearm too).
- **Blast craters** (and a blast's lesser wounds round it): held to the part they land on, at most 0.8 of the part's
  half width (half its rest points' middle extent: the grunt's chest's cap 5.8, a forearm's 2), 0.4 on the head and
  its jaw (the grunt's cap 1.9: an eye, not the face); no rig, 0.6 of the thickness under it; never under 1.5. Where the crater
  lands, the head counts 10 units further than it is: a rocket at his face craters his chest or shoulder unless the head
  is clearly nearest. Merges keep the smaller cap.

Tests (vrtesthall, `vr_wounds 0`, `scratch/fleshshots.sh <monster> <distance> <eye m> <prefix> <cvars> "<rights>"
"<ups>"`: a grid of nails at the weapon): the grunt (40 nails over the gun and the hands on it, `fl_grunt_gun.png`), the
enforcer (28 at the rifle, `fl_enf.png`) and the ogre (20 at the launcher and arm, `fl_ogre_box.png`): flesh dented
right up to each weapon, the weapon as before. `scratch/fleshblast.sh 0 50 1.15 fl_blast 26`: a blast at the grunt's
face height cratered his chest (r 5.4); a 120 one a little aside left a crater the size of an eye on his brow
(`fl_blast_face.png`). `scratch/fleshrag.sh`: a dented ragdoll still shows its craters on the body. e1m1 smoke clean.
Cost: placing a dent 9.1-9.7 us (8.8-8.9 before; the table made once per model); the shader adds one read per vertex
of a dented body. The GPU A/B (`dentperf.sh`) was swamped by other load on the machine (both builds swung 0.5 ms
either way): to be measured again on a quiet machine.

In VR: shoot a grunt's hands and chest round his gun, an enforcer's round his rifle, an ogre's left forearm: craters
on the flesh, none on the weapons. Rocket a grunt at his face: a crater on his chest or a small one on his face.
## More gore: blood streams, back-splatter, teeth (worktree `gorefx`, branch `agent/gorefx`, 2026-10-16)

His words: "I basically want this to look like a continuous thin blood waterfall. Research some techniques to implement
that. The blood hitting the floor should paint it with blood as well." (BACKLOG.md, "More gore"; the vertex
deformation of torn flesh is another worker's.)

**Research: a thin continuous stream, cheaply.**
- *Chained particles drawn as a ribbon* (the classic "trail"/"beam" particle: Unreal's ribbon emitters, Source's rope
  and spritetrail): each particle emitted at a steady rate and simulated on its own (gravity, the emitter's motion), the
  quads drawn between neighbours, so the chain reads as one unbroken line however the emitter moves. Cheap (a point a
  1/45 s, a quad a pair), CPU-side, no new shader.
- *A velocity-stretched quad per particle* (sparks, rain): reads as a stream only when they overlap; many more particles
  for the same continuity, gaps when the emitter moves fast.
- *A GPU particle stream with width taper* (compute or transform feedback): the same chain on the GPU; the win only shows
  past thousands of points, and Quake VR's particles are CPU-built anyway.
- *A spline or Verlet ribbon from the wound* (hair/rope solvers): smooth, follows the body, but a stream is ballistic
  (each drop independent once out); constraints only add cost.
- *A scrolling-UV tube* (a cylinder from the wound to the floor, its texture scrolling down: many games' waterfalls): one
  draw, but a rigid shape: it neither arcs from a moving body nor trails behind it; it needs the arc traced anyway.
- *Painting where it lands*: a decal at each landing (too many), or one growing pool per spot, moved on when the stream
  moves (Brutal Doom's and F.E.A.R.'s pools under bodies; the existing gore's Pool marks, which spread over seconds and
  bake into the permanent layer).
Picked: the chained particles as a soft ribbon (vr_lines' depth-tested scene lines), the landing traced as one arc, a
growing pool per spot.

**Blood streams** (`vr_bloodstream.cpp`, `vr_gore_streams*`): every wound event (QVR_SVC_WOUND) of a shot or a nail
(each pellet) opens one at `vr_gore_streams_chance` (0.6), on a monster or a corpse (not you). It binds to the model's
vertex nearest the hit (the model as drawn then: the animated frame, or a ragdoll's skinned mesh), and each frame takes
that vertex as drawn (`anchor::posedVertex` through the entity's matrix; a ragdoll's body: `ragdoll::drawnPart`, the
vertex's bone), so it follows the animation and, once the monster falls, the ragdoll; the way it pours (back toward
the shot, out from the body, a little up) turns with the body (its yaw, or the ragdoll body's turn). Points leave the
wound 45 a second at up to `vr_gore_streams_speed` (70 u/s) times the flow (a gush, then weakening to nothing over
`vr_gore_streams_time`, 7 s, beating at 1.35 Hz), plus 0.7 of the body's own motion, then fall freely; drawn as ribbon
segments `vr_gore_streams_width` (0.35 units) wide, thinner as the flow weakens and as each point speeds up falling
(a real stream's continuity: the width times the square root of its starting speed over its speed now). Where it lands:
one arc (12 steps of 0.1 s) traced through the static world each 0.08 s from the wound at the current speed; the points
live until then. Painting every 0.15 s while it lands: on a floor a Pool (16-24 units, `vr_gore_streams_pool`) spreading
over what is left of the stream and darkening, a new one when the landing has moved over a third of the last one's size
(at most `vr_gore_streams_pools`, 4), drops between; on a wall a run down it (gore's runs; at most 3); specks each time.
Caps: `vr_gore_streams_per_body` (3) and `vr_gore_streams_max` (24, storage for 64): past one, the weakest (the one with
least time left) gives its place. Fixed storage (64 streams of 56 points, 32 pending wounds); the vertex binding reuses
one grown scratch vector: no allocations per frame. `vr_decal_count` prints them (`bloodstreams: n (n pouring), points`).

**Back-splatter** (`vr_gore.cpp`'s hit, `vr_gore_backsplatter`, `vr_gore_backsplatter_reach`): each hit of a shot or a
blast's pellets (not explosions or blows) also throws 2 + damage / 6 thin streaks (at most 12, 16 lines in all) along
the shot's line (a cone of 0.12 rad, 0.2 for pellets), reaching 80 + 4 x damage units (to 480) for shots and 60 + 1.5 x
damage (to 160) for pellets: a shotgun's close behind, a big hit across the room. Each lands as a Splatter 3.5-7 units
wide and 4-8 times as long, drawn out along its flight and away from the shot's line: head on they radiate from where
the line meets the wall (real back-spatter's look), glancing they run long along it; they show 1/1400 s a unit after
the hit. Independent of Blood Sprays now (Blood Sprays 0 leaves only them).

**Teeth** (`QC/vr_teeth.qc`, `Misc/quakevr/make_teeth.py` -> `progs/vr_tooth.mdl`, `vr_teeth*`): a hand's blow
(T_Damage_VRMelee) with a fist (or, `vr_teeth_blunt`, a gun's butt, a crowbar, a club, a pommel; never a slap) on the
head zone of a live humanoid (its head gib: grunt, enforcer, knight, hell knight, zombie, AD's soldiers, knights,
judges and zombies), from the front, at Decapitation's blunt hardness (the hand's speed and the damage) of at least
`vr_teeth_hardness` (0.3): up to `vr_teeth_per_hit` (4) teeth for the hardest, `vr_teeth_per_enemy` (10) at most from
one. They are small gibs (VR_SmallGib_SpawnAs's xBrain 2: Most Small Gibs caps them, carried, thrown, fading as the
others), `vr_teeth_size` (3 cm) long, `vr_teeth_mass` (0.05 kg), out of the mouth (the head zone's front, low) toward
the face's front and the sides, up, along the blow, at `vr_teeth_speed` (3 m/s); no squish, blood, sticking or blood
trail (their model has no gib flag); they click on landing (weapons/tink1.wav, quiet) and knock as stone
(`vr_physsound.cpp`). The model: a lathe of a molar's profile (crown, neck, tapering root), 99 vertices, 140
triangles, a 16 x 16 skin, ivory with the root bloody.

**Settings and tests.** Gore > "Blood Streams, Back-splatter and Teeth" (every cvar above, archived). Debug > Tests:
Back-splatter: Shotgun (`vr_gore_test 30 pellets`: `vr_gore_test <damage> pellets` is new), Blood Streams on Monster
Ahead (`vr_gore_stream_test 3`), Knock Teeth Out (`impulse 198`: the monster spawned last punched as hard as can be).

**Checked** (headless): streams on a standing grunt (`start`, frozen, looking down) pour from its chest in thin arcs
and spread pools under them over seconds; on a corpse (`vr_test_spawn_dead 1`: a ragdoll) they pour short onto the floor
round it; a real shotgun blast (the bench's gun set-up) prints `back-splatter 7 streaks, reach 105`; on the calibration
room's wall the back-splatter alone (Blood Sprays 0) shows thin streaks radiating from the line's point, more and wider
for 80 damage; impulse 198 on a grunt knocks out 4 teeth that bounce and lie by your feet (8 after a second punch).
A real shotgun fight in E1M1's computer room (three blasts) keeps 6 streams pouring through the grunts' deaths and
ragdolls (a first build stopped those on bodies that became ragdolls: the "#rag" model swap; fixed, `ownModel`).
Cost (kit bench, 90 Hz paced, exclusive, 2 reps, all of it off with `vr_gore_streams 0; vr_gore_backsplatter 0` vs on):
`play_e1m1_fight` (shotgun) frame p50 11.110 both, CPU busy avg 2.050 -> 1.957 ms (noise), heap allocations a frame
5.36 -> 4.85; `play_e1m1_lights` (rockets) rep 1 CPU busy 2.742 / 2.728 off -> 2.714 on, its "vr particles" phase
0.358 -> 0.390 (+0.03 ms); its rep 2 on was slower in every phase alike (3D, physics, swap, xr wait all about 1.8x:
the machine, not the gore). So: within the runs' noise, under 0.05 ms a frame.

In VR: shoot a grunt with the shotgun up close and watch the streams pour as it stands, falls and lies; check that the
pools under it grow where the streams land; shoot one standing a little in front of a wall (the streaks behind it);
punch a grunt's face hard with a fist (teeth; a soft punch none: Punch Hardness for Teeth) and pick a tooth up.

## Blood streams diverge; his gore values (worktree `gorefx`, branch `agent/gorefx2`, 2026-10-16)

His notes: one shotgun blast's streams "all seem to go in the same direction": randomize each as it starts and let it
wobble as it flows. And "I've tweaked many values for the gore features. Please make those the new defaults."

**Streams** (`vr_bloodstream.cpp`): every pellet of a blast shares the hit's direction, so the way out (back along the
shot, out from the body's middle) differed only by a small jitter. Now each stream's way out is turned at random within
a cone of `vr_gore_streams_spread` (35 degrees, evenly over the cone's cap), its force is times a random 1 +-
`vr_gore_streams_speed_var` (0.3), and as it pours its way out wanders by up to `vr_gore_streams_wobble` (10 degrees):
two sines a side, each at its own random rate (0.7-1.3x and 1.7-2.6x `vr_gore_streams_wobble_rate`, 0.7 Hz) and
phase, applied each frame to the emitted points (the falling chain keeps what it left with, so the stream curls). The
heartbeat phase was random already. Same storage, caps and traces; a few sines a stream a frame. Gore > Blood Streams:
Stream Spread, Stream Force Variation, Stream Wobble, Stream Wobble Rate.

Checked (headless, `-RealTime`, a grunt at 80 units, `vr_gore_stream_test 6` from one direction, side view
`vr_mock_camera -2.4 1.1 -2.4 12 -90`): with the three at 0 the six arcs are parallel copies of one; with the defaults
they leave at different angles and lengths. (In fast mode the chains show stair steps, before and after alike: the
uncapped fixed frames, not the streams.)

**His gore values are the defaults** (configVersion 139: a config still at the old default takes the new one):
`vr_corpse_health_mult` 1 -> 2, `vr_gore_dents_depth` 1 -> 3, `vr_gore_dents_max` 8 -> 12, `vr_gore_dents_size`
1 -> 1.25, `vr_gore_streams_per_body` 3 -> 4, `vr_gore_streams_speed` 70 -> 50, `vr_gore_streams_width` 0.35 -> 0.75,
`vr_ragimpact_crush` 1 -> 1.5, `vr_ragimpact_crush_speed` 200 -> 350, `vr_ragimpact_curve` 1 -> 1.5,
`vr_ragimpact_curve_ref` 3 -> 4, `vr_ragimpact_damage` 0.15 -> 0.08, `vr_ragimpact_max` 0 -> 40,
`vr_ragimpact_piece_hp` 0.4 -> 0.65, `vr_ragimpact_stomp_heavy` 130 -> 120, `vr_ragimpact_stomp_speed` 420 -> 400,
`vr_teeth_per_enemy` 10 -> 8. Not promoted (look like a test set-up): `vr_dummy_gore` 1 -> 0 and `vr_dummy_gib` 1 -> 0
(the training dummy clean and unkillable). His Dent Depth is at its slider's top (3).

In VR: shotgun a grunt up close: the streams should fan out and curl a little as they pour, not run as parallel copies.

## VR menu panel: grip also pushes and pulls it

A gripped panel already followed the hand in 3D, but 1:1 (a 30 cm arm move shifted a far panel ~30 cm). Now the hand's move towards/away from the panel (from the head) counts `vr_menu_grab_reach` times (default 3; 1 = as before), the distance clamped to `vr_menu_grab_dist_min` 30 .. `vr_menu_grab_dist_max` 200 cm (a panel taken farther than the max, like the 150-unit default, can be pulled in but not pushed further out; one taken nearer than the min is not pinched). The panel keeps its hand-set turn (as before). The distance is part of the placement (hide/restore, remembered). No thumbstick use: sticks while gripping are the fine modifier and disarm the grab. Test (headless, mock hand): hand 30 cm back: 150 -> 121.7 units from the head; forward again: 150; far forward stays 150; far back clamps at 9.4 units (30 cm). menu_panels_test.py passes.

## Gloves: worn leather, a painted layer he can edit (worktree `flatfist`, branch `agent/glovepaint`, 2026-10-16)

His request: a painted texture on top of the gloves, worn and used (knuckles, wear over the finger joints, stitching
along the seams), editable by hand in an image editor. The procedural leather alone read smooth, more rubber than
leather, at arm's length.

- **Engine.** `vr_glove_paint` (default 1; VR Settings > Player Appearance > Gloves > "Worn Leather"; 0 the plain
  leather as before). Per hand skin `progs/hand_rig_0N_00_glove` (else skin 0's): **RGB** detail multiplied with the
  HSL colour (128 grey = the colour as it is), **A** wear (the leather paler, greyer and matte there: a black glove's
  scuffs grey, a brown one's tan; the sheen down to 30%); `_glove_height` its relief (128 flat; its slope bends the
  normal as the mask's seams did). With the painted layer the mask's A seams step aside (the paint has them), the
  shader's own grain goes finer and to 35% (close-up texture under the paint), and the hem's band counts as worn (0.4).
  Units 7 and 8 of the model shader (`GloveTex`, `GloveHeightTex`); the instance's `Glove.w` carries the flags (+4
  the layer, +8 the layer with its relief). Loaded unowned outside the draw loops (`VR_PrecacheSkinMasks`), freed and read again at
  every map load (`worldGeneration`) and by `vr_gloves_reload` (Debug > Rebuild and Reload > "Reload Glove Paint"),
  so his edits show without a restart; `developer 1` prints each read. Missing files: today's procedural look.
- **Content.** `Misc/quakevr/make_glove_paint.py` (genguarded; deterministic; ~15 s) paints at 1024 x 1024 (twice
  the skin: ~3 texels a mm) from the rig's 3D positions: a 3D cellular pebble grain (~1.7 mm) with a coarser crease
  network in places and a mottled tone; the knuckles scuffed through in abraded patches with grime round them; the
  PIP/DIP joints' backs scuffed less, the leather wrinkled across them, folded deep on the palm's side; the palm's
  folds (heart, head and life lines); three stitched "points" down the back; `make_skin_masks.py`'s seam lines (now a
  function, `seam_lines`; its masks bit for bit the same) sewn: a groove, a lip either side, slanted thread stitches
  with needle holes, the thread paler; the fingertips worn; 50 scratches. One file pair serves all four hand skins
  (they differ only by static blood, which the glove masks leave out); a skin can have its own by name.
- **Docs.** `docs/vr-port/GLOVES.md`: the files, the channels, the reload, the generator and its guard.

Tested (headless, vrfiringrange, 2048 x 2048 eyes, about a headset's pixels a degree; `scratch/glove_paint_shots.py`,
before/after sheets `scratch/gp_final_close.png`, `scratch/gp_final_far.png`; 1:1 crops `scratch/glove_paint_close.png`,
`scratch/glove_paint_far.png`): full brown, black, red, fingerless brown, black, blue; backs and palms, 25-30 cm and
~55 cm away. At arm's length the knuckles' grey/tan scuffs, the joints' wear, the stitched points and the palm's folds
read as worn leather on all six; close up the wrinkles, thread and scratches hold. Bare hands: `vr_glove_paint` 0 vs
1 in separate runs differ by 11019 px, two runs with 0 by 11552 (the scene's own run-to-run noise). GPU (`--exclusive
-RealTime`, hands 13 cm from the eyes, gloves 2): frame GPU 1.094 / 1.080 / 1.092 / 1.066 ms for paint off / on / off
/ on: no measurable cost (4 taps more, on gloved pixels only). e1m1 then e1m2 with gloves, the reload, paint toggled:
no errors; the files read at each map and on the reload.

For him in VR: whether the wear is too pale or too much (paint the PNG's alpha, or `make_glove_paint.py`'s
`scuffed()` levels), the stitched points down the back (three rows; delete them in the PNG if unwanted), the relief's
strength (`vr_glsl.h`: `paintSlope = 2.0 * ...`). Note: `developer 1` warns that QuakeSpasm can't read the PNGs (a
compatibility note for mods; Quake VR reads them).

## The buzzsaw cuts ragdolls (worktree `dequant`, branch `agent/buzzrag`, 2026-10-16)

His note: ragdolls and active ragdolls went through Rogue's buzzsaw untouched; they should be obviously destroyed and
dismembered like anything else.

- **Why they were skipped.** The buzzsaw (`rogue_buzzsaw.qc`) hurts by its touch, a SOLID_TRIGGER's; a ragdoll is
  Box3D's bodies (a corpse's entity SOLID_NOT, a knocked-down one's SOLID_NOT_BUT_TOUCHABLE, its box not where its parts
  are), which no trigger touches.
- **The blade's query.** Two engine builtins (`vr_box3d.cpp`, "Ragdolls in a box"): `ragdollinbox(mins, maxs, after)`
  the next ragdoll (edict order) with a part in a world box (the broad phase: Box3D's tree at the box, ragdoll parts
  only; then GJK on each part's shapes), `ragdollboxpart(e, mins, maxs, after)` the next part of one in it, trace_endpos
  its point nearest the box's middle. Nothing is looked at unless a buzzsaw was spawned in the map (`vr_buzzsaws`).
- **Each frame** (StartFrame, `VR_Buzzsaw_Frame`) every running buzzsaw looks into its blade (its box grown 2 units). A
  ragdoll in it is struck at most every 0.1 s (`.vr_buzz_next`): each part in it takes the blade's damage x10
  (currentammo 10: 100) through one shared path, `VR_Limb_RagdollHit` (`vr_limbs.qc`): a corpse's head or limb wears
  down that piece (`VR_Limb_CorpseWear` / `VR_Limb_WearInto`, its piece health: a grunt's 80, so one strike) and comes
  off as the chainsaw's cut; a living one's (knocked down, staggering) is armed (`VR_Decap_Arm` / `VR_Limb_Arm`) and the
  strike kills it, Killed cutting it off; a torso strike is the whole body's (a corpse gibs). A corpse's limbs and head
  are struck before its torso; a living one once a strike. Blood (meat spray, SpawnBlood) and the blade's buzz. The
  buzzsaw counts as a saw for corpse damage and small gibs (`VR_SmallGib_Kind`: the chainsaw's kind, x1). Its old touch
  skips ragdolls.
- **Debug > Combat, "Buzzsaw on the Nearest Body"** (`vr_knockdown_test 70`): a running buzzsaw at the middle of the
  nearest ragdoll.

Tested (headless e1m1, `developer 1`): a grunt corpse (`vr_test_spawn_dead 1`), the blade on it: limbs 7 and 10 cut
off, then its torso gibbed in the same strike (`scratch/buzz_corpse.png`). A knocked-down living grunt: limb 10 cut, it
died; limb 9 cut, then gibbed (`scratch/buzz_living.png`). A grunt falling through a running blade one frame after its
knockdown: beheaded, dead (`scratch/buzz_fall.png`). e1m1 smoke clean.

For him in VR: whether a strike every 0.1 s at 100 is too quick (a corpse goes in one frame: limbs, then its torso):
`VR_BUZZ_RAGDOLL_SCALE` and `VR_BUZZ_GAP` in `rogue_buzzsaw.qc`.

## Tracy profiling run (worktree `tghost`, branch `agent/profile1016`, 2026-10-16)

His request: a full Tracy run with call stacks over prepared scenarios, the uncontroversial wins fixed, the rest
listed for a decision. Full report (per-scenario tables, top zones, the ranked decision list B1-B8):
`qvr-kit/scratch/PROFILE_20261016.md`; captures and scripts in `qvr-kit/scratch/profile_20261016/`.

Release-Tracy at callstack depth 8 (Release-TracyFull's depth; its ETW sampling needs an elevated session, left off),
his config of today, 2048 eyes, 90 Hz paced, exclusive. Scenarios: `play_e1m1_fight`, `play_e1m1_lights`,
`combat_48`, `ragdolls_32_active`, `load_e1m1`, and the new bench group `profile` (BENCHMARKS.md): `play_gore_fight`,
`play_calibration_turn`, `play_ad_tfuma_fight` (native AD, the map awake), `load_ad_tfuma`.

- Every scenario holds 90 Hz at 2048: Release CPU p50 0.9-3.2 ms, p99 at most 6.5; GPU 3D p50 1.0-3.7. The steady
  frame has no single hot spot; the biggest shares are the per-eye alias set-up (0.35-0.67 ms in fights), Box3D with
  32 ragdolls (workers 4 ms, the main thread waiting 0.8), AD's 1792-edict server frame (QC 0.54 + SV_Physics 0.37).
- **Fixed: pak lookups by hash** (`COM_FindFile` strcmp'd every entry of every pak per lookup; most of a load's are
  misses): AD warm load 644 -> 562 ms (its failed image probes 83 -> 41, vr_extmaps 86 -> 38).
- **Fixed: vr_mdl_dequant on the job pool** (2 ms a model on the main thread at its first upload): AD cold load
  ~1490 -> ~1135 ms, e1m1 cold ~880 -> 816; identical output (hashes of all 322 models' buffers, serial vs split).
- Hitches seen are known kinds: a gun's first convex cut in a session (the cache is keyed by build: every new build
  cuts again, B6) and a spawned-in kind's first limb cut (B5).

For him: pick options for B1-B8 in the report; nothing to test in VR (no visible change).

## Floating pickups: a moment's grace for the reaching hand (worktree `flatfist`, branch `agent/floatgrab`, 2026-10-16)

A player's feedback: weapons floating in the world were impossible to grab with his real hands; reaching for one, it
fell to the ground. His decision: a split second of leniency where the hands don't knock a floating, spinning object
before it becomes a physics prop.

- **Why it fell.** A map's pickup hanging in the air (`VR_PickupObj_Place`, MOVETYPE_NONE, spinning) is knocked loose by
  the first touch of a moving hand (`VR_Carry_Nudge` -> `VR_Carry_Loose`): anything over 10 u/s, 0.38 m/s, which every
  reach is. The grip closes a moment later, on a falling gun. And a spinning shotgun touches a slow hand only in short
  passes (a hand crossing it: 0.1 s, then nothing for half a turn), so a grip closed between passes was a fist at the
  next one (`VR_Carry_GripPressed` only counts a press 0.2 s old).
- **The grace** (`vr_carry.qc`, `VR_Carry_FloatGrace`, before the nudge): an empty tracked hand reaching into a hanging
  pickup doesn't knock it for **`vr_pickup_float_grace_time`** (0.4 s) from its first touch, unless it moves faster than
  **`vr_pickup_float_grace_swat`** (2 m/s: a swat knocks it at once, as before). Once the grace is over, a hand still
  pushing into it knocks it as today. The hand's reach is forgotten when it leaves the pickup's spin sphere and a hand's
  width (`VR_Carry_FloatGraceFrame`, from the hanging pickup's think), so the window runs from when the hand came near
  it, not from each spin pass.
- **The grip in the grace** (`VR_Carry_FloatGraceGrip`): a grip closed from 0.2 s before the hand reached it to the end of
  the grace takes it at its next touch (weapons: `VR_PickupObj_WeaponTouch`; keys, armour, runes: `VR_Carry_Handtouch`),
  where it hangs. An older fist still doesn't take it.
- Only pickups still hanging (`VR_Carry_Hanging`); loose ones, props, a hand holding a weapon, force grab: unchanged.
  **`vr_pickup_float_grace`** 1 (0: the old knock). Rows on VR Settings > Carrying, after Mid-Air Catch Window:
  Floating Pickup Grace, Floating Grace Time, Floating Grace Swat Speed.
- **Debug > Tests > Weapon Pickups, "Floating Pickup Ahead"** (`vr_pickup_test 6`): a super shotgun pickup hanging 40
  units ahead, watched (`floatgrab:` hanging / knocked loose / taken, with times; `developer 1` adds each hand's reach
  and knock).

Tested (headless e1m1, `vr_pickup_test 6`, the main hand crossing the hanging shotgun at its middle height through
`vr_mock_play`; `scratch/fg_*.txt`, `scratch/fg_run.sh`):

| case | grace off (before) | grace on |
|---|---|---|
| slow reach 0.45 m/s, grip 0.2 s after the reach | knocked at the first touch (4.63 s) | taken as a weapon at 5.26 s, z 74 (no fall) |
| the same reach, no grip | knocked at the first touch (4.63 s) | reached 4.61 s, knocked 5.26 s (its next touch after the window) |
| swat 3 m/s | knocked at once (4.04 s) | knocked at once (4.05 s) |

`vr_pickup_test 1` + `2` (taking the nearest pickup) still takes it; e1m1 smoke clean.

For him in VR: reach slowly for a floating gun and close the hand (also while it spins past the open hand); swat one;
reach in and hold the hand still, then push on. Tune Floating Grace Time / Swat Speed.
## Loading and caches: B4, B5, B6, B8 of the profiling run (worktree `dequant`, branch `agent/loadperf`, 2026-10-16)

His picks on `PROFILE_20261016.md`: B4 (a) then (b), B5 (a), B6 (b), B8 (a) and (b). One commit each.

- **B6, disk caches keyed by the maker's version, not the build.** The gun pieces' folder (`cache/gunpieces/<v>`) is a
  hash of `vr_convex.cpp`'s `cutVersion` and Box3D's version; the skin normal maps' (`cache/normalmaps/<v>`) of
  `NORMALCACHE_VERSION` (`vr_normalmaps.cpp`). The keys already hashed every input (triangles and settings; texels,
  size, heights, texels per unit, islands). **Bump the constant** when the cut, the normal-map maker or the skin
  islands (`Mod_SkinIslands`) change; `vr_gun_pieces_cache 2` / `vr_normalmap_cache 2` make them anyway and compare.
  The first session on this build cuts and makes once more (new folder names); after that a new build keeps them.
- **B8.** A cache folder is made once a session (at its kind's first write, with the prune), not checked at every
  write. Tracy-only scopes split `Mod_LoadAliasModel` (replacements, skins, skin normal maps, frames, VR poses, flags
  and bounds, draw lists, cache copy); compiled out of normal builds.
- **B4a, session results per texture** (`mem::Cache`, dropped at a game directory change, or when the context
  changes: `vr_extmaps_dir`, `vr_extmaps_match`, normal maps on, the palette): vr_extmaps' picture match and pack
  lookups by the texture's name and a wyhash of its pixels; the pack's missing files; vr_anim_surface's frame
  matches by both pictures' pixels; vr_pbr's SHA-256 of a texture by a quick hash of its pixels (its companions'
  found/missing was already kept). Stale only if the pack's files change while the game runs.
- **B4b, COM_FindFile's misses while loading** (`vr_fscache.cpp`, its window, main thread): a name found in no search
  path is not looked for again. Forgotten with any directory listing (any file written, on any thread: an epoch keeps a
  miss looked up before a write from being noted after it) and on `VR_FileCacheSearchChanged`: the search paths
  (common.c), the campaign (`publishCampaign`), the map installer's stock-only lookup.
- **B5, a kind first spawned after the load** (`limbmodel::spawnFrame`, `VR_ServerFrameEnd`): a monster kind prebuild
  didn't make (dispensers, `vr_physics_spawn`, AD's monsters that become monsters after the load: 9 kinds on
  ad_tfuma) has its limbs' .mdl files built on the pool (the job owns copies of the rig and the file), then precached
  one a frame (skins and normal maps: ~1-3 ms; more in a session that makes the normal maps); a cut before that takes
  the built file. `vr_limbs_prebuild 0` keeps the old behaviour.

Checks: imagelists (sorted) of e1m1 and ad_tfuma, cold and warm, identical to the base build's (AD's only extra lines
are B5's limb models); the cold and warm loads' identical to each other; 39 limbs built on the pool byte-identical to
`buildNamed`'s; e1m1 (restart, changelevel) and ad_tfuma (restart) smoke clean.

Release, his config, 2048 eyes, exclusive, 2 runs each (`benchresults/dequant_before`, `dequant_after`):

| | before | after |
|---|---:|---:|
| `load_e1m1` cold / warm / restart (ms) | 886, 834 / 216, 199 / 205, 191 | 792, 789 / 149, 154 / 144, 146 |
| `load_ad_tfuma` cold / warm (ms) | 1363, 1144 / 597, 563 | 1224, 1220 / 454, 457 |
| AD warm: vr_extmaps / vr_anim_surface / vr_pbr / misses (ms) | 39 / 40 / 85 / 43 | <2.4 / <2.4 / 53 / 35 |
| `combat_48` worst frame (knight's first cut) | 36.7, 21.9 | 11.2, 11.4 |
| first gun cut, first session after a rebuild (`play_gore_fight` r1 worst) | 34.3 | 11.4 |

The cold AD load is within its run-to-run spread (1144-1363). Steady fights unchanged (CPU p50 the same or a little
lower). Not done: the hulls' disk cache (`vr_hull.cpp`) is still keyed by the build's compile time (outside B6).

For him in VR: nothing visible. A knight (or another kind) spawned on the firing range and cut at once: no hitch.
## Frame time: B1, B2, B3, B7 (worktree `tghost`, branch `agent/frameperf`, 2026-10-16)

His picks from the profiling run's list (`qvr-kit/scratch/PROFILE_20261016.md`): B1 (a), B2 (c), B3 (a), B7 (b).
Release numbers: his config, 2048 eyes, 90 Hz paced, exclusive, 3 reps (CPU = `cpu_busy_ms` p50 / p95 / p99, ms).
Two sessions an hour apart move by ±0.2 ms on their own (the calibration room, untouched by any of this, 1.17 -> 0.92),
so each item is also measured on and off in the same session.

| scenario | before (old build) | after (new build) | item off, same session |
|---|---|---|---|
| `combat_48` | 3.42 / 5.09 / 5.96 | 2.88 / 4.56 / 5.63 | B1 off 2.94 / 4.60 / 5.52 |
| `play_ad_tfuma_fight` | 3.82 / 6.04 / 7.08 | 3.22 / 5.39 / 6.37 | B1 off 3.41 / 5.60 / 6.65 |
| `play_gore_fight` | 2.70 / 3.53 / 4.18 | 2.27 / 3.82 / 5.23 | B1 off 2.44 / 3.58 / 4.51 |
| `play_calibration_turn` | 1.17 / 1.39 / 1.66 | 0.92 / 1.25 / 2.04 | - |
| `ragdolls_32_active` | 2.96 / 5.45 / 6.16 | 2.43 / 5.02 / 6.26 (B2 on) | B2 off 2.58 / 4.75 / 5.66 |
| `ragdolls_32_active`, 5 reps | | B2 on 2.80 / 5.57 / 7.31 | B2 off 2.69 / 5.01 / 6.46 |
| `load_ad_tfuma` | 5.91 / 7.55 / 47.6 | 4.95 / 6.14 / 48.9 | - |

- **B1: the second eye reuses each alias model's eye-independent set-up** (`vr_alias_eye_cache`, default 1; Debug >
  Profiling and Memory > Second Eye's Model Set-up; r_alias.c `R_AliasEyeSlot`). The first eye's scene keeps per entity
  its lerp and transform, the light at its origin (`R_LightPoint`'s), vertex count, zero blend, Quake VR instance data
  and smooth-animation swing rows; the second eye's reuses them. Per eye still: the portal split, the cull, the alpha,
  the dynamic lights (culled by each eye's frustum) and the light's minimums, the near-eye flags, the batching, and the
  view entities' instance data (a bare forearm reads the body's ambient as last drawn: the first eye draws the hands
  before the body, the second after; the check found that 1e-5 difference). Never cached: `cl.viewent` (its offsets
  follow the eye's axes), teleporter views, shadow maps, showtris/showbboxes, the spectator, the drawn-moves log.
  `vr_alias_eye_cache 2` sets everything up again in the second eye and compares it bit for bit with the first eye's
  (`vr_alias_eye_cache_stats`): 0 differences in 5 scenarios (combat_48, the AD fight, the grunts through a teleporter,
  the teleporter loop, e1m1's fight with the gun held: about 200 000 set-ups, paused and live) and in e1m1. Every input
  the GPU gets is the same, so the image is. Eye images could not show it directly: two shots of the same paused moment
  with the same setting already differ (realtime-driven effects), and on/off pairs differ by as much as on/on pairs.
  Gain: 0.06-0.19 ms p50 in the fights, under the report's estimate: in the Tracy capture of the AD fight `alias` self
  is 0.66 ms against 0.67 before, so most of `alias` is the culling, batching and submission, not the set-up.
- **B2: the stepping thread orchestrates Box3D's solver** (`vr_box3d_caller_solves`, Debug > Profiling and Memory >
  Main Thread Solves): the solver's worker-0 task is no longer posted (b3Solve's caller raced it for the orchestrator's
  place and usually lost, then waited out the solve: the 0.8 ms "Job blocking wait"), and a finish runs the step's
  unstarted tasks before blocking (`jobs::Task::tryRun`). The same results: the ragdoll scenario's `vr_physics_hash` at
  11 points over 1000 frames equals the old build's with `vr_jobs_parallel` 0 and 1 and with the switch off;
  `vr_physics_mtbench 400 300` gives one hash for every worker count either way. **No win:** the main thread now does
  the solve it used to wait for, so its busy time is the same (3 reps: on better; 5 reps: on worse at every percentile;
  both within the scenario's noise). **Shipped off** (default 0); the switch stays for his test.
- **B3: Tracy-only zones per edict** (sv_phys.c `SV_PHYS_ZONE`, nothing in Release). The AD fight (1792 edicts, 244
  monsters awake), ms a frame self (calls a frame): `SV edict toss` 0.22 (265: gibs, items, missiles, resting or
  falling), `SV edict step` 0.09 (124 monsters; 0.34 with their QC), `SV edict push` 0.04 (228 doors and plats),
  `SV edict none` 0.03 (840; 0.15 with QC thinks), `SV edict VR begin` 0.02 (1478). The zones' own cost lands in
  `Server physics` self (0.37 -> 1.21 in this capture): read the split, not the total. Toss is the one worth a look
  (0.8 us an edict, mostly ones lying still).
- **B7: Tracy-only zones round V_RenderView's steps and the eye loop's** (view.c, vr_stereo.cpp): calc refdef, the
  portal views, R_RenderView's own part, poly blend, foveated end and debug, the UI, the mirror: each under 0.004 ms.
  The eye's self (calibration room: eye L 0.26, eye R 0.18) is not in them: it is in the eye loop between the profiled
  scopes (target binding, the late acquire's `glGetIntegerv(GL_FRAMEBUFFER_BINDING)`, the eye's GPU timer queries) and
  the zones' own callstack captures. Next step if wanted: an elevated TracyFull sampling capture (B7 a).

For him in VR: nothing visible should change (B1 on, B2 off). Debug > Profiling and Memory: Second Eye's Model Set-up
(Checked, then Model Set-up Stats: 0 differences expected) and Main Thread Solves to try.

## Tracy QC zones cheap enough to leave on (worktree `tghost`, branch `agent/qcdetail`, 2026-10-16)

His four decisions, all done (vr_tracy.cpp; the hooks in pr_exec.c / vr_qcexec.c; the tables built at the end of
pr_edict.c's PR_LoadProgs; normal builds compile all of it out):

- QC zones never take a callstack, in any Tracy build (depth-8 captures: the stacked zones are exactly the non-QC ones).
- One persistent source location per QC function and builtin, built at every progs load for the loading VM (server,
  client), interned by name so map changes reuse them; zones use the plain non-alloc begin. No per-call allocation,
  string copy or zone text: the kind (`QC function` / `QC builtin`) is the location's function field, and
  `qvr_tracy_capture_check` counts QC zones by it (older captures still by their text). Stripped builtin names now come
  from the engine's table (they were all `QVR`).
- Trivial builtins (`qcTrivialBuiltins`: vector/scalar math, conversions, `cvar_hget`, `nextent`, `isragdoll`), picked
  from level-3 captures of the three fights by calls a frame x mean time (TRACY.md, "QC zones").
- `vr_tracy_qc_detail` 0 off, 1 functions, 2 + non-trivial builtins, 3 everything (the old zones, same counts).
  Debug > Reports: Tracy QC Detail Functions / + Builtins / All / Off. No depth cap: not needed at these costs.

Overhead (TRACY.md "QC zones": 2 reps, Release-Tracy depth 8, paced 90 Hz, connected capture): `QC VM` time a frame
+0.03-0.11 ms at levels 1-3 against +0.6-1.0 ms with the old code (about 25-45 ns a zone against 400). Whole-frame CPU
p50: the new levels within the fights' own +-0.3 ms spread; the old +0.57 / +0.80 / +0.84 ms (combat_48 / e1m1 fight /
AD fight; AD p95 +4.7 ms). Checks: every capture 0 errors, 0 truncated; a stage-2 capture (memory and GPU detail,
level 2) passes `--stage2`; e1m1 -> e1m2 -> hip1m1 -> e1m1 at levels 3 and 1 rebuilds the tables (no `builtin #` or `?`
names, 0 drops); normal e1m1 smoke. Scripts and captures: `qvr-kit/scratch/qcdetail`.

For him: capture with `vr_tracy_qc_detail 1` by default now; 2 to see the builtins, 3 for complete call counts.

Rerun (worktree `tghost`, 2026-10-16): `combat_48` levels 0 and 1, rep 2, had overlapped his VR session (12:07-12:10);
rerun on a quiet machine (tags `*_r2b`): level 0 4.26 / 5.71, level 1 4.28 / 6.11 ms (were 4.48 / 6.17 and 5.37 /
9.01), QC VM 0.544 and 0.584 ms (were 0.540 and 0.641). TRACY.md's table now: level 0 4.34 / 5.93, level 1 4.30 / 5.93
(-0.04, was +0.40), levels 2 and 3 -0.04 and -0.09 (were -0.15, -0.20), old +0.57 (was +0.47). The guidance stands.

## The game window losing focus in VR (worktree `tghost`, branch `agent/xrfocus`, 2026-10-16)

His decision: while an XR session runs, never throttle on a lost window focus. The incident (qvr_openxr.txt
12:09:30-12:10:05): another game's window took the focus, the frames fell to exactly 100 ms and Virtual Desktop stopped
the session. The log shows the session FOCUSED all along and `end` (xrEndFrame alone) 88-90 ms a frame: the time went
inside the runtime's call, not in the engine's own background sleep (main_sdl.c already skipped it with VR on). The
likely cause is the GPU driver capping an unfocused window's presents (an RTX 4090: NVIDIA's Background Application Max
Frame Rate, here 10 fps), the mirror's SwapBuffers queued on the driver's thread and xrEndFrame's sync waiting for it.
Changes:
- `vr_mirror_unfocused_hz` (4, archived): in VR with the window unfocused, the desktop mirror is presented at most 4
  times a second (VR_SkipSwap); 0 presents every frame as before. Presents spaced wider than the driver's cap never wait.
- Sounds: the lost focus no longer blocks them in VR (in_sdl.c; flat unchanged), nor leaves them blocked as VR starts.
- The main loop's sleep: unchanged (flat only); `vr_debug_window_unfocused 1` (Debug page, "Act as if the Window Lost
  Focus") acts out a lost focus for tests, also in a hidden test run.
- The menus' warning: the top right status box gets three white-on-red lines ("Game window not focused: input/
  performance may suffer; click the game's desktop window") while the window lacks the keyboard's focus, on a flat
  screen and in the headset, with `vr_menu_status 0` too (the box then holds the warning alone; the menus keep clear of
  it as of the box).
- qvr_openxr.txt's timing line adds "window unfocused, N presents skipped" while unfocused.
- `vr_debug_clock`: the frame count and wall clock, and the mean frame period since the previous call.
The input in VR comes from the controllers (OpenXR), untouched by the window's focus; the runtime's menu pause stays.
Test (`scratch/focus_test.sh`, frame period over 120 frames, before / unfocused / after): flat 4.0 / 17.3 / 4.0 ms (the
sleep, as before); mock VR 4.0 / 4.0 / 4.0; a fakexr OpenXR session 4.0 / 4.0 / 4.0, its timing line "window unfocused,
416 presents skipped". Screenshots (scratch/focus/): menu_flat.png and menu_vr.png, the warning and without it.
Not verifiable here: the driver's cap itself. For him: in VR, click another window and watch the fps (the wrist
counter) and qvr_openxr.txt's `end` time; if it still drops, set NVIDIA's Background Application Max Frame Rate off.

## The eyes' driver sync: no glGet at the late acquire (worktree `tghost`, branch `agent/lateacq`, 2026-10-16)

His real VR trace (`qvr-kit/scratch/vrtrace_20261016/VRTRACE.md`, B7) put eye L/R self time (0.27/0.24 ms) in NVIDIA's
threaded driver waiting for its thread, called from `VR_RenderView`'s `acquire` lambda with `vr_xr_late_acquire 1`: its
`glGetIntegerv(GL_FRAMEBUFFER_BINDING)` (any glGet makes the threaded driver run everything queued first).
- The engine keeps its own record of the framebuffer bindings: `GL_BindFramebufferFunc`, through which every bind goes,
  is wrapped at `GL_Init` (gl_vidsdl.c; `GL_BoundDrawFramebuffer`), and the late acquire puts back the recorded binding
  instead of reading GL's. vr_foveated.cpp's own wrapper of the pointer sits over it and puts it back as before.
- `vr_debug_glstate 1` (Debug, "Graphics State") checks the record against GL at each late acquire and at each frame's
  end, printing "glstate: ... framebuffers draw N read N bound, the engine believes ..." on a difference.
- Audit of the per-frame path, nothing else to remove in Release: `glFinish` only with `r_speeds`/`gl_finish`;
  `glIsEnabled` and the texture cache's glGets only with `vr_debug_glstate`; GPU timer reads check availability first;
  the other framebuffer glGets are at creation or map load (vr_gfx_gl.cpp, vr_permdecals.cpp) or in debug commands
  (vr_wounds.cpp, vr_gadget.cpp); the OpenXR backend's `glGetError` drains are at swapchain creation (SteamVR needs
  them). `GL_AcquireFrameResources`'s `glClientWaitSync` (with the flush bit) waits on the fence of three frames ago before
  their buffers are reused: needed, normally already signalled.
A/B, Release-Tracy, headless mock, `idle_e1m1` bench (900 frames, 90 Hz paced, 2048 eyes, his settings), self time a
frame (p50 / p95 ms) and CPU busy (p50 / p95 ms), `scratch/la_cap.sh`:
| run | eye L self | eye R self | CPU busy |
|---|---|---|---|
| before, late acquire 1 | 0.277 / 0.383 | 0.200 / 0.249 | 1.191 / 1.427 |
| before, late acquire 0 | 0.001 / 0.002 | 0.001 / 0.001 | 0.661 / 0.926 |
| after, late acquire 1 (two runs) | 0.002 / 0.002 | 0.001 / 0.001 | 0.767 / 0.950; 0.624 / 0.800 |
| after, late acquire 0 | 0.001 / 0.001 | 0.001 / 0.001 | 0.622 / 0.785 |
The headless run reproduces his headset's numbers, so NVIDIA's threaded optimization is on in the hidden-window run too
(the driver's per-application setting, not tied to a display). The glGet cost about 0.5 ms of CPU a frame; late
acquire now costs the same as early. Images (`vr_eyeshot 3`, late acquire on, before / after builds): e1m1 differs by 11
and 15 bytes (max 1-2), as two runs of the old build do (23 / 22 bytes, max 5); start's teleporter view differs only
inside the portal, as it does between two runs of one build (10-50 thousand bytes there). `vr_debug_glstate 1` printed
no difference over both maps, so the GL calls are the same ones as before.
For him: in his next capture, eye L/R self time should be near 0 (was 0.27/0.24 ms); the driver's syncs left are the
runtime's (`xrBeginFrame` / `xr submit`, VDXR's `wglMakeCurrent`).
## The grip's search (worktree `dequant`, branch `agent/gripcache`, 2026-10-16)

His VR trace (vrtrace_20261016, finding 3): foegrab's grip search cost 2-8.4 ms a frame with a grip pressed near
monsters. `tryTake` asked `hitmodel::nearest` for each of the fist's ~94 spheres against each monster near, and each call
posed the model anew (every vertex of every triangle through `Verts::at`, a ragdoll's skinned again) and tested every
triangle.
- `hitmodel::pose(ent)` poses the model once (its vertices as drawn, each triangle's bounds), `hitmodel::nearestPosed(p,
  beyond, ...)` searches it for one point: nearest()'s result to the bit (the same triangle test, now `nearestOnTri`,
  shared with nearestDrawn; the same order and ties), a triangle passed over when its bounds (grown 0.03 units, more than
  a nearest point's rounding) are no nearer than the nearest so far or farther than the previous sphere's triangle.
- `search()` in vr_foegrab.cpp: a sphere farther from the posed model's bounds than both the deepest inside it counts
  (maxInsideCm) and the leniency plus its radius is passed over (it could not be taken). The "no living enemy" debug line's
  nearest says "over N cm" when only such a sphere could have been nearer.
- `vr_foegrab_debug 2`: each search prints its time, the old search's (run after it, for the check) and its result
  ("foegrab: search main, same: ..." or "DIFFERS").
Test (`Misc/quakevr/foegrab/gripcrowd_test.py <agent> <tag>`: a ring of 10 grunts 56 units round you, both hands grip at
30 places): 583 searches (8.3 models, 94 spheres each), all "same", 15 takes; 0.15 ms a search (median 0.10, max 0.45)
against the old 14.8 (median 14.6, max 22.9), about 100 times less. Two separate runs can't be compared line by line
(the grunts' idle frames vary), hence the in-run check. judo_test lift and hold (with `vr_foegrab_debug 2`): all PASS, 24
searches "same". Melee eval 168/177. For him: grip near a crowd and see that the hands take hold as before.

## His knockdown values are the defaults (2026-10-16, config 140)

His request: "make my five knockdown values the defaults". From his `ironwail.cfg`: `vr_knockdown_chance` 1 -> 0.6,
`vr_knockdown_damage` 2 -> 3, `vr_knockdown_getup_speed` 1 -> 0.8, `vr_knockdown_time_min` 2.5 -> 1.75,
`vr_knockdown_time_max` 4.5 -> 3.5. Migration 140 moves configs still at the old defaults. (His lower shove/bash damage was
already the default; the melee eval canary's halved parry-bash damage is that tuning, intentional.)


## Load hitches from his VR trace: hull cache, textures, menu banner, prop hulls (worktree `dequant`, branch `agent/loadperf2`, 2026-10-16)

His go on VRTRACE_20261016 (B, C, D, finding 4). One commit each.

- **Hull disk cache keyed by the compile's version** (`vr_hull.cpp`, `treeVersion`): the folder under
  `cache/hulls/` is a hash of `treeVersion`, the file layout's version, the brushes' `buildVersion` and the node and
  plane sizes, no longer the build's compile time. **Bump `treeVersion`** when the tree compile changes (growing,
  units, speculate, merging, rebounding); `vr_hull_cache 2` compiles anyway and compares. vrstart after a rebuild
  (touched `vr_hull.cpp`): 7 trees read in 107 ms, 7 the same under `vr_hull_cache 2`; before, a rebuild meant 7 trees
  compiled (10.6 s of tree builds on the pool; his trace: 1.8 s wall, 34 s of worker CPU).
- **Textures decoded ahead at map loads** (`vr_texture_prefetch 1`, `vr_imgprefetch.cpp`; Debug > the image cache's
  rows, `vr_image_prefetch_info`): `Mod_LoadTextures` first finds each texture's replacement image, its glow and the
  external pack's maps (picture, normal, specular, glow) the way its loop will, reads the png/tga/jpg files not decoded
  before, and up to 8 tasks decode them (at most 256 MB of pixels waiting); the loop takes each as it asks
  (`Image_LoadImage`: the same file by identity, offset and length), uploads in order on the main thread. Files read
  and never asked for (the pack's maps of a texture its picture doesn't match) aren't read again that session; the
  pack's `.mat` texts are kept for the session (`SessionResults`). `imagehash` identical on and off (QRP + Quetoo
  e1m1: 1087 textures; AD ad_tfuma: 2963).
- **Menu images at start-up** (`VR_MenuBrandPreload`, after `Draw_Init`): the banner and Ko-fi's logo load in the
  start-up's window (so the next start decodes them on a worker from its list). First menu open frame 34.0 -> 11.9 ms;
  start-up +23.9 ms the first session, +6.8 ms after.
- **A frame's new props' hulls side by side** (`vr_box3d_hulls_ahead 1`, `vr_box3d.cpp` `prebuildPropHulls`; Debug >
  Physics, `vr_physics_hull_stats`): before `syncEntities`' loop, every entity about to become a prop (kindOf's Prop
  branch) whose hull isn't made has its hull built on the pool (the same points, the same `propHullOf`); `propHull`
  takes it (waiting if needed), leftovers go into `propHulls` at the loop's end and at the world's end. The runtime
  hulls are gibs (brains, heads), dropped weapons, a different key per gib size, so they can't be made at precache;
  the gain is a burst's hulls built in parallel. `vr_box3d_hulls_ahead 2`: built again on the main thread and
  compared (Box3D's hull hash): 108 of 108 the same.

Numbers (headless, exclusive; QRP base + `vr_extmaps_dir` his Quetoo folder; AD native):

| | before | after |
|---|---:|---:|
| QRP e1m1 cold (second load) / warm (ms) | 702 / 318 | 555-570 / 323-336 |
| QRP `start` first load (ms; in the start-up's window, its list already decodes most) | 1462 | 1452-1469 (world model 533 -> 476) |
| qbase e1m1 first load + AD switch (ms) | 1341 | 1064 |
| AD ad_tfuma cold / warm (ms) | 1871 / 509 | 1316 / 485 |
| vrstart's 7 trees, first session after a rebuild | compiled, 10.6 s of pool | read, 107 ms |
| first menu open frame (ms) | 34.0 | 11.9 |
| play_gore_fight script: runtime hulls' main thread time / worst CPU frame (ms) | 17.1 / 9.2 | 7.4 / 6.9 |

Main thread cost of the texture look-ahead: 35-60 ms on a cold load (much of it the pack's file probes the loop would
have made), 3-10 ms warm. Single runs: loads vary by ~5%. Not done: the remaining 11.9 ms of the first menu frame
(other first-draw work); a single slow hull still waits for its job (the gore script's slowest wait 3.1 ms).

For him in VR: maps with his texture packs load faster the first time; the first menu open and gib bursts smoother.
## Single-pass stereo, stages 1-3 (worktree `flatfist`, branch `agent/stereo1`, 2026-10-16)

His decision: single-pass (instanced) stereo, staged, behind `vr_stereo_single` (default 0). Design, stages and what is
left: `docs/vr-port/STEREO.md`. Done: GL_OVR_multiview2 (the driver has it, and GL_EXT_multiview_texture_multisample for
MSAA); every ported program gets a multiview twin compiled from the same sources with `QVR_MULTIVIEW` (gl_shaders.c);
the views' block (binding 5) replaces `ViewProj`/`EyePos` in the twins only; both eyes' light clusters, AO tiles and
foveation rates; a frustum holding both eyes, both eyes' PVS and backface cull; a 2-layer colour and depth/stencil array
drawn in one pass (the opaque world and brush models, the alias models but the view entities), then each eye's own
scene from a copy of its layer. Two-pass while a teleporter's view is in sight. Debug > Profiling and Memory:
"Single-Pass Stereo", "Single-Pass Stereo Info" (`vr_stereo_single_info`); `vr_stereo_single_passes` (bits) splits it
per pass.
- **Images** (`vr_eyeshot 1`, both eyes, e1m1 start, e1m1's computer room with 2 spawned monsters and the shotgun in the
  hand, vrteleporters, warden; fixed frames, seeds; `scratch/shots.sh`, `cmpshots.py`): two runs of the old build differ
  by at most 0.004% of pixels over 2 levels (one 13); toggle off against the old build: the same (0.007% at most, in the
  teleporter view's own noise); toggle on, `vr_foveated` 2 and MSAA 4 (the test config) or both off: e1m1, the room and
  warden 0.000% over 2 levels (max 1-7). vrteleporters renders two-pass (its gates are in view); single-pass there
  differed by 2.8% (the floor behind the middle gate drawn: below), which is why.
- **Timings** (bench.sh, 900 frames, 90 Hz paced, exclusive, 2 reps each; `play_ad_tfuma_fight` cannot run: no AD in
  the kit's game folder): combat_48 draw calls 385 -> 317, alias models drawn 738 -> 589, but CPU busy 2.68 -> 2.90 ms
  (p99 5.2 -> 8.6: the twins compile on first use, mid-run), GPU 4.41 -> 6.63 ms (eyes 4.38 -> 3.96, the pass 2.67).
  play_e1m1_fight: CPU 1.45 -> 1.49 ms, GPU 3.56 -> 5.54 ms. Single-pass is slower as it stands.
- **Why** (STEREO.md, "What the first measurement says"): the world's draws are GPU-built indirect commands (0.02 ms of
  CPU an eye) and alias models are instanced (0.41 -> 0.34 ms with the pass), so halving them saves under 0.1 ms; the
  pass adds a third `R_SetupView` (each eye still sets up its own for its own passes); copying the depth/stencil into each
  eye's target loses its compression and hierarchical Z for all that follows (the eyes' particles 0.82 -> 1.22 ms GPU).
- Next run, in order: no copy (the eyes' targets as views of the array layers), each eye's view set-up reusing the
  pass's, the twins compiled when the toggle turns on; measure again; only then stages 4-5 (particles and the VR
  effects are where the per-eye CPU is), and both eyes' teleporter views before the pass.

## One derived-data cache, its warm-up (worktree `dequant`, branch `agent/warmcache`, 2026-10-16)

His decision: a unified derived-data cache, a background warm-up at game start, cache-only map loads with the lazy path
as fallback, and a map load's look-ahead. Details and how to bump a product: `docs/vr-port/CACHES.md`.

- **One layer** (`vr_diskcache.cpp`, was `vr_texcache.cpp`): one root for every game folder (`<base>/cache`, or
  `vr_cache_dir`: tests point it at an empty folder), a product table (folder, verify setting, budget), one prepare
  (other builds' folders and the oldest files past the budget removed, once a session, any thread), one atomic write,
  per-product counts. Moved onto it: normal maps, gun pieces, hulls, occlusion bakes (their files' layouts unchanged).
  `vr_cache_info` (Debug > Profiling and Memory > Cache Info). Misses while a map is in play are counted and printed
  with developer 1 (`cache miss in play: ...`).
- **New products:** dequantised poses (`vr_mdl_dequant_cache`, `dequantVersion`) and ragdoll rigs
  (`vr_ragdoll_rig_cache`, `rigVersion`; read on the pool inside warmRigs).
- **Warm-up** (`vr_cache_warmup 1`, `vr_warmup.cpp`; Debug status line, `vr_cache_warmup_info`): every installed
  game's .mdl files (pak entries and loose progs/), their occlusion bakes made one at a time on one idle-priority pool
  thread; paused during loads and after late frames; resumes from a done list. The other products need a loaded model
  or map, so they stay with the loads and the installer's preparation run.
- **Look-ahead at load:** what exists already covers it (QC's probes spawn one of each kind that can appear later:
  monsters waiting for a trigger, dispensers; their models, sounds, hulls and limbs made in the load). The rigs now come
  from the disk, so the probes' kinds cost less. Not done: a held weapon's convex pieces for weapons not yet held (cut
  at first pickup when the disk lacks them; now visible as "in play" misses).

Checks (headless, his base folders): every product at setting 2 over e1m1 then ad_tfuma: normal maps 808 same, AO 395
same, dequant 396 same, rigs 2 same, 0 differed. After the warm-up into an empty cache, e1m1's AO: 119 read, 21 made
(the VR-derived models and limb models, which have no file).

| e1m1 / ad_tfuma (empty cache root vs second session) | first-ever | second session |
|---|---:|---:|
| e1m1 load (ms) | 1103 | 860-896 (old build, its caches warm: 917) |
| ad_tfuma load, AD native (ms) | 2130 | 1176 |
| e1m1 rigs (warmRigs, ms) | 14.0 + 8.4 | 1.0 + 0.8 |
| e1m1 dequant (ms: computed / read) | 28.9 | 18.4 read |

Warm-up: 902 models in 163 files (listing 32-40 ms), 677 bakes made in 63.7 s of one worker's time (66 s wall); a
second session lists 0. Cache size after e1m1 + ad_tfuma + the warm-up: about 90 MB (normal maps 74, dequant 11, AO 8,
hulls 2.5, rigs 0.7). Frame time while it runs: one idle-priority thread; in the paced headless run the frame period
stayed 4.0 ms; not measured in VR. First use: the old per-game `cache/` folders are no longer read, so his first
session on this build makes everything once more (vrstart's hulls: 10 s of pool).

For him: start the game, open Debug > Profiling and Memory: the warm-up line counts up; play meanwhile and watch for
hitches; `developer 1` shows `cache miss in play` lines for anything the loads didn't prepare.

### Ammo switch only with an empty other hand (agent/ammoswitch)

A player: no ammo type switch while akimbo. vr_ammo_switch_empty_hand 1 (archived; VR Settings, Ammo Button page, "Ammo Switch Needs Empty Hand"): ToggleSecondaryAmmo (QC/weapons.qc) runs only when the other hand is empty by VRIsHandEmpty (no weapon, nothing carried: prop, torch, grabbed enemy); otherwise a short lock4 click and a haptic on the pressing hand, and developer 1 prints "ammo switch: refused". 0 restores the old behaviour. Checked headless (scratch/ammosw.sh): empty hand toggles; a gun in the other hand refused; option off toggles with the gun there. A carried prop was not set up in the mock (same VRIsHandEmpty test). Backlog: ammo type following the loaded magazine.
## Held guns' convex pieces at map load (worktree `dequant`, branch `agent/gunprep`, 2026-10-16)

His decision: prepare the held-weapon pieces at the map's load for every weapon the map can give, so first pickups are
cache hits (no background process). Done in `vr_box3d.cpp` `prepareHeldGuns` + `QC/vr_gunprep.qc`.

- **Which guns:** QC (`VR_GunPrep_Collect` / `VR_GunPrep_Model`, so the drop rules stay in QC) lists the weapons lying
  in the map (`weapon_*` classes incl. upgrades and crowbar, `thrown_weapon`, `func_weapon_grabbable`), the guns its
  monsters drop (grunt, enforcer, knight, hell knight, ogre; AD's armed soldiers through `VR_ADGun_ForClass`), backpacks'
  weapons and the first player's hands and holsters. Each gun's primary and secondary-ammo model (`WeaponIdToModel`).
  The engine resolves the names in `sv.models` (a model the map did not precache is skipped).
- **How:** the triangles (`held::modelTriangles`, the held key -2) on the main thread, then `cutGun` per model in one
  `parallelFor` (reads the file, cuts and writes only on a miss), the results appended to `gunShapes` as `gunPieces`
  would. A lying drop's shape has the same triangles, so its file is the same one (a dropped grunt's gun is a hit).
  Run at the first frame of a world, and again whenever the player's hands/holsters change (they are filled after that
  first frame), so it is load time; in play only a gun from outside the map's set would still cut.
  `vr_gun_pieces_ahead 0` turns it off (Debug > Profiling and Memory > Gun Shapes at Load). `vr_debug_box3d 1` prints
  the list; `cache miss in play: gunpieces (<model>, <key>)` now names the model. `GunPiecesDisk` counters are atomic.
- **Checks** (headless, empty cache root, `developer 1`): ad_tfuma, `vr_ad_test 71` (drops all 8 AD guns) then
  `vr_pickup_test 2`: off 7 `cache miss in play` for gun pieces (grunt, enforcer, AD soldier guns), on 0. e1m1: the
  loadout (axe, shotgun) and the map's SSG, nailgun and grunt gun are hits; guns `vr_pickup_test 4` drops that the map
  cannot give (enforcer rifle, swords, chainsaw...) still miss, as designed.

| ad_tfuma (empty root, 21 guns) | prepare | map load |
|---|---:|---:|
| off, cold | - | 9873 ms |
| on, cold (21 cut) | 54-82 ms | 9894 ms |
| on, warm (21 read) | 4.1 ms | 8911 ms (off, warm 8918) |

e1m1 cold: 37.7 ms in all (5 + 1 + 1 guns over three runs).

## His ragdoll hand-follow value is the default (2026-10-16, config 141)

His note: "I changed some settings regarding the ragdoll and the hand follow limb values. Please make those the new
defaults. I like if the hand follows the ragdoll more accurately." From his ironwail.cfg: vr_ragdoll_hand_stick 4 -> 30
(the only ragdoll hand-follow setting he changed). Migration 141.

## Corner shortcuts act on the panel pressed (worktree `tghost`, branch `agent/panelshortcut`, 2026-10-16)

His note: the Levels shortcut opened only on the main panel. Cause: `tidyInstances` pushed any Quake (non-VR) menu back to
the first panel. Now Ironwail's Levels (m_maps) stay on the panel pressed on; Back returns that panel to its page.
Checked on a second panel (mock laser + trigger): Levels (before: first panel; now: second), VR Settings, Advanced VR,
Relighting, Checklist (Developer level), Map Library, Search, Console, Recording (already the pressed panel, unchanged).
Back to game, Quicksave/Quickload, Toolgun have no page (unchanged); New Panel opens a panel. Main Menu (Quake's own menu
with sub-menus) still opens on the first panel. `menu_panels_test.py` step 13 covers them; `vr_menu_path_check`: 0 missing.
## Temporal AA: motion vectors and a first TAA (worktree `flatfist`, branch `agent/taa1`, 2026-10-16)

His decision: "start a worker on motion vectors + initial TAA implementation". This covers TEMPORAL.md stages 2a and
2b, first cut. The design as built, the departures from the plan and the section-12 answers taken are in
TEMPORAL.md section 1a. Code: `Quake/vr/vr_temporal.cpp`, plus hooks in `gl_rmain.c`, `r_alias.c`, `r_world.c`,
`gl_shaders.h`, `vr_glsl.h` and `vr_stereo.cpp`.

- **`vr_taa 0` is today's picture.** Eyeshots of the AA review's two frozen e1m1 views (same script, same cfg):
  - the world and brush models are bit-identical;
  - only the models' pixels differ, by as much as two runs of the same build differ from each other (their animation
    timing).
- **Vectors** (`vr_temporal_debug 1`): a smooth field under a head turn; the grunts, the hands and the held gun show
  their own vectors. The reprojection test (`vr_temporal_debug 3`, `vr_temporal_error`) in the computer room, with a
  slow head turn plus shake:
  - error 1.13 with the vectors against 2.56 without (0-255 luma);
  - with a gentler motion, 0.94 against 1.11;
  - with a hand swinging, 0.85 against 1.02 (L eye);
  - 99.4-99.7% of pixels compared;
  - the floor of about 0.9 is the bilinear resampling of the previous frame;
  - disocclusions show red at the swinging hand's trailing edge and the view's edges, as they should.
- **Cost** (RTX 4090, mock eyes 2048², his cfg with MSAA off, 2 x 600 frames paced 90 Hz,
  `benchresults/taa1_taa0` vs `taa1_taa1c`):
  - e1m1_fight: 2.88 to 3.39 ms (+0.51);
  - combat_48: 3.90 to 4.50 ms (+0.60);
  - about 0.25-0.30 ms an eye, of which about 0.05-0.09 is the models' motion output;
  - 4x MSAA costs +1.2 / +0.8 ms on the same scenes;
  - the first version cost +2.7 ms in combat_48, because it switched `glDrawBuffers` per alias batch; fixed.
  - Memory at 2048²: motion 64 MB, histories 4 x 32 MB, depths 4 x 16 MB, mask and debug 36 MB, positions 32 MB:
    about 320 MB.
- **Stills** (centre 1024², against the review's 32-sample reference):
  - E1M1 start, geometry edges: off 30.9, 4x MSAA 13.5, TAA 14.4, TAA with sharpening 0.3 18.1.
  - Interiors: 0.63 / 0.61 / 0.58 / 0.59.
  - Sharpness (mean Laplacian): reference 2.68, off 3.60, TAA 2.55, sharpening 0.3 2.86.
  - TAA alone is a touch softer than the reference, and 0.3 sharpening brings it back to about the reference, so 0.3
    is the default. Retro textures keep their blocks.
  - The computer room's numbers are muddied by the grunts' poses differing from the reference run (edges: off 28.2,
    MSAA 9.6, TAA 15.9).
- **Motion** (main hand swinging, `vr_mock_swing 0.8`, three consecutive TAA frames vs off): no visible trail where the
  hand had been in the frame after, and the floor and grunts there match the frame without TAA (mean difference 3.1,
  mostly the grunts' animation).
  - Not tested: melee-speed swings at the headset's real 90-120 Hz pacing. Mock fast mode's frames are irregular.
  - The weak spots to look at in the headset: thin silhouettes of fast hands and guns against bright backgrounds;
    particles and liquids, which are reactive, so they are aliased rather than ghosted; and the sky while strafing,
    which has no direction-only reprojection.
- **For the author in the headset**:
  - Graphics > "Temporal Antialiasing (TAA)" on.
  - Look at e1m1's railings and the planks' gaps while moving your head.
  - Swing the axe and a gun fast in front of a bright wall, looking for trails.
  - Try "TAA Sharpening" 0-0.6.
  - Debug > "Show Motion Vectors" > "Reprojection Test" should hold still as you turn and walk. Red means history thrown
    away.

## One source of defaults: his values move into the code (2026-10-16)

His decision: every vr_defaults.cfg value equal to his current value is the source of truth. 192 cvars got that value
as their compiled default in vr_cvars.inc and left vr_defaults.cfg (behaviour unchanged: vr_default set the same value
at startup). Left in vr_defaults.cfg for his review (his value differs from both): vr_window_view (code 0, cfg 1, his 0),
vr_spectator_fov (90 / 130 / 120), vr_spectator_scale (0.75 / 1 / 1.5), vr_foveated (0 / 3 / 2),
vr_retrolight_model_steps (16 / 15 / 16). Script: qvr-kit/scratch/review/defaults_apply.py.
## A late sprite freed under a touch (worktree `gorefx`, branch `agent/spritecrash`, 2026-10-16)

His crash (vrfiringrange, hit by an AD gargoyle spawned with the toolgun, 083612d7): an access violation in
R_GetSpriteFrame, drawing the soft sprites. The dump: the sprite's `cache.data` pointed into an alias model's frame
descriptors (`fr_233`, `fr_234`...), so its "frame group" pointer was a name's zero padding.

- **Cause.** Sprites and brush models live on the hunk; Quake loads them only with the map. Quake VR also loads them in
  play (late precaches, VR_LatePrecacheModel), from QuakeC that can run inside a temporary hunk allocation: the touch
  lists of VR_TouchLinks (vr_physics.cpp) and SV_TouchLinks, SV_PushMove's moved lists. The gargoyle's fireball touches
  the player, its explosion precaches AD's `progs/s_exp_small.spr` (not the firing range's), and the touch's
  Hunk_FreeToLowMark then freed the sprite with its list; the next alias model loaded into the cache there.
  Found with a temporary check in Hunk_FreeToLowMark (a loaded sprite above the mark): VR_TouchLinks, freeing under
  `progs/s_exp_small.spr`.
- **Fix** (zone.c, gl_model.c, host.c): a model whose data stays on the hunk (not an alias model) sets a keep mark
  when it loads (Hunk_KeepLoaded); no Hunk_FreeToLowMark goes under it until Host_ClearMemory (Hunk_ClearKeep). The
  temporary list under such a model stays until the map's end (7 and 17 KB in the test; `developer 1` says so). This
  covers every such window, present and future, not only the touches.
- **Guard** (r_sprite.c): a sprite with no frames, a group with none or a null frame pointer, or a frame without a
  picture is skipped, said once with `developer 1`, instead of crashing.
- **Test** `Misc/quakevr/late_sprite_touch_test.sh <agent>` (needs `qbase/ad`): gargoyle fires at the god player, then
  entries 0-160 of the spawn list while it keeps firing. Before: R_DrawSpriteModel_Real crashed 11 s in (`no such
  frame 5 for 'progs/s_exp_small.spr'`, then an access violation). After: PASS twice.
  `toolgun_spawnall_test.sh` (vrfiringrange, AD data): 191 spawned, 0 failed, 9 unavailable, PASS.

## Climbing footholds: the legs find support while climbing (worktree `backlogclean`, branch `agent/climblegs`, 2026-10-16)

His voice note (BACKLOG, 2026-10-14 23:10:39): legs should seek plausible support on walls, ledges, pillars and ladders.
vr_avatar.cpp `updateClimbing` / `findFoothold`, purely drawn (no collision or movement change).

- While a hand holds on (`climb::heldAt`, the server's holds from STAT_QVR_CLIMB*), each foot looks for a hold below its
  hip: straight in towards the holds' face at the preferred height (half the leg under the hip climbing up, 0.6
  hanging), and towards the holds when that misses (a pillar's or a pipe's side); then down for a top (a ledge, a rung,
  the floor) just inside what it met or at the holds' lip (a ladder's rungs in front of its wall), and in front of what
  it met (a ledge sticking out lower). The top nearest the preferred height wins (higher ones favoured climbing up); else
  a spot on the face: at a 16-unit spacing (a flat ladder's "rungs"), the preferred height, or the spacing above. Two
  clearance traces keep the toes out of the face (just over the sole and just over the ankle: a rung's underside). A
  hold must be within 0.97 of the leg and under the hip by at least 0.45 of how far out it is (else straight legs
  sticking out). 4-9 point traces a search (SV_Move on the listen server, its VM pushed), only when a foot looks.
- A foot on a hold stays put in the world until it is overstretched (0.98 leg), too high (less than 0.15 leg under the
  hip), 0.45 m to the side, or, climbing up, left low; it then steps on an out-first arc (out from the face by its own
  length plus 12 cm, across, then in) to the next hold, one foot at a time, the other waiting. None found: it hangs,
  slightly bent, swinging a little and trailing the body's motion, and looks again 0.25 s later. Letting go (a fall, a
  mantle) steps the feet out first, then eases back to the legs' own pose. Knees point at the face and a little out.
- While climbing the body hangs under the head (the lean over the box's middle eased out: `climbHang` in solveTorso, so
  the holsters, flashlight and self-collision torso follow it): the hips came 0.2-0.3 m nearer the face, which put the
  holds within a leg's reach (before, the hips were 0.8-1.0 m from the rungs and the feet mostly hung).
- Settings (archived; VR Settings > Body, after Swimming Kick Rate): `vr_body_climb_feet` 1 (Climbing Footholds),
  `vr_body_climb_feet_reach` 1 (Foothold Reach, 0.5-1.2x), `vr_body_climb_feet_speed` 1 (Foothold Step Speed, 0.3-3x,
  1 = a 0.35 s step). Debug: `vr_debug_climb_feet` (Climbing Footholds Debug): 1 `climbfoot` per search (what, where,
  traces, ms), 2 `climbfeet` per frame (state, hold number, drawn ankle, ankle or toes inside something, the update's
  us), 3 also drawn.

Tests (headless, `climb_plays.py ladderlean` on vrclimb's rung wall `setpos 71 0 24`, 24 s, three climbs): feet on holds
2098/2477 frames (L/R), stepping 1495/1671, hanging 1605/1050; 17/19 holds (0.7-0.8 a second a foot); planted feet slid
0.0 cm; ankle or toes inside geometry 0 frames (earlier builds 200-300: toes through rungs while stepping and letting go,
fixed by the out-first arc, the clearance trace over the ankle and the weight waiting for the feet to step out).
Searches: 16 tops, 20 face spots, 8 none, 2-5 us each; the update 0.5 us median a frame, 1.4 us p99. Screenshots (look
down and side view, footholds on and off): the ledge hang (`setpos 78 176 24`) has the feet against the face 0.7 m under
the hips; the rung hang one foot on the face, the other higher, knees bent at the wall. AD's ad_crucial ladder (`setpos
-417 -2404 -107 0 90 0`), before the hang-under-the-head change: the rungs were 1.0 m from the hips and the feet hung (7
face spots, 79 none, nothing inside geometry); not re-run after it (that setpos didn't take in two runs of four).
Scripts: the worktree's scratch/cf_run.sh, cf_stats.py, cf_shots.sh, cf_plays.py (`leanledgehang`, `leanrunghang`: the
hangs with the head leant in, held 30 s).

For his VR check: climb a wall, a ladder (AD's too) and a ledge and look down; the body now hangs under the head while
climbing (say if he prefers the old lean); Foothold Reach and Step Speed. Open: rungs further out than a leg (a ladder
standing off a wall) leave the feet hanging.

## QC cleanup: the R2 review's FIX-NOW items (worktree `goreflesh`, branch `agent/qcclean`, 2026-10-16)

No behaviour change intended anywhere; one commit per item. QC still compiles with 0 warnings.

- **QC/ad matches its importer again.** `Misc/quakevr/ad_import.py` now makes every hand edit QC/ad had collected:
  the melee arc (`ARC_HITS`: 22 `ai_checkmelee` tests behind `VR_MeleeArc_Hits`; the 21 `th_melee` launches in
  ai_checkattack.qc behind `VR_MeleeArc_Launch`; the turns held in ai_charge*, Demon_Claw, drole_claw, mino_claw,
  seek_punch), the melee space (`vr_melee_time`, `VR_MeleeSpace_Step` in ai_charge*/ai_melee*), the AD guns' effects
  (electric bolts' crackle, shock and FLYMISSILE; the pyro's flames, its poison with the attacker credited) and the
  toolgun's live torches (`vr_spawn_live`), as `FUNC_PATCHES`/`PATCHES`; the Quake VR names they call are listed in
  `QVR_NAMES` (never renamed). New `--check [DIR]` writes to `scratch/ad_check` and diffs against QC/ad and progs.src
  (ARCANE_DIMENSIONS.md 14 says to run it before committing a QC/ad change). The Steam AD 1.80p1 folder holds the
  pinned paks: QC/ad regenerated from it, `--check` says 0 of 142 files differ, progs.dat byte-identical (the only
  text change: `VR_MeleeArc_Hits(FALSE)` reads AD's `ad_FALSE`, both 0). `// AD support` marker added at
  vr_gunprep.qc's AD gun row.
- **Test impulses** (weapons.qc `ImpulseCommands`): the 42 single-player test impulses moved into
  `float() VR_TestImpulse` (new `QC/vr_impulse_test.qc`), called first under one `!deathmatch && !coop`; TRUE where a
  block returned before. The debug prints that worked in coop and deathmatch (208, 213, 218, 228, 239, 253) and the
  150..189 weapon impulses (off in deathmatch only) stay where they were.
- **Test harness out of feature files** (pure moves, 4.2k lines, each new file in progs.src right after its feature
  file; the tests' globals stay with the feature): `vr_decap_test.qc` (708 lines), `weapons_test.qc` (635),
  `vr_ad_enemyguns_test.qc` (534), `vr_axestick_test.qc` (358), `vr_hotblade_test.qc` (307), `vr_burning_test.qc`
  (226), `vr_ragimpact_test.qc` (186), `vr_knockdown_test.qc` (171), `vr_walltorch_test.qc` (129),
  `vr_mg_activation_test.qc` (128), `vr_ad_bridge_test.qc` (122), `vr_hitreact_test.qc`, `vr_hitstagger_test.qc`,
  `vr_grapple_test.qc`, `combat_test.qc`, `vr_dummy_test.qc`, `vr_shock_test.qc`, `vr_crates_test.qc`,
  `vr_enemyshove_test.qc`, `buttons_test.qc`. Left in place (under 35 lines each): rogue_buzzsaw, vr_wpnforcegrab,
  vr_melee, vr_teeth, vr_ad_stealth's test hooks.
- **Campaign numbers named**: `VR_CAMPAIGN_DOPA/MG1/MG3` (3/4/5, vr_packutil.qc), `Dopa_Campaign()`; the 11 raw
  `cvar("vr_campaign")` reads in client, items, honey_items, vr_ammoutil, vr_mg_hub, vr_mg_triggers use the predicates.
- **MG branches out of id code**: `SUB_KillTarget` (Honey_KillTarget / `MG_KillTarget` in vr_mg_activation.qc),
  `MG_MonsterStartPaused()` (monsters.qc's three starts), `MG_ExplicitMoveDir()` (doors, buttons, SetMovedir),
  `MG3_DamageFilter` (vr_mg3_defs.qc; FALSE: no damage, else `mg3_damage`, so a Bloody Nightmare's 0 is still dealt).
- **Small ones**: `VR_Decap_Damage(targ, inflictor, attacker, damage, pop)` at the 5 sites where T_Damage and
  VR_Decap_After were adjacent (enforcer laser, carry, crates, multi-damage, thrown weapon; the bolt, enemy gun and
  Quad sites keep work between the two, left as they were); `VR_Decap_Thrown(Arm)` renamed
  `VR_Dismember_Thrown(Arm)`; `VR_Limb_IsZombie` at the two zombie-or-mummy sites (vr_decap.qc's own head-only
  zombie-or-mummy rule left: it excludes AD zombies); the 7 `damage_weapon = IT_AXE` through
  `VR_DamageCredit_SetWeapon` (axe, sword, chainsaw x2, crowbar, Super Axe, the decap test's saw); one
  `CreateThrownWeapon` call for both throw modes; 5 dead cvar handles and 5 dead vr_util getters removed.
- **Throw helpers** (no merge): `VR_Throw_Gravity()`, `VR_Throw_GravityScale()`, `VR_Throw_DmgSpeed()` (weapons.qc),
  used by the weapon throw and `VR_Carry_Throw`; same values.

Verified: build (engine + QC, 0 warnings) after each item; melee eval 168/177 with the same three known differences
(parry bash damage halved), its output identical to the base progs' run; limbs, ragimpact, judo and toolgun spawnall
tests against the base progs: the same results within run-to-run noise (limbs: base against base differs by as much;
ragimpact 4 FAILS now, 5 in the base run; judo 0 FAILS; spawnall 115 spawned, 0 failed). AD's own tests not run: no
AD data in the kit base (the importer's `--check` and the byte-identical progs.dat cover item 1).

## Crush feel: the body jolts, a thud unless something pops, same-level jumps, kept down while you work it (worktree `goreflesh`, branch `agent/crushfx`, 2026-10-16)

His voice note (vrfiringrange, 2026-10-16 18:23): crushing a ragdoll by jumping on it left it completely still; the
sound was always the gib's; a plain jump from the same level should damage and pop (a downed grunt's head in one or
two jumps, other parts more); and working a downed one (stomping, punching, dragging, lifting it) should keep it down
longer, up to a cap, getting up on its own time (or as soon as you stop, past it) otherwise.

- **Push** (`vr_ragimpact_crush_push`, 1, menu Combat > Knockdowns > Impacts "Crush Push"): a landing that crushes a
  ragdoll (down or a corpse) pushes the part under your feet down and out from its body's middle at that times your
  landing speed (at most 350 u/s), and flings the parts round it (6-56 units from your feet) up and away at up to 0.6
  of that, less the farther (QC `VR_RagImpact_CrushPush`, the engine's `physicspush` with your
  `vr_ragimpact_crush_mass` behind each). Your feet stand on the body and pin the part under them, so the jolt shows in
  the parts round it. No engine change.
- **Sound** (`VR_RagImpact_CrushSound`): the gib's (`player/udeath`) only when something came off (a decap, a limb
  popped or cut, the body gibbed: the counters before and after the strike); otherwise the arms-crossed parry's body
  hit, `fisthit` (pitch-jittered) with `demon/dland2` at 0.6 of it, 0.45..0.85 by the landing's speed (crush speed to
  pop speed).
- **Same-level jumps**: `vr_ragimpact_crush_speed` 350 -> **180** (a jump lands at ~270 on the floor but ~200 when your
  feet stop on the body: the shin's case; a stair step is 160), `vr_ragimpact_crush_wear` 0.3 -> **0.5** (a same-level
  jump wears ~0.55 of a head's own health: the second pops it). Config **142** moves configs still at 350 / 0.3.
- **Kept down** (QC `VR_Knockdown_Keep`, vr_knockdown.qc): each hit on a downed living one (a punch, a shot, a stomp:
  T_DamageImpl's knockdown branch, AD's too) keeps it down at least `vr_knockdown_hit_time` after the hit (0.5 -> **1**,
  config 142; it used to add 0.5 s a hit, capped at Time Max from the hit, so a flurry kept it down long after you
  stopped); a hand holding it (dragging, lifting: `VR_Carry_Held`, checked each think) keeps it down while held and
  `vr_knockdown_keep_time` (**1** s) after; never past `vr_knockdown_keep_max` (**6.5** s) down in all; its own timer
  is never cut short, and nothing extends it once its get-up began (`.vr_kd_rise`: a hit mid-rise no longer stalls the
  rise). Corpses and staggering ones are untouched. Menu rows next to "Hit Keeps It Down": "Handling Keeps It Down",
  "Kept Down, Most". Debug (`vr_knockdown_debug 1`): "kept down (hit|held): up in .. s at the soonest" and "get-up due
  after X s down (its own timer Y s)"; the per-think "held" lines only at 2.

Tests (`Misc/quakevr/foegrab/ragimpact_test.py`; new test hooks `vr_knockdown_test` 63 over its middle, 64 its parts'
displacement, 65 a 1-damage hit):
- crush: a same-level jump onto a settled corpse's head moves its parts up to 17.9 units (2.8 mean), onto its torso
  7.0 (3.3); 0.0 with the push off. A living one's active ragdoll moves on its own (not measured against).
- stompwear: a downed grunt's head pops at the 2nd same-level jump; sounds thud, gib; its torso dies at jump 3, its
  shin at jump 8 (health 26, 22, 19, ... -1).
- keep (own timer 3 s, keep_max 6.5): alone up at 3.1; hit every 0.5 s: 6.6 (capped); hits until 4.3 s: 5.3 (a second
  after the last); hits until 1.8 s: 3.1 (own timer); dragged by the head and let go at 4 s: 5.0; held 9 s: 6.6, out of
  the hand.
- melee eval 168/177 (the three known differences).

To test in VR: jump on a shoved-down grunt's head (2 jumps), its legs (more); landings that pop nothing thud; the body
jolts round your feet; punch or drag a downed one past its usual get-up and stop: it rises a second later, never past
6.5 s.
## The last differing defaults (2026-10-16)

His decision: vr_window_view, vr_spectator_fov, vr_spectator_scale and vr_foveated are personal: the shipped values
(1, 130, 1, 3) are now the compiled defaults; vr_retrolight_model_steps is 16 everywhere. The 21 remaining vr_default
lines equalled the compiled defaults and were removed: vr_defaults.cfg sets nothing now; vr_cvars.inc is the only source.
## Enemy Melee menu: arcs and reach by size and kind, Show Melee Arcs (worktree `gorefx`, branch `agent/meleemenu`, 2026-10-16)

His note (18:26): the melee arc works; put its settings in their own place, let him see the arc and the reach, make them
grow with the monster's size (a knight easier to dodge than an ogre), and a slider for the swing's turn.

- **Combat > Enemy Melee** (new page, `pageEnemyMelee`): Melee Arc, Show Melee Arcs; Arc: Arc Angle, Wide Swings, Arc
  Growth with Size, Reach Growth with Size, Kind Multipliers; Swinging: Swing Turn, Melee Space. The arc rows and Melee
  Space left Combat > Holding Enemies. No board or calibration-map path named them (`vr_menu_path_check
  maps/vrcalibration.map`: 6 found, 0 missing).
- **Size**: a monster's level width against a grunt's 24 units (this port's model-fitted boxes: knight, grunt, rottweiler
  24, enforcer 28, fiend 32, ogre 40, shambler 64; clamped 0.5..4 times). Arc = Arc Angle x (1 + `vr_enemy_melee_arc_size`
  x (size - 1)) x kind, plus Wide Swings for a sweeping blow (at most 180). Reach = Quake's own reach x (1 +
  `vr_enemy_melee_reach_size` x (size - 1)) x kind: id's `vlen(delta) > 60/100` checks (12, in the files the arc already
  patched) and AD's `ad_ai_checkmelee` (also its launch distance; the importer patch `ai_checkmelee`, `VR_MeleeReach`)
  now call `VR_MeleeReach(base)`. All of it only against a player with Melee Arc on; else Quake's.
- **Kind** (`VR_MeleeArc_Kind`, `vr_enemy_melee_kind` 1): the only entry is the knight, arc 0.85 (a quick thrust); every
  other kind 1. One `if` per kind, by classname: add one there.
- **Defaults** (arc growth 0.1, reach growth 0.1): a grunt-sized monster as before (65 deg, Quake's reach). Logged
  (`vr_enemy_melee_arc_show 2`, vrtesthall): knight arc 55.2 (wide 75.2) reach 60; ogre 69.3 (89.3) reach 106.7; shambler
  75.8 (95.8) reach 116.7; a grunt (no melee, not shown) would be 65 / 60. A shambler's wide claw reaches 96 deg each side.
- **Show Melee Arcs** (`vr_enemy_melee_arc_show`, also in Debug > Tests > Holding Enemies): QC's
  `VR_MeleeArc_ShowFrame` (StartFrame) sets, for each live melee monster (`th_melee`, AD's `ad_th_melee`) within 600
  units after a player, its arc, its sweeping arc (once it swung one), its reach (its blows' own, or Quake's usual 60/100
  until the first) on four engine-read fields; `vr_bodycontact.cpp` gathers them on the server and draws them with the
  view: the reach circle grey, the arc a fan on the floor (red with him inside it and in reach, green outside), the
  sweeping blows' extra fan orange (bright when he is inside only that), its kind, arc, reach and his angle and distance
  above it. 2 also prints `meleearc: show` once a second. Log Melee Arc's lines now give the reach too.

Verified: build (0 warnings); screenshots from an overhead mock camera (`vr_mock_camera 0 4.5 2.5 55 0`) of a knight and
an ogre facing him (red fan) and after he stepped aside (green); `melee_arc_test.sh gorefx id 1`: front, behind and the
knight's step-aside as before; the ogre's step-aside still lands 6 hits after the step and the fiend's 1, the same with
both growths 0 (today's behaviour on this tree; ROUND22's first run had 1 and 0), so not from this change; melee eval
168/177.

In VR: Combat > Enemy Melee, Show Melee Arcs on; let a knight, then an ogre come at you and sidestep: does the fan turn
green as you leave it, and do blows land only while it is red? Tune the two growth sliders and Swing Turn there.

## Single-pass stereo, second run: no copy, setup reused, twins ahead (worktree `flatfist`, branch `agent/stereo2`, 2026-10-16)

His decision: continue only if single-pass can become competitive. Details and numbers: `docs/vr-port/STEREO.md`, "The
second run". Done, all behind `vr_stereo_single` (default 0; toggle off unchanged):
- **No copy:** each eye renders on into its layer of the arrays through texture views (`glTextureView`, MSAA too) put
  into `framebufs` in its `R_SetupGL` (`VR_StereoSingleEyeTargets`); the mirror reads the eye's own targets. Three
  caches keyed to the scene's texture (liquids' scene size, haze's scene format, half-size particles' check) keep one
  per eye (each eye has its own texture now; otherwise 0.1-0.3 ms of CPU a view in GL queries).
- **Setup reused:** each eye keeps the pass's lights and light clusters (`R_PushDlights`). The marks are not reused
  (0.002 ms an eye); the eye's whole remaining set-up is 0.02 ms.
- **Twins ahead:** the 59 ported programs' multiview twins begun when the toggle turns on (or at start), compiled by the
  driver's threads (GL_KHR_parallel_shader_compile); frames two-pass until done. A cold world twin takes ~1 s to compile
  (the old first-use compile would have hitched that long), warm 59 in 0.4 s.
- **Numbers** (combat_48 / play_e1m1_fight, quietest runs, the machine shared with builds): CPU p50 2.60 -> 2.51 / 1.51
  -> 1.49, p99 4.87 -> 4.86 / 2.76 -> 2.76; GPU 3D p50 4.15-4.38 -> 4.45 / 3.41 -> 3.51-3.60, p99 6.37-6.74 -> 7.13-7.27 /
  4.90-5.27 -> 4.99-5.06. The copy's +2.2 ms GPU is gone; the GPU is still 0.1-0.3 ms behind: alias models in the pass
  +0.14, the VR particles after it +0.17 (the layered depth loses the driver's coarse early-Z), the world -0.05.
- **Gate not met** (GPU): stages 4-5 and TAA under single-pass not started. Estimate: stages 4-5 could save 0.2-0.4 ms of
  CPU in particle-heavy scenes but the GPU would stay behind on this driver; not worth it on his PC.
- Pre-existing, found on the way: no MSAA with `vr_foveated` on (his settings) the toggle's images differ by 0.2-0.8% in
  the foveated rings (the old copy build too); not found why.
## Parallax surface level: props on carved floors (worktree `gorefx`, branch `agent/parallaxplane`, 2026-10-16)

His note: props lying on parallax-mapped ground can look like they float a bit. Parallax carves down from the polygon
(the polygon is the relief's top) while props rest on the polygon, so over deep parts the floor seen is below the
prop's contact. His decision: try shifting the reference plane, with a slider.

- **`vr_parallax_plane`** (archived, default **0** = as before; Graphics > Surfaces > Parallax > Parallax Surface Level,
  0..1 by 0.05): where the polygon sits in the relief. 0 its top (carved down), 0.5 its middle, 1 its bottom (raised).
  The world's shader (and brush models', item boxes': everything `ParallaxUV` walks in the world shader; models stay
  at 0) walks the ray from where it crosses the relief's top, `plane` x depth over the polygon, to its bottom;
  `ParallaxDist` is then negative for a raised hit. Silhouettes stay the polygon's; item boxes' texture clamp holds both
  ends of the walk. The frame data has a new `Parallax3` (x: the plane) at its end (`gpuframedata_t.parallax3`).
- **Capped at the texture's average height** (its smallest mip's alpha): the surface is never put below the average
  level. Why: many textures carry heights that are all at the top (e1m1's and most id floors: alpha 255 everywhere;
  `vr_parallax_debug` shows them blue at 0 and wholly green at 1 without the cap), or mostly at the top (the rune
  panels, brick grout). Literal "polygon at the bottom" raised those whole surfaces by the full depth, and props sank
  2.5 units into a flat floor, a worse look than floating. So 1 means "at the texture's average level" for every
  texture, a flat one is never raised, and below the cap the slider is literal. Two textures of different averages
  meeting on one plane now show a step of at most `plane` x depth x the difference (none at 0).
- **Depth** (`vr_parallax_depth_write`, on by default) already wrote the hits' depth; a raised hit is nearer than the
  polygon, so `ParallaxFragDepth` no longer clamps it to the polygon's depth for negative distances, and the PDO programs
  declare `depth_any` (was `depth_less`/`depth_greater`, which promised never nearer). ROUND21 timed the three layouts
  alike on this driver. The world depth pre-pass keeps its full-reach bound (it has no texture to know the cap), so a
  hit is never further than it. Without depth write, raised relief is drawn at the polygon's depth (props' bottom edges
  stay in front of it).
- **Debug**: `vr_parallax_debug 1` shows raised hits green (brighter higher), sunk ones red, as before.

Checked (headless; the only id floors with relief are the rune panels: e1m1..e4m4 floors all have none):
- start's rune panel, a small crate (`vr_test_spawn 107`) and an AD-stand-in gun (`312`) on it, views standing looking
  down, crouched (head 1.0 m, pitch 10) and from the side, planes 0 / 0.5 / 1, at depth 2.5 and 6. At 2.5 the three look
  alike; props read as resting at all three. At 6, 0.5 and 1 (the same here: the panel's average caps both) raise the
  panel a little, its relief hides the gun's lower edge and nicks the crate's bottom edge with small notches where the
  raised runes stand in front of it (in the crate's shadow they read more like chips than like sitting in relief). The
  screenshots don't favour another value: the default stays 0. Without the cap (first try) 1 sank the crate visibly.
- Seams: e1m1 walls and corners at 0 and 1 (polygon edges, texture changes): no seams or gaps; continuous math per
  pixel, the same shift length as before, so no new swimming.
- Cost (`--exclusive`, eyes 3406 square, GPU world+brush p50, 3 alternating rounds of 240 frames in one process):
  e1m1 corridor 1.795 / 1.798 / 1.796 ms at 0 / 0.5 / 1; start's rune panel 2.135 / 2.144 / 2.127 ms: within noise
  (one extra smallest-mip read per parallax pixel only when the plane is over 0).
- e1m1 smoke: no errors.

Not done: a per-texture plane (each texture's own average as its level, "auto") would be the same as 1 here. Unrelated:
the flat-height floors (alpha 255 everywhere) still get `in_pdepth` > 0 and walk the ray for nothing at every plane
(a cost since round 21, small).

In VR: Graphics > Surfaces > Parallax Surface Level at 0, 0.5 and 1 with a crate and a dropped gun on a carved floor
(start's rune panels, or a custom map's carved ground): which reads as resting, and do the raised parts' edges against
the props look right or chipped?
## One throw for props and weapons (worktree `tghost`, branch `agent/throwunify`, 2026-10-16)

R2 review #13 (weapons.qc `DropWeaponInHandScaled` against vr_carry.qc `VR_Carry_Release`), his decisions: one
two-handed window for both, 0.2 s; props get the weapons' release-lag back-projection and throw assist (the same
settings); one share of the thrower's velocity for both, 0.9, no weight scaling. The shared pieces live together in
vr_carry.qc ("One throw"): `VR_Throw_2hRecent` (the window), `VR_Throw_BackProject` (where it would be by now: the
weapon's old code, plus the thing's offset from the point the hand's estimate follows (the hand; a prop held in both,
its centre) and, for a prop, its box traced against the walls: where it can't go it stays), `VR_Throw_Inherit`; with
`VRThrowAssist`, `VR_Throw_GravityScale`/`DmgSpeed` and `VR_Throw_Spin` as before. Each keeps its own: a weapon's
velocity (both hands' average times `vr_2h_throw_velocity_mult`, its weight), handle and spin axis; a prop's (the
engine's estimate from both hands, its Throw from its mass). The thrown-hit paths are untouched.

- `vr_throw_2h_window` (0.2) replaces `vr_carry_two_hands_window` (props 0.1, plus 0.05 on the server: no slack now)
  and the weapons' hard-coded 0.3. Used by both throws, the holster/pack guard, the ragdoll's two-handed throw, the
  no-strike rule and the engine's `held::bothHandsThrow`. A weapon's gap runs from the last frame it was aimed with both
  hands (`last_2h_time`: about 0.08 s after the foregrip's release in the mock), a prop's from the other hand letting go.
- `vr_throw_inherit` (0.9) replaces `vr_carry_throw_inherit` (1) and the weapons' full inheritance (`MakeThrown`, a
  player's throw; a monster's drop keeps all of it). Menu row "Throws Keep Your Motion" unchanged.
- Config 142: the old names are aliases (`cvarAliases`); at their old defaults (0.1, 1) they take 0.2 and 0.9 (his
  config holds the old defaults).
- Developer lines: `throw: a weapon|prop held in both hands until N s ago`, `carry: released N s ago: from ...`.

Verified headless (scratch/tu_test.sh: one synthetic main-hand throw, 6 m/s peak; a nailgun and a gib; base progs and
engine against these): weapon still -265.8 / z 16.1 both; weapon running (320 u/s) -585.6 -> -553.6 (inherit 1 ->
0.9); prop still z 16.9 -> 9.8 (back-projected 0.023 s, as the weapon); prop running 514 -> 482 u/s; assist (a grunt
150 ahead, cone 30): the prop z 9.9 -> 19.5 and turned (before: no change), at 300 ahead out of a gib's reach, only
turned. Two hands: prop gaps 0.153 s two-handed, 0.253 s one (before, 0.15 s was already one); weapon gaps 0.154 and
0.168 s two-handed, 0.252 s one (before: two-handed to 0.3). `flick_slowmo_compare.sh` as recorded above (overhand
x1.49, lob50 x1.23: engine estimates, untouched); `throw_trigger_test.sh` as before; `gib_2h_throw_test.sh` push, hard,
over: 0 bursts of 6, all held in both; melee eval 168/177.

To try in VR: a weapon and a brick thrown running (a little shorter: 0.9); a gib or brick thrown at a monster (bends
towards it as a weapon does); a crate let go of by one hand and thrown by the other within 0.2 s (two-handed), and a
weapon thrown just after letting go of its foregrip.
## C++ cleanup: R1's FIX-NOW batch, configs before 1.0.0 reset, grasp on the drawn mesh (worktree `backlogclean`, branch `agent/cppclean`, 2026-10-16)

The C++ review (R1) found uncontroversial fixes. The author also made decisions 3 and 9 (DECISIONS.md, 2026-10-16).

**FIX-NOW (behaviour-identical), one commit per item:**
- **F1:** ragtest's overrides are DeathInfo fields; it no longer writes cvars around `ragactive::beginDeath`.
- **F3:** one `geom::closestOnTriangle` (vr_geom.hpp). vr_anchor keeps its double-precision copy.
- **F4:** the shrink-corners helper is shared.
- **F5:** vr_b3glm.hpp.
- **F6:** `hands::basisOf`.
- **F7:** vr_crates uses vr_debris' helpers.
- **F8:** `grabStart`/`beginGrab`.
- **F9:** diskcache leftovers. AO's FNV seed is kept, so the files are unchanged.
- **F10:** `modelmeta::isRound()`.
- **F11:** vr_throw.cpp leftovers.
- **F12:** the throw Motion is built once; `writeHandFields`; `throwEstimateFor`.
- **F13:** one `surfaceOnSegment` and one `motion::split`.
- **F14:** `weaponFingers`, `beginFree`, `bothFrames`, handOn's locals.
- **F15:** `files::timestamp/profileFile/baseCacheDir`. The base cache dir is now guarded everywhere.
- **F16:** one copy of the death seed.
- **F17:** one quantile, `flickOf`; the redundant guards are dropped.
- **F18:** stale comments.

Checked before and after under `vr_fixed_frames 1`: physics hashes, ragdoll lists, grabs and throws match line for line (339 lines). Cache files are read, not rebuilt. Melee eval 168/177.

Not identical: `aimTurn`, a debug field, is now exactly 0 for the very same samples, where acos noise used to give about 0.02 degrees.

**Configs before 1.0.0 (decision 3).**
- A config with vr_cfg_version below 122 (1.0.0's version, from the v1.0.0 tag) is no longer migrated. `resetOldConfig` resets every archived `vr_` setting to its default and prints one console line ("this config is older than 1.0.0 ... reset to the defaults"). It also sets vr_tutorial_started to 1.
  - Kept: the per-player settings (`personal()`: body measurements, runtime, tips seen, bindings version) and the engine's own settings.
- **vr_cvars.cpp:** removed the 486 defaultChanges rows and the 15 hand-written steps before 122. The 1.0.0 Loading-notice step is now the row `{123, vr_loading_notice, "0"}`.
- **vr_weapons.cpp:**
  - Steps 1-40 removed; 41-43 are a `{version, fn}` table.
  - markCurrent only sets the version (F2: its copies of steps 22 and 23 went with them).
  - Removed the round-21 hotspot and cup migrations: legacyHotspots, migrateCups and cupAnglesFor in vr_view.cpp.
- **vr_props.cpp:** its whole `migrate` (461 lines, all before 1.0.0's 68) now only takes the version.
- **Other files:** removed body::migrateHolsters, retro::migrateShippedLook/migrateAllPanel and bodycal's migrateUndo. bodycal::migrate stays: it also maps the old arm cvar names typed in the console.
- Check: full config fixtures at 141, 126 (judo, knockdown and hand-stick rows at their old values, weapons at 40 with an anchor index and the toolgun's old muzzle) and 122 (Loading notice off, props at 50). Each goes through `vr_migrate_config` plus a map load, on the old and the new build.
  - All three dumps are identical: 14,397 settings, 0 differ.
  - A config at 100 is reset, with the note: its vr_gunangle 55 goes back to 70, while its dominant eye and tutorial flag are kept.

**The agents' test configs were from before 1.0.0.** Every `quakevr/ironwail.cfg.baseline` in qvr-agents is at vr_cfg_version 34 (weapons at 21, props at 26). With this change those configs are reset at startup instead of migrated. The settings the baseline had changed from the defaults would differ in tests.
- `scratch/mig/ironwail.cfg.baseline.migrated` in this worktree is the same baseline as the old build left it after startup and a map load (141/43/68). Its runs start exactly as before.
- This worktree uses it already. The kit's source config (new_agent.sh's REPO) and the other agents' baselines should be replaced with it.

**Grasp and held on the drawn mesh (decision 9).**
- held's `drawnTriangles`, `modelVertices`, `modelTriangles` and `magazinePoints`, and grasp's alias shapes, read `mdldequant::poseVertex` (the vertices as drawn with vr_mdl_dequant) instead of the stored bytes.
- New `mdldequant::prepare(model)` computes a model's dequantised offsets before its first upload, as GLMesh_LoadVertexBuffer would. The held guns' shapes are prepared at map load, before the client uploads the models.
- Grabs, prop hulls and gun pieces move by the dequantisation's fraction of a byte step, so physics hashes differ from before. The gun-pieces disk cache cut its pieces again once, because its key is a hash of the triangles.

For his VR check:
- A pre-1.0.0 config is reset: nothing to see with his config, which is at 140.
- Grab guns and props as usual; the grips should feel the same.
## Melee arc: Dissolution of Eternity's monsters, the ogre's smash (worktree `dequant`, branch `agent/roguemelee`, 2026-10-16)

His request: audit Rogue's melee too. And a trace: `melee_arc_test.sh <agent> id 1` gave the ogre 6 hits after the
step-aside and the fiend 1, where "Melee arc" recorded 1 and 0.

The trace: not a regression. The shipped commit itself (`f89dd4365`, built and run here) gives the same 6 and 1: the
recorded numbers came from the branch before it was merged onto integ. The ogre picks its smash or its swing by
`random()` (`ogre_melee`), so any change in what draws random numbers first flips which one the test meets; the
recorded 1 was the swing (its sweep's first blow, 85 degrees off, inside the wide 89; then Quake's spin takes it away
from him). The smash had two real flaws, both from the first melee arc commit:
- `chainsaw` calls `ai_charge(10)` itself, so a smash frame charged twice and turned 8 degrees, not
  `vr_enemy_melee_turn`'s 4: the turn cap was per call. Now per frame (`VR_MeleeArc_TurnBegin` keeps the frame's first
  facing: `.vr_melee_turn_time`, `.vr_melee_turn_yaw0`), for every monster.
- The smash was a "wide" (sweeping) blow, so 89 degrees caught a player 101 degrees off after 3 frames of turning.
  The swing sweeps side to side and stays wide; the smash comes down ahead: now the normal arc, and its hit frames
  (smash6-11, and chainsaw's own step in them) step straight ahead without turning after a player
  (`VR_MeleeArc_ChargeAhead`: Quake's `ai_charge` with the arc off or against monsters). With only the first fix the
  smash still caught him with 2-3 blows at its end (turned 33 degrees back); with both, none.
The fiend's 1 is its second claw of the same attack (atta11) landing 61 degrees off 0.99 s after the step, just
inside the test's 1 s window, after it turned 40 degrees back to him: the design's "it comes after him and strikes
again", left as is.

Rogue audit (how each decides a hit; arc as id's: arc on the hit frame, reach grown by size, turn before striking):

| Monster (file) | Hit test before | Now |
|---|---|---|
| Phantom swordsman `monster_sword` (`rogue_invis_sw.qc`: `ai_charge` and `ai_melee`) | id's `ai_melee`, generic CheckAttack | already arc and launch gated (id's helpers); kind 1 |
| Overlord `monster_super_wrath` (`overlord_smash`, 3 attacks) | 100 + CanDamage | arc (reach grown); launch gated (generic CheckAttack) |
| Guardian `monster_morph` stab (`morph_stab2`: 90 to the eyes, else 3 lasers) | 90 | arc (reach grown): beside or behind it the stab misses (no point-blank lasers); launch gated |
| Guardian knockback smack (`morph_smack`) | 100 + CanDamage | arc (reach grown) |
| Guardian attack frames (37 `ai_face`, full turn speed) | its full yaw speed | `morph_mface`: turn held to Swing Turn, melee time set (melee space) |
| Electric eel zap (`eel_melee`, `T_EELZap`: a burst all round it in the water) | radius 85 | unchanged: no blow, no facing (launch gated by the generic CheckAttack) |
| Dragon tail (`dragon_tail`, 250) | meant for behind it | unchanged on purpose; `dragon_tail_touch` (150) is called by nothing (only the parry test) |
| Lava man (`th_melee` = its fireball), wrath, mummy (thrown flesh), statues (id's knights) | missiles / id's | - |

No kind multipliers added: none of them is a light thrust like the knight's (the swordsman's sword blow is id's
`ai_melee` at a grunt's size; left 1 to keep the list short). Show Melee Arcs skips the eel and the lava man (a
`th_melee` that is no blow). The test (`melee_arc_test.sh <agent> rogue`, vrtesthall: swordsman 15, overlord 17,
Guardian 18; the swordsman's 10 s wake delay zeroed by the test) and the id set, arc 1 (arc 0 -> 1 for Rogue):

| Monster | facing him: hits, first | its back to him: first (angle) | step aside: hits after |
|---|---|---|---|
| phantom swordsman | 12, 0.5 -> 12, 0.5 | 0.5 (80) -> 1.2 (25) | 3 -> 0 |
| overlord | 5, 0.6 -> 4, 0.7 | 0.7 (100) -> 2.1 (13) | 2 -> 1 (1.0 s after, turned 40 back) |
| Guardian | 4, 0.9 -> 4, 0.9 | 0.9 -> 1.7 (0) | 1 -> 0 or 1 (its knockback at frame 9, 36 degrees back) |
| ogre (id) | 17, 0.7 | 1.3 (26) | 0 smash (6 before), 1 swing (unchanged) |
| knight, hell knight, fiend, dog (id) | as before | as before | 0, 0, 1 (above), 0 |

The Guardian's hits counted beyond its blows are its lasers once a smack pushed him off. Melee eval 168/177 (the 3
rows that differ from the baseline are bash damage 6 -> 3, from earlier: verdicts the same).

In VR (Dissolution of Eternity): sidestep a Guardian's stab and an overlord's smash as they wind up: they should miss
and turn after you; an ogre's overhead smash should come down where it faced.

## A/B switches removed (worktree `backlogclean`, branch `agent/abremove`, 2026-10-16)

His decision on the review: remove the dead code paths and the cvars the A/B switches leave obsolete. Each switch is
kept at its default (his value), the other side deleted, the cvar listed in `retiredCvars` (an old config's line is
dropped quietly), its menu row, QC handle and test cases gone. One commit each. No default changed: config version
unchanged.

| Retired | Kept | Deleted |
|---|---|---|
| `vr_hull_method` | 1, the compiled clipping hull | the brush sweep as the moves' path (clipBSP, the crouch fit test), its menu row (Movement > Player Hitbox > Method) and the world tree's "method 1 only" build gate. `boxTrace` (the sweep) stays: the portals' clipping and `vr_hull_bench`'s reference use it |
| `vr_prop_think_native` | the engine's think (vr_propthink.cpp) | QC `forcegrabbable_think_qc`, `wpnthrow_stabilize`, `wpnthrow_impl_stabilize` (weapons.qc, 180 lines); `vr_propthink_check` and its Debug row; `srvrandom::stream`/`setStream` (only the check used them). A settling angle past 65536 (or infinite), which the native left to the QuakeC's loop, is brought into a turn with `fmodf` (infinite: NaN, set straight as a NaN was) |
| `vr_throw_mass_model` (+ `vr_weight_throw_mass`, read only with it 0) | 1, throws by weight | the pre-2026-10-02 path (weight.cpp's 3 gates, props.cpp's mass-scaled throw), the menu toggle, `vr_throw_table`'s "off" column |
| `vr_carry_grab_drawn` | 1, explosive boxes by the fist on their drawn surface | its gate (the box touch stays for hands without the fist push and other players' hands) |
| `vr_weapon_grab_box` | 0, the fist only | the 5-unit-box catch; `weapon_catch_test.sh` check 2 |
| `vr_box3d_caller_solves` | off | the main-thread-solves experiment (enqueue/finish hooks), its Debug row |
| `vr_weapon_throw_mode` | 0, a thrown weapon is a prop to pick up | Vanish on Hit and Discard, their enum, the menu row. The disappear-on-hit prop flag (`.lip`, `CreateThrownWeapon`'s `xDisappearOnHit`, ~20 callers all passing FALSE) is left: its removal touches 15 files |
| `vr_button_weapon` | 1, the whole held weapon presses buttons | the switch (the engine's muzzle line never read it) |
| `vr_parry_interrupt` | 1, a parry cancels and staggers | the switch; `parryinterrupt/test.py` keeps the enabled cases |
| `vr_decap_pop_chance` | on: pops by chance | a toggle whose off made every headshot kill pop (an early return in each chance): nothing of it is left, so retired. `ad_shots_test.ps1` set it 0 for sure pops: now `vr_decap_pop_always_range 1000` |

Kept as settings:
- `vr_bash_sound`: 0 is now silent (a volume, as the slider reads), not the old axhit/tink/thud sounds (their own
  branch, removed). The default 0.7 is unchanged.
- `vr_limbs_mass_scale`: unchanged. Its 0 is not a separate path: it joins the fallback for a piece with no rig (its
  model's volume of flesh), which stays.

Tests (at the defaults): build and the statics checks clean; `vr_menu_path_check` 6 and 14 found, 0 missing;
`hullhit/walk_test.sh` e1m1 and e2m2 30 s: stuck 0, embedded 0, outside 0; `weapon_catch_test.sh` 4/4 PASS;
`parryinterrupt/test.py` (forced, animations, timing, dragon) all PASS; `flick_slowmo_compare.sh` as recorded (lob50
x1.23, overhand x1.49, overhand0 slowly 0.88); two crowbars dropped into vrtesthall's pool float and settle, no error;
a held sword's button line printed each frame (`vr_debug_wallbuttons 2`); melee eval 168/177, only the known bash
damage diffs (6 -> 3, 10 -> 5).

For his VR check: nothing should feel different. Walk along walls (hitbox), throw a heavy prop and a gun, drop a gun
in water, catch a tossed weapon, press a wall button with a sword, parry a knight.

### Debug > Automation & Self-Tests (tghost)

The Debug menu gets a page for what is mostly for headless agent runs: **Debug > Automation & Self-Tests** (`vr_menu_automation.inc`, page 186, developer level; the Debug page lists it after Tests). 286 rows moved textually from Debug > Tests (human-oriented rows stay, now in sensible groups: Doom, Arcane Dimensions, Monsters in the Map, Spawn and Behaviour Pages, then the existing sections), Reports, Tools and Profiling and Memory; five sub-pages (Stealth AI Tests, Machine Horde Tests, Dawn of the Machine Tests, Shub, Chthon) moved with them (their parent is now the new page). Nothing was renamed, so no board path changed: `vr_menu_path_check` says 0 missing on vrcalibration (6 paths) and vrtesthall (14); `menu_panels_test.py` passes. Page numbers 64, 66-72 are unchanged. Veto any move: cut the row back into its old page (git history has the original place).

**Automation (moved).** Group on the new page -> rows, with the reason.

- **Official Campaigns and Dawn of the Machine** (acceptance/destructive checks for the official and Machine campaigns (reload afterward); reports pair with them): Official Triggers: Acceptance Test; Official Triggers: Retarget Teleporter; Official Triggers: Lightning Damage; Machine: Hub and Rune Acceptance; Machine: Electrode and Rune Egg; Machine: Equipment Carry Setup; Machine: Progression Report; Machine: Walk Into the Next Exit; Machine: Die Here; Machine: mge5m2 Trigger Route; Official World: Fog Report; Official World: Environment Test; Official Monsters: Activation Test; Official Campaign: Progress Report; Dopa: Ordinary Health Pickup; Dopa: Megahealth Carry Setup
- **Arcane Dimensions** (scripted AD test modes (vr_ad_test N), hit tests, ladder probes and their reports): Arcane Dimensions: Play a Cinematic Camera; Arcane Dimensions: Cinematic Report; Arcane Dimensions: Upgrades Report; Arcane Dimensions: Artifacts Test; Arcane Dimensions: Nail Piercer Test; Arcane Dimensions: Nail Piercer Result; Arcane Dimensions: Burning, Poison, Bleeding; Arcane Dimensions: Debuffs Report; Arcane Dimensions: Shotgun Hit Test; Arcane Dimensions: Super Shotgun Hit Test; Arcane Dimensions: Burst Rifle Hit Test; Arcane Dimensions: Ammo Model Test; Arcane Dimensions: Ammo Resistance Test; Arcane Dimensions: Evil Skill Report; Arcane Dimensions: Buttons Test; Arcane Dimensions: Backpacks Test; Arcane Dimensions: Ladder Movement Test; Arcane Dimensions: Flat Ladder Test; Arcane Dimensions: Ladder Rung Probes; Arcane Dimensions: Ladders Report; Arcane Dimensions: Spawnflags and Sight Report
- **Scripted Test Pages** (sub-pages of scripted scenes and acceptance checks (stealth scenes, horde waves, DotM state/Shub/Chthon checks)): Stealth AI; Machine Horde Tests; Dawn of the Machine Tests; Dawn of the Machine: Shub; Dawn of the Machine: Chthon
- **Spatial Audio** (offline PASS/FAIL renders, benchmark, recordings and dumps to files): Spatial Audio Tests; Spatial Audio Benchmark (12 s); Record the Mix (2 s); Record the Game-Time Mix (2 s); Save a Sound's Two Copies; Save the Sound Scene
- **Parry** (mock-crowbar sequence, pose check and same-callback checks (vr_mock_hand scripts)): Dragon Parry (Mock Crowbar); Dragon Tail Swing (Pose Check); Same-Frame Parry Hits
- **Reloading Scenarios and Self-Test** (scripted round/gun scenarios, boundary checks, self-test and shape dumps (vr_reload_test N)): Magazine Reinsertion Boundaries; Run the Self-Test; Toss a Round Into the Off Hand's Gun; Lay a Round Sideways at Its Opening; Lay a Round Under Its Load Point; Drop a Round From Above Its Load Point; Toss a Round in the Wrong Way Round; Hold the Round at the Load Point; Hold It There the Wrong Way Round; Break the Off Hand's Super Shotgun Open; Empty and Break Open the Main Hand's Gun; Spent Lava Nails or Plasma in the Off Hand's Gun; Toss a Round Into the Lying Gun; Drop a Round Onto the Lying Gun; Toss One Sideways at the Lying Gun; Hold a Round at the Lying Gun; The Lying Gun's Magazine Is Solid; The Held Prop's Shape Against Its Box; Print the Collision Shapes
- **Weapon Checks** (scripted grab, damage-credit sweep over every weapon, test-aid toggle): Take the Nearest Weapon Pickup; Damage Credit of Every Weapon; Load Stuck
- **Player Hitbox** (hull keep/approach/bench/leaf/probe, shots-hit tests and the random walk): Hitbox Keep Test; Hitbox Approach; Hitbox Bench; Shots Hit Test; Hitbox Leaf; Hitbox Probe; Shots Hit Test; Random Walk (60 s)
- **Teleporters** (scripted `map start; wait..; setpos` camera and crossing tests): Looking Through A Gate, Or Why Not; Torch Light Views; The View Through A Gate, Read Back; Against A Gate's Frame; In The Opening, Short Of The Plane; Lean Into A Gate; Through A Gate; Frame Strip Through A Gate; A Shot Through A Gate; The Whole Feature Off; Slide A Crate Through; Quake's Effects Behind A Gate; Hidden Staircase Probe
- **Menus and Dialogs** (menu/dialog test routines: dialog previews, text dump, board path check, help fit): New Game Confirmation (3 s); Confirmation Buttons; List the Standard Menu's Texts [Tools]; Check the Boards' Menu Paths [Tools]; Menu Help Fit [Reports]
- **Feature Tests and Benchmarks** (feature test modes (tap feedback, trails, companion maps, input latency), AO benchmark, fake update notices): Screen Tap Feedback [Reports]; Distortion Trails Test [Reports]; Companion Map Tests [Reports]; Input Latency: Walk [Reports]; AO Bake Benchmark [Reports]; Update Notice: Fake 9.9.9 [Reports]; Update Notice: Real Version [Reports]
- **Toolgun and Boxes** (spawn-every-entry test, carry load test, boxes self-test): Spawn Every Toolgun Entry [Tools]; Check and Clear the Spawn Test [Tools]; Carry Load Test [Tools]; Boxes' Self-Test [Tools]
- **Small Gibs Tests (developer 1 for each hit)** (vr_*_test N scenario section (setup, hit, report rows) (from Debug > Tools: `vr_*_test`/`vr_*_stress` scripted scenes on the nearest monster or corpse)): A Grunt Ahead [Tools]; A Grunt's Corpse Ahead [Tools]; On the Training Dummy [Tools]; A Grunt's Ragdoll Ahead [Tools]; Blast Beside the Nearest Ragdoll [Tools]; Ragdoll and Prop Drawn Motion [Tools]; Drop the Nearest Prop on the Nearest Corpse [Tools]; Gib Props: Gib the Nearest [Tools]; Gib Props: Kill the Nearest Whole [Tools]; Gib Props: Gib the Nearest Corpse [Tools]; Gib Props: Grab Its Head [Tools]; Gib Props: Let Go of It [Tools]; Gib Props: Shoot Its Head [Tools]; Shotgun Blasts [Tools]; Super Shotgun Blasts [Tools]; Nails [Tools]; Axe, Pommel, Sword, Punch [Tools]; Shove, Bash, Slap: No Blood [Tools]; Quad Damage [Tools]; Chainsaw a Second [Tools]; Burst a Gib and a Head [Tools]; Brain Chunks Ahead [Tools]; Most Lying About [Tools]; Held, Then Let Go [Tools]; Pass Through the Body [Tools]; Throw Gibs at a Wall [Tools]; Thrown Gibs Stick, by Mass [Tools]; Step Up to the Wall Ahead [Tools]; Flight by Situation [Tools]; Blow Up a Crowd [Tools]; List Small Gibs [Tools]; Gib a Body Underfoot: a Blow [Tools]; Gib a Body Underfoot: a Shot [Tools]; Kill a Monster Underfoot [Tools]; Drop a Monster's Backpack on You [Tools]; Throw a Gib Made 1 s Ago [Tools]; Trace Small Gibs [Tools]
- **Training Dummy Tests (dummytest: ...)** (vr_*_test N scenario section (setup, hit, report rows) (from Debug > Tools: `vr_*_test`/`vr_*_stress` scripted scenes on the nearest monster or corpse)): What the Dummy Is [Tools]; Hit the Dummy for 10 [Tools]; Hit the Dummy for Its Health [Tools]; The Dummy's Health [Tools]
- **Decapitation Tests (developer 1: decap: ...)** (vr_*_test N scenario section (setup, hit, report rows) (from Debug > Tools: `vr_*_test`/`vr_*_stress` scripted scenes on the nearest monster or corpse)): A Zombie Ahead [Tools]; A Mummy Ahead [Tools]; Slash at Its Head [Tools]; Axe Slash at Its Head [Tools]; Axe Swept Through Its Neck [Tools]; Stab at Its Head [Tools]; Pommel at Its Head [Tools]; Slow Slash at Its Head [Tools]; Slash at Full Health [Tools]; Slash a Corpse's Head [Tools]; Chainsaw at Its Neck [Tools]; Chainsaw a Corpse's Neck [Tools]; Throw an Axe at Its Head [Tools]; Gib the Headless Corpse [Tools]; Shotgun at Its Head [Tools]; Super Shotgun at Its Head [Tools]; Super Shotgun at Its Head, Overkill [Tools]; Lightning at Its Head [Tools]; Shotgun at Its Body [Tools]; Shotgun at Its Head, Not Killing [Tools]; Lightning at Its Body [Tools]; Sweep the Head Zone [Tools]
- **Limb Gore Tests (limbtest: ... in the console; developer 1: limbs: ...)** (vr_*_test N scenario section (setup, hit, report rows) (from Debug > Tools: `vr_*_test`/`vr_*_stress` scripted scenes on the nearest monster or corpse)): A Grunt Ahead [Tools]; A Grunt's Corpse Ahead [Tools]; Spawn Its Limbs [Tools]; List Its Limb Models [Tools]; Cut a Corpse Apart, Ends First [Tools]; Cut a Corpse Apart, Whole Limbs [Tools]; Slash at a Limb [Tools]; Punch a Limb [Tools]; Shotgun at a Limb [Tools]; Lightning at a Limb [Tools]; Explosion by a Limb [Tools]; Gib It [Tools]; Slash at Full Health [Tools]; Chance Rates [Tools]; Most Limbs [Tools]; Where Its Limbs Map [Tools]; The Limbs Lying About [Tools]; Cut Ends' Fountains [Tools]; Limb Weights Here [Tools]; Enemy Laser at the Head [Tools]; Enemy Laser at a Limb [Tools]; Repeated Blunt Hits on a Corpse Limb [Tools]; Repeated Blunt Hits on a Corpse Torso [Tools]; One Blunt Hit on a Corpse Limb [Tools]; Stored Limb Damage [Tools]; Wear a Corpse Limb Off: Shotgun [Tools]; Wear a Corpse Limb Off: Super Shotgun [Tools]; Wear a Corpse Limb Off: Lightning [Tools]; Wear a Corpse Limb Off: Nails [Tools]; Wear a Corpse Limb Off: Rifle Laser [Tools]; Wear a Corpse Limb Off: AD Pellets [Tools]; Wear a Corpse Limb Off: Axe [Tools]; Shotgun a Corpse Torso [Tools]; Wear a Corpse Head Off: Shotgun [Tools]; Held Prop at a Corpse Limb [Tools]; Axe at a Corpse Limb [Tools]; Hand to the Last Limb [Tools]
- **Crowd Gore Tests (goretest: crowd ... in the console; the benchmarks' gore scenarios)** (vr_*_test N scenario section (setup, hit, report rows) (from Debug > Tools: `vr_*_test`/`vr_*_stress` scripted scenes on the nearest monster or corpse)): Crowd Radius [Tools]; Spawn a Crowd (16 Grunts) [Tools]; Crowd: Slash a Limb [Tools]; Crowd: Cut Corpses Apart [Tools]; Crowd: Gib [Tools]; Crowd: Head Pops [Tools]
- **Head Pop Chance Tests (poptest: ... in the console)** (vr_*_test N scenario section (setup, hit, report rows) (from Debug > Tools: `vr_*_test`/`vr_*_stress` scripted scenes on the nearest monster or corpse)): Test Range [Tools]; Chance Table [Tools]; Shotgun Headshot Kill [Tools]; Super Shotgun Headshot Kill [Tools]; Lightning Headshot Kill [Tools]; Super Shotgun Body Kill [Tools]; Shotgun Rates [Tools]; Super Shotgun Rates [Tools]; Throw a Weapon at Its Head [Tools]; Throw an Explosive Box at Its Head [Tools]; Blunt Melee Rates [Tools]; Hard Punch Kill [Tools]; Hard Gun Butt Kill [Tools]; Hard Crowbar Kill [Tools]; Hard Pommel Kill [Tools]; Hard Club Kill [Tools]; Hard Mjolnir Kill [Tools]; Nail at Its Head [Tools]; Rocket at Its Head [Tools]; Grenade at Its Head [Tools]; Give Quad Damage [Tools]
- **Burning Tests (developer 1: burning: ...)** (vr_*_test N scenario section (setup, hit, report rows) (from Debug > Tools: `vr_*_test`/`vr_*_stress` scripted scenes on the nearest monster or corpse)): Set It on Fire (a Torch's Blow) [Tools]; Set It on Fire (a Lava Nail) [Tools]; Set It on Fire (a Touch) [Tools]; Load Lava Nails [Tools]; A Nail Through a Torch's Flame [Tools]; Smash the Nearest Crate [Tools]; Burn the Nearest Crate Through [Tools]; A Lava Nail at the Nearest Piece [Tools]; Count the Pieces [Tools]; How It Burns [Tools]
- **Incandescent Blades (developer 1: hotblade: ...)** (vr_*_test N scenario section (setup, hit, report rows) (from Debug > Tools: `vr_*_test`/`vr_*_stress` scripted scenes on the nearest monster or corpse)): Heat the Held Blades [Tools]; Into Their Fade [Tools]; Half Heated (Held There) [Tools]; Quench Them [Tools]; Strike the Nearest Monster or Corpse [Tools]; Report [Tools]; Blade into the Nearest Flame [Tools]; Blade into the Nearest Lava [Tools]; Blade into the Nearest Water [Tools]; Take the Nearest Wall Torch (Off Hand) [Tools]; Report What the Client Draws [Tools]; Report the Lying Blades [Tools]; Throw It at the Nearest Monster [Tools]; Drop It Here [Tools]; Off Hand's Torch to the Blade [Tools]
- **Wall Torches Shot Off Their Walls (developer 1: walltorch: ...)** (vr_*_test N scenario section (setup, hit, report rows) (from Debug > Tools: `vr_*_test`/`vr_*_stress` scripted scenes on the nearest monster or corpse)): Shoot the Nearest Wall Torch (a Pellet) [Tools]; Shoot It (a Nail) [Tools]; Shoot It (a Rocket) [Tools]; Shoot It (a Grenade) [Tools]; Shoot It (a Lightning Bolt) [Tools]; A Blast at It [Tools]; Throw the Nearest Prop at It [Tools]; Torch Flames to Console [Tools]
- **Self-Tests and Benchmarks** (pool/heap/Zancle/SHA self-tests, benches, no-GL-context startup test): Tracy Pool Self-Test [Profiling]; QuakeC Loop Bench [Profiling]; Thread Pool Split Bench [Profiling]; Thread Pool Self-Test [Profiling]; Zancle Math Self-Test [Profiling]; SHA-256 Self-Test [Profiling]; Grasp Bench [Profiling]; Heap Self-Test [Profiling]; Test: No GL Context at the Next VR Start [Profiling]
- **Crashes** (deliberate crashes and errors to test the crash report): Crash: Access Violation [Profiling]; Crash: On a Worker Thread [Profiling]; Crash: Stack Overflow [Profiling]; Crash: Fatal Error [Profiling]; Crash: Failed Assert [Profiling]; Crash: abort() [Profiling]; Error: Host_Error [Profiling]

**Human-oriented (stay).** Reason: used in the headset while playing or tuning, or a diagnostic/report he reads.

- Tests: Doom Info/Show Touches/Debug/Restart: Doom diagnostics and a restart, used while playing Doom.
- Tests: Arcane Dimensions Play, Widowmaker/Artifacts Ahead, Screen Tint Report, Status: start AD, spawn pickups to walk into, tint/status reports.
- Tests: Whole-Map Monsters (Report, Bring In, Wake All, Kill All), Hit Probe: tools for a map while playing.
- Tests sub-pages: Ragdoll Behaviours, Spawn Pickup Weapons, DotM Bestiary/Weapons/Monsters, AD Monsters: spawn-ahead pages; DotM and AD pages still hold a few Check/Test rows beside their spawn rows (not split, say if you want them moved).
- Tests: Physics Stress, Spatial Audio (info, 32 sounds, directional and 22 kHz sounds, five explosions): perf feel and listening checks in the headset.
- Tests: Ahead of You (spawn, facing, corpse/ragdoll, Put It There, ragdoll rows, crate cover, crouch): the test-monster spawner.
- Tests: Parry (pose check, Dragon Parry Sequence, A Melee Blow Now), Holding Enemies, Enemy Shoves: melee/hold/shove diagnostics and actions he drives by hand (Snap the Nearest Neck, Why Can't I Knock It Down?, ...).
- Tests: Chainsaw, Weapon Instances, Reloading (off-hand guns, shells, reports, Show Load Points): hand-handling tuning; Chainsaw Fit and Hotspot Fit are fit reports.
- Tests: Tutorial, Trailer Scene, Climbing, Weapon Pickups (ahead/dropped/lying/floating), Precache rows, Saves, Crowbar, Enemy Guns, Flung Props, Thrown Axe, At You, Getting Hit, Grappling Hook reports, Stamina, Loading Notice, Stuck in Walls: spawn/throw/hit aids and diagnostics for playing.
- Tests: Player/Monster Hitbox settings, Hitbox Stats, Hitbox cache cycles; Teleporters (gates info, test map jumps, Print Gate Cuts, Lightning Gun, wall unstick); Cheats: settings and test-map shortcuts.
- Reports (except the 8 moved), Views, Logging, Cheats and Recording, Checklist: reports and toggles he reads.
- Tools: Blood Feedback, Rebuild and Reload, Save to Files, External Map Index, Test Effects, VR Calibration, Toolgun/Brush tools, Boxes, Spawn Pickup Weapons: previews, reloads, dumps and builders used by hand.
- Profiling and Memory: panel, capture, benchmark capture, hitch log, memory log, allocation sites: profiling he runs while playing.

Not done: TESTING.md/BENCHMARKS.md/archived rounds still say "Debug > Tests > ..." for moved rows (Spatial Audio Tests/Benchmark, Stealth, DotM Tests, Tools test sections now live under Automation & Self-Tests).
## Leg sweep: a staff through a humanoid's shins, a judo foot sweep (worktree `fastship`, branch `agent/legsweep`, 2026-10-16)

His request: a horizontal sweep at a humanoid's legs with something long and blunt enough to be used as a staff (the
shotgun, the laser cannon, a sword's hilt held backwards, the crowbar, a long piece of wood, the grenade and rocket
launchers) throws it down as a judo foot sweep: its feet slide the way the sweep goes, the rest of it topples after.
Short things never (the lightning gun, the axe, a sword's blade, the super shotgun, rocks, bricks, crates). No list of
weapons: rules from their shapes. Then: from its side too, toppling forward or backward as the sweep goes.

**The staff rule** (`vr_legsweep.cpp`, `vr_legsweep_table` prints it): every held thing is measured once from its drawn
shape.
- A weapon: its model's vertices (zero pose, the weapon scaling in) along the line from its grip (the entity's origin, as
  the hand holds it) to its melee tip (the muzzle anchor: the QC melee's line). **Length**: their extent along it.
  **Cross-section**: the median of its 8 slices' (each slice's larger extent across the line). The thickest slice was the
  first try: every gun's is its grip and trigger guard, about 20 cm, so the shotgun (2.58), the grenade launcher (2.58),
  the super shotgun (2.61) and the lightning gun (2.22) came out alike; the median is the barrel (shotgun 10 cm, grenade
  launcher 12, lightning gun 20: its body is fat all along). **Slenderness**: length over that.
- A prop in a hand (`mainhand_held` / `offhand_held`): its drawn box in its own axes (`held::modelBox`, its networked
  scale), the length its largest extent, the cross-section its next. Nothing on a prop is sharp.
- **Blunt where it meets the legs**: a weapon with a blade is sharp from the QC layout's edge on (`vr_melee.qc`
  `VR_Melee_PartAt`, the hot blades' share): the axe's head from 0.6 of the grip-to-tip way, a sword's blade from 0.2 (past
  the guard), the chainsaw's bar from 0.45. The crowbar's bar is blunt (no special case: it has none). Of the thing's
  points that met the legs this frame the lowest decides (what takes the feet: a sword held hilt down sweeps with its
  pommel, the blade's root above it), and on a blade it is a cut, never a sweep.
- Thresholds: `vr_legsweep_min_length` 50 cm, `vr_legsweep_min_slender` 4 (both archived; Combat > Knockdowns > Leg
  Sweep). The super shotgun is 49.1 cm long and the shotgun 51.5 (the smallest margin in his examples: 2.4 cm between
  them); the lightning gun is long enough (51.6) and too fat (2.6).

`vr_legsweep_table` (vrfiringrange; cm; cross: the typical, thick: the thickest slice; reach: grip to tip):

| model | length | cross | thick | reach | slender | blade from | verdict |
|---|---|---|---|---|---|---|---|
| v_shot (shotgun) | 51.5 | 10 | 20.0 | 44.5 | 5.0 | - | staff |
| v_shot2 (super shotgun) | 49.1 | 11 | 18.8 | 36.1 | 4.4 | - | no (length) |
| v_rock (grenade launcher) | 53.3 | 12 | 20.7 | 38.9 | 4.3 | - | staff |
| v_rock2 (rocket launcher) | 75.2 | 13 | 22.3 | 59.2 | 5.8 | - | staff |
| v_laserg (laser cannon) | 133.2 | 20 | 38.6 | 119.4 | 6.7 | - | staff |
| v_light (lightning gun) | 51.6 | 20 | 23.3 | 37.3 | 2.6 | - | no (slenderness) |
| v_nail / v_nail2 | 47.0 / 58.5 | 18 / 19.5 | 21 | 34 / 47 | 2.5 / 3.0 | - | no |
| v_crowbar | 62.3 | 3 | 16.0 | 35.0 | 21.9 | - | staff |
| v_ksword / v_hksword | 121.0 / 144.7 | 6 | 27 / 35 | 89 / 113 | 19.6 / 24 | 0.41 / 0.38 of its line | staff, its blunt part only |
| v_axe | 47.6 | 5 | 32.8 | 27.1 | 10 | 0.64 | no (length) |
| v_hammer (Mjolnir) | 49.2 | 18 | 32.8 | 28.0 | 2.7 | - | no |
| v_chainsaw | 90.8 | 29 | 40.4 | 80.4 | 3.1 | 0.51 | no |
| vr_plank1 / vr_plank4 | 95.2 / 73.4 | 20.6 / 12.1 | | | 4.6 / 6.1 | - | staff |
| vr_plank2 / vr_plank3 | 60.3 / 38.8 | 20.6 / 12.7 | | | 2.9 / 3.1 | - | no (boards, short) |
| vrtorch | 51.3 | 14.9 | | | 3.4 | - | no |
| vr_crate1 / vr_crate2 | 101.6 / 152.4 | 101.6 / 127 | | | 1.0 / 1.2 | - | no |
| vr_brick1 / vr_rock1 | 19.8 / 12.5 | 9.4 | | | 2.1 / 1.3 | - | no |
| vr_barrel, gib1, h_guard, b_explob | | | | | 1.1 - 2.3 | - | no |

(The blade column is the share of the thing's line, end to end, where its sharp part starts: the QC layout's share of the
grip-to-tip way, moved onto the drawn ends.)

**The sweep** (each server frame, the local player's hands): the thing's line placed where it is now (a weapon's from the
hand to its muzzle as the server has them, QC `VR_Melee_Line`'s `handpos` and `muzzlepos`, stretched over its drawn
extent; carried off its handle, its drawn handle and tip in the hand, `VR_Melee_FreeLine`'s; a prop's along its box's long
axis at its entity), sampled at 9 points end to end, each point's move since the last frame swept (its thickness: half
the cross-section, at most 4 units) against each live humanoid's lower legs: the same humanoids as the lift and the neck
snap (`vr_foegrab_lift_classes`, AD's included; the training dummy shows "swept"), its drawn model (`vr_hit_precise`)
below 0.4 of its height, else its box there; a point already in them where it starts doesn't count (resting there,
moved through), nor one faster than 25 m/s (put there). That point must move level (its rise or fall at most 0.7 of its
level speed, about 35 degrees) at `vr_legsweep_speed` (1.5 m/s) or more, a heavy one more: times the cube root of its mass
(QC `VR_Grapple_MonsterMass`) over `vr_legsweep_heavy` (100 kg: a grunt is 80, unchanged). From any side.

**The fall** (`foegrab::sweepDown`, beside the throw's `throwDown`; QC `VR_LegSweep_Down` in the new `vr_legsweep.qc`):
the QC knocks it down as the two-hand throw does (`VR_Knockdown_Start`: its ragdoll and get-up, room, the pool; not
flying, swimming, or down already), marks it thrown by you (its impacts count: `vr_ragimpact`), deals
`vr_legsweep_damage` (5). The engine then turns its ragdoll over about its feet against the sweep at
`vr_legsweep_topple` (200 deg/s, its top's speed) with its feet swept along at `vr_legsweep_feet` (0.5) times the sweep's
speed (`box3d::ragdollTopple`, the throw's own sweep), its arms bracing (`ragdollThrown`), and the swept thing passing
through it for 0.4 s (`ragdollGrace`). So the feet always go the sweep's way and its top the other: in front of it, to a
side; from its side, along its facing, forward or backward (front to back: its feet go back, it falls forward).

**Settings**: Combat > Knockdowns > Leg Sweep (on, Least Length, Least Slenderness, Least Speed, Heavy Mass, Damage,
Topple Speed, Feet Slide; all archived). Debug > Tests > Holding Enemies: Log Leg Sweeps (`vr_legsweep_debug`: each leg
contact's measure and verdict, a sweep's fall traced for a second; 2 also every frame) and Leg Sweep Table. Test spawns
114 (a long plank) and 115 (a brick) for the props (`vr_impulse_test.qc`).

**Verified** (`Misc/quakevr/foegrab/legsweep_test.py fastship`, the mock's main hand swept through a grunt's shins, real
time; the logs `scratch/legsweep_<case>.txt`): swept down, feet along the sweep and the body over the other way: shotgun
left-to-right and right-to-left, rocket launcher, grenade launcher, laser cannon, crowbar, the long plank, the sword held
hilt down (its pommel, point 0.00 of its line); the shotgun from its side front-to-back fell forward. Not: the super
shotgun (not staff-like), the lightning gun (not staff-like), the sword's blade ("the blade: a cut"), the slow sweep (0.9
m/s: too slow). A trace (shotgun, left to right): feet 0.3, 3.3, 5.8, 8.0, 9.7 units along the sweep at 0.0-0.46 s while
the pelvis dropped 34 and stayed within 3 units along; its head ends 24 units against the sweep from its feet. The hand
under a heavy gun arrives at 2-3.5 m/s where a light melee weapon's arrives at 8-9 (Held Object Weights' spring): the
least speed is 1.5 m/s for that. Melee eval 168/177.

**Open** (out of budget): the side sweep back-to-front (shotgun) swept it down but it lay diagonally (19.8 against the
sweep, 20.3 across) with its feet sliding 12.6 units and flopping back as it landed: tune Topple Speed / Feet Slide in VR;
the axe, the crate and the brick never reached the legs in the test's pose (their verdicts by the table only: the axe 47.6
cm, the crate 1.0 slender, the brick 19.8 cm); a screenshot sequence of a topple from a side mock camera was not
captured (the camera moved the test spawn; `scratch/shots.sh` sketches it).

In VR: hold the shotgun or the crowbar low and sweep it level through a grunt's shins, from in front and from its side;
does it go down feet first the way you swept? Try the super shotgun and the axe (nothing), a sword by the blade swinging
the hilt. Log Leg Sweeps prints why one didn't.

## Leg sweep labels in the motion recorder (worktree `tghost`, branch `agent/sweeprec`, 2026-10-16)

His idea: the leg sweep gets recorder labels like the neck snap, so his real sweeps become a regression set.

- **Labels** (`vr_motion.cpp`, the data-driven category table): **Leg Sweep Expected** (`leg_sweep`; Detail shotgun, prop,
  crowbar, left_right, right_left, away, toward, hilt) and **Leg Sweep Not Expected** (`no_leg_sweep`; axe, slow, blade,
  not_level, short, super_shotgun, high), in the menu next to the neck snap's. `expect.cfg`: Expected needs a `legsweep`
  event, Not Expected none (blows on the way in allowed).
- **The event**: `vr_legsweep.cpp` `sweepHand` emits `legsweep` (sub `knocked_down`, `standing` for the training dummy) when
  `foegrab::sweepDown` took the sweep (result >= 1); this is the only edit to that file (a read-only hook: one `qcEvent` call
  and the include). The replay runs the game's own detector with its own thresholds.
- **The weapon**: in the take already (`m_wid`/`o_wid`); a prop held in a hand is now written in the header (`main held`,
  `off held`; `vr_legsweep*` settings join the header's melee settings line), but playback does not put a prop back in the
  hand: `eval.sh` lists such takes apart ("not replayed (prop)"), not counted. Open: replaying props (spawn the prop by model
  into the hand during the setup) once he records some.
- **eval.sh** (backup `kit/scratch/eval.sh.bak_sweeprec`): `LEG_KINDS="leg_sweep:legsweep"` is the one table: the melee sets
  leave those labels out, `legkinds_set` replays each listed kind once (`vr_motion_eval list`), printing expected caught /
  missed, false sweeps, then per weapon, then each take; verdicts in `eval_status.csv`. The leg pull / lift labels add their
  `<label>:<event>` to `LEG_KINDS`, their categories to `vr_motion.cpp` and `expect.cfg`, and nothing else. Reported only.
- **Synthetic takes** (`motion_synth.py legsweep`): `leg_sweep_shotgun`, `no_leg_sweep_axe`, `no_leg_sweep_slow`. Through
  eval.sh (`EVAL_TAKES=` that folder): `leg sweep: expected caught 1/1, missed 0, false 0/2`; per weapon shotgun 1/1 and 0/1
  false, axe 0/1 false. The real set: melee eval 168/177 (3 bash-damage differences from the baseline, as before), neck
  snap 6/6 caught, 0/6 false; build 0 warnings.

Checklist (proposed): "Motion Recorder: record Leg Sweep Expected takes with a shotgun, a plank and a crowbar, and Not Expected
ones with the axe, a slow swing and the super shotgun; `eval.sh` should catch and clear them."
## Leg grabs: pull a humanoid's legs out, or lift it by them (worktree `fastship`, branch `agent/legpull`, 2026-10-16)

His requests: both hands on a humanoid's legs (one leg with both, or one each), ideally below the knee, pulled towards
you: it topples, its feet pulled to you. Pulled up instead: lifted by the legs, an active ragdoll, with an upward
threshold; heavier ones need more. Also the leg sweep's open issue: a side sweep back-to-front landed diagonally and its
feet slid back as it landed.

**Where a hand holds** (`vr_foegrab.cpp` `legPartAt`): the held spot's nearest vertex of the monster's ragdoll rig as
drawn (new `box3d::boneAt`, limbAt's search, with the bone's far end): `shin_*`, `foot_*`, `calf*` below the knee,
`thigh_*` above it unless the spot is no higher than the thigh's far end (its knee joint) plus 1 unit (a thigh's
vertices wrap the knee); no rig: by height (under 0.27 of it below the knee, under 0.48 a thigh). A grunt's knees are
only 10-12 units over its feet (it stands crouched; its left foot 12 units behind its right). A hand below the knee
counts 1, on a thigh `vr_foegrab_leggrab_thigh` (0: not a leg grip, the lift and the judo throw go on as before; 0.5: a
leg grip whose speeds needed are doubled), elsewhere 0; the grip is the lesser hand's.

**The moves** (`legCheck`, in `twoHandCheck` after the neck snap, before the lift and the throw: a leg grip is theirs
alone), both hands settled on one humanoid (the lift's classes, AD's included), the needs times the cube root of its
mass over `vr_foegrab_leggrab_heavy` (100 kg: a grunt and an enforcer unchanged, a death knight x1.14), over the grip:
- **Leg pull**: the hands' mean velocity towards you (level, from it to you) at `vr_foegrab_legpull_speed` (1.2 m/s) or
  more, more towards you than up, in a stroke (since they came at half that speed) at least `vr_foegrab_legpull_distance`
  (8 cm) long. Knocked down by the leg sweep's `foegrab::sweepDown` (QC `VR_LegSweep_Down`, no damage), its feet sliding
  the hands' way (within 60 degrees of towards you) at `vr_foegrab_legpull_feet` (60 u/s), its body turned over away at
  `vr_foegrab_legpull_topple` (200 deg/s); held on as a judo throw's (`vr_foegrab_throw_hold`), the throw's grunt.
- **Leg lift**: their mean velocity up at `vr_foegrab_leglift_speed` (1.0 m/s) or more within 45 degrees of straight
  up, you within Lift Reach: the lift's QC `VR_FoeGrab_Lift` (no hop), its ragdoll turned over away from you about its
  feet at `vr_foegrab_leglift_topple` (220 deg/s), held by the hands (`handOver` at Lift Strength: the limbs nearest
  them, its shins), flailing; carried, thrown by letting go, breaks free after Lift Time, as the lift.

**The side sweep** (`sweepDown`, the leg pull's too): the sweep's way snapped to the nearest of its own four axes when
within `vr_legsweep_align` (45 degrees: always; 0 as swept): from its side it falls straight forward or backward. The
feet sliding back: falling backward its legs went limp at once (`ragactive::beginFall`), its knees folded under it as it
sat down and drew its feet back to its pelvis (forward, the knees can't fold). Now a backward fall keeps its legs' stance
`vr_legsweep_legs` (0.5 s) before they give (`ragdollThrown`'s new `legsHold`); sideways it doesn't (stiff legs in the
grunt's staggered stance turned it diagonal).

**Diagnostic**: Leg Grabs > Show Diagnostic, Debug > Tests > Holding Enemies > Leg Grab Diagnostic
(`vr_foegrab_leggrab_debug`): each palm's circle (green below the knee, orange a thigh, red off its legs), the pull
towards you and up as arrows against their needs (grey), a panel over its head: each hand's bone and part, the grip
(one leg, both legs, not a leg grip), pull / stroke / up against their needs with bars, its mass and factor, what stops
it now, the last try. Each try prints `leg diag:`; a two-hand hold that ended untried prints its best; with
`vr_foegrab_debug` the grip's parts (`foegrab: both hands on ...'s body:`) and the fall's trace (`legpull trace`,
`leglift trace`).

**Settings** (archived; Combat > Holding Enemies > Leg Grabs): Leg Grabs, Show Diagnostic, Thigh Grip, Pull Speed, Pull
Distance, Pull Topple Speed, Pull Feet Slide, Leg Lift Speed, Leg Lift Swing, Heavy Mass. Knockdowns > Leg Sweep: Fall
Along Its Facing, Legs Stay Straight.

**Verified** (`Misc/quakevr/foegrab/leggrab_test.py fastship`, batched; the hands put on the legs by `vr_mock_hand_to ...
nearest <class> -21`, then aside: the left leg 0.38 m further back; Grip Leniency 12 cm for the mock's hands): 10/10.
pull_one (both on its right shin) and pull_both (one per leg) pulled down at 1.8 m/s, its head 20-25 units against the
pull from its feet, its feet 2-4 units on towards you (the hands, holding on, stop with them); thigh (Thigh Grip 0): no
leg pull, no judo throw; thigh_half (0.5, 3 m/s: needs 2.4) pulled down; thigh_half_slow (1.8 m/s) nothing; slow (0.45
m/s) nothing; lift (1.8 m/s up): lifted by 2 hands, flailing, tilted 164-171 degrees, its head 35 units under its feet,
swung away and let go: its pelvis 28 units on; lift_gentle (1.1 m/s) lifted; a death knight at 1.1 m/s not (needs 1.14),
at 1.8 lifted. `legsweep_test.py`: the side cases pass (away: forward, feet 32-45; toward: backward, feet 4-19 where
they were 0), lr/rl/rocket/grenade/crowbar/plank/hilt and the not-swept ones pass; flaky in real time (contact missed
or slow: laser_lr at 1.2 m/s twice, shotgun_lr and sword_lr once, passing on a rerun); axe/crate/brick never reach the
legs (as before). Its `rror` check matched the shader log's "vr mirror": now `\b[Ee]rror`. Melee eval 168/177.
Screenshots (`scratch/leglift_shots.png`): hands on its shins, swinging over, hanging head down under the hands.

**Open**: the drawn hands don't sit on its boots once it hangs (the ragdoll hold's anchor lags the hands); a leg pull's
feet stop where the hands stop (held on), not further.

In VR: crouch, take a grunt's shins (or one shin with both hands) and yank them towards you: does it go down on its
back, feet to you? Then lift one by its legs and swing it. Try the thighs (nothing with Thigh Grip 0) and a death
knight (harder). Watch Leg Grab Diagnostic for where your hands are and why it doesn't go.

## Body tracking: Quest 3 through Virtual Desktop (worktree `tghost`, branch `agent/bodytrack`, 2026-10-16)

VDXR offers `XR_FB_body_tracking`, `XR_META_body_tracking_full_body`, `_fidelity` and `_calibration`. The game now
enables them when offered and blends the tracked body into the guessed one.

**Plumbing** (`vr_bodytrack_xr.cpp`, five one-line hooks in `vr_backend_openxr.cpp`): the extensions are enabled in
`createInstance`. After the session starts, `XrSystemBodyTrackingPropertiesFB` is checked and an `XrBodyTrackerFB` is
created with `XR_BODY_JOINT_SET_FULL_BODY_META` (84 joints; falls back to the 70-joint default set), and
`xrRequestBodyTrackingFidelityMETA(HIGH)` is called. Each frame `xrLocateBodyJointsFB` runs in the hands' space at
the views' time, with the fidelity and calibration status structs chained. It reads per-joint valid/tracked flags,
`isActive` and `confidence`. The tracker is destroyed before the session. Calibration is read and shown in the status,
but no body height is suggested to the runtime: the avatar takes only directions and angles, so it keeps its own
calibrated proportions.

**Blending** (`vr_bodytrack.cpp`): joints are smoothed in the room (`vr_bodytrack_smooth` 0.04 s) and placed in the
world exactly as the hands are. Each region has a weight that eases in or out over `vr_bodytrack_fade` (0.25 s) as
tracking turns good or bad. Good means `vr_bodytrack` on, `isActive`, confidence >= `vr_bodytrack_confidence` (0.5),
and the region's joints valid. While a region fades out, its last joints are held.
- Torso (`vr_bodytrack_torso` 1). The spine's tilt (hips to neck) feeds `solveTorso`'s lean in place of the guess. With
  it, the drop the tilt explains is a lean and the rest is a crouch, and the crouch's own forward tilt is replaced. After
  the crouched pose, the trunk is swung about the neck top to the tracked tilt. The chest gets the tracked twist from the
  hips (half in the spine). `bodyYaw` gets the tracked hips' facing. The lean detection (`senseLean`'s hold) gets the
  spine's tilt: a lean tilts the spine, a step or a neck bend does not.
- Arms (`vr_bodytrack_arms` 1). The elbow swings about the shoulder-to-wrist line to the tracked elbow's side. The
  hands stay on the controllers. The elbow self-collision (Elbows Out of the Torso) still wins.
- Legs (`vr_bodytrack_legs` 0, since Quest 3's legs are generated). The foot target and knee direction blend towards
  the tracked leg, scaled to the avatar's leg, times (1 - climbing) (1 - swimming).
- Holsters, flashlight, anchors and body contact read `avatar::torso()`/`skeleton()`, so they follow the blended pose.
  The reference poses (`standing()`, `uprightChest`) take none of it.
- With no source (no extension, plain mock), `place()` returns at once and every hook returns before touching anything
  (weights 0). The changed `solveTorso` lines are exact identities at weight 0.

**Debug**: Debug > Show Tracked Skeleton (`vr_bodytrack_debug` 1 on the body, 2 beside it; cyan = tracked, magenta =
inferred) and Print Body Tracking Status (`vr_bodytrack_status`: extensions, system support, tracker, fidelity,
calibration, confidence, joints valid/tracked, region weights, µs per frame). `vr_bodytrack_stats [label|reset]` logs,
per posed frame against the tracked body: lean error, tilt, twist error, elbow and knee swivel error, lean hold, and the
largest per-frame joint jump (relative to the neck top).
Mock: `vr_bodytrack_synth 1` adds a synthetic body (`_pitch _roll _twist _crouch _neck _elbows _knees _confidence
_active`). It moves the mock head as that body would carry it, so the head-only guess sees a real lean.

**Measured** (synthetic body, e1m1, 150 frames settle and 150 measured per case; lean error = angle between the
avatar's spine and the tracked tilt applied to its upright spine):

| case | lean err before | after | twist err before / after | elbow err before / after | lean hold before / after |
|---|---|---|---|---|---|
| stand | 0.0 | 0.0 | 0 / 0 | 16.5 / 0 | 0 / 0 |
| forward 25 | 5.1 | 0.0 | 0 / 0 | 58 / 24 (*) | 1 / 1 |
| sideways 20 | 6.1 | 0.0 | 17.7 / 0 | 65, 2 / 0 | 1 / 1 |
| twist 30 | 0.0 | 0.0 | 22.3 / 2.6 | 11, 23 / 0 | 0 / 0 |
| crouch 0.35 m | 6.6 | 0.0 | 7.7 / 0 | 98 / 0 | 1 / 0 |
| crouch 0.3 + forward 20 | 4.5 | 0.0 | 8.6 / 0 | 180 / 0 | 1 / 1 |
| neck bend 35 (false lean) | 5.5 | 0.0 | 8.7 / 0 | 14, 27 / 0 | 1 / 0 |
| elbows winged | 0.0 | 0.0 | 7.7 / 0 | 69, 56 / 0 | 0 / 0 |

(*) The elbow self-collision pushing the elbow out of the tilted torso: 0 with `vr_body_collide_elbows 0`.
Legs (knees out 30): knee swivel error 25.1 deg with legs 0, 4.7 deg with legs 1.
Fallback, holding forward 20 with elbows out: confidence 0.2 eases back to the guess (elbow error 53 deg, as the IK);
regained, back to 7 deg within the window; `isActive` 0 and switching off also ease out. The largest per-frame joint
jump in all of these is 0.42-0.49 cm, against 0.00 for a held pose. A hard switch would move an elbow about 20 cm in
one frame.
CPU: `place()` plus the synthetic body cost 1.2-2.9 µs per frame. The avatar hooks are a few vector operations.

**On the Quest / Virtual Desktop side** (names vary by version):
- Quest: Settings > Movement Tracking (or Privacy > Body tracking): body tracking on. Then allow the Movement
  Tracking / body tracking permission for Virtual Desktop (Settings > Privacy & Safety > App permissions > Movement
  tracking).
- Virtual Desktop (Quest app, Streaming tab): set the OpenXR runtime to VDXR, and turn on forwarding of tracking data
  for body tracking ("Forward tracking data" / "Body tracking"). Upper body is inside-out tracked; legs are generated.
- In game: Print Body Tracking Status should show `fb 1 fullbody 1 fidelity 1`, `tracker 1 (full body)`, `high
  fidelity asked`, `source live`, confidence near 1 and the joints tracked.

**Test in VR**: turn on Show Tracked Skeleton (Beside the Body) and check that its lean, twist and elbows match yours.
Lean round a corner, then step sideways: the body should stay upright on the step. Bend your neck to look down at your
belt: the torso should stay upright. Crouch with a straight back, then crouch leaning forward. Wing your elbows out
with the guns held. Check that holsters and the flashlight follow a lean. Try Legs at 1 if the generated legs look
right. Put a hand behind your back or turn away to lose tracking: the body should ease back without popping.

**Open**: the tracked spine's upright is taken as vertical. If his real hips-to-neck line leans a few degrees standing,
a learnt neutral would fix that. Shoulders (clavicles) take nothing from tracking yet; elbows only.

## Source splits: vr_box3d, vr_foegrab, vr_view, vr_menu (worktree `dequant`, branch `agent/splits`, 2026-10-16)

His decision 5 (R1_cpp D1/D5/D6/D7: "yes to all, as suggested"): pure moves. docs/vr-port/SOURCE_MAP.md has the map.

- **vr_box3d.cpp** (17,634 lines) is 15 parts (vr_box3d.cpp, _mesh, _props, _corpses, _ragdoll, _knockdown, _ragparts,
  _hands, _step, _contacts, _api, _debugdraw, _standing, _climb, _diag; 194 to 2,141 lines) around
  vr_box3d_internal.hpp (1,396). 15 rather than R1's 12: the ragdoll and step areas would each have been 2,700+ lines.
  What more than one part uses of the old anonymous namespace (256 of 475 names) is `box3d::detail`: types and
  constants in the header, variables `inline` there (one object, as before), functions declared there and defined in
  their part. What one part uses stays in an anonymous namespace nested in `box3d::detail` (so its lookups still find
  the shared names before qvr's, as in one file). Two functions of the API part's anonymous namespace that other parts
  call (ownBox, boxInLevel) are `qvr::box3d::detail`, used from qvr::box3d as before.
- **vr_foegrab.cpp**: its diagnostics and tests (NeckDiag, GrabDiag and their notes and frames, the test commands,
  status, the debug draws, init(): it only registers the test commands) are vr_foegrab_diag.cpp; shared:
  `foegrab::detail` in vr_foegrab_internal.hpp.
- **vr_view.cpp** (9,782): vr_view_weapon.cpp (setupWeapon and its helpers), vr_view_hands.cpp (fingers, grasps,
  pushes, the rigged hand, setupHand, drawHand), vr_view_diag.cpp (the debug and test commands); shared: `view_detail`
  in vr_view_internal.hpp (not `view::detail`: `view::` is written 242 times in the file, and a second `view` namespace
  would make each ambiguous). vr_view_diag.cpp registers its commands (`view::registerDiagCommands`, called where
  vr_main.cpp's ten lines were). magMounts stays in vr_view.cpp: anchors.py, polish_weapons.py and mdl.py read it
  there. No migration code was left in it.
- **vr_menu.cpp** (14,559 -> 6,539): the page builders are 8 area `.inc` files included where they were (gameplay,
  body, gore, physics, debug, debug_tests, settings, wofs), as the existing vr_menu_*.inc.
- The only edits that are not moves, all forced: `box3d::`/`foegrab::` written `qvr::box3d::`/`qvr::foegrab::` in the
  moved global-scope code (the new namespace made the short name ambiguous: 20 and 2 places), default arguments of
  shared functions on their declaration only, `inline` on shared variables (and on a constexpr reference),
  `#define`/`#undef` pairs copied with what they enclose.
- How: scratch tools in the worktree (cppchunks.py, b3split.py, msplit.py, incsplit.py) cut the file into its
  namespace-level entities, find which part uses each name, write the header and the parts; linecheck.py compares the
  lines before and after (only the forced edits differ).

Checks: Release, Debug and Release-Tracy build; check_statics passes. The exported symbols of every Quake/vr object
(llvm-nm, defined external) are the same before and after, plus the new detail ones (box3d 320, foegrab 46,
view_detail 142), view::registerDiagCommands and an inline constexpr from vr_handrig_data.inc now emitted by one more
object. `vr_physics_hash piles` at 10 points over 1000 frames (30 explosive boxes spawned, piled, flung twice:
scratch/hash_test.sh; deterministic, unlike a scene with monsters or crates) and the `vr_physics_mtbench 200 200` hash
are identical before and after. Melee eval 168/177 (the same 3 damage-number differences from the baseline as before:
bash 6 -> 3). vr_menu_path_check: vrcalibration 0 missing, vrtesthall 0 missing. menu_panels_test.py: all PASS.
e1m1 smoke (a blast, the gun out) clean; the moved commands are registered (cmdlist).

Not done (left for a next round): the other modules' test and bench commands to *_diag.cpp (vr_hull 485 lines,
vr_wounds 380, vr_jobs_engine 341, vr_weight 317, vr_hitmodel 225, vr_brushes 164, vr_animsmooth 148, vr_zancle 122,
vr_ragdoll's recorders ~250, and smaller ones), and the other ~65 commands vr_main.cpp registers for other modules
(each module registering its own). msplit.py does a two-part split from a config entry (msplit_cfg.py).

## Per-weapon reload overrides, Turning Mode Off (worktree `goreflesh`, branch `agent/menuopts`, 2026-10-16)

The author's request: a reload mode of its own for each weapon, and a way to switch stick turning off entirely.

- **Storage: eight archived cvars**, `vr_reload_mode_<shot|sshot|nail|snail|gl|prox|rl|light>`, default -1 (follow `vr_reload_mode`),
  else 0..3 as the global. Chosen over the `vr_wofs_*` weapon slots because the reload logic is QC and reads cvar handles each frame
  (a `weaponvalue` lookup is a string search over the slots and keys); it also follows the existing per-gun `vr_reload_pouch_*`.
  Only the eight guns that reload (WeaponIdToSupportsReload); the lava and plasma variants follow their base gun's id.
- **Logic (QC)**: `VR_Reload_ModeOf(wid)` (vr_weaponutil.qc) resolves a weapon's effective mode. `VR_Reload_ManualForInst`,
  `VR_Reload_MagIn`, `VRIsWeaponReloadingEnabledForWeapon`/`...For`, `isReloadHolsterHotspot(hotspot, weapon)` and the pickup's
  magazine fill use it. The pouch and its furniture (`VR_Reload_Immersive()`, `VRIsWeaponReloadingEnabled`) are on when any gun
  is Immersive (`VR_Reload_BestMode`).
- **Engine**: `STAT_QVR_RELOADMODE` and the seated-magazine collision use the highest mode of the global and the overrides
  (`qvr::reloadModeBest`, vr_cvars.hpp), so the clients draw the pouch, magazines and ports if any gun needs them.
- **Menu**: Advanced VR Options > Weapons > Reloading > Per-Weapon Reload Overrides (one row per gun: Use Global, Immersive,
  Simple, Simple (all holsters), Disabled; the proximity gun only with Hipnotic installed).
- **Turning Mode Off**: the existing archived `vr_enable_joystick_turn` toggle (it gates `input::turn`, the only path from the stick
  to yaw: menus, climbing, swimming and the toolgun/posing sticks never turned you) is now the third choice of VR Settings >
  Comfort > Turning Mode (Smooth, Snap, Off; the menu's wrapper `vr_menu_turning` 2). No new cvar, no migration.
- **Tests**: `vr_reload_test 9; impulse 125`: 72 passed (five new "override:" checks: the shotgun by hand over a global of 2 while
  the nailgun reloads at the hip, and the reverse with the super shotgun off). Mock stick `vr_mock_stick main 1 0` for 30 frames:
  hand yaw 90 -> -46.53; with `vr_enable_joystick_turn 0` it stays -46.53. `vr_menu_path_check`: 0 missing.
