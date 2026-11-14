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
| **B2b** (hands done; the rest is B2c) | The rest of B2: finger curls in the move and the avatar; remote players drawn with the player's own body (vr_avatar.cpp's IK from head and hands) or the armed humanoids' arm solver (vr_agarms.cpp), whichever shares more, and their held weapons' models in their hands; the avatar calibration message (height, arm length, handedness, gloves, body), reliable on change; melee sub-tick samples from every move | `mp_avatar` grown: fingers within a step, the weapon model drawn in B's view at A's hand (a screenshot pair) |
| **B2c** (weapon and calibration done; the rest is B2d) | The harness's weapon in hand and `mp_avatar`'s weapon checks with a screenshot pair; the avatar calibration message; the remote body; melee sub-tick samples | `mp_avatar` 17/0 (weapon checks live), `mp_avatarcal` |
| **B2d** (done; pauldrons and gloves to B6) | The remote player's body posed from his avatar (vr_avatar.cpp's IK and skinning made per instance: one code path for the local body and remote ones), sized and dressed by his calibration (height, arm length, build, bracers, pauldrons, gloves) | `mp_body` (B's copy of A's body: wrists at A's drawn hands, head at his head, a screenshot pair; A's height changed: B's copy taller) |
| **B3** (prefs and fists done; the rest is B3b) | Per-client data: the prefs message and `clientpref`; the rig (palm frame, fists, hotspots, weapon hull) and body calibration up; the ~25 host shortcuts replaced; client-side traces for teleport, crosshair, flashlight, hand collision | `mp_prefs` (A left-handed, B right-handed: each judged by his own), `mp_teleport` (a remote client teleports), `mp_grab` (B's own fist grabs) |
| **B3b** (the suite reliable, the take-back a check; the rest is B3c) | The rest of B3: the palm frame (vr_grip.cpp), weaponhotspot and the drawn pose (vr_builtins_held.inc), the weapon hull, the 2H carry and foe grab frames, the ragdoll grab fit and the crawl's hand bottom (the fists' other users), body calibration up (his measured arms and shoulders, for his copy's shoulders); client-side traces for teleport, crosshair, flashlight, hand collision; why B's dropped shotgun is not always taken back | `mp_teleport`, `mp_grab` grown (taken back every run), `mp_body` (shoulders within 0.2 with A calibrated) |
| **B3c** (palm grab and body calibration up done; the rest is B3d) | The rest of B3b: the palm frame (vr_grip.cpp), weaponhotspot and the drawn pose (vr_builtins_held.inc), the weapon hull, the 2H carry and foe grab frames, the ragdoll grab fit and the crawl's hand bottom; body calibration up (his measured arms and shoulders); client-side traces for teleport, crosshair, flashlight, hand collision | `mp_teleport`, `mp_body` (shoulders within 0.2 with A calibrated), `mp_grab` grown (a 2H carry, a foe grab) |
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

### B2b results (2026-11-14, release no-LTO build, loopback; `mp_test.py suite`: all 5 scenarios pass)

| Scenario | Result |
|---|---|
| `mp_avatar` | B's copy of A's head and drawn hands 0.00 units from the server's, the main hand's angles exact; A closes his off hand (the mock's grip): its 15 joint curls at B equal the server's (mean 4.0 frames); B draws both of A's jointed hands where A drew them (0.00 units); B idle with A's avatar: down 19.3 KB/s; the server sent 39 avatars and skipped 149 as unchanged |
| `mp_bandwidth` | B idle on e1m1: down 16.6 KB/s (was 15.5-22.5); up 50 KB/s, 702 B a move (was 640: the two drawn hands) |
| `mp_coop`, `mp_coop_lag` | Unchanged: pass (A seen moving off in 55 ms, 218 ms at 150 ms lag) |

What changed:

- The move carries each hand as its client drew it (`VrDrawnHand`, `vr_move.cpp` `writeDrawnHand`): the jointed hand's
  entity place and turn (after the palm's fit and the firing motion), its 15 joint curls as posed (the grasp's wrap of
  what it holds included; sixteenths of a frame) and the thumb's turn, and the held weapon's model (its
  `model_precache` index), frame, skin and place: 32 B a bare hand, 48 B with a weapon; multiplayer only
  (`view::netHands`). The avatar passes them on instead of the controllers' poses.
- Decision (what shares more): other players' hands are drawn from the pose their own client drew, not solved again:
  the same handrig pose and skin (`view::remoteHandSkin`: the rig's fingers' shifts as this client's settings put them)
  and no grasp solve, wall push or two-handed placement of their own. Exact, cheap, and every local feature (grips,
  recoil, blades in bodies) shows as it was drawn. The body (vr_avatar.cpp's IK) is still to come: its state is the
  local player's (one posed skeleton read by holsters, pauldrons, self-collision), so a remote body needs it made per
  instance first (B2c).
- `vr_mpavatar.cpp` draws each other player's hands and weapons on his entity as drawn (its lerp; an avatar older than
  1 s or an entity not in the last message is not drawn): `VR_AddMpAvatars` (view.c, after the view's entities),
  `mpavatar::bonePoses` (VR_AliasBonePoses), `mpavatar::mirrored` (VR_AliasMirrored). `vr_mp_avatar_draw` 1 (Debug >
  Reports > Network: Draw Player Hands).
- An avatar unchanged since it was last sent goes again only every `vr_net_avatar_keyframe` (8) frames (Debug > Reports
  > Network: Avatar Keyframe): an idle player costs an eighth. Its offsets are from his entity's origin, so it rides
  his entity between messages.
- `QVR_NET_VERSION` 3. `vr_mp_avatars` prints the curls, what was drawn (`mp_avatar_drawn:`), the client's own hands
  (`mp_avatar_own:`) and the server's counts (`mp_avatar_sent:`). The harness stops its games when a scenario raises.

Open items (B2c and later):

- No weapon reaches a hand in the harness: `impulse 154` (and `vr_test_give_wid 4; impulse 149` with the server's cvar
  set) leaves A's main hand empty in coop on a dedicated server, and the single-player kit run shows no weapon index
  either (`mp_avatar_own: ... wmdl=0 0`): either the give does not take, or the drawn VR weapon model is not one the
  server precached (`netHands` sends only precached models). The weapon checks in `mp_avatar` are written and skip
  until it does. First thing of B2c: find which, then a screenshot pair of A's hands and weapon at B.
- The body under the hands (vr_avatar.cpp per instance), the avatar calibration message (height, arm length,
  handedness, gloves, body; reliable on change), melee sub-tick samples from every move: B2c.
- Remote hands use this client's cosmetics (hand size, gloves, nails): B6.
- A held weapon's parts (the super shotgun's barrels, the revolver's crane, pumps, magazines) are drawn as the whole
  model at its frame.

### B2c results (2026-11-14, release no-LTO build, loopback; `mp_test.py suite`: all 6 scenarios pass)

| Scenario | Result |
|---|---|
| `mp_avatar` | 17/0, the weapon checks live: A's main hand holds the given shotgun (server weapon 4); B draws it (`progs/v_shot.mdl`) 0.10 units from where A drew it; the screenshot pair `A_own_hands.png` (A's raised hands and shotgun) and `B_sees_A.png` (B facing A: A's hands and shotgun on his body) in the run's folder |
| `mp_avatarcal` (new) | 9/0: each client has the other's calibration exactly as the other's game has it, the server both; A's glove change reaches B; B leaves: A forgets his; B joins again: each has the other's (A's from B's spawn) |
| `mp_coop`, `mp_coop_lag`, `mp_bandwidth`, `mp_version` | Unchanged: pass |

Single player on the branch: scenario_check PASS (129 scenes, 0 failed), load_models_test 4/0, Release and Debug builds
0 warnings. `vr_run_check` (no canonical script: `scratch/b2c_runcheck.sh` from ROUND25's description) 11/1: run B (melee
only, `map e1m2` after run A) fails the QC game check right at its spawn, with no message; every SP path this phase
touched is a no-op in single player (`cl.maxclients`/`svs.maxclients` 1), so not chased here.

What changed:

- Why the harness's hand was empty: the give took (the server's weapon 4 the next frame), and the default grip-to-hold
  let it fall at once (the mock's main grip was not held). `mp_avatar` holds it; `vr_dumpplayer` takes 1-based clients
  (the scenario asked for 0). `Instance.screenshot(label)` (`mp_harness.py`): the window's image moved into the run's folder.
- The avatar calibration (`QVR_SVC_AVATARCAL` 43; `vr_mpavatar.cpp` calFields): height (`vr_height_calibration`), seated
  height, arm length (`vr_body_arm_length`), world scale, handedness, the gadget's arm, gloves (kind, hue, saturation,
  lightness), nails, skin tone, body mode, build, bracers, pauldrons; integers (mm, thousandths, bytes), clamped on both
  sides. Up as the `vr_avatarcal` client command (reliable, whitelisted in `sv_user.c`) when it changes, at most every
  `vr_net_avatarcal_interval` (0.25 s; Debug > Reports > Network: Calibration Send Interval); the server keeps one per
  slot, passes a change to every client (`sv.reliable_datagram`), all of them to a spawning client
  (`VR_WriteClientSpawnState`), a cleared one when a slot is reset. Multiplayer only. `vr_mp_avatars` prints
  `mp_avatar_cal:` (the others'), `mp_avatar_cal_own:`, `mp_avatar_cal_sv:`. `QVR_NET_VERSION` 4.

Open items:

- Nothing uses the calibration yet: the remote body (B2d) will size and dress itself by it; B6's cosmetics draw remote
  hands with the sender's gloves, nails and skin tone (they still use this client's).
- The body (B2d): `vr_avatar.cpp` keeps one posed body in file-scope state (`posed`, `posedSkeleton`, `armEase`,
  `costTrace`, the motion's ease and follower state, the legs' and climb/lie modules' state) read by holsters, pauldrons
  and self-collision. Plan: gather it into a `BodyInstance` (the local player's one of them), `pose(instance, ...)` from
  head and hands (a remote player's from his avatar: head, the drawn wrists as the hand targets), skinning per instance
  for VR_AliasBonePoses; the local body's hashes and the body tests unchanged first, then remote instances.
- Melee sub-tick samples: the melee tracker (`vr_melee.qc` VR_Melee_Track) already measures speeds over the poses' own
  clock (`vr_posetime`), so bunched moves (jitter, a 90 Hz headset) are not misread as faster or slower; what a merged
  frame loses is the swing's middle poses (its sweep is a chord across them). Feeding each move's pose to the tracker
  changes its sample cadence, single player's at 90/120 Hz too: a melee-tuning change (archived takes, "don't tune
  melee"), so it goes with B5's melee between players, gated to multiplayer, with a test of a swing split over merged moves.
- Our own calibration comes back to us too (the server's broadcast goes to every client): kept, unused, harmless.

### B2d results (2026-11-14, release no-LTO build, loopback; `mp_test.py suite`: all 7 scenarios pass)

| Scenario | Result |
|---|---|
| `mp_body` (new) | 11/0: A on the brawny build, B draws A's body as `progs/vrbody_brawny_bare.mdl` (A's own model) after 1 report; its wrists 0.00 units from A's drawn hands' wrists; its head bone, pelvis, shoulders and wrists within 1.05 units of A's own body (the shoulders: A's own Body Calibration, not sent; the rest within 0.1); ankles 0.17; its scale 29.602 against A's 29.609 (his Height in mm). A raises his Height to 1.95 m: B's copy's scale x1.260 and shoulders x1.269 for the Height's x1.261, its scale A's own (37.313 both) after 2 reports. The pair `B_sees_A_body.png`, `B_sees_A_tall.png` in the run's folder |
| `mp_coop`, `mp_coop_lag`, `mp_bandwidth`, `mp_version`, `mp_avatar`, `mp_avatarcal` | Unchanged: pass. B idle down 16.7 KB/s; up 50.6 KB/s, 710 B a move (was 702: the body pose) |

Single player: the local body posed exactly as before: `scratch/b2d_sp.sh` (e1m1, walking, both hands placed, the body's
skeleton shown in front, vr_body_debug 2) gives the same image pixel for pixel before and after. scenario_check PASS (129
scenes), load_models_test 4/0, vr_run_check 11/1 (run B's QC check at e1m2's spawn, as in B2c), Release and Debug
builds 0 warnings.

What changed:

- `vr_avatar.cpp` `avatar::poseOther(player, Subject, state, model, entity, hand poses)`: pose() itself, with that
  player's body state swapped in for the frame (`Other`: the legs', climbing's and pitch's state, `detail::Motion` and
  `swapMotion` in vr_avatar_legs.cpp; the arms' easing, the forearms' twist, the posed skin, the skeleton) and what pose()
  reads of the local player read from `avatar::Subject` (`detail::metresToUnits`, `eyeHeight`, `bodyScale`, `localFlat`,
  `velocity`, `onGround`: the local values exactly when posing your own): his Height, arm length (its proportions; his
  measured arms are his client's), world scale, velocity (from his entity's last two messages; on the ground unless
  rising or falling fast). Not for him: body tracking, flat-screen play's offsets, the blades his hands are on, the elbow
  pushed out of the local torso, the body's pitch (swimming, lying down) and climbing feet, the footsteps, the traces
  and debug lines. `VR_AliasBonePoses`/shadows draw his body with its head; `modelScale` knows it.
- `bodycal::defaultShoulderOffset/Rise/Swing` (the default body's, for others' shoulders).
- `VrBodyPose` in the move and the avatar (`vr_move.cpp`): the body's yaw (`hands::State::bodyYaw`, the torso estimate),
  the head's height over the floor and the learnt standing height (mm), the lean's hold: 8 B, multiplayer only;
  `QVR_NET_VERSION` 5.
- `vr_mpavatar.cpp` `setupBody`: his body from his head, his drawn jointed hands' wrists (`view::remoteHandPose`, the
  local `drawnHand`'s own code, `handPoseOf`) and his body pose, his build's model with or without bracers
  (`view::bodyModel`, the local body's fallback), in his player model's place in the visible entities. `vr_mp_body` 1
  (Debug > Reports > Network: Draw Player Bodies; 0: the player models). `vr_mp_avatars` prints `mp_body:` (each other
  body: scale, head, head bone, pelvis, wrists, shoulders, ankles, the hand wrists it reached for) and `mp_body_own:`.

Open items:

- Dressing: his pauldrons and gloves (and nails, skin tone) on his body and hands are B6's cosmetics; the armour plates on
  other players' bodies too.
- His Body Calibration (measured arms, shoulders) is not sent: his copy's shoulders 1 unit off when he has calibrated
  (B3, with the rig and body calibration up).
- His swimming tilt, lying down, climbing feet and jumps are not his (upright, walking legs): B5, with the actions synced.
- A flat-screen player's body is posed as a headset player's (no flat-play offsets) at his headset Height, not
  `vr_flat_body_height`: B6.
- Each other body costs about what your own does to pose (the "avatar IK" zone) every frame it is seen: B13's CPU of many
  players.

### B3 results (2026-11-14, release no-LTO build, loopback; `mp_test.py suite`: 8 of 9 pass, `mp_avatar` 14/1 then 17/0 alone)

| Scenario | Result |
|---|---|
| `mp_prefs` (new) | 10/0: the dedicated server has both players' preferences (A's Sticky grip, B's 32 the defaults); A on Sticky keeps a given shotgun with his grips let go, B on Hold drops his (the server's own: Hold), for 5 reports in a row; the other way round after both change (A's falls at once, B keeps a new one) |
| `mp_grab` (new) | 7/0: the server has each player's fists (93 spheres a hand) as his client poses them (their coordinate sum within the 64ths sent: 477.23 against 477.00); B's given shotgun falls from his opened hand; he closes it again: the server's grab test is his own fist's (25-32 touches on the dropped gun, 2-11 cm). Taken back into his hand: 2 runs of 4 (a note, `b_retaken`; open item) |
| the rest | `mp_coop`, `mp_coop_lag`, `mp_bandwidth`, `mp_version`, `mp_avatarcal`, `mp_body`: pass. `mp_avatar`: once 14/1 (A's main hand empty after the give with both grips held: the server's weapon 0), 17/0 run again: an open item |

Single player on the branch: scenario_check PASS (129 scenes), load_models_test 4/0, vr_run_check 12/0 (run B checked as
`3 1 0 1 0` with `vr_run startgun 3`: Melee Only forces the gun to Nothing; `scratch/b3_runcheck.sh`), the Release
build 0 warnings (the Debug build not run this phase). Every path this phase touched is a no-op in single player
(`svs.maxclients` 1; the fist of player 1 the local one, as before).

What changed:

- `vr_clientprefs.cpp` (new): 32 per-player preferences, the settings of how a player's own hands and controls work that
  the server's QC reads: the gun angles (`vr_gunangle`, `vr_gunyaw`, `vr_offhandpitch`, `vr_offhandyaw`,
  `vr_handcal_off_mirror`), grips and holsters (`vr_weapon_grip_mode`, `vr_weapon_cycle_mode`, `vr_holster_mode`,
  `vr_holster_haptics`, `vr_2h_handoff`, `vr_ammo_switch_empty_hand`), the reload modes (`vr_reload_mode` and the 12
  per-weapon ones), `vr_forcegrab_mode`, `vr_handgrenade_arm`, and flat play's (`vr_flat_autoreload`,
  `vr_flat_reload_hold`, `vr_flat_grenade_arm_mode`, `vr_flat_holster_drop_ahead`, `vr_flat_holsters`,
  `vr_flat_saw_autostart`). Up as the `vr_prefs` client command (reliable, whitelisted in `sv_user.c`) when one changes,
  at most every `vr_net_avatarcal_interval`; the server keeps them per slot (forgotten with the slot: `VR_ClientReset`).
- The QC reads them unchanged: `cvar_hget` of a preference, in multiplayer, answers with the value of the player in
  `self`, else in `other` (a touch: the item is `self`), else the server's; `clientpref(player, handle)` (new builtin)
  names the player (used by the grip mode's two reads, `vr_wpnholster.qc`). Single player, demos, a slot whose prefs have
  not come yet: the server's settings, as before (one compare more in `cvar_hget`).
- The rig up: each hand's fist as the client poses it (`held::fist`: the closed jointed hand's spheres in the hand's
  frame) as `vr_rig` commands (64ths of a unit, 24 spheres a command, one a frame, only with room in the reliable buffer;
  again when a sphere moves more than a step). `held::fistOf(player, hand)` / `fistInWorld(player, ...)`: the local fist
  for this game's own player (single player, a listen host's player 1: unchanged), else the one his client sent. The
  server's fist users take it: the grab test (`held::grabTouch`), the armed humanoids' weapons, worn packs, weapons lying
  about, the disarm gap, Box3D's fist spheres (`vr_box3d_hands.cpp`). Before, a remote player on a listen server was
  judged by the host's fist, and on a dedicated server by the old box test.
- `vr_mp_prefs` (Debug > Reports > Network: Player Preferences): this client's preferences off the defaults and its fists
  (`mp_prefs_own:`), and hosting each player's as the server has them (`mp_prefs_sv:`: those unlike the server's own, his
  fists' spheres and sums, the touches his own fist made). `QVR_NET_VERSION` 6.

Decisions:

- A preference's QC read answers for `self`, else `other` (not every read site converted to `clientpref`): the 2048
  settings the QC reads stay one call each, and the player a personal setting is about is `self` in his own think and
  weapon code and `other` in an item's touch. A read whose player is neither (a monster's code about a player) gets the
  server's: convert it to `clientpref` when one matters.
- Not preferences: aim assists (`vr_throw_assist`) and anything balance-like stay the server's (B6's rules); the
  handedness settings (`vr_stick_swap`, `vr_gadget_arm`, `vr_flashlight_side`) are read by the client only (the gadget's
  arm already goes up with the calibration).

Open items (B3b):

- `mp_avatar` once found A's main hand empty after the give with both grips held (14/1; 17/0 again): perhaps A's own
  fists, now known to the dedicated server, closing on something first. Look at it with the take-back item below.
- B's dropped shotgun, his fist on it 2-11 cm (allowed 5-6), was taken back in 2 runs of 4: the handtouch answers yes and
  the QC's take still refuses (a remote-player shortcut in the take, `vr_builtins_held.inc`'s player-1 sites, or the
  dropped gun's regrab rules): chase it first in B3b, then make `mp_grab`'s take a check.
- The other fist users still read the local fist: the ragdoll grab fit (`vr_box3d_ragparts.cpp`, player 1 only), the
  crawl's hand bottom (`vr_physics_water.cpp`), the carry check's debug lines.
- The palm frame, hotspots, drawn pose, weapon hull, 2H carry and foe grab frames, body calibration up, the client-side
  traces: B3b.

### B3b results, part 1 (2026-11-14, release no-LTO build, loopback; `mp_test.py suite`: all 9 scenarios pass)

| Scenario | Result |
|---|---|
| `mp_grab` | 10/0, the take-back a check now: B's dropped shotgun taken back into his hand by his own fist in 10 runs of 10 (9 on the first press, 1 on the second; at most 3 allowed) |
| `mp_avatar` | 17/0 (the give no longer races the grip: below) |
| the rest | `mp_coop`, `mp_coop_lag`, `mp_bandwidth`, `mp_version`, `mp_avatarcal`, `mp_body`, `mp_prefs`: pass |

Single player on the branch: scenario_check PASS (129 scenes, 0 failed), load_models_test 4/0, vr_run_check 12/0 (`scratch/b3_runcheck.sh`), the dev build 0 warnings (the Release-config build not run this part).

Why the take-back and the give failed (no fist or QC take was at fault: the fist touched, the QC refused by its rules):

- A grip pressed within 0.125 s of letting go is no grab (`vr_handgrabutil.qc`, the trigger's bounce). The harness pressed
  as soon as the server had the gun fall, often within that. Now each press waits `GRIP_REPRESS_S` (0.25) of the
  server's game time after the let-go: `vr_mp_status`'s `mp_server:` line prints `time=`; the let-go's time is the
  server's `grip:` line (below).
- The server saw a grip's edge again in every frame with no move from that client (a dedicated server's frame between
  his moves): his `.vrbits0` was rewritten only when a move came, so last frame's edge stayed (a let-go noted twice
  moved QC's release time on, into the bounce window). At the frame's end its previous bits now follow the current ones
  (`vr_server.cpp`; bots keep their own). Single player too, in a server frame without a move: its own commit.
- The give (`impulse 154`) in the same frame as the grip's press reached the server a move before the grip bit (the
  move's grip bit is the client's next frame's): the default grip-to-hold let it fall at once (B3's `mp_avatar` 14/1).
  `grip_then_give` waits for the server's grip, then gives.
