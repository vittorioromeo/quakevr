# Review of `next` (2026-11-18)

Whole-project review of `next` at `0e973d4ae`, on branch `review/next-2026-11-18`. Three questions: the architecture,
the implementation's correctness and cost, and what could be simpler.

## Summary

- **Fixed and committed:** 8 small defects (list below).
- **Written up:** 20 findings that need a design choice, touch several files, change behaviour, or cannot be tested
  headless on Linux (OpenXR, GPU-visible, coop).
- **Build:** `cmake --build --preset linux-release` passes with no new warnings (the one warning is mimalloc's own
  `alloc-override.c` alias warning, there before). The dedicated server loads `e1m1` and runs.
- **Not run:** anything with a window, a headset or a GPU; the kit's test suites (Windows); coop.

### Scope

The project is far too large to read whole (`Quake/vr` alone is about 540,000 lines). What was read, and how:

| area | depth |
|---|---|
| CODE_STYLE.md, ARCHITECTURE.md, PLAN.md | read |
| Frame loop: `host.c` `_Host_Frame`, `Host_Error` / `Host_EndGame`, the tick loop | read |
| Job pool: `vr_jobs.hpp`, `vr_jobs.cpp` | read in full, no finding |
| Protocol and glue: `vr_protocol.hpp`, the move block's reader (`vr_server.cpp`), the entity update's writer and reader | read; the rest of `vr_server.cpp` / `vr_client.cpp` skimmed |
| Resets and loads: `vr_progs.cpp` (`resetServerWorld`, `VR_OnClearMemory`, `VR_OnLoadGame`), `VR_OnClientClearState`, `host_cmd.c`'s load path, a sample of the entity-keyed maps' resets | read |
| Slow motion's per-entity clock (`vr_timescale.cpp` begin/end pairs), the sound mix on the pool (`snd_dma.c` kick/join) | read in part |
| Physics: `vr_box3d_*` (contacts, hands, step, props in full; standing, corpses, ragdoll, ragparts, api, mesh in part), `vr_physics.cpp`, `sv_phys.c`'s hooks | sub-review, its findings re-read at the cited lines |
| VR backend and renderer: `vr_backend*.cpp`, `vr_stereo.cpp`, `vr_glutil`, parts of `vr_gfx_gl.cpp`, `vr_stereo_single.cpp`, `gl_rmain.c`, `gl_shaders.c` | sub-review, its findings re-read at the cited lines |
| Menus, settings, memory: `vr_menu.cpp` (most), `vr_menu_internal.hpp`, `vr_cvars.cpp` (most), `vr_slottable.hpp`, `vr_tunables.cpp`, `vr_mem.*`, a sample of `mem::Cache` users | sub-review, its findings re-read at the cited lines |
| QuakeC core: `client.qc`, `combat.qc`, `items.qc`, `weapons.qc`, `vr_carry.qc`, `vr_grenade.qc`, `vr_reload.qc` (the main paths of each; none end to end) | sub-review, its findings re-read at the cited lines |

**Not read:** hands, weapons and held things (area W) beyond the move block; body, rigs and the studios (B); melee,
gore and stealth (M); campaigns (C); audio's internals, navigation, the hull compiler, the save writer, the network
layer, the installer, `Misc/quakevr`'s tools, `QC/ad`, `QC/frikbot`, every `*_test.qc` and `_diag.cpp`.

**Checked and found sound** (no finding): the job pool's claim/run/wait protocol and `Task`'s re-post; Box3D's
worker-side callbacks (`shouldCollide`, `preSolve` and what they call only read, count with atomics, and print only
with one worker); the move block's bit packing (QC's `.vrbits0` stays under 2^24); the entity update's writer against
its reader; the acquire/release balance of the swapchain images in `VR_RenderView`; `mem::Cache` events against their
keys in the sample read; the entity-keyed maps sampled (each is cleared at a map change, by a reset or by its own
frame).

## Commits made

| hash | subject |
|---|---|
| `9b3b96389` | vr_box3d_hands: a dead player's fist going no longer orphans his reach body |
| `8f209f780` | vr_stereo: a scene-target set's first making starts from nothing (no other set's OIT composite framebuffer) |
| `06f19e14e` | vr_box3d: a change of Throw Friction leaves the corpses' friction alone |
| `577d0b154` | vr_cvars: vr_default refuses a data table's stand-in (it freed the table's string as a zone block) |
| `031abed75` | vr_menu: a panel's cursor is never past the end of a page another panel rebuilt shorter |
| `093aa01b4` | combat.qc: the empathy shield's sound is throttled on its wearer, not on whoever hit him |
| `a6bf28bbe` | client.qc: a player who leaves lets go of what he carried |
| `3e394212f` | vr_box3d_standing: a walking player's shove and nudge skip a ragdoll's cut parts |

