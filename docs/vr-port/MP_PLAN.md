# Multiplayer: verified state, architecture, phase plan, test harness

The multiplayer programme (the author, 2026-11-14): the whole game playable in coop and deathmatch, campaigns start to
finish in coop (level changes, saves, loads), clients fetching the maps a server needs; every VR feature synced
(bodies, hands, fingers, holsters, weapons, interactions between players: drops and pickups, melee, parries, shoves,
climbing, crouch, prone); flat and VR players together; per-player settings seen by others as chosen, balance settings
dictated by the server; proximity voice chat on the spatial audio; deterministic, synced physics and navigation;
bespoke delta compression with general compression on top; client prediction for VR responsiveness; all of it test
driven, with end-to-end tests of several real games at once under simulated lag and loss.

This file is the plan. MULTIPLAYER.md (2026-09-30) is the older audit, file by file; where the two differ, this file
is the newer (each claim below was re-checked on 2026-11-14 at `origin/next` 17de8a86, branch `agent/mp`).

- **[run]**: measured with the harness (below) on 2026-11-14.
- **[code]**: re-read in the code on 2026-11-14 (file:line at that commit).
- **[old]**: from MULTIPLAYER.md, not re-checked.

## 1. Verified state

### 1.1 What works

| Item | State | Evidence |
|---|---|---|
| Protocol | Ironwail's PROTOCOL_RMQ (999) with `PRFL_INT32COORD`/`SHORTANGLE` plus `PRFL_QUAKEVR` (1 << 16) and `PRFL_QUAKEVR_PROGS` (1 << 17). Not negotiated per client: a server with VR progs reads a VR block from every client's move | `vr_api.h:48-49`, `sv_main.c:35,481-483` [code]; "Server using protocol 999 (RMQ)" [run] |
| VR extensions | `svc_quakevr` (39) with 41 sub-commands now (was 17): particles, late precaches, world texts, haptics, hand impacts, casings, wounds, catch blends, ropes, shocks, fire/tracer FX, tips, server rules, collects, view messages, hit reactions, gore sounds, armed humanoids' arm poses, Rig Studio sequences, armour, x-ray, blade wounds, arena messages. Entity bits `U_QVR_*` 21 and 24-30 (scale, scale origin, offset, no-rotate, weapon uid and clip, spin, no magazine, SSG open). Stats `STAT_QVR_*` from 64 | `vr_protocol.hpp:20-77, 102-189` [code] |
| Joining | A dedicated server (`-dedicated N`) or a listen host and remote VR or flat clients join coop e1m1, each its own player entity (1 and 2), each sees the other's | `mp_test.py coop`: 6 runs, all PASS [run] |
| Seeing others move | Another player's step off from rest shows 40-64 ms after his input on loopback, and his own 44-68 ms (a round trip: there is no prediction) | [run] (section 4) |
| Under lag | Client B behind 150 ms each way and 2% loss: B sees A move off after 214-221 ms, A sees B after 206-214 ms, B sees himself after 357-368 ms (2 x 150 + the loopback's 60) | [run] |
| Level changes | `changelevel e1m2` on the server takes both clients along; they see each other again | [run] |
| Dedicated server quit | Exits 0 (MULTIPLAYER.md's crash on quit, `vr_imgprefetch` workers, is gone) | 6 runs [run] |
| Flat with VR | A flatscreen client (`-flatscreenmode`) joins a VR host, walks (+forward), is seen; the server knows him per player (`.vr_flatscreen`, the flat crouch bit) | `vr_server.cpp:795-819` [code]; listen run [run] |
| Server rules | The melee timing settings go to every client (`QVR_SVC_RULES`); a remote client's menu shows them dimmed | `vr_serverrules.cpp:17-27` [code] |
| Late precaches | Broadcast to every client (`QVR_SVC_PRECACHE_*`) | `vr_server.cpp:1253-1254` [code] |
| Per-client server state | Climbing, Box3D hand bodies and standing, carry, melee, force grab, rope tags are kept per player | [old], spot-checked `vr_box3d_hands.cpp:421,643` [code] |
| Server tick | A steady 72 Hz for listen and dedicated servers (`host_fixedtick 1`) | `host.c:67` [code] |
| Bots | FrikBot is compiled in and hooked (`BotInit`, `BotFrame`, `BotPreFrame`/`PostFrame`, rankings) | `QC/progs.src:372-379`, `QC/world.qc:303,606`, `QC/client.qc:1858,2195,2317` [code] |

### 1.2 What is broken

| Problem | Where | Evidence |
|---|---|---|
| **Two players from one IP kicked each other (fixed in this phase).** NetQuake took a connect from a known IP on another port as that player back from a crash and closed his connection: two players behind one router, or two games on one machine, could not both play | `net_dgrm.c:1021-1053` (now compares the port too) | [run]: before the fix, B's connect dropped A ("Client unconnected removed") |
| **Armed humanoids' arm poses go to every client every server frame, unculled. (fixed in B2: a remote client gets only the poses of those in its PVS, vr_net_cullposes; B idle 15.5-19.6 KB/s down, was 97-100)** `VR_AG_SendPose` writes `QVR_SVC_ARMPOSE` to `MSG_BROADCAST`: about 16 messages of 20-104 B, about 1.2 KB of every 1.4 KB datagram on e1m1, wherever the client is. The shared datagram is copied into a client's packet only whole: a map with more armed humanoids loses every broadcast sound, particle and pose for remote clients | `QC/vr_armedgrunt_pose.qc:1213-1230`; `sv_main.c:1285` | [run]: `cl_shownet 2` on B; down 97 KB/s, mean 1350 B a packet, of which entities 124 B (`vr_net_stats`) |
| **The client's move is 640 B, 72 a second (45 KB/s up).** The VR block grew from 291 B to 613 B (+32 with the lamp lit), all floats: 2 x 11 hand vectors, magazine boxes, pouch, load ports, launch offsets... | `vr_move.cpp:50-115` | [run]: proxy, up mean 640 B, 71 pkts/s |
| **Reliable messages go out in datagrams of up to 64 KB (fixed in B2: chunks of at most 1200 B, 16 in flight; largest packet down now 1405 B, an unreliable datagram)** (`MAX_DATAGRAM` 64000): a 15 KB UDP datagram at signon. Over the internet it is IP-fragmented; one lost fragment loses it all, and some routers drop fragments | `quakedef.h:100`, `net_dgrm.c:185-225` | [run]: proxy, largest down 14998 B |
| **The server uses the host's state for every player.** Listen-host shortcuts: `vr_grip.cpp:439` (palm frame, player 1 only), `vr_box3d_hands.cpp:421` (fist spheres, player 1), `vr_box3d_hands.cpp:643` (drawn weapon hull, player 1), `vr_physics.cpp:379` (fist grabs, player 1), `vr_builtins_held.inc:77,172,503,850,858` (`weaponhotspot`/drawn pose, player 1 only), `vr_carry2h.cpp:273,326`, `vr_foegrab.cpp:208,216,530,701`, `vr_climb.cpp:1318-1326` (host's body calibration and scale), `vr_held.cpp:865,1232` (host's drawn fists), `vr_hitmodel.cpp:453,457` (host's lerp cvars) | [code] |
| **Every setting QC reads is the host's.** QC reads 1851 distinct cvars (`cvar_hget`), personal ones among them (`vr_gunangle`, holster, reload, grip, throw, force-grab modes...): one value for all players | `QC/*.qc` [code] |
| **Client code reads the local server.** `worldtrace::move` is `SV_Move` (teleport aim, crosshair, flashlight, hand collision): nothing on a remote client; `serverInHand` and the 2H hold read `sv.qcvm` | `vr_trace.cpp:37`, `vr_held.cpp:1854,2235` [code] |
| **Last move wins. (fixed in B2: the buttons and VR bits of every move since the last frame ORed; melee sub-tick samples still open)** Each move overwrites the edict's fields and the VR bits (`bits.received =`), so a press and release inside one tick is lost, and the melee speeds see one sample a tick | `vr_server.cpp:758,795` [code] |
| **Per-client state survives a slot change. (fixed in B2: VR_ClientReset on connect and drop)** `clientMoves` is cleared only on a world reset; a player taking a freed slot inherits its last state until his first move | `vr_server.cpp:1303-1306` [code] |
| **The mock's autopilot is listen-only.** `vr_mock_walk_to`/`turn_to` read the server's edict 1; a remote test client walks with `vr_mock_stick` instead | `vr_backend_mock.cpp:1490` [code] |

### 1.3 What is missing

| Missing | Notes |
|---|---|
| Remote VR avatars | B2: each player's head, hands (positions, angles) and weapons reach the clients that see him (QVR_SVC_AVATAR, vr_mpavatar.cpp; mp_avatar: within 0.10 units). Still missing: fingers (not in the move), drawing them (body IK, held weapon models), holsters, lamp, climb holds, calibration |
| Downloads | No `svc_download`/chunked download; the `Download` in `host_cmd.c:581` is the map library's HTTPS fetch (`vr_mapindex.cpp`, `vr_mapinstall.cpp`) [code] |
| Coop saves | `save` refuses with more than one player ("Can't save multiplayer games", `host_cmd.c:2571`); autoload too (`host_cmd.c:2196`) [code] |
| Single-player-only systems | 33 `svs.maxclients == 1` gates in `Quake/vr`: bullet time, timescale, crates placed about, flashlight's save state, relight, Setup, holster setup, the toolgun, arena cards, the chainsaw's cord state, live preview, late precache of `vr_prepare` [code] |
| Per-client preferences, rig, body data up | No userinfo/setinfo/prefs message; the server cannot know a player's handedness, height or holsters [code] |
| Prediction, lag compensation, interpolation buffer | NetQuake's two-message lerp only (`cl_main.c`); hits judged where the server has things now [old, code unchanged] |
| Delta compression against acknowledged state | Entities are deltas against their baseline only; `U_QVR_*` extras resent every frame [old] |
| A lag simulator | None in the engine; the harness's `mp_proxy.py` adds it outside |
| Voice | Nothing (Steam Audio's spatial mixer is there to place voices: `vr_audio.cpp`) |

## 2. Research and the chosen architecture

### 2.1 What the others do

- **NetQuake** (ours today): a server-authoritative world; every visible entity sent each frame as a difference from its
  baseline; the client draws a lerp between the last two messages; no prediction. Simple and robust on a LAN; over the
  internet every own move waits a round trip.
- **QuakeWorld** (and ezQuake): the same authority, plus client-side prediction of the local player with the
  movement code shared by client and server (`pmove`), replaying the moves the server has not acknowledged on top of
  its latest state; each packet carries the last three moves (loss costs nothing); entities are delta-coded against
  the last frame the client acknowledged; `qport` tells clients behind one NAT apart; a `rate` caps each client's
  bytes per second. ezQuake adds better prediction of other players and smoother interpolation.
- **FTEQW / QSS (and QSS-M)**: protocol extensions negotiated per client (FTE's `PEXT_*`, replacement deltas against
  acknowledged frames), chunked downloads, CSQC with input frames for prediction in game code. Quakespasm-VR
  (ObeseCatLord, GPL, MULTIPLAYER.md's reference) ships a shared pmove with CSQC prediction, co-op rules, ICE/WebRTC
  NAT traversal and Opus voice chat with VAD, a jitter buffer and 3D placement: the closest prior art, to borrow from
  with its headers and credit.
- **DarkPlaces**: an entity frame database per client (deltas against acknowledged frames), client movement
  prediction, CSQC.
- **Quake III**: the reference design. The server keeps a ring of snapshots per client and sends each as a delta
  against the last one the client acknowledged (or the full state); usercmds are delta-coded and duplicated; the whole
  packet is Huffman-coded with a fixed table; the client interpolates between buffered snapshots a little in the past
  and predicts its own player with the shared movement code. The "unlagged" work (and Source later) added lag
  compensation: the server rewinds other players' hit boxes to the shooter's view time.
- **Rollback (GGPO, fighting games, Overwatch's local simulation)**: deterministic simulation of all inputs with
  rewind and replay on late input. Exact, but it needs the whole game deterministic and cheap to replay: our Box3D
  world, QC monsters and navmesh at 72 Hz are neither cheap to replay many ticks nor deterministic across machines.
  We use rollback only where it pays: the local player's movement (QW-style prediction is rollback of one entity).
- **VR multiplayer games** (Pavlov, H3VR's H3MP, the Boneworks/Blade & Sorcery multiplayer mods): the player's head
  and hands are authoritative on the owning client (never wait a round trip for your own hands) and replicated to
  others at a high rate, quantized; objects a player holds are drawn in his hand on his client; contested physics
  stays with one authority (the server, or an owner with hand-over). Quake2Quest and QuestZDoom keep their engines'
  vanilla netcode; their VR state is not replicated.

### 2.2 The architecture

1. **One authority: the server.** QC, monsters, navmesh, Box3D, hits, pickups, melee judgement and parries all stay
   on the server at its fixed 72 Hz tick. Clients never run their own copy of shared physics (two Box3D worlds
   diverge). Deterministic means: the server's run is reproducible (same inputs, same hashes: the existing audit
   tools), and every client shows the server's results.
2. **The owner's body is the owner's.** Head, hands, fingers, two-hand aim, throw estimates and the weight spring stay
   computed on the owning client at the headset's rate (as now) and are sent up; the server validates (caps on hand
   distance, throw speed, hotspot reach) instead of second-guessing them.
3. **Moves up: every move, quantized, delta-coded, duplicated.** A move carries its client time; positions relative
   to the player's origin as int16 eighths, angles as shorts or smallest-three quaternions, velocities scaled; the
   rarely changing parts (magazine boxes, pouch, load ports, launch offsets) only when they change; the last two moves
   repeated in each packet; the server runs every move (sub-tick samples for melee), ORs edge bits.
4. **Snapshots down: per client, delta against the last acknowledged.** The client acknowledges the last snapshot it
   received in each move; the server keeps a short ring per client and codes entities, player states and VR avatar
   blocks as deltas against that snapshot (or the baseline). Unacknowledged reliable data stays on NetQuake's reliable
   channel, cut to the MTU.
5. **A VR player-state block per visible player**: head (relative to origin), both hands (position, rotation),
   finger curls, held items and grip ids, holster contents, flags (flashlight, climbing holds, 2H grip, crouch,
   prone, flat player), at the snapshot rate when near, less when far (a priority accumulator, as Q3/Source).
   Calibration (height, arm length, handedness, gloves, body appearance) goes once and on change, reliable. Remote
   bodies are drawn with the same arm solver and skinning as the armed humanoids (`vr_agarms.cpp`, `vr_hitreact.cpp`
   `parseArmPose`): one code path for grunts, remote players and, later, the flat player's own body.
6. **The armed humanoids' poses join the snapshot**: per client, PVS-culled, delta-coded, instead of the broadcast.
7. **Prediction and reconciliation of the local player's movement**: the walk/swim/crouch/prone physics and the
   narrow hulls shared by client and server (one C++ module both call), the server's acknowledged state plus a replay
   of the unacknowledged moves; climbing pull-ups and the grapple's swing next; held props already drawn in the hand.
8. **Interpolation of everything else** a fixed delay behind (about two ticks, adaptive to jitter), positions lerped,
   rotations slerped as quaternions (props, ragdolls).
9. **Lag compensation**: the server keeps the last 250 ms of hit-model and hand transforms per entity and judges a
   client's hitscan, melee, parry and shove against the world as that client saw it (capped, favouring the attacker
   within the cap).
10. **Settings in three kinds**: server rules (balance: damage, presets, armed humanoids, mutators, melee timing,
    stamina...: `QVR_SVC_RULES` grown to all of them, their menu rows dimmed on a remote client), per-player
    preferences the server needs (handedness, holster/grip/reload modes, gun angle...: a prefs message up, stored per
    client, a QC builtin `clientpref(player, name)` in place of the global reads), and cosmetics others see (body,
    gloves, height: up once, out to others with the avatar calibration).
11. **Compression in layers**: quantize and delta first (the big win), then bit-packing, then an entropy stage over
    the whole packet (a static Huffman/range coder with tables trained from harness traces: deterministic, no
    dependency, as Q3); a per-client `rate` cap with the priority accumulator deciding what waits.
12. **Downloads**: the server names each map's package (the map library's sha256) so a client installs it through
    the existing trusted installer (`vr_mapinstall.cpp`); anything else comes in MTU-sized chunks from the server,
    allowed paths and sizes only (maps, models, sounds, textures; never a .cfg, progs replacement or library).

### 2.3 Bandwidth budget (72 Hz, per client)

| Link | Now [run] | Target | How |
|---|---|---|---|
| Up (moves) | 45 KB/s (640 B x 71) | 8-12 KB/s | about 60 B a quantized delta move + two repeats |
| Down, 2 players idle, e1m1 | 97 KB/s (1350 B x 72) | 6-10 KB/s | arm poses culled and delta-coded; acked deltas |
| Down, 4 players and 20 monsters in sight, combat | (datagram full: entities dropped) | 25-35 KB/s | about 40 B a player block, 8-12 B a moving monster, 30 B an armed pose delta, events; far things at 36/18 Hz |
| Reliable | up to 64 KB datagrams | at most 1200 B each | chunked to the MTU |

A 1 Mbit/s up, 2 Mbit/s down home line carries 4 players with room to spare at the targets; 8 players need the
priority accumulator to thin far players and monsters.

### 2.4 Risks

- **Scope.** The listen-host shortcuts (about 25 sites), the 1851 QC cvar reads and 33 single-player gates are spread
  over the whole game: each phase converts one area with a harness test before it moves on.
- **Single player must not change.** Every phase runs the single-player regression (the coordinator's suites) and
  the hash audits; protocol changes stay behind the per-client negotiation so demos and single player keep working.
- **CPU of the tests.** Three or four games at once (each about 2-3 GB, a 17 s first map load at low priority): the
  harness runs below normal priority, idle while the author plays; real-time tests use generous bounds (300 ms + the
  added lag) and conditions, never sleeps.
- **Prediction mismatches** where server movement has extras the client cannot see (Box3D standing, pushes, climb
  holds): predict plain movement first, snap smoothly on mismatch, extend case by case with tests that compare the
  predicted and the authoritative paths.
- **Voice needs third-party code** (Opus, a noise suppressor such as RNNoise): both BSD-licensed, to be vendored as
  source; the author's go-ahead before adding them (see Decisions).
- **NAT traversal** (ICE/WebRTC) needs TLS libraries: deferred to polish, optional.
- **Save format**: coop saves add per-player blocks; old saves must still load (versioned).

## 3. Phases

Each phase is shippable to `agent/mp` on its own, starts with its harness scenarios (failing first), and keeps single
player unchanged.

| Phase | Scope | Tests first |
|---|---|---|
| **B1** (done) | Audit, design, plan; the harness; first e2e tests; the same-IP kick fixed | `mp_test.py suite` |
| **B2** (mostly done; the rest is B2b) | Protocol hygiene and VR player state for two players: per-client protocol version in the handshake (a clean reject on mismatch); every move processed, edge bits ORed, per-client state reset on connect/drop; reliable chunks at most 1200 B; arm poses per client, PVS-culled; the VR player-state block and calibration message; remote players drawn with the armed humanoids' arm solver and skin on the player body (head, hands, fingers, held weapons) | `mp_avatar`: B sees A's mock hands within 2 units of where A's server has them, fingers within a step, the held weapon's model; `mp_bandwidth`: down under 25 KB/s idle on e1m1; `mp_coop` stays green |
| **B2b** | The rest of B2: finger curls in the move and the avatar; remote players drawn with the player's own body (vr_avatar.cpp's IK from head and hands) or the armed humanoids' arm solver (vr_agarms.cpp), whichever shares more, and their held weapons' models in their hands; the avatar calibration message (height, arm length, handedness, gloves, body), reliable on change; melee sub-tick samples from every move | `mp_avatar` grown: fingers within a step, the weapon model drawn in B's view at A's hand (a screenshot pair) |
| **B3** | Per-client data: the prefs message and `clientpref`; the rig (palm frame, fists, hotspots, weapon hull) and body calibration up; the ~25 host shortcuts replaced; client-side traces for teleport, crosshair, flashlight, hand collision | `mp_prefs` (A left-handed, B right-handed: each judged by his own), `mp_teleport` (a remote client teleports), `mp_grab` (B's own fist grabs) |
| **B4** | Coop campaign flow: coop saves and loads (per-player blocks, versioned), autosaves, level transitions with inventories, intermission, deaths and respawns, map download (package names through the map installer; chunked fallback) | `mp_campaign` (e1m1 to e1m3 with saves and a load, both players' inventories), `mp_download` (B lacks a map: installs it, joins) |
| **B5** | Interactions between players: drop a weapon / the other picks it up; force grab of world weapons in multiplayer; melee, parries and shoves between players; climbing, crouch, prone, jump, punch synced and judged per player | one scenario per interaction, each from both sides |
| **B6** | Server rules and cosmetics: every balance setting a rule (presets, armed humanoids, damage, mutators, stamina, grapple...), rule pages dimmed on a remote client; holster positions, height, seated/standing, gloves, body seen by others | `mp_rules`, `mp_cosmetics` (B's height and gloves seen by A) |
| **B7** | Snapshots: acknowledged per-client snapshots and deltas for entities, players and avatars; quantized delta moves with repeats; interpolation buffer with quaternion slerp | `mp_loss` (10% loss: positions still continuous), byte counts per scenario |
| **B8** | Prediction and lag compensation: shared movement code, reconciliation, then climbing and grapple; rewind of hit models and hands for hitscan, melee, parry, shove | `mp_predict` (own move off at frame 0 under 150 ms lag; corrections under 1 unit on flat ground), `mp_lagcomp` (a hit on a moving target at 150 ms lands) |
| **B9** | Compression and rate: entropy stage with trained tables, per-client rate, priority accumulator | byte budgets per scenario from section 2.3 |
| **B10** | Proximity voice chat: capture, Opus, VAD, noise suppression, push-to-talk, a jitter buffer, voices placed by Steam Audio at the speaker's head; VR and flat; menu rows | `mp_voice` (a test tone from A heard at B, louder near, delay under 250 ms) |
| **B11** | Deathmatch and bots: frags, respawns, DM item rules with VR weapons, FrikBot with VR-aware bodies | `mp_dm` (two clients and two bots, a fraglimit reached) |
| **B12** | Determinism: Box3D, navmesh and pathfinding hashes equal across runs with several clients; the server-thread audit in multiplayer | hash scenarios |
| **B13** | Polish and performance: ICE/NAT traversal (optional), server browser, latency HUD, CPU of many players | soak tests |

## 4. Test harness

`Misc/quakevr/mp/` (standard library Python only):

| File | What |
|---|---|
| `mp_harness.py` | `Session` (a server, clients, proxies, checks, the verdict and `summary.json`), `Instance` (one game: its args, `cmd()` into its inbox, `wait()`/`quiet()` on its log with safety timeouts, `stop()` with its exit code) |
| `mp_proxy.py` | The UDP proxy: lag each way, jitter, loss of game packets; rewrites the server's accept so the client keeps talking through it; counts packets and bytes (`window()` between two snapshots) |
| `mp_test.py` | The scenarios: `coop`, `suite` (coop, then coop with B at 150 ms and 2% loss) |

The engine's aids (`Quake/vr/vr_mptest.cpp`):

- `-cmdinbox <file>`: each frame the lines appended to that file run as console commands (the harness's way to
  drive a game; works on a dedicated server too).
- `-condebugfile <path>`: the `-condebug` log written there (several games share one basedir).
- `vr_mp_watch 1` (Debug > Reports > Network: Watch Players): wall-clock stamped `mp_event joined/left`, `mp_seen`,
  `mp_move` (off from rest, farther than `vr_mp_watch_dist` 2), `mp_pos` (every 32 units), `mp_stop`.
- `vr_mp_mark <label>`, `vr_mp_status` (Debug > Reports > Network: Players Seen): a timestamped step; the players
  seen and, on a server, its clients (`mp_server: map=... spawned=N`).
- A dedicated server started with `QVR_TEST_HIDDEN` opens no console window (`sys_sdl_win.c`).

Running it (the kit's base for this worktree; `--reset-autoexec` empties its `id1/autoexec.cfg`, which the kit's
runs leave with their script):

    python Misc/quakevr/mp/mp_test.py suite --base C:/OHWorkspace/qvr-kit/bases/mp/qbase --reset-autoexec
    python Misc/quakevr/mp/mp_test.py coop --listen --flat B --port 27600 --base ...    (a listen host, flat B)
    python Misc/quakevr/mp/mp_test.py coop --lag 150 --jitter 20 --loss 0.02 --base ...

Each run writes `scratch/mp/<test>_<time>/` (logs, inboxes, `summary.json` with checks and metrics) and ends with
`mp_coop: N passed, M failed`. The games are real-time by nature (separate processes over UDP share the wall clock:
`vr_fixed_frames 0`), but every step waits on a log condition; timeouts are safety nets that fail the step.

First results (2026-11-14, release no-LTO build, loopback):

| Run | Joined, seen | A walks: B sees / A sees himself | B walks: A sees / B sees himself | changelevel | Quit | B's link in play |
|---|---|---|---|---|---|---|
| dedicated, VR + VR | PASS | 59-60 / 56-60 ms | 40-57 / 44-60 ms | PASS | exit 0 x3 | up 45.5 KB/s (640 B), down 97 KB/s (1347 B) |
| dedicated, B at 150 ms + 2% loss | PASS | 215-221 / 52-68 ms | 206-214 / 357-368 ms | PASS | exit 0 x3 | up 44 KB/s, down 97 KB/s |
| listen host A, flat B | PASS | 55 / 56 ms | 64 / 64 ms | PASS | exit 0 x2 | up 45 KB/s, down 98 KB/s |


### B2 results (2026-11-14, release no-LTO build, loopback; `mp_test.py suite`: all 5 scenarios pass)

| Scenario | Result |
|---|---|
| `mp_bandwidth` | B idle on e1m1: down 15.5 KB/s (216 B a datagram) with `vr_net_cullposes 1`, 96.6 KB/s with 0; 19.6-22.5 KB/s with A's avatar added. Largest packet down 1405 B (was 14998 B) |
| `mp_version` | A client with `-mpnetversion 1` is told "This Quake VR server has network version 2, your game 1..."; the server logs it; a right one joins |
| `mp_avatar` | B's copy of A's head and hands within 0.10 units of the server's, the main hand's angles exact, the weapon the same; B has none of himself |
| `mp_coop`, `mp_coop_lag` (150 ms, 2% loss) | Unchanged: pass; B sees A move off in 184-211 ms under the lag; signon over the lossy link with the chunked reliable channel |

What changed:

- `net_dgrm.c`: reliable messages in chunks of at most 1200 B (`DGRM_RELIABLE_CHUNK`), up to 16 in flight
  (`DGRM_RELIABLE_WINDOW`); the receiver is NetQuake's (acks counted in order only; a 1 s timeout resends what is in
  flight, go back N). Works with unchanged peers in both directions.
- The connect request carries "QVR" and `QVR_NET_VERSION` (2, `vr_api.h`): a server with the VR protocol rejects a
  client of another version or none, with the reason; `qsocket_t.qvrNetVersion` keeps it per client.
  `-mpnetversion N` fakes another (the test).
- `sv_user.c`, `vr_server.cpp`: every move since the server's last frame counts (buttons and VR bits ORed:
  `VR_MoveMerges`); `VR_ClientReset` clears a slot's moves and bits on connect and drop. (Single player too: a
  press and release between two ticks was lost.)
- Entity-scoped messages (`vr_server.cpp` ScopedRoom, `server::scopedBegin/End`): written after the broadcast into
  each remote client's datagram when the entity passed its PVS test this frame (`VR_NoteEntityInSight`); the loopback
  client gets them all (single player unchanged). The armed humanoids' poses (`agposesend`) use it
  (`vr_net_cullposes`, Debug > Reports > Network: Cull Poses); so does the avatar.
- `QVR_SVC_AVATAR` (42): each spawned player's head and hands from his last move (eighths of a unit from his origin,
  16-bit angles) and weapons, about 47 B, to the clients that see him; multiplayer only (`svs.maxclients > 1`).
  `vr_mpavatar.cpp` keeps them per player; `vr_mp_avatars` (Debug > Reports > Network: Player Avatars) prints them
  and the server's.