- Debug aids (`vr_debug_carry 2` with `developer 1`, on the server): `grip: <name>'s main hand pressed/let go at <time>`;
  `weapon: <gun> not taken by the <hand> hand at <time>: grip g, pressed s ago, let go s ago`; the grab probe's line
  also gives the thing's place, speed and whether it is on the ground.

Open items (B3c):

- A weapon let go of from a still, open hand rests on the palm (the hand's Box3D reach body, ROUND21's design: "a slower
  release keeps meeting them"). From there the closing fist does not take it: it is 3-5 cm over the fist, and with
  nothing solid under its handle it gets no "lying" slack (allowed 0). The harness's hand, held still before the give,
  showed it every run; the scenario gives with the hand just placed, and the gun falls to the floor. Single player has
  the same code: check whether a gun on your own palm is retaken by closing the hand (if not: a slack for a weapon
  resting on a hand's body, or the fist's body passing it).
- Everything else of B3b: the palm frame, hotspots and drawn pose, weapon hull, 2H carry and foe grab frames, the
  ragdoll grab fit, the crawl's hand bottom, body calibration up, the client-side traces, `mp_teleport`.

### B3c results, part 1 (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: all 9 scenarios pass)

| Scenario | Result |
|---|---|
| `mp_body` | 12/0, grown: A's arms calibrated (Body Calibration's results set: his shoulders 6.2 units wider), B's copy's shoulders within 0.10 units of A's own (allowed 0.2) |
| the rest | `mp_coop` 11/0, `mp_coop_lag` 11/0, `mp_bandwidth` 7/0, `mp_version` 6/0, `mp_avatar` 17/0, `mp_avatarcal` 9/0, `mp_prefs` 10/0, `mp_grab` 10/0 |