Notes on them:

- `577d0b154`, `031abed75`, `093aa01b4` and `a6bf28bbe` were built together (four files with nothing in common),
  not one build each; the build and the headless run passed on the four.
- `8f209f780` is a GL lifetime fix that could not be watched on a GPU here. It only clears a struct that
  `GL_DeleteFrameBuffers` clears on every other path. Worth one look in the headset with `vid_fsaa 0`, `r_oit 1` and a
  dynres step.
- `a6bf28bbe` and the coop side of `9b3b96389` were not played in coop.

## Findings

### 1. An item's targets (and Honey's secrets) fire on a touch that takes nothing

- **Category:** correctness. **Severity:** medium-high.
- **Where:** `QC/honey_items.qc:4-42` (`itemTouch`), installed for every item at `QC/items.qc:253-254`.
- **Finding:** every pickup's touch is `itemTouch`, which calls the real touch (`self.th_die()`) and then, whatever it
  did, runs `activator = other; SUB_UseTargets();`. The real touches return early without taking: `health_touch` at
  full health (`items.qc:433-441`), `ammo_touch` at the cap, `armor_touch`, `key_touch` with the key held,
  `weapon_touch` with the hand not gripping or coop's `leave`. `items.qc` has no `SUB_UseTargets` of its own any more,
  so id's rule (targets fire on a pickup) is gone. A body walking over an item reaches the same function
  (`VR_Carry_Handtouch` calls `carry_use`).
- **Failure:** a health box with a `target`, walked over at full health, fires its targets on every touch; a hand
  brushed through a weapon pickup without gripping springs its ambush, and the real pickup fires it again; a
  `trigger_counter` counts one item several times; a toggling door flips twice. In Honey mode a secret item counts as
  found, and its message is spent, without being taken.
- **Recommended fix:** a global `item_taken`, cleared by `itemTouch` before `self.th_die()` and set by each touch
  function where it really takes; the secret block and `SUB_UseTargets` run only when it is set.
- **Drawbacks:** `self.solid == SOLID_NOT` cannot stand in for it (a coop key stays solid and still fires), so every
  touch function needs its line: about a dozen sites, each a place to forget. Maps that came to rely on the eager
  firing (none known) would change.
- **Status (2026-11-19): fixed.** `item_taken` (vr_defs.qc), set by every pickup `itemTouch` wraps (health, ammo,
  armour, keys, weapons, id's powerups and runes, Hipnotic's and Rogue's powerups, Dawn of the Machine's shards, head,
  bunny, runes and upgrades; its Super Axe and bloody shotguns through `weapon_touch`); not taken, `itemTouch` returns
  before the secret, the respawn and the targets. Hipnotic's and Rogue's own second `SUB_UseTargets` are gone; the Horn
  of Conjuring keeps its own (charmed) and marks `ITEM_TAKEN_FIRED`; a draught ring is never taken (with OVERRIDE_DEST
  `teleport_touch` fires its target). Rogue's random powerup installs its pickup where `itemTouch` calls it (three of
  its five picks had none). Backpacks don't go through `itemTouch`. Test: `vr_pickup_test 19` (vr_itemrules_test.qc).

### 2. A session or instance lost by return code never ends the session, and frames without `xrWaitFrame` spin unpaced

- **Category:** correctness. **Severity:** medium.
- **Where:** `Quake/vr/vr_backend_openxr_frame.cpp:37-42`, `:56-62`, `:72-75`; `pollEvents` in
  `vr_backend_openxr.cpp`.
- **Finding:** `beginFrame` returns false (the session ended: "playing flat") only for the loss *events*.
  `XR_ERROR_SESSION_LOST` and `XR_ERROR_INSTANCE_LOST` appear nowhere in `Quake/vr`: a failed `xrWaitFrame` is logged
  by `checkFrameCall` and `beginFrame` returns true; `xrPollEvent`'s errors read as "no events".
- **Failure:** a runtime that dies without queuing the event first (a crashed compositor) leaves `vrActive()` true for
  good: no flat fallback, no message, the swap interval 0 and the frame pacing left to a runtime that no longer paces,
  so the loop spins at `host_maxfps`. The same spin happens in the legitimate `!sessionRunning` early return (IDLE,
  or between STOPPING and READY).
