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
two-texel margin). Not done: the lava super nailgun and multi rocket launcher (same meshes, own skins): one SPECS line
each.

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
