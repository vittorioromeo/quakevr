# The server tick on a worker thread: design, audit and plan

**SHELVED (2026-10-17).** The threading is not being pursued, and the audit tool was removed with the other perf
attempts that did not pay off (branch `agent/perfrevert`; ROUND22.md, "Removed perf attempts"). This page stays as the
design and the audit's findings, should the work be taken up again; `vr_server_thread_check`, vr_servertick.cpp and
their check points below are in git history (merge bdd2d27d2), not in the code.

Status before the shelving (2026-10-17, branch `agent/servertick`): **audit shipped, threading not shipped.** This
page holds the design the audit supports, what the audit found, and the steps left. The tool was
`vr_server_thread_check` (vr_servertick.cpp, Debug > Profiling: Server Tick Thread Audit / Report).

## The cost and the gain

The listen server's tick runs on the main thread between the client's send and its read (`_Host_Frame`, host.c):
input, commands, CL_SendCmd, **Host_ServerFrame** (SV_RunClients, SV_Physics with QuakeC and the Box3D step,
VR_ServerFrameEnd, SV_SendClientMessages), CL_ReadFromServer, SCR_UpdateScreen, particles, sound.

| Scene | server, main thread, ms a frame | main-thread CPU busy | source |
|---|---|---|---|
| hip1m1, his VR play | 1.00-1.07 (2 ms a 72 Hz tick, p99 3.2) | 3.3-3.5 | Tracy, scratch/tracy_gpu1.md item 7 |
| vrfiringrange `combat_48` (mock, 90 Hz paced) | 0.96 (SV_Physics 0.88) | 3.66 (screen 2.77) | bench.sh, benchresults/servertick_base |

Only running the whole tick beside the render takes that time off the main thread: QuakeC is single-threaded and
touches every edict, and the Box3D step is in the middle of the tick (edicts synced in, stepped, written back, then
QuakeC's touch callbacks), with its solver already on the job pool.

## Options weighed

- **(a) The whole tick on a worker, overlapped with the drawing of the previous snapshot.** The only one with the
  ~1 ms. The frame becomes: input, commands, CL_SendCmd, *kick* Host_ServerFrame, screen and particles (the world as
  the last read left it, the head pose new), *join*, CL_ReadFromServer, sound. The server's inputs (the commands and
  the moves) are made before the kick exactly as now, so its results are the same; the world is drawn one tick later
  (the head and hands' poses are not: they are the frame's own). That latency is a design decision for him.
- **(b) The Box3D step as a job beside the render.** Its results feed the same tick (write-back, QuakeC touches,
  then the snapshot). Moving the step to the next tick changes the order of QuakeC's callbacks and what the snapshot
  carries, so `vr_physics_hash` / `vr_bench_statehash` change; and its solver is already parallel, so the main thread
  would save 0.1-0.15 ms. Not worth it.
- **(c) The per-edict parts in parallel.** SV_Physics' loop runs QuakeC thinks and links/unlinks between traces: each
  edict sees the ones before it. Nothing self-contained is big enough (sync of entities into Box3D is 0.10 ms, and
  Box3D's API is single-threaded per world).

Chosen design: **(a)**, in steps that each keep the hashes, behind one setting (`vr_server_thread`, default 0 until
every check passes). This round ships its first step, the audit.

## The audit (shipped)

`vr_server_thread_check 1` notes, by the frame's phase and the caller's stack (hash, described once):
- the **server's state touched from the main thread outside the tick**: the server's VM switched in (PR_SwitchQCVM /
  PR_PushQCVM with `&sv.qcvm`: since `qcvm` is thread-local and NULL outside the tick, every EDICT_NUM-style read
  goes through it), SV_Move, SV_PointContents, SV_LinkEdict, SV_UnlinkEdict;
- the **client's state touched from inside the tick**: S_StartSound, R_AllocParticle, the VR particles' spawn, decal
  placement, Cvar_SetQuick, Cbuf_AddText/InsertText, Con_Printf.

Only the phases a threaded tick overlaps (`screen`, `particles`) are races; `input`, `commands` and `client read` run
while the worker would be idle. `vr_server_thread_report` prints them (RACE marked). `vr_server_thread_check 2` also
runs the frame in the threaded order on the main thread (the tick after the screen, then the read), for frames of one
tick in a game: the hashes taken at a command (`vr_bench_statehash`, `vr_physics_hash`) must stay those of check 0,
or something the drawing computes reaches the server outside the snapshot.

Off, each choke point tests one int (`vr_servertick_checking`).

### What it found (scratch/audit.sh: fixed frames, seed 7, god mode, 48 or 16 monsters spawned, 600 ticks)

No client state touched from inside the tick on either map (sounds and effects go through the messages). Server
state from the drawing, by site (vrfiringrange calls / hip1m1 calls, 600 frames):

| Site | Calls (range / hip1m1) | What it reads | Fix |
|---|---|---|---|
| R_DrawAliasModel_Real, R_DrawAliasModels (VR_PortalAlias) | 295k / 41k | teleporters' edicts, per model per eye | snapshot the gates' transforms at the read |
| R_BModelPortalSplit, R_DrawBrushModels_Real (VR_PortalAlias) | 108k / 0 | the same, for brush props | same snapshot |
| VR_RenderShadowMaps (vr_lighting.cpp:1754, selectPortalLights) | 10.5k / 1.9k | the gates again (lights seen through them) | the gates' snapshot |
| VR_SetupViewEntities (vr_view.cpp:4386, applyEyeView inlined) | 1.5k / 1.5k | to look at (once a frame) | client copy at the read |
| modelcollide::beginView | 1.5k / 1.5k | edicts for the drawn models' collision | client copy |
| bloodstream::frame: SV_Move | 3.5k / 1.3k | traces in the server's world (area nodes the tick relinks) | a client-side trace (world BSP and client brush entities) |
| flashlight::setupView | 750 / 750 | the player's flashlight fields | client copy at the read |

Not races (the worker idle): handpose's gun planes and wall stops (traces at input, 37.5k), VR_BeginFrame,
VR_TorchLights (in the read), commands.

**The threaded order changes one thing on the firing range**: `vr_physics_hash` stays (c6a29a71b11071f5,
c198549bafcff6fb at both checkpoints) but `vr_bench_statehash` differs, in the fields `ammopouchpos_y/z` alone
(`vr_bench_statehash fields`). The ammo pouch's rounds are placed in the drawing (vr_view.cpp `ammoPouchRounds`,
from the body drawn at the last read) and sent with the move (vr_client.cpp:360 -> VrMove::ammoPouch ->
vr_server.cpp:664). Drawn a tick later, they are a tick older. Check 0 and check 1 give the same hashes
(0042de6b57046121 / 04962d8888d1bf4e), so the audit itself changes nothing.