- **Recommended fix:** treat those two codes from `xrPollEvent`, `xrWaitFrame`, `xrBeginFrame` and `xrEndFrame` as
  `endReason = Lost; return false`. Separately, sleep a display period when a frame ran no `xrWaitFrame`.
- **Drawbacks:** not testable headless without a new fault kind in `Misc/quakevr/fakexr` (it has `loss` and
  `instanceloss` as events only). Which shipping runtimes skip the event is not known; a runtime that returns
  `SESSION_LOST` transiently would now drop to flat where it recovered before.

### 3. A panel swapchain that fails after `xrCreateSwapchain` is kept and used

- **Category:** correctness. **Severity:** medium-low (a rare trigger, a bad outcome).
- **Where:** `Quake/vr/vr_backend_openxr_frame.cpp:476-517` (`createSwapchain`),
  `Quake/vr/vr_backend_openxr_panel.cpp:49-53` (`ensurePanelSwapchain`).
- **Finding:** `createSwapchain` stores `sc.handle`, `sc.width` and `sc.height`, then can still return false at four
  points (no or too many images, the context not ready, the enumeration failing or faulting, an image of texture 0).
  The eyes' swapchains are safe (the start fails and `stop()` destroys them). The panel's is made mid-session: next
  frame `ensurePanelSwapchain` sees a handle of the right size and returns true.
- **Failure:** with the failure before `images.resize`, `acquireImage` indexes an empty vector. After it (VDXR's
  documented fault in `xrEnumerateSwapchainImages`), the image is 0: acquired, never released, and every later frame
  fails `xrAcquireSwapchainImage` with an unthrottled warning. With the handle null instead, `xrCreateSwapchain` is
  retried every frame with its log writes.
- **Recommended fix:** on any failure after the handle exists, destroy it and reset `sc` inside `createSwapchain`;
  a "failed for this size" latch in `ensurePanelSwapchain`.
- **Drawbacks:** a latch needs its reset (a new session, a size change); a runtime whose failure was transient would
  need a restart to get its panel back.

### 4. Every menu or console close rebuilds a string map of all archived settings

- **Category:** performance. **Severity:** medium (not measured: a hitch on the frame the player returns to the game).
- **Where:** `Quake/vr/vr_cvars.cpp:1623-1650` (`saveConfigNow`), `:713-728` (`archivedSettings`), `:679-711`
  (`readConfigSettings`), `VR_ConfigMergeOthers` / `VR_ConfigWritten`.
- **Finding:** `configFrame` calls `saveConfigNow` when `key_dest` leaves the menu or the console. It loops over
  `archivedSettings()`, which builds an `ankerl` map of `za::String` pairs with an entry for every archived cvar and
  every data-table stand-in (thousands of weapon, prop, monster and group values, most names past the small-string
  size). When one setting changed, the write adds a read of the file into a second map of the same size, a sort of
  the whole map, and after the write the file read again and `archivedSettings()` again.
- **Cost:** on the order of 10^4 allocations on a VR frame when nothing changed, several times that plus a full
  string sort when something did.
- **Recommended fix:** the no-change check walks the cvars and the tables directly and looks each up in
  `configTrack.here` (no temporary map). The merge compares the file's lines against `configTrack.file` and sorts only
  those that differ. `VR_ConfigWritten` updates `here` / `file` in place rather than reading back what it just wrote.
- **Drawbacks:** the map dedupes a name that is both a cvar and a stand-in; the direct walk must keep the same winner.
  Updating the tracking in place trusts the writer; the read-back today also catches a write that failed.

### 5. A shotgun blast walks every edict once per pellet

- **Category:** performance. **Severity:** medium.
- **Where:** `QC/vr_carry.qc:3682-3712` (`VR_Gib_TraceHeld`), called from `FireBulletsImpl`'s pellet loop
  (`QC/weapons.qc:2383`) and from `LightningDamage`.
- **Finding:** for a client shooter the function runs `for(entity e = nextent(world); e; e = nextent(e))` testing
  `e.vr_gib && e.vr_skewer_p != world` before any early-out. It is there for a gib skewered on a blade, which is
  almost never the case.
- **Cost:** a super shotgun blast is 14 full edict walks in QC, the lightning gun several every 0.1 s: with a few
  thousand edicts, on the order of 10^5 QC statements a shot to find nothing.
- **Recommended fix:** a global count of skewered gibs, raised where `vr_skewer_p` is set (`vr_skewer.qc`) and lowered
  in `VR_Skewer_Gone`; the walk only when it is above 0. Or the skewered gibs gathered once per `FireBulletsImpl`.