Open items (B2b and later):

- Nothing draws the avatar yet. `vr_avatar.cpp` already poses the local player's skinned body from head and hands:
  feeding it a remote player's avatar is likely the shortest path to one code path for both (the plan said the
  armed humanoids' solver; B2b decides by what shares more).
- Finger curls are not in the move (`VrMove` has none): add them (5 bytes a hand) and to the avatar.
- The up link is still 45 KB/s (640 B a move): B7's quantized delta moves.
- The avatar is sent whole every frame (about 4 KB/s per visible player): B7's deltas and B9's priority.
- Fast retransmit for the reliable window (a lost chunk waits the 1 s timeout): with B7.

## Decisions

- The harness is Python with an in-engine inbox rather than stdin or RCON: it drives GUI clients and dedicated servers
  the same way, needs no network control channel, and stays inert unless `-cmdinbox` is given.
- The lag simulator is a proxy outside the engine (no engine cvars): it tests the real code paths unchanged, and it
  also measures each link's bytes.
- The same-IP fix compares the port too; a client back from a crash on a new port leaves a ghost until the timeout
  (QuakeWorld's `qport` is the full answer: B7, with the snapshot protocol).
- Voice chat (B10) needs Opus and a noise suppressor vendored as source: to be confirmed by the author before B10.

Next phase: B2b — remote players drawn: finger curls in the move and the avatar; the remote player's body posed from his avatar (vr_avatar.cpp's body IK, or the armed humanoids' arm solver: whichever shares more) with his held weapons' models in his hands; the avatar calibration message (height, arm length, handedness, gloves, body); melee sub-tick samples from every move; `mp_avatar` grown (fingers, a screenshot pair of A's hands at B).