### What it cannot see yet

- Reads through kept pointers that never switch VMs: `sv_player->v`, `svs.clients[0].edict`, `sv.qcvm.edicts`
  arithmetic (vr_held.cpp:1288), Box3D's world (the bodies' transforms, climbBody) from the drawing.
- Paths the mock does not take: held weapons and props, climbing, foe grabs, the tool gun, the gadget, menus' live
  previews, the spectator camera, demos, multiplayer.
- Other state the drawing writes and the tick reads: module-level globals (smoothed poses), `mem::Scratch` buffers of
  files with both server and drawing code, `host_frametime` (the tick sets it to the tick's length; the drawing reads
  it: about 40 files).

## Steps left (each keeps the hashes; the setting stays 0 until the last)

1. **Complete audit.** Under `vr_server_thread_check 3`, protect the edicts' block (page-aligned in that mode) with
   `PAGE_NOACCESS` during `screen`/`particles`, note the faulting stack in a vectored handler, unprotect, single-step,
   protect again: every read, through any pointer. The same for Box3D's world (its allocator's pages). Run it over
   his recorded motion takes (vr_motion_play) as well as the mock scenes.
2. **Feed-back fixes** so check 2 keeps `vr_bench_statehash`: the ammo pouch's rounds (and the holsters', load
   ports' and anything else `VrMove` takes from the drawing) computed in CL_SendCmd from the frame's own pose, not
   from the last drawing.
3. **Render-side snapshot.** At CL_ReadFromServer (main thread, worker idle), a module copies the few server fields it
   draws from (portal gates, the player's flashlight and view fields, the shadow casters' flags) into client-side
   state; the drawing reads only that. bloodstream's traces go to the client's world (CL_ trace). Each site's
   VR_SERVER_ACCESS then disappears from the report.
4. **host_frametime split**: the tick's code reads its own `sv_frametime` (set in the tick loop), the drawing keeps
   `host_frametime`; or the worker sets a thread-local copy and every server-side `host_frametime` read goes through
   it. Checked by the audit (a new choke point on the old name in server files).
5. **Console and cvars from the worker**: Con_Printf off the main thread queues the line for the next frame (the
   console's buffer is main-thread only), Cvar_Set/Cbuf from QuakeC queued likewise, or the tick joined first.
6. **The threaded tick**, `vr_server_thread 1`: for frames of exactly one tick (more, after a hitch: on the main thread
   as now), CL_SendCmd on the main thread, the tick posted to the job pool (one long job, like S_MixKick), joined
   before CL_ReadFromServer and before any command, menu, save, load, map change, disconnect or console command that
   touches the server (a `VR_ServerJoin()` at Cbuf_Execute, Host_ShutdownServer, SV_SpawnServer, the save thread),
   and at every remaining VR_SERVER_ACCESS (the fence: correct, if slower, for a site not yet moved). Checks for
   default on: check 2's hashes equal check 0's on the bench's combat, physics and teleporter scenarios and hip1m1;
   the fence never taken in the report; `vr_zone_threadcheck 1` silent (the tick allocates from the zone: that needs
   the zone's lock or the tick's own arena first); eval.sh; Tracy: the main thread's server zone gone, frame time.
7. **Latency.** The world (not the head or hands) is drawn one tick late in the threaded order. He decides whether
   that is worth ~1 ms of main thread; `vr_server_thread 0` keeps today's order.

Expected gain once done: the main thread loses the tick (0.96-1.07 ms a frame on average) and gains the join's wait,
which is about nothing while a tick (about 2 ms on hip1m1) is shorter than the screen (2.8 ms on combat_48); a long
tick (p99 3.2 ms) waits out the difference.

The portal sites are most of the calls and one module (vr_portals.cpp's gates): the snapshot of step 3 for them alone
removes over 95 % of the drawing's switches into the server VM.