- **Drawbacks:** a count that drifts (a gib removed by a path that skips `VR_Skewer_Gone`, a loaded game: it must be
  rebuilt or saved) turns the gate into a bug where shots pass through a skewered gib; the count needs a rebuild at
  load.

### 6. `callBodyHits` walks every edict once per moving ragdoll, every server frame

- **Category:** performance. **Severity:** low-medium.
- **Where:** `Quake/vr/vr_box3d_step.cpp:1296-1385`, the inner loop at `:1365`.
- **Finding:** inside `for(RagdollBodies& r : world->ragdolls)`, any ragdoll moving at 40 u/s or more (with
  `vr_kdtrig_flung` at its default 1: every falling, dying or blasted one, up to `vr_ragdoll_max` 8) runs
  `for(int n = 1; n < qcvm->num_edicts ...)` reading several fields of every edict. This is the pattern the push grid
  and `edictindex` exist to avoid.
- **Recommended fix:** monsters through `edictindex::nextFlagged(n, FL_MONSTER)` (as `watchCorpses` does), the
  `.vr_mount` things through that field added to the index, or one `SV_AreaEdictsUnordered` over the ragdoll's box.
- **Drawbacks:** the area query only finds linked edicts (whether mounted things are always linked was not checked);
  a new watched field in the index costs every `VR_EdictIndex_Touch`. The same pattern, rarer, in
  `addSkeweredShapes` (`vr_box3d_props.cpp:140`) and `blast` (`vr_box3d_api.cpp:386`).

### 7. Holding Mjolnir without Hipnotic's `playham.mdl` makes the player invisible to others

- **Category:** correctness. **Severity:** medium (coop, deathmatch, third person).
- **Where:** `QC/client.qc:1242-1247` (`PutClientInServer`), `:2067-2068` (`CheckPowerups`).
- **Finding:** `modelindex_hammer` is 0 unless `vr_have_playham`; every living frame
  `else if(self.weapon == WID_MJOLNIR) self.modelindex = modelindex_hammer;`. Mjolnir is Quake VR's own weapon,
  available without the mission pack. The server skips an entity with model index 0 (`sv_main.c`).
- **Failure:** on an install without Hipnotic, a player with Mjolnir in his main hand is not sent to the other
  clients, has no third-person body, and leaves no corpse.
- **Recommended fix:** `if(!modelindex_hammer) modelindex_hammer = modelindex_player;` after the player's model is
  set, and the Mjolnir frame macros in `player.qc` (and `player_die_ham1`) gated on `vr_have_playham`.
- **Drawbacks:** without the gate the fallback plays `playham.mdl`'s frame numbers on `player.mdl` (wrong poses),
  so both halves are needed; the swing then shows the axe's animation.

### 8. `Host_Error`'s `longjmp` crosses C++ frames

- **Category:** architecture. **Severity:** low-medium.
- **Where:** `Quake/host.c:370`, `:419` (the jumps), `:1693` (the `setjmp`); every `VR_*` hook that can reach QC.
- **Finding:** `Host_Error` and `Host_EndGame` jump from wherever they are called back to `_Host_Frame`. Much of the
  call stack between is now C++ (`progs::callQc` from Box3D's `touchNearby`, the hooks round `SV_Physics_Client`,
  builtins that run QC): `PR_RunError` inside any of them skips every destructor on the way. Today that is leaked
  `za::Vector` / `za::String` locals, a `progs::ServerVm` that never pops (harmless: `Host_Error` sets the VM to
  null), begin/end pairs left open (`VR_PhysicsEntityBegin` without its `End`: `playerTime.worldFrame` stays set
  until the next pair), and profile scopes left open. The code knows about some of it by hand (`vr_startlog`'s
  `frameEnd`, `vr_modelkeep`'s recording, `Temp_FreeToMark (0)`, `NET_RestoreMessage`).
- **Failure:** nothing fatal found. The risk is the next RAII guard that matters: a `za::LockGuard` or a
  `cvars::Override` live across a call that can reach `Host_Error` would leave a mutex held or a setting changed for
  good, and nothing warns of it. With MSVC the behaviour of `longjmp` over frames with destructors is also formally
  undefined.
- **Recommended fix:** one documented rule plus a hook: a `VR_OnHostError` called before the jump that resets the
  frame-scoped state (the timescale's pair, the profile's stack, "inside the Box3D step" flags), and a line in
  CODE_STYLE.md's "Threads" that no lock or override is held across a call that can run QC.
- **Drawbacks:** a rule is only as good as its reading; the full cure (errors returned, not jumped) is a rewrite of
  Quake's error model and not worth it.

### 9. The sound mix on the pool is made safe by a join each caller must remember

- **Category:** architecture. **Severity:** low-medium.
- **Where:** `Quake/snd_dma.c:1292-1345` (`S_MixKick`, `S_MixJoinAt`), about 40 `S_MixJoin ()` sites in `snd_dma.c`,
  `snd_mix.c`, `zone.c`, `bgmusic.c` and nine VR files.
- **Finding:** the frame's mix runs on a worker while the screen draws. Every function that touches the channels,
  the paint buffer or the cache begins with `S_MixJoin ()`. Nothing checks it: a new reader of `snd_channels` on the
  main thread during the render (a HUD readout, an effect that asks whether a sound still plays) without the join is
  a silent race with the mixer. The VR files that read the channels (`vr_audio.cpp`: 32 uses, one join) are safe
  today because they are the mixer's own hooks, which nothing in the code states.
- **Recommended fix:** in development builds, a check in the accessors (or a macro round `snd_channels`) that the
  caller is the mixing thread or that no mix is posted (`snd_mixstats.posted`), with the crash report naming the
  site; a comment on each hook file saying which thread runs it.
- **Drawbacks:** `snd_channels` is a plain array read in many places: wrapping it touches Ironwail's files (more
  diff against upstream); a check only in the VR module's readers covers less but costs nothing upstream.