Single player on the branch: `vr_pickup_test 16` 3/0 (new), scenario_check PASS, load_models_test 4/0, the dev build 0
warnings (vr_run_check and the Release-config build not run this part: nothing of the run or the release config changed).

What changed:

- **A weapon lying on a hand** (single player too, its own commit): one resting on a hand's Box3D body
  (`box3d::restsOnHand`) is taken by that hand closing within `vr_weapon_palm_grab_slack` (12 cm, menu row Palm Grab
  Slack beside Handle Grab Leniency): it sits over the fist that closes under it, with nothing solid under its handle,
  and had no slack at all. Single player's answer to B3b's question: a gun let go of from a still, open hand (palm up
  or sideways) passes through the hand's new open-hand body (`reachSink`) to the floor, and one laid on the open palm
  slides off it within a second, so there no gun is left over a closing fist (`vr_pickup_test 16` checks the first).
  The grab's debug line (`vr_debug_carry 2`) says `on this hand` / `against this hand`.
- **Body calibration up**: the avatar calibration carries his arms as his own body poses them (`bodycal::Arms`:
  Body Calibration's measured upper arm and forearm, his tweaks, the shoulders' place, rise and swing), 9 fields more
  (`numCalFields` 25, `calCommandVersion` 2, `QVR_NET_VERSION` 7). Another player's body is posed with them
  (`avatar::Subject::arms`; `detail::arms()` is your own `bodycal::ownArms()` or his), where before it had the default
  body's arms; the local body reads the same values through the same struct (no change).

