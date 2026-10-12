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
shortcut **Quake VR Unleashed (Diagnostics)**. Everything goes into `quakevr/diagnostics/<date>_<time>/`, which is
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
  corpse-health threshold pops that piece; hits on the torso bleed/push but preserve the body. Blades, chainsaws,
  shots and blasts retain their existing paths. With Limb Gore/Corpses disabled, the previous whole-body path remains.
  Per-piece damage uses the existing corpse damage multipliers and persists with ordinary QC entity fields. Untagged
  flung props use their own position/velocity, matching the existing living thrown-limb hook; held blows use their tagged
  contact point.
- `vr_limb_test` 24: two blunt 48-damage blows at one forearm (grunt threshold 80), 25: an 800-damage pelvis-surface
  blow, 26: two physical-prop blows at that forearm, 27: an 8 m/s axe corpse slash through the real cut classifier.
  All leave corpse health 80 and `gib 0`; 24/26 pop one piece, 25 pops none, 27 cuts one and pops none. Debug > Gore
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
alias batching (cache-safe) and use sampler 6, leaving retro LUT sampler 3 intact. Both bracer modes share body
lighting for hands. The calibration board links the slider. No diffuse tone variants are generated.

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