### 10. `game <dir>` into a folder with no saved config re-arms the first-start VR Calibration; the tunables stay the old folder's

- **Category:** correctness. **Severity:** medium-low (whether a plain `game` switch into such a folder is a
  supported flow is the author's call).
- **Where:** `Quake/vr/vr_cvars.cpp:778-787` (`migrateConfig`), `Quake/vr/vr_setup.cpp:1002-1010`,
  `Quake/vr/vr_tunables.cpp:617-621`.
- **Finding:** a game switch executes `quake.rc` again; with no `ironwail.cfg` in the new folder `Cmd_Exec_f` inserts
  `vr_migrate_config new`, which sets `vr_setup_pending 1`. `setup::frame` then opens VR Calibration (the server is
  down right after a switch). Commit `97ac5b867` meant it for a first start only. In development builds
  `tunablesLoad()` returns at `if(fileLoaded)`, so the new folder's `tunables.cfg` is never read and the old folder's
  overrides are written into the new folder at the next dirty write (`filePath()` uses `com_gamedir`).
- **Recommended fix:** `vr_setup_pending` set only on the session's first config (a file-scope flag in
  `migrateConfig`); `fileLoaded` and the overrides reset from `VR_OnGameDirChanged`.
- **Drawbacks:** a player who really starts a second, fresh install's folder in one session gets no calibration
  prompt there; which folder owns a tunable's override needs deciding first.

### 11. The map's ammo boxes roll their weapon drop against `world`, not a player

- **Category:** correctness. **Severity:** medium-low. Partly verified.
- **Where:** `QC/client.qc:181-195`, `:1324-1334`; `QC/items.qc:41-47`, `:112-131`, `:256-258`;
  `Quake/vr/vr_progs.cpp:85-96`.
- **Finding (verified):** `OnSpawnServerAfterLoad` runs with `self` the world (`callEntryPoint`) and calls
  `SpawnAllAmmoBoxWeapons`, which passes `self` as the player to `SpawnAmmoBoxWeaponsImpl`. The eligibility test
  reads the world's `.items`: with `vr_ammobox_drops 0` ("when eligible") a map's own boxes can never drop a super
  shotgun, nailgun, launcher or lightning gun whatever the player owns. `IsAmmoBox` also lists only the four id
  classnames, so Rogue's lava nails, multi-rockets and plasma never get the map-start roll although the function
  has their branches.