Open items (B3c, the rest):

- The rig's other parts per player: the palm frame (vr_grip.cpp), weaponhotspot and the drawn pose
  (vr_builtins_held.inc), the weapon hull, the 2H carry and foe grab frames, the ragdoll grab fit
  (`vr_box3d_ragparts.cpp`, player 1 only), the crawl's hand bottom (`vr_physics_water.cpp`); client-side traces for
  teleport, crosshair, flashlight, hand collision; `mp_teleport`; `mp_grab` grown (a 2H carry, a foe grab).
- The palm slack has no multiplayer check yet: `mp_grab` gives with the hand just placed (the gun falls to the floor);
  a case with B's hand held still before the give (B3b saw the gun rest on his palm there) would check it.

## Decisions

- The harness is Python with an in-engine inbox rather than stdin or RCON: it drives GUI clients and dedicated servers
  the same way, needs no network control channel, and stays inert unless `-cmdinbox` is given.
- The lag simulator is a proxy outside the engine (no engine cvars): it tests the real code paths unchanged, and it
  also measures each link's bytes.
- The same-IP fix compares the port too; a client back from a crash on a new port leaves a ghost until the timeout
  (QuakeWorld's `qport` is the full answer: B7, with the snapshot protocol).
- Voice chat (B10) needs Opus and a noise suppressor vendored as source: to be confirmed by the author before B10.
- Other players' bodies (B2d) are posed by the local body's own code with its per-frame state swapped per player, rather
  than the state gathered into an instance struct read through a pointer everywhere: the same results for the local
  body by construction (its reads unchanged; checked pixel for pixel), one code path, a small diff in five files. Seen
  from outside, they always have legs (his body mode 1 or 2 is his own first-person view); his body mode 0 draws his
  player model as before.

Next phase: B3d — the rest of B3c: the rig up (palm frame, weaponhotspot and the drawn pose, weapon hull, 2H carry and foe grab frames, the ragdoll grab fit, the crawl's hand bottom); client-side traces for teleport, crosshair, flashlight, hand collision; `mp_teleport`; `mp_grab` grown (a 2H carry, a foe grab, the palm slack with B's hand held still before the give); melee sub-tick samples stay with B5.