- **Not verified:** whether the second path (`SpawnAmmoBoxWeapons(self)` at the item's placement, with
  `VR_Player()`) runs before or after a player exists. Before: it finds none (and would print "player entity not
  found"; the dedicated run's log had no such line, which a server with no player does not settle). After: each box
  is rolled twice.
- **Recommended fix:** one roll per box, at the first `PutClientInServer` of the map (the inventory known), and
  `IsAmmoBox` covering the three Rogue classnames.
- **Drawbacks:** the drop odds players have now (ROUND24.md's table) change either way; in coop "the player" needs a
  rule (the first, or any who is eligible).
- **Status (2026-11-19): fixed.** Measured: the map's roll (`OnSpawnServerAfterLoad`) ran before the boxes were
  placed, and their placement (the first frame) before any player was in, so each map box rolled once, against the
  world; only boxes placed later (crates, the toolgun) rolled against the player. Now each box rolls once
  (`.vr_ammobox_rolls`): the map's at its first `PutClientInServer` against that player (in coop the first one), a box
  placed after that at its placement (`SpawnAmmoBoxWeapons`), none again on a respawn or after a load; `IsAmmoBox` has
  Rogue's three. The odds are now ROUND24.md's (the player's inventory): with `vr_ammobox_drops 0` a map box can carry
  a gun the player owns, which it never could. Test: `vr_pickup_test 20`.

### 12. Menu panels on one page: the other panel's selection moves when rows are added or removed

- **Category:** correctness. **Severity:** low (the out-of-range half is fixed in `031abed75`).
- **Where:** `Quake/vr/vr_menu.cpp:2417-2482` (`items()`' rebuild).
- **Finding:** the rebuild takes `anchorOf` for `ui()` (the panel that asked) alone and re-finds its row by section
  and label. Another panel on the same page keeps its index: rows added or removed above it (the menu detail
  changed, Changed Settings or Search rebuilding every page) put its cursor on a different row, and its next A or
  left/right changes a setting the player was not looking at.
- **Recommended fix:** in the rebuild, the anchor of every open `menuInstances[i]` taken before `built[page].clear()`
  and each re-found after (about 15 lines).
- **Drawbacks:** none of note; the scroll of the other panels must be re-anchored too or the row jumps on screen.

### 13. Three different "same as its default" tests

- **Category:** simplification. **Severity:** low.
- **Where:** `Quake/vr/vr_cvars.cpp:574-585` (`sameValue`, exported as `sameSettingValue`),
  `Quake/vr/vr_menu.cpp:2059-2077` (`changedSetting`), `:3105-3106` (`resetAll`), `Quake/vr/vr_tunables.cpp:211-215`.
- **Finding:** the config writer compares as doubles within 1e-6, the menu's "*" mark and Changed Settings compare
  as floats exactly, Reset All compares strings. A value the menu lists as changed can be one the config does not
  save; Reset All counts "0.50" against "0.5".
- **Recommended fix:** `changedSetting` and `resetAll` call `sameSettingValue`.
- **Drawbacks:** Reset All's count changes (fewer); a value 1e-7 off its default loses its star.

### 14. Eye-sized GL objects are never released once VR stops; each dynres step remakes them all

- **Category:** performance / architecture. **Severity:** low-medium.
- **Where:** `Quake/vr/vr_stereo.cpp` (`ensureSceneTargets` is the only `GL_DeleteFrameBuffers` caller: `eyeTargets`,
  `spectatorTargets`, the portals' sets, `resample` have no release path), `Quake/vr/vr_stereo_single.cpp:151`,
  `Quake/vr/vr_gfx_gl.cpp` (`halfTargets`, `underTargets`); `stopBackend()` in `vr_main.cpp`.
- **Finding:** after "session lost: playing flat", `vr_enabled 0` or a failed restart, the whole multisampled eye
  set stays allocated for the rest of the run (an estimate for 3000x3200 eyes at 4x: about 1 GB). And with
  `vr_dynres 1` (the default) each bucket change frees and allocates that same set, and everything keyed to its size,
  in the frame that is already over budget; `vr_dynres.hpp` documents this as chosen.
- **Recommended fix:** a `stereo::releaseTargets()` called from `stopBackend`. For dynres: allocate at the ceiling
  once and vary the viewport, the resample reading a sub-rectangle.
- **Drawbacks:** the viewport way touches every pass that assumes the target's size is `vid.width/height`
  (`drawReverseOrder` does), a large change for a hitch nobody has measured; releasing on stop makes a quick
  off/on of VR pay the allocation again.

### 15. Whole-`slots` scans every frame where `nextActiveSlot` exists

- **Category:** performance / simplification. **Severity:** low.
- **Where:** `Quake/vr/vr_box3d_contacts.cpp:527` (`syncPortalCopies`), `:639` (`carryCorpses`), `:804`
  (`notePushed`, up to three times a frame); `Quake/vr/vr_box3d_standing.cpp:177` (`nudgeWalkedRagdolls`: every slot
  per walking player, where `world->ragdolls` is the list it wants).
- **Finding:** each loops over `world->slots` (one per edict) filtering on `kind`; `World::nextActiveSlot`
  (`vr_box3d.cpp:463-481`) exists for exactly this and `beforeStep` and the write loop already use it.
  `syncPortalCopies` also pays a `b3Body_ComputeAABB` and `portals::splitBounds` for every prop, asleep or not.
- **Recommended fix:** the scans swapped to `nextActiveSlot` / `world->ragdolls`.
- **Drawbacks:** skipping sleeping props in `syncPortalCopies` is not a swap: a sleeping prop astride a gate still
  needs its copy, so that part needs a "astride a gate" flag kept from when it last moved.

### 16. CTF's level change mixes the new game's weapons with the old weapons' state

- **Category:** correctness. **Severity:** low (CTF only).
- **Where:** `QC/client.qc:83-88`, then `:138-170` (`SetChangeParms`).
- **Finding:** `if(teamplay >= TEAM_CTF) { SetNewParms(); }` does not return (the dead player's branch above does):
  the fresh loadout is followed by `parm23 = self.weaponflags; ... parm31..38 = ...wi_clip; parm41..48 = ...wi_uid;`
  from the weapons the player had. The new axe and shotgun are decoded with the old slots' clips, flags and ids.
- **Recommended fix:** `parm22 = self.steam; return;` after `SetNewParms()` in that branch.
- **Drawbacks:** `parm49` (the next weapon id), `parm74` and `parm75` are then not carried either, as for a dead
  player; left to the author since CTF could not be played here.
- **Status (2026-11-19): fixed** as recommended. The three parms checked: `parm49` is carried anyway (SetNewParms
  writes `qvr_weaponinst_next`, the same value); `parm74` (bullet time's meter, single player only) and `parm75` (the
  armour plates shown: CTF carries no armour) start afresh, as a dead player's; the grapple (parm18) is SetNewParms's
  CTF loadout. Test: `vr_pickup_test 21`.

### 17. `readytime` is one global shared by all players

- **Category:** correctness (coop). **Severity:** low.
- **Where:** `QC/vr_defs.qc:343`; written in `PutClientInServer` (`QC/client.qc:1310-1317`), read in `weapons.qc`,
  `vr_reload.qc`, `vr_grenade.qc`, `vr_wpnholster.qc` as `!deathmatch && time < readytime`.
- **Failure:** in coop every respawn or late join blocks every other player's firing, pouch reloads, pouch grenades
  and holster actions for half a second.
- **Recommended fix:** a player field (`.float vr_readytime`), read as `self.vr_readytime`.
- **Drawbacks:** a new field in the progs (saves made before it read 0: ready at once, which is right).
- **Status (2026-11-19): fixed** as recommended: `.float vr_readytime` (vr_defs.qc), set by `PutClientInServer`, read
  as the player's own at every gate (weapons.qc, vr_reload.qc, vr_grenade.qc, vr_wpnholster.qc's `ent`) and by the
  revolver, nail pistol and flare gun tests. agent/mp has no change of its own here (the same global, the same reads):
  the name follows its per-player fields (`.vr_movejump`, `.vr_spawn_wait`, ...). Two players can't be run headless here:
  `vr_pickup_test 22` stands another player's spawn in with another entity's field.

### 18. Shape-id buffers smaller than the most shapes a body can have

- **Category:** correctness. **Severity:** low (non-default `vr_box3d_gun_pieces` only).
- **Where:** `Quake/vr/vr_box3d_contacts.cpp:494` (`za::Array<b3ShapeId, 16>` in `addThrough`);
  `maxBodyShapes`-sized arrays at `vr_box3d_hands.cpp:682`, `:1012` and `vr_box3d_props.cpp:453`.
- **Finding:** a gun may have up to `maxBodyShapes` (32) pieces, plus a magazine's hull, plus skewered gibs' spheres.
  `b3Body_GetShapes` is capped by the capacity passed, so nothing overruns, but with 16 pieces or more a gun's portal
  copy silently lacks pieces (part of it meets the wall behind the gate), and at 32 plus a magazine `ignoreInside`,
  `sweepReach` and `floorDepth` miss a shape.
- **Recommended fix:** the pieces clamped to `maxBodyShapes - 1`, the portal copy's array sized from
  `maxBodyShapes`.
- **Drawbacks:** none of note.

### 19. Smaller items

Each verified at the cited lines; none worth a section.

- **Performance, low:** `World::stepSamples` grows every frame to 65,536 entries (512 KB a world) whether or not
  `vr_physics_steptime` was ever asked (`vr_box3d_step.cpp:1598-1601`); its sibling `frameSamples` is gated. Gating it
  loses the history before the first print.
- **Architecture, low:** three scratch vectors at file scope outside `mem::Scratch` (`vr_box3d_hands.cpp:1308`
  `braceContacts`, `vr_box3d_props.cpp:313` `propHullsGone`, `vr_physics.cpp:173` `armedFist`): not counted by
  `vr_memstats`, not given back at a map change; `Box3dScratch` already holds their sibling.
- **Performance, low:** `Item`'s chained modifiers (`help`, `advanced`, `vrOnly`, ... in
  `vr_menu_internal.hpp:264-360`) each copy the whole row with its `choices` vector; `&&`-qualified overloads that
  move would make page builds (Changed Settings and Search build every page) allocate a third as often.
- **Performance, low:** the hidden-area mesh is recomputed and uploaded for every scene clear
  (`vr_stereo.cpp:516-549`): per eye, per portal view, per layer; it changes only on a visibility-mask event or a
  field-of-view change.
- **Performance, low:** `drawReverseOrder` (`vr_gfx_gl.cpp:1605-1650`) keys its target by size alone and re-attaches,
  re-validates and clears the stencil whenever the scene's depth texture alternates (a portal view, single-pass
  stereo's per-eye depth); the half-size path already keeps two entries for this.
- **Correctness, low:** the Doom page keeps its old WAD rows after "Search Again" (`vr_menu.cpp:675-692`: the count
  is taken at build time and `items()` has no generation check for that page).
- **Correctness of tests, low:** the mock backend does not override `frameActive()` (`vr_backend.hpp:219-222`), so a
  second `SCR_UpdateScreen` in one host frame renders and "submits" again where OpenXR skips it; the override is one
  line but may shift mock-based image tests.
- **Latency, low:** frames without eyes (menus, loading) are ended by the next `VR_BeginFrame`
  (`vr_backend_openxr_frame.cpp:22-25`), after the window's swap; ending them right after the panel's copy needs a
  headset to confirm.
- **Simplification:** `gfx::Target` / `ensureTarget` (`vr_gfx_gl.cpp:2231-2300`, 13 call sites) beside the canonical
  `gfx::RenderTarget` (`vr_glutil.cpp:230-284`); the former never checks completeness and reads the bound framebuffer
  back with `glGetIntegerv`.
- **Dead code:** `parm39` / `parm40` are written in `SetChangeParms` (`client.qc:165`, `:168`) and never decoded;
  `BackpackTouch`'s owner grace (`items.qc:2110`: `self.nextthink - time > 119`) can never be true since the removal
  moved to `nextthink2`.
  **Status (2026-11-19):** the two writes removed (nothing decodes them: not DecodeLevelParms, not the engine, which
  copies the parms by name, not agent/mp; defs.qc marks both free). The grace removed, not mended: besides the timer,
  `Backpack_Setup` sets every backpack's owner to world (the thrown-object and force-grab code read `owner`), so
  `other == self.owner` never held either; the code came with the import (`adec81964`), ZOID's CTF rule for a pack
  its dropper tossed, and a player's own pack here is his death's.
- **Coop, noted only:** slow motion's per-entity clock treats edict 1 as "the player"
  (`vr_timescale.cpp:503`: `const bool player = num == 1;`).

### 20. The world's resets are three mechanisms kept in step by hand

- **Category:** architecture. **Severity:** low (no missing reset found in the sample).
- **Where:** `Quake/vr/vr_progs.cpp:566-584` (`resetServerWorld`: 16 calls), `Quake/vr/vr_client.cpp:961-1004`
  (`VR_OnClientClearState`: about 40 calls), `mem::on(MapChange)` (the registered scratch sets and caches), and files that
  notice a new map themselves (`vr_emissive.cpp:1026`, `vr_walltorch.cpp:596`).
- **Finding:** per-map state is forgotten by an explicit call in one of two lists, by the `mem` registry, or by the
  subsystem's own check. A new subsystem with a map keyed by entity number must add its line to the right list;
  nothing fails if it does not, and the bug shows as a stale entity number after a load. About 250 file-scope
  containers exist outside the registry in non-test files; the dozen entity-keyed maps checked are all reset.
- **Recommended fix:** a third registered kind beside `mem::Scratch` and `mem::Cache` (state with a reset function
  and an event: server world, client state), so that the two lists become registrations next to the state, and
  `vr_memstats` can list what is not registered.
- **Drawbacks:** the lists' order matters in places (`hull::keepForReload` before the caches go; Box3D before what
  holds its ids), and a registry hides the order; the migration is mechanical but touches about sixty files.
