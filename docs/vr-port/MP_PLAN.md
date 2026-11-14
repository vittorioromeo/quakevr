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
| Server rules | Every balance setting (1853: the balance pages', the gameplay preset's, the melee timing; B6) goes to every client (`QVR_SVC_RULES`); a remote client plays by them while joined (his own kept, back when he leaves) and his menu shows them dimmed | `vr_serverrules.cpp:17-27` [code] |
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
| Downloads (B4c: a Map Library package the server names, installed by the client's Map Library; nothing else) | No `svc_download`/chunked download; the `Download` in `host_cmd.c:581` is the map library's HTTPS fetch (`vr_mapindex.cpp`, `vr_mapinstall.cpp`) [code] |
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
| **B3d** (client-side traces, teleport, palm frame up done; the rest is B3e) | The rest of B3c: the rig up (palm frame, weaponhotspot and the drawn pose, weapon hull, 2H carry and foe grab frames, the ragdoll grab fit, the crawl's hand bottom); client-side traces for teleport, crosshair, flashlight, hand collision | `mp_teleport`, `mp_grab` grown (palm frames; a 2H carry, a foe grab, the palm slack) |
| **B3e** (the held weapon's drawn pose up done; the rest is B3f) | The rest of B3d: the drawn pose up per player (weaponhotspot, weaponanygrip, weapondrawnpose's carried weapon and axes, the drawn weapon hull, the 2H carry's reach offset); `mp_grab` grown | `mp_held` (B's hotspots on the server where his view draws them; his two-handed hold and hand-off judged by his own) |
| **B3f** (done: B3 is done) | The rest of B3e: the grip fit's hand rig per player (`vr_gripfit.cpp`), the ground weapon's grab hotspot and the reverse grip's choice per player (weapongrabspot, weapondrawnpose 43), the magnetic holsters' poll and places per player; `mp_grab` grown (a 2H carry, a foe grab, the palm slack) | `mp_grab` grown, `mp_held` grown (a reverse grip) |
| **B4** (done: saves, loads, level changes with inventories; B4b: coop autosaves, the exit and intermission, a real death and respawn; B4c: slots by name, coop autosaves in Load Autosave, the intermission save, map download through the Map Library) | Coop campaign flow: coop saves and loads (per-player blocks, versioned), autosaves, level transitions with inventories, intermission, deaths and respawns, map download (package names through the map installer; chunked fallback) | `mp_campaign` (e1m1 to e1m3 with saves and a load, both players' inventories), `mp_download` (B lacks a map: installs it, joins) |
| **B5** (pickups and force grabs done; the rest is B5b) | Interactions between players: drop a weapon / the other picks it up; force grab of world weapons in multiplayer; melee, parries and shoves between players; climbing, crouch, prone, jump, punch synced and judged per player | one scenario per interaction, each from both sides (`mp_pickup`, `mp_forcegrab`) |
| **B5b** (punches, guard parries, bashes; crouch, prone, jump and swim flags done; the rest is B5c) | The rest of B5: melee, parries and shoves between players (judged by each one's own hands and timing, gated to multiplayer where single player has none); climbing, crouch, prone, jump, swim and punch synced (the remote body lying, swimming, climbing, jumping as he does: the avatar's flags) and judged per player | `mp_melee` (A's swing hits B, B's parry stops A's, a shove moves him: each way), `mp_moves` (B crouches, lies prone, jumps, climbs: the server's hull and A's copy of him follow) |
| **B5c** (climbing, swimming, the palm's shove done; the rest is B5d) | The rest of B5b: climbing between players (the server's holds per player; the remote body's climbing feet and hanging trunk from his holds); swimming checked in water (a pool map spot); a blade's directional parry between players (his swing's line, as an armed humanoid's); the open palm's shove; melee sub-tick samples (a swing split over merged moves, multiplayer only) | `mp_moves` grown (climbs a ledge, swims: A's copy follows), `mp_melee` grown (a sword's swing parried across its way, a palm shove, a swing at 30 ms jitter) |
| **B5d** (done: B5 is done; sub-tick samples not needed, to B8) | The rest of B5c: a blade's directional parry between players (his swing's line from his hand lines, `handlineat`, as an armed humanoid's `VR_AG_WeaponLine`: VR_DRP_Eval's third kind); melee sub-tick samples (each merged move's pose to the tracker, multiplayer only) | `mp_melee` grown (a sword swung across B's crowbar: parried by the line, not the guard; a swing at 30 ms jitter: the same hit as without) |
| **B6** (rules, the look of hands and body, holsters, seated, flat height, pauldrons, magazine hull done; the plates are B6c) | Server rules and cosmetics: every balance setting a rule (presets, armed humanoids, damage, mutators, stamina, grapple...), rule pages dimmed on a remote client; holster positions, height, seated/standing, gloves, body seen by others | `mp_rules`, `mp_cosmetics` (B's height and gloves seen by A) |
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
- `vr_mp_drawn` (Debug > Reports > Network: Held Weapons): each hand's held weapon as this client draws it
  (`mp_drawn_own:` its hotspots in the world, the two-handed hold, a weapon carried off its handle) and, on a server,
  as it places each other player's from what his client sent (`mp_drawn_sv:`).
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

### B3d results, part 1 (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 9 of 10 pass, `mp_teleport` then 9/0 in 5 runs of 5 after the fix below)

| Scenario | Result |
|---|---|
| `mp_teleport` (new) | 9/0: B, a client of the dedicated server, traces his own map straight down to the floor his server has him on (24 under his origin, `hosted=0`); his off hand's teleport aim (his own sweep, `vr_teleport_enabled 1`) lands on the floor 88 units ahead; let go, the server moves him there (within 2 units: exactly) and A sees him there. Failed first: the old build had no trace on a remote client (`mp_trace: none`, no aim) |
| `mp_grab` | 12/0, grown: the server has each player's palm frames as his client measures them (their 13 values' sum within 0.7: equal to 3 decimals) |
| the rest | `mp_coop` 11/0, `mp_coop_lag` 11/0, `mp_bandwidth` 7/0, `mp_version` 6/0, `mp_avatar` 17/0, `mp_avatarcal` 9/0, `mp_body` 12/0, `mp_prefs` 10/0 |

Single player on the branch: `vr_teleport_both_test` 14/0, `vr_pickup_test 16` (palm rest) 3/0, `crawl_test.py` 18/0,
scenario_check PASS (129 scenes, 0 failed), load_models_test 4/0, vr_run_check 12/0 (`scratch/b3_runcheck.sh`), the dev
build 0 warnings (the ReleaseLTO build not run this part). Every path changed is the same in single player: the local player's trace is the server's
`SV_Move`, `handFrameOf`/`fistOf` of player 1 are the local hand's.

What changed:

- **Client-side traces** (`vr_trace.cpp`): `worldtrace::move` on a remote client (no local server; never in a demo)
  traces the client's own copy of the map: the world and the lifts, doors and platforms it has (worldtrace::world's
  list, unrotated), with the hull `SV_Move` picks for the box's size and its offset, so a box sweep stops where the
  server's would. Not monsters or players (the client has no solidity of its entities; the hands' sweeps pass what bleeds
  anyway), no portals or hit models, `ent` null. Its users on a remote client now work: the teleport aim, the crosshair's
  reach, the flashlight beam's length, the hands stopping at walls (`vr_handpose.cpp`), the flat sights. Before, all of
  them got nothing there. Smooth + Teleport's "from the ground only" on a remote client reads his `cl.onground` and
  `cl.inwater` (the server checks again).
- **The palm frame up**: each hand's `grip::HandFrame` as the client measures it (the jointed hand as drawn: the palm,
  its normal, the grip channel) goes up as `vr_rigframe` (reliable, whitelisted; again when a value moves 0.05, at most
  every `vr_net_avatarcal_interval`). `grip::handFrameOf(player, hand, left)` (your own measured one for this game's own
  player, his for another, else the default hand's) is used where the server places what a player holds
  (`grip::serverTake`), by the foe grab's palms (`vr_foegrab.cpp`) and by the ragdoll grab fit (`vr_box3d_ragparts.cpp`,
  which also takes his own fist: before, player 1 on a listen host only). The crawl's hand bottom
  (`vr_physics_water.cpp`) takes his own fist (was the host's). `QVR_NET_VERSION` 8.
- **A teleport in an earlier move of a server frame was lost** (single player too, its own commit):
  `VR_ClientTeleport` reads the stored move once a frame, and a later move of the same frame without the bit
  overwrote it (the suite's `mp_teleport` failed so once: B's release never reached the server's move). Now the
  bit and its spot are kept (`vr_server.cpp`). In single player: two moves in a server frame whenever the headset
  runs faster than the tick.
- Debug aids: `vr_mp_status` on a client prints `mp_teleport:` (aiming, valid, sent, the aim's spot) and `mp_trace:`
  (its own trace down); `vr_mp_prefs` gives each hand's frame (`frameN`, `framesumN`) on both sides; the grip's log says
  `measured` when the frame placing a prop is a measured one (his or yours).

Open items (B3e):

- The drawn pose per player: `weaponhotspot` and the other `vr_builtins_held.inc` builtins that read the local drawn
  weapon (`view::weaponHotspot`, `heldWeaponAxes`, the reverse grip, the carried weapon's pose: about 12 sites gated on
  `NUM_FOR_EDICT(player) == 1`), the drawn weapon hull (`vr_box3d_hands.cpp` `view::drawnWeapon`), the 2H carry's reach
  offsets (`vr_carry2h.cpp`), the grip fit's hand rig (`vr_gripfit.cpp`: `local` only). These need the drawn weapon's
  place in the hand per player up (a small block on change, as the rig), then the same per-player lookups.
- `mp_grab` grown with a 2H carry, a foe grab and the palm slack (B's hand held still before the give).
- The client trace has no monsters: on a remote client the crosshair reaches through a monster to the wall (B7's
  snapshots could carry the entities' solidity).

### B3e results, part 1 (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: all 11 scenarios pass)

The suite re-run first, with B3d's `mp_teleport` fix: 10 of 10 pass.

| Scenario | Result |
|---|---|
| `mp_held` (new) | 13/0: B, a client of the dedicated server, holds a given shotgun still in his main hand; the server places its 2 hotspots (QC `weaponhotspot`) within 0.03 units of where B's view draws them (allowed 0.5); B's off hand takes its grip hotspot: the server's `weaponanygrip` for it is B's own (2: anywhere, `vr_weapon_grab_anywhere`); B lets go with his main hand: the server hands the shotgun off to his off hand (QC VRTryHandOff, by that `weaponanygrip`) and has it carried off its handle (`weapondrawnpose` 2) as B's view draws it. Before: none of it for a remote player (hotspots '0 0 0', anygrip 0: no hand-off) |
| the rest | `mp_coop` 11/0, `mp_coop_lag` 11/0, `mp_bandwidth` 7/0, `mp_version` 6/0, `mp_avatar` 17/0, `mp_avatarcal` 9/0, `mp_body` 12/0, `mp_prefs` 10/0, `mp_grab` 12/0, `mp_teleport` 9/0 |

Single player on the branch: `vr_embed_test 42` (reverse grip) 24/0, `vr_embed_test 24` (2H hotspot) 22/0, scenario_check
PASS (129 scenes, 0 failed), load_models_test 4/0, vr_run_check 12/0, the dev and the ThinLTO builds 0 warnings. Single
player reads the view as before by construction (`heldpose::remote` is null with one client and for a listen host's own
player).

What changed:

- **The held weapon's drawn pose up** (`vr_heldpose.cpp`): each hand's held weapon as the client's view draws it, in
  the hand's frame (relative to the move's hand, the frame the fists use): its hotspots (type, place, bias, share), the
  two-handed hold's kind (`twohand::helpKind`), a weapon carried off its handle (handle, tip, axes), its axes, reverse
  grip share and flags (`heldWeaponAxes`, `heldMayReverse`, `heldGripFlippable`), the drawn weapon's slot, mirroring and
  place in the hand (`DrawnWeapon::inHand`), and how far the hand is drawn held out of a prop the other hand holds
  (`view::heldOutOf`). Up as `vr_heldpose <version> <hand> <62 values>` (reliable, whitelisted) when a value moves 0.05,
  at most every `vr_net_avatarcal_interval` a hand. Hotspots are measured on the hand pose the weapon was drawn on (its
  entity less its place in the hand), so they do not lag the hand. `QVR_NET_VERSION` 9.
- **Its users, per player**: QC `weaponhotspot`/`weaponhotspotinfo` (placed on the hand of the move the server runs),
  `weaponanygrip`, `weapondrawnpose` 0..5 (the carried weapon: thrown from where it is drawn, the melee's free line) and
  44..46 (axes, reverse grip); Box3D's drawn weapon hull (`vr_box3d_hands.cpp`: his weapon's own mesh, the server's model
  of the same name, not the muzzle capsule); the 2H carry's reach offset at the grab (`vr_carry2h.cpp`).
- Debug aid: `vr_mp_drawn` (Debug > Reports > Network: Held Weapons).

Open items (B3f):

- The grip fit's hand rig (`vr_gripfit.cpp`: `local` only); the ground weapon's grab hotspot (`weapongrabspot`) and the
  reverse grip's choice for a loose weapon (`weapondrawnpose` 43), both asked of one weapon lying about at the grab (the
  client would send them with the grip, per entity); the magnetic holsters' poll and places (`magholsterpoll`,
  `holsterpos`: the local player's only).
- A carried weapon's hotspots on the server are 1.4 units off B's drawn ones (the main hand's: 0.03): the server places
  them on QC's `.offhandpos`, which differs from the move's off hand while it carries. Within what the melee needs; check
  whether QC moves the field, or use the move's hand (`server::clientMove`).
- The magazine's box is not sent (a remote player's drawn weapon hull has the gun's body only).
- `mp_grab` grown with a 2H carry, a foe grab and the palm slack (B's hand held still before the give).
- The client trace has no monsters (B7).

### B3f results (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 12 of 14 pass, `mp_grab` and `mp_teleport` then pass alone; see the flaky retakes below)

B3 is done with this phase: everything a remote player's hands do with weapons and props in coop is judged by his own
client's view, hands and settings. What is left of B3's list is cosmetic or rare and moves on (see "Moved on" below).

| Scenario | Result |
|---|---|
| `mp_ground` (new) | 14/0: B's magnetic holster press (`vr_magholster_return`) reaches the server's QC as his (`magholsterpoll`); his empty main hand over the grip hotspot of the shotgun he dropped: the server's ground spot of it (`weapongrabspot`) is his view's, and his grip carries it by that hotspot (the server's "by its hotspot 0"); his off hand over a knight's sword he dropped (hotspots off: by its handle), turned so his view takes it reversed: the server's choice for him (`weapondrawnpose` 43) is his view's (cosine -0.99 both), and his grip takes it reversed. Before: none of it for a player of another machine (spot 0, never reversed, presses lost) |
| `mp_grab` | 13/0, grown: the server has each player's grip fit hand rig as his client measures it (27 values, sums equal to 3 decimals); the dedicated server poses the hand's points (40). Its retake now reaches into the gun (below) |
| `mp_held` | 14/0, grown: the carried shotgun's hotspots on the server within 0.03 units of B's drawn ones (1.42 before: the fix below) |
| the rest | `mp_coop` 11/0, `mp_coop_lag` 11/0, `mp_bandwidth` 7/0, `mp_version` 6/0, `mp_avatar` 17/0, `mp_avatarcal` 9/0, `mp_body` 12/0, `mp_prefs` 10/0, `mp_teleport` 9/0 (6/1 once in the suite: B's aim never came, `aiming=0`; alone 9/0) |

Flaky (test mechanics, not the per-player code): a gun come to rest under a still mock hand is never touched (a hand's
touch is judged as it or the thing moves), so the retakes press with the hand where it let go and then move it onto the
gun (`vr_mock_hand_to ... spot`, which now works on a remote client); `mp_grab`'s retake still misses in about 1 run of 4
(the gun still sliding as the hand is placed on it: all 3 presses miss); `mp_ground` passed 14/0 in its last 3 runs, its
earlier misses fixed (a sword let go onto the carried shotgun was flung away: the main hand is now raised out of the
way; a hand's ground spot lost as it closed on the gun: `spotHold`). Next: place the hand on the gun every report while
pressing.

Single player on the branch: `vr_embed_test 42` (revgrip) 24/0, gun_wall_test PASS (a crowbar taken by its hotspot),
crate_grip_test 10/0 (the grip fit), magholster_test `time` (bound by the stick click, home in holster 3), load_models_test
4/0, scenario_check PASS (129 scenes, 0 failed), vr_run_check 12/0; the dev build 0 warnings, ThinLTO 0 warnings. Every local path reads what it read before: the
local player's ground spot, reverse grip, holster presses and hand rig come from this game's own view as before
(`heldpose::isRemote` is false with one client and for a listen host's own player).

What changed:

- **The ground spot up** (`vr_heldpose.cpp`): a remote client sends `vr_grabspot <hand> <entity> <index>` when the hotspot
  an empty hand is near changes (`twohand::groundSpotNow`); a spot lost goes up 0.25 s late (`spotHold`: the hand closing
  on the gun leaves the spot in the move whose touch takes it, as this game's own server reads the view's spot of the
  frame before); the server's `weapongrabspot` answers with it for that weapon (`heldpose::grabSpot`).
- **The reverse grip's choice per player**: QC passes the taker (`weapondrawnpose(w, hand, 43, p)`); for another player
  the server chooses by his move's hand angles, his own `vr_revgrip` (now a preference: `vr_prefs` grows by one) and his
  flat-screen play (`view::reverseGripChoiceAt`, `heldpose::reverseGrip`).
- **Magnetic holster presses up**: a remote client sends `vr_magpoll <bits>` (what `magholster::poll` took) when there
  are some; the server's `magholsterpoll` hands them to his QC (`heldpose::magPoll`). Before, a dedicated server's
  players never bound a holster.
- **The grip fit's hand rig up**: `vr_rigframe` version 2 carries each hand's rig-to-hand matrix and fingers' shifts with
  the palm frame (40 values); `gripfit::fit` takes the player (his own rig: `gripfit::playerRig`), so props a remote
  player takes are fitted out of his hand as the host's are.
- **A carried weapon's hotspots on the hand** (the 1.4 units of B3e): the client measured them in the frame of the
  weapon's entity less its place in the hand, which the carry's pose moves after that place was taken; a carrying hand's
  are now measured in the tracked hand's frame (the frame the carry places it in). The server's `.offhandpos` was right
  (the hands agree within 0.03).
- Both new commands whitelisted in `sv_user.c`; `QVR_NET_VERSION` 10. QC logs the off hand's grip presses as the main
  hand's (`vr_debug_carry 2`), the mock's `spot` target works on a remote client (its own entities), the server's avatar
  report gives both hands' weapons (`wpn=<main> <off>`), `vr_mp_drawn` each empty hand's ground spot, the weapon lying
  nearest and how it would be taken (`gs=`, `rev=`, `at=`, `rot=`).
- The harness: a hand's touch is judged as it or the thing moves, so a gun come to rest under a still mock hand is never
  touched; `mp_grab`'s and `mp_ground`'s retakes press with the hand above it and then lower it into the gun (`retake`).
  Kit runs and the harness share the base's `id1/autoexec.cfg`: never run both at once (a kit run's script quits the
  harness's games).

Moved on (none blocks coop play):

- `holsterpos` for another player (the magnetic holsters' flight home and its distance check use QC's estimate of his
  holsters, `VR_Grapple_HolsterPos`, a few units off): with his holster places seen by others, B6.
- `mp_grab` grown with a 2H carry, a foe grab and the palm slack: B5 (interactions, one scenario each, from both sides).
- The magazine's box (a remote player's drawn weapon hull has the gun's body only): B6.
- The client trace has no monsters: B7. Melee sub-tick samples: B5.
- The reverse grip's cosine on the server can differ from his view's by up to ~0.15 while the weapon still slides (the
  server's place of it, not the client's interpolated one); the choice only flips near a right angle.

### B4 results, part 1 (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 14 of 16 pass, `mp_teleport` 6/1 (B3f's flaky aim, `None`) and `mp_ground` 14/1 (below))

| Scenario | Result |
|---|---|
| `mp_campaign` (new) | 17/0: a dedicated server's coop e1m1 to e1m2 to e1m3, both players' weapons, ammo and weapon in hand carried by each change (shells raised to 25: QC's SetChangeParms); a save on e1m2 (`// qvr_save 2`, `// qvr_coop 1 4 1`, a `// qvr_coopslot` block for A and B); its load from e1m3 with both kept, each in his own slot (the same entity) with what he had at the save, the server's "coop save: 2 players of 4 slots"; B's suicide after the load respawns him with the loaded e1m2's start (his saved parms: no rocket launcher), not e1m3's |
| `mp_campaign_listen` (new) | 11/0: a listen host's coop save loaded with B connected (both kept; the host's own client reconnecting as at a changelevel), then by the host alone after he went single player (`maxplayers 1`): the save's 4 players set again, B joining into his saved slot with his saved inventory |
| `mp_grab` | 13/0 in 9 runs of 9 (B3f: 1 run of 4 missed): the hand placed on the gun again every report while pressing, into it and over it by turns |
| `mp_ground` | 15/0 in 4 runs of 4, then 14/1 in the suite (it had failed 2 of 3, 3 of 3 once the sword was waited for): the sword let go waited for at rest (the server's nearest weapon's speed, `vr_mp_drawn` `nspeed=`); the hand turned up high in one frame, then put over the sword in the next. Still flaky, about 1 in 5: the sword lying on its flat side, the off fist placed 2 cm below its hotspot stops 2.9 cm off its surface ("against this hand", allowed 0: the hand barely moves), so the reversed take never comes |

Single player on the branch: load_models_test 4/0, vr_run_check 12/0 (`scratch/b3_runcheck.sh`: a save and its load, a
changelevel), scenario_check PASS (129 scenes, 0 failed); the dev build 0 warnings. A single player save is written and
read as before (format 1, no `qvr_coop` line, slot 0's edict the loaded player's for the whole level).

What changed:

- **Coop saves** (`vr_coopsave.cpp`): a save of more than one player (any but deathmatch: console, menu Save, the
  Quicksave tool) adds `// qvr_coop <format> <maxclients> <coop>` and per slot `// qvr_coopslot <slot> <in> [<count>
  <parm1..> <name>]` (the player's level-start parms, ext parms included; `count` lets another build's number of parms
  load). It is save format 2: older builds refuse it (they would load its players' edicts as a single player game's);
  single player saves stay format 1, and Continue skips coop saves.
- **The load** (`Host_Loadgame_f`): played with the save's number of players (the edicts after theirs are numbered from
  it: a single player save loaded in a game of 4 was misnumbered before, and a coop save of more players than
  `-maxplayers` allows is refused). Connected players are kept when that number is the game's already: they reconnect,
  each in his slot, as at a changelevel, and get the serverinfo again after the save's models are precached. Otherwise
  the game is shut down and `maxplayers`, `coop` and `deathmatch` set from the save. Each slot gets its saved parms (a
  coop respawn after a load gives the loaded level's start; before, every slot but the first kept whatever level's parms
  it had). A player joining a slot that was empty in the save enters as a new player (ClientConnect, PutClientInServer)
  instead of into a blank edict; the game stays paused until every saved player still connected is back in (or left).
- `vr_mp_status`'s `mp_client:` lines carry each player's inventory (`inv=health,armor,items,shells,nails,rockets,cells,
  weapon`); `vr_mp_drawn`'s server lines the speed of the weapon lying nearest each hand (`nspeed=`).

Open (B4b):

- `mp_ground`'s sword retake (1 in 5): reach the sword's surface (its middle, not hotspot 0, or a sweep across it).

- Autosaves in coop (`Host_CheckAutosave` follows the host's own player; a dedicated server has none): the level-start
  autosave at least, all players alive.
- Intermission and the exit: a player in the exit trigger, the intermission seen by both, a press going on.
- A real death (not a suicide: ClientKill respawns at once) and the press that respawns in coop.
- A rejoining player matched to his saved slot by name (now by slot: a player joining another free slot after a load
  is a new player).
- Map download (`mp_download`: package names through the map installer; a chunked fallback).

### B4b results (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 15 of 15 pass, `mp_flow` new)

| Scenario | Result |
|---|---|
| `mp_flow` (new) | 14/0: on a dedicated server, B put in e1m1's exit (`vr_goto_exit 1`): A is told B exited, both clients see the intermission (`cl.intermission`), A's trigger held goes on to e1m2 (QC's IntermissionThink after its 2 seconds), both arrive; e1m2's level-start autosave `autosave/coop~e1m2` made with A and B in it (a monster in sight at the start: made after `vr_autosave_safe_maxdelay`, 2 s in the test); B, given a rocket launcher, killed for real (`vr_hurt 1000 1`: QC's T_Damage): dead on the server and his client, not respawned at once; his trigger pressed respawns him (first press) with his e1m2 start inventory, no rocket launcher (QC's respawn in coop: setspawnparms). Failed first: no coop autosave (`Host_CheckAutosave` returned for `svs.maxclients != 1`), no `vr_goto_exit` / `vr_hurt` |
| `mp_ground` | 15/0 in 4 runs of 4 (B4: 1 in 5 failed): the sword retaken at its handle (`vr_mock_hand_to off spot -1`: its origin), where `vr_weapon_grab_handle_leniency` (5 cm) allows the fist held 2.9 cm off the sword's flat side by the hand's body; at its blade nothing does (`vr_weapon_grab_slack` 0) |
| `mp_teleport` | 9/0 in 4 runs of 4 (3 with B on the spawn spot that failed): the aim turned 35 degrees left and right too. The failures were B's spawn spot (528 -296): straight ahead a wall 46 units off, the aim never on the floor (`aiming=1 valid=0`, the impact climbing it); the other spot (432 -296) passed |

Single player on the branch: load_models_test 4/0, vr_run_check 12/0 (`scratch/b3_runcheck.sh`), scenario_check PASS
(129 scenes, 0 failed), a single player level-start autosave still `autosave/e1m1`; `vr_goto_exit` on e1m1 alone: the
intermission. The dev and Release-config (ThinLTO) builds 0 warnings.

What changed:

- **Coop autosaves** (`host.c` `Host_CheckCoopAutosave`): in a coop game (more than one player, `coop` set, not
  deathmatch), with `sv_autosave` and `vr_autosave_level_start` on, an autosave as each level starts (a map, a
  changelevel; not a load), on a dedicated server too: once every player in the game is in and alive and none is in
  danger (`Host_AutosaveUnsafe` for each player; the longest wait, `vr_autosave_safe_maxdelay`, ends it as in single
  player). Named `autosave/coop~<the single player name>` (`autosave/coop~e1m2`), kept in rotation (`save ... 0 rotate`);
  a coop save (format 2), so Continue and Load Autosave pass over it: `load autosave/coop~e1m2` loads it. The interval's
  autosaves follow one player's health and fire: none in coop. A player joining after the level's start (the first map
  of a dedicated server) is not in that level's autosave.
- **Debug > Tests > Cheats**: `vr_goto_exit [<slot>]` ("Go to the Level's Exit": you, hosting the player in that slot,
  put in the level's first `trigger_changelevel`, touching it) and `vr_hurt <damage> [<slot>]` ("Take Damage: 1000":
  QC's T_Damage from the world, a real death; not in deathmatch). Both work in single player too.
- `vr_mp_status`'s `mp_status:` line carries the client's `intermission=` and `health=`; the mock's `vr_mock_hand_to
  <hand> spot -1 [<cm>]` is the weapon lying nearest's handle (its origin).
- The intermission's press and the coop respawn press needed no change: a VR client's trigger reaches QC's
  IntermissionThink and PlayerDeathThink as `button0`.

Open (B4c):

- A rejoining player matched to his saved slot by name (now by slot).
- Coop autosaves in Load Autosave (a list of coop saves for the host: format 2 is passed over now).
- Map download (`mp_download`: package names through the map installer; a chunked fallback).
- Saving during a dedicated server's intermission: `Host_Savegame_f` checks `cl.intermission`, the server's own client's
  (none on a dedicated server); QC's `intermission_running` is the server's.

### B4c results (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 15 of 16 pass, `mp_download` then 8/0 alone after its index wait was fixed (below))

| Scenario | Result |
|---|---|
| `mp_download` (new) | 8/0: a test package (Quake VR's own `vrexample.bsp` as `maps/qvrmpdl.bsp`, zipped by the test, listed in a local index, served from 127.0.0.1) installed by A's `maps_install` in the user dir he shares with a dedicated server, which plays its map; A (installed, not mounted) told the server's package, mounts it and joins again; B (his own user dir, lacking it) told what to install with `vr_mp_download 0` (nothing written), then with 1 installs it through the map installer (into his `qvr_addons/<sha16>/`), mounts it and joins: the server has him in. Failed first: "Model maps/qvrmpdl.bsp not found" (a client never mounted the server's package, installed or not) |
| `mp_campaign_listen` | 14/0: a third player C; after the host's lone load C joins first (the first free slot is B's) and is given his own saved slot by his name, then B his: each with what he had at the save. Failed first: C took B's slot and inventory; then the host's own client took slot 3 (its name unknown: the loopback carries none) |
| `mp_flow` | 16/0: a save on the dedicated server in the intermission refused ("Can't save in intermission."; it was written before); the Load Autosave list (`vr_autosave_list`) has `autosave/coop~e1m2` ("Coop, 2 players") |

Single player on the branch: scenario_check PASS (129 scenes, 0 failed), load_models_test 4/0, vr_run_check 12/0
(`scratch/b3_runcheck.sh`); the dev build 0 warnings, the Release-config (ThinLTO) build 0 warnings. `mp_download`'s
first suite run waited for the index's "started" line and timed out on the fresh user dir's cold start (63 s: its
shader cache empty); it now waits for "map index: N packages", up to 240 s.

What changed:

- **Map download** (`vr_mpdownload.cpp`): the server sends each remote client `QVR_SVC_PACKAGE` (the sha256 of the Map
  Library package its map is played from, `mapinstall::active()`; "" for the stock game's) before its svc_serverinfo,
  read whatever protocol the last server had. A client whose mounted package differs leaves at once (mounting rebuilds
  the game folders, which ends a connection) and then: has it installed: mounts it and joins again at the address it
  connected to; lacks it, with `vr_mp_download` 1 (default; Debug > Reports > Network: Install the Server's Map):
  `mapinstall::begin` on its own index's entry for that sha256, the Map Library's own job (the index's mirrors, the size,
  disk, zip-bomb, path and progs.dat guards, the sha256 check), then mounts and joins again; a package its index does not
  list: the index fetched once (as `maps_fetch`), else "not in the Map Library's index"; with 0: told what to install. A
  client with a package mounted joining a stock map unmounts it and joins again. One rejoin per package (no loop).
  `QVR_NET_VERSION` 11.
- **Slots by name**: the connect request carries the player's name after the network version; in a loaded coop game a
  new connection takes the free slot of the saved player of that name (`VR_CoopPickSlot`; the host's own client by his
  `name`), else a free slot no saved player waits for (a new player there), else the first free one (as before). Two
  saved players of one name: the first free of them.
- **The intermission**: `Host_Savegame_f` refuses while the server's QC is in its intermission (`intermission_running`,
  bound in vr_progs.cpp), not only the host's own client's (`cl.intermission`).
- **Load Autosave** lists coop autosaves (format 2) with "Coop, N players" on their second line; the host loads one as
  any (the save's players set again: B4). Continue and the launch rows' Continue still pass over them.

Open (B5 and later):

- A dedicated server reads no map index (`mapindex::start` is a client's): installing on one needs `maps_fetch` first,
  or a client sharing its user dir (as the test does). It could read its cached copy at start-up (no fetch).
- A server's map that is neither the stock game's nor a Map Library package (a map copied in by hand) cannot be joined
  by a client lacking it: "Model maps/x.bsp not found", as before (no chunked fallback: Decisions).
- A stranger joining a loaded coop game when every free slot is a saved player's still takes that player's edict.

### B5 results, part 1 (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 17 of 18 pass, `mp_ground` 14/1 (its shotgun retake: 3 presses, none took it), then 15/0 alone)

Before it, the full suite (B4c's build merged with `next`: the vis over-read fix and `-memguard`): 16 of 16 pass,
`mp_download` 8/0 with its index wait.

| Scenario | Result |
|---|---|
| `mp_pickup` (new) | 9/0: on a dedicated server A (put at an open spot of e1m1, given a shotgun) opens his low hand: it falls; B, put 24 units beyond it, closes his empty main hand on it (his grip, then his fist onto its hotspot 0: `retake`): the server puts it in B's hand ("taken into the main hand ... by its hotspot 0"), A's stays empty. Then the other way: B lets it fall, A takes it. Passed with no game change: a thrown weapon is taken by whoever's hand closes on it |
| `mp_forcegrab` (new) | 9/0: a super shotgun pickup of the level (the map's `weapon_supershotgun`, an object with `vr_item_objects` 1: `vr_test_spawn 116`) 150 units ahead of B; B's force grab (the mock's: point down at it, trigger, flick up, grip) brings it to his hand: caught, his; in coop it goes back to its place for the others ("pickup: weapon_supershotgun back at"); then A, put where B stood, force grabs it: his too. Passed with no game change (VR_Forcegrab_IsEligible takes a level weapon in coop as the object it is; its catch, weapon_touch with `leave`, gives it and VR_PickupObj_Return puts it back) |

Single player on the branch: scenario_check PASS (129 scenes, 0 failed), load_models_test 4/0, vr_run_check 12/0
(`scratch/b3_runcheck.sh`); the dev build 0 warnings, the Release-config (ThinLTO) build 0 warnings.

What changed (test aids only; nothing a player meets):

- Impulse 241 (`vr_test_spawn`: things put ahead of the player who asks) works in coop too, as the test gives (150..189)
  already did (`ImpulseCommands`: the other test impulses stay single player). `vr_test_spawn 116`: a super shotgun pickup
  of the map's kind on the floor ahead.
- `vr_mp_drawn`'s server lines give where the weapon lying nearest each hand is (`norg=`): the harness puts the other
  player beside a weapon let go of.
- A force grab that arrives uncaught says why (developer 1): the hand free, gripping, how long before its arrival the
  grip was pressed.
- The harness: `put_at` (setpos, then noclip off: a player put somewhere of e1m1 by the server, waited for), `lying`,
  `hand_over`, `force_grab`. A hand held still `THROW_SETTLE_S` (0.5 s of game time) before it opens: the mock reports a
  hand put elsewhere in one frame as moving that fast, and a release inside the throw's window threw the gun 500 units
  (1 run of 3). The mock's force grab presses the grip 0.3 s after its flick: the block test's 0.05 s came 0.354 s
  before the pull arrived on the dedicated server, every run (`vr_forcegrab_catch_early` 0.35: just too early).

Open (B5b and later):

- Melee, parries and shoves between players; climbing, crouch, prone, jump, swim and punch synced and judged per player
  (B2d's remote body is upright with walking legs whatever he does: lying, swimming, climbing, jumping): B5b.
- A force grab never started for A with a weapon lying at his feet under his open hand (B's caught one let go there; 1
  run of 1, the test now keeps the catch): to look at in single player (the hand's touch of a weapon lying, or the
  lock-on, may take the hand first).
- Level weapons with `vr_item_objects` 0 (plain pickups) are not force grabbed in coop or deathmatch (they stay for all);
  deathmatch 1's weapons (taken, then respawned): B11.

### B5b results (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: all 20 scenarios pass)

| Scenario | Result |
|---|---|
| `mp_melee` (new) | 17/0, each way: A's closed fist punched at B 40 units ahead (the mock's 5.7 m/s punch): the server's blow, judged by A's own hand, 3.1 damage (`vr_melee_dmg_multiplier` 0.25), B's health 100 -> 96; B holds a crowbar across (the guard): A's punch parried ("parry: player with hand 1"), its damage cut to 0.78, A pushed 16-17 units back; B pushes the crowbar forward (a bash): "bash hit: player, 3.0 damage, knocked back at 227 u/s", A moved 18-24 units. Failed first: no parry (VR_Parry took only monsters), the bash never ran (VR_Bash_Find took only monsters); the punch already landed |
| `mp_moves` (new) | 17/0, each way: B (40 units from A, face to face) crouches (the mock's head at 0.9 m): the server's crouched box, A's copy of him crouched (its head 25 units lower); lies down (0.4 m): the server's lying box, A's copy laid forward (pitch 55, lying 1.0); gets up: upright again (pitch under 1); jumps: off the ground on the server and in A's copy (its head 20 units up). Failed first: the copy upright (pitch 0) while the server had him lying |
| the other 18 | Unchanged: pass (`mp_body` 12/0 with the server's on-ground flag for the legs) |

Single player on the branch: scenario_check PASS (129 scenes; one run before it: `bench_crowd_ad` "what it placed is gone
after 3.0 s", not again on the rerun: a crowd scene, nothing this phase touched), load_models_test 4/0, vr_run_check 12/0
(`scratch/b3_runcheck.sh`); the dev build 0 warnings, the Release-config (ThinLTO) build 0 warnings. Every path this
phase changed is gated to multiplayer (`coop || deathmatch`, a player's blow on another; others' bodies only) or reads
the local player's values exactly as before (`updateBodyPitch`: `localFlat()` is `flatplay::active()` and
`detail::metresToUnits()`/`bodyScale()` are units' own when posing your own body).

What changed:

- **Parries between players** (`combat.qc`): T_Damage_VRMelee hands a player's blow on another player (coop or
  deathmatch) to VR_Parry with `vr_parry_pvp`: the other's guard (a weapon held across, VR_Parry_HandFor) or crossed
  arms take it as a monster's punch (no directional parry: a player's swing has no baked line yet), its damage cut by
  `vr_parry_reduction`, both pushed apart, his stamina and counter window as ever; VR_Parry_Interrupt does nothing to a
  player (no `th_run`).
- **Shoves and bashes find other players** (`vr_melee_bash.qc` VR_Bash_Find, multiplayer only): the push and damage as
  a monster's; no forced pain or stagger (`attack_finished`) for a player (his pain is T_Damage's own); knockdowns stay
  monsters'.
- **Moves synced** (`vr_mpavatar.hpp` `movesByte`): the avatar's first byte (was: flat-screen) carries how the server has
  him move: crouched and lying boxes (vr_hull.cpp), on the ground, his waterlevel. `QVR_NET_VERSION` 12. Another
  player's body runs `updateBodyPitch` (vr_avatar_pitch.cpp) from his `avatar::Subject` (lying, waterlevel, on the
  ground; his eyes over his box's feet): it lies down, swims tilted and leaves the ground as he does; his legs' on-ground
  is the server's flag (was: his vertical speed under 120). `vr_mp_avatars`' `mp_body:` lines give `moves= pitch= lying=`,
  the server's `mp_client:` lines `moves=`.

Open (B5c and later):

- Climbing between players: the server's holds per player and the remote body's climbing feet and hanging trunk
  (`updateClimbing` is still the local player's only).
- Swimming not tested in water (e1m1's start has none: a pool spot on another map for `mp_moves`).
- A sword's swing between players parried only by the guard, not across its way (the directional parry needs his swing's
  line, as `VR_DRP_*` reads an armed humanoid's); the open palm's shove untested (the same Find and Hit as the bash).
- Melee sub-tick samples (a swing split over merged moves; multiplayer only, B2c's note): not done.

### B5c results (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: all 21 scenarios pass)

| Scenario | Result |
|---|---|
| `mp_climb` (new) | 11/0, each way, on vrclimb (A joins alone first and is moved off the map's one start: B's spawn telefragged him there, 1 run): B takes the ledge with both hands (vr_climb_try's hold, 98 176 49) and hangs: the server's holds (`moves=192`, both hands) and A's copy of him (`moves=192`, climbing weight 1.0); he lets go: both back to 0; he mantles over the top: the server puts him on it (z 24 -> 48-51). Failed first: the copy had no holds and no climbing weight (the server's own holds per player already worked) |
| `mp_moves` | 19/0: the B5b checks, then a changelevel to e1m4 and each swims in its deep water (e1m1's pool is 24 units deeper than a player under it: on its bottom at once; the spot found from the BSP's player hull) while the other watches: the server has him under (waterlevel 3) off the bottom, the copy tilted forward (pitch 15-38). Passed with no game change (B5b's waterlevel bits) |
| `mp_melee` | 21/0: the B5b checks, then each way an open palm (the empty main hand turned palm forward) pushed into the other: "bash hit: player", the other moved 8+ units. Passed with no game change (VR_Bash_Find takes players since B5b) |
| the other 18 | Unchanged: pass |

Single player on the branch: scenario_check PASS (129 scenes, 0 failed), load_models_test 4/0, vr_run_check 12/0
(`scratch/b3_runcheck.sh`); the dev build 0 warnings, the Release-config (ThinLTO) build 0 warnings. The local body
climbs as before (updateClimbing's reads of the local player unchanged; calcStats' holds through the same `holdOf`).

What changed:

- **Climbing between players** (`vr_mpavatar.hpp`, `vr_climb.cpp`): the avatar's moves byte has two more bits, a hand
  holding a ledge (`climb::serverHold`: the hold calcStats sends a player's own client, shared code); each hold follows
  the weapons in the avatar (eighths from his origin, a byte of yaw out of the face: 7 bytes a hand, only while it
  holds). `QVR_NET_VERSION` 13. Another player's body runs `updateClimbing` from his holds (`Subject::holds`, the
  swapped per-player `climbing` state since B2d): the trunk hangs under the head, the feet take footholds (traced on a
  listen host, past his own box) or hang (a client of a dedicated server has no world to trace: as his own feet there).
  `vr_mp_avatars`' `mp_body:` lines give `climb=` (the copy's climbing weight).
- The harness: `join(level=, first=)` (a map with one start: A in alone first, moved off it), `mp_climb`, swims in
  `mp_moves`, `shove` in `mp_melee`; `face_off` moves the defender to the coop start the attacker is not on (B onto A's
  stayed stuck, 1 run of 2 before this phase).

Open (B5d and later):

- A sword's swing between players parried only by the guard, not across its way (the directional parry needs his
  swing's line: `handlineat` keeps each player's weapon lines a few frames back, enough for VR_DRP_Eval's armed-humanoid
  branch); a weapon's swing landing on a player at all is untested.
- Melee sub-tick samples (a swing split over merged moves; multiplayer only, B2c's note): not done.
- A climber's feet on a client of a dedicated server hang (no footholds: no world to trace, his own body's too); a
  client-side trace of the world model would give both theirs.
- climb_plays.py's ledge hold (1.638 m up, 0.69 m ahead at setpos 78 176 24) is inside the ledge at the default world
  scale (31.7 units a metre: "no hold"); `mp_climb` uses 1.54 and 0.63 (vr_climb_try's hold). Its plays may have been
  made at another scale: worth a look in single player.

### B5d results (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 20 of 21 pass, `mp_melee` 27/2 (a swing struck a weapon just let go of, still falling between them: the test's), then 29/0 with the test fixed, `mp_blade` 13/0 twice)

| Scenario | Result |
|---|---|
| `mp_melee` | 29/0: the B5c checks, then a sword's swing each way (`blade`; `mp_test.py blade` runs it alone: 13/0), the sword (`impulse 163`) swung level from the right to the left across the front (the hand 1.5 m across in 0.3 s, the blade turned 175 degrees): it lands on the other empty-handed (5.9-6.8 damage at `vr_melee_dmg_multiplier` 0.25, the head's zone); his crowbar held level across in front (the guard's pose, along its way) does not parry it ("dirparry: player's blow lands: score 0.01-0.08, 86-89 deg off square"; 4.4-6.6 damage); held upright across its way, it does ("parried by hand 1: score 1.00, 25 deg off square, gap 0.2-6 units, 0.010-0.014 s ago", no damage). Then B's swing with 30 ms of jitter on his link: 35-37 of his moves merged on the server during it, the same blow (strength 3.95 against 3.94 without, the damage less the zone's multiplier). Failed first: the level crowbar parried it ("parry: player with hand 1", the guard's), the upright one did not (the blow landed); the jittered swing already passed |
| the other 20 | Unchanged: pass |

Single player on the branch: scenario_check PASS (132 scenes; one run before it: `bench_crowd_ad` "what it placed is gone after 3.0 s", B5b's flake, not on the rerun), load_models_test 4/0, vr_run_check 12/0 (`scratch/b3_runcheck.sh`); `vr_agrunt_test 78` (the directional parry) 28/19, the same 28/19 on this phase's tests-only commit (its QC before the change): failing on the branch already, not this phase's; the dev build 0 warnings, the Release-config (ThinLTO) build 0 warnings. Every path
this phase changed is gated to another player's blow on a player (`T_Damage_VRMelee`'s `coop || deathmatch`, VR_Parry's
pvp); the armed humanoid's reading in VR_DRP_Eval only reads its last frame's line into locals (the same numbers).

What changed:

- **A blade's directional parry between players** (`vr_dirparry.qc`, `combat.qc`): another player's swing of a melee
  weapon (a sword, an axe, a crowbar...: VR_AG_Class's melee, not the chainsaw's thrust; VR_DRP_PvpApplies, from
  `vr_melee_blowhand`) is judged as an armed humanoid's: VR_DRP_Eval's third kind reads his swing's line from his hand
  lines (`handlineat`: each frame's weapon parry line, VR_DRP_HandLines), the pair of frames within 0.1 s whose sweep
  came nearest your weapon (his blade crosses it a frame or two before it meets your body), sampled 16 times; its way
  from the turn of the blade between them. No hold-back (his blow is his own hand's: no way ahead to follow); parried:
  `vr_dirparry_through_armed`'s share, sparks of his weapon's material on yours, the parry's sound, push, stamina and
  counter window as ever. A fist's blow is still parried by the guard and crossed arms (B5b).
- `vr_mp_status`' `mp_client:` lines give `merged=` (his moves that came in a frame with one of his already:
  `server::mergedMoves`): the jitter check sees its moves merge.
- The harness: `blade`/`swing` in `mp_melee`, `give_until`, `merged_moves`; `mp_test.py blade`; `MP_SWING_TRACE=1` puts
  the server on developer 3 for the swings (the melee tracker's lines).

Melee sub-tick samples: not done, on the test's word. Merged moves are routine (a 90 Hz client on a 72 Hz server: 170
merged before the jitter), and at 30 ms jitter the swing lands with the same strength: the tracker reads speeds over
the poses' own clock (`vr_posetime`) and VR_Melee_TurnVel turns a chord's speed back into the arc's, so a merged frame
loses only the arc's middle (its sagitta: about 6 cm for two moves of a 13 m/s blade). Feeding each merged move's pose
to the tracker would change the melee's cadence (a melee-tuning change); the moves' poses kept per frame belong with B8's
rewind of hands for melee (Decisions).

Open (B6 and later):

- A thrust (a stab) between players has a line too and goes through the directional parry as a swing does (an armed
  humanoid's thrust is read by the guard only with the chainsaw); untested.
- The parry line kept per frame only while `vr_parry` is on and his hands are tracked (VR_Parry_GuardFrame): a flat-screen
  attacker's swing (VR_HandMotionAction) has no hand lines, so it is read by the guard as before.

### B6 results, part 1 (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 22 of 23 pass, `mp_ground` 12/3 (B's grip takes no weapon at the server: "None", its known flake), then 15/0 twice alone)

| Scenario | Result |
|---|---|
| `mp_rules` (new) | 14/0: the server on `vr_parry_stamina 0`, `vr_melee_speed 4.5`, `vr_mgrp_ogre_kdscale 0.25` (Monster Groups), `vr_grapple_drop_grace 0.3`; B joins with his own (1, 2, 2, 0.05, and `vr_knockdown 0` where the server keeps its default): B's cvars hold the server's values (the default for `vr_knockdown`), his own kept, the five rows locked; `vr_gunangle` (a preference) and `vr_bash_sound` (a sound on a rule page) stay his, unlocked; the server and B know the same 1853 rules; B's Gameplay Presets > Classic is locked; `writeconfig` writes his own values; the server changes two: B follows within 2 reports; B leaves: his own values back, nothing locked. Failed first: no such rules (`vr_serverrules` had 9) |
| `mp_cosmetics` (new) | 9/0: B on full gloves (hue 120, saturation 0.8, lightness 0.4), skin tone -0.4, painted nails (hue 300), height 1.9; A (none of them) draws B's hands with B's tone, glove and nail colours (the shader's inputs within 0.0024 of B's own: his calibration's 255ths), B's body with his tone and his glove's cuff over the bare forearm, his own hands bare; A's copy of B's body posed at 1.9 m. Failed first: B's hands and body drawn bare at A (no look for another player's entities) |
| the other 21 | Unchanged: pass (`mp_ground` as above) |

Single player on the branch: scenario_check PASS (147 scenes, 0 failed), load_models_test 4/0, vr_run_check 12/0 (`scratch/b3_runcheck.sh`); the dev build 0 warnings, the Release-config (ThinLTO) build 0 warnings. Single player gathers no rules and sends none (`svs.maxclients` 1), never shadows (not `remote()`); your own hands' and body's look is read from the same settings as before (`ownLook`).

What changed:

- **Every balance setting a server rule** (`vr_serverrules.cpp`): the settings under the balance pages (Advanced VR Options
  > Combat, Carrying and Throwing, Arcane Dimensions, World; Movement > Climbing, Grappling Hook, Swimming, Player and
  Monster Hitbox; VR Settings > Gameplay), gathered once from Search's index (`menu::forEachSettingPath`), the gameplay
  preset's rows and the melee timing: 1853. Left out: the preferences (`clientprefs::indexOf`), unsaved settings (no
  `CVAR_ARCHIVE`: tests, debug) and a player's own by name (`ownParts`: sounds, haptics, colours, shown, debug, weapon
  offsets, the climbing hand's pose, the grip fit, his stick). Gathered only with a server of more than one player:
  single player builds nothing and sends nothing.
- **A remote server's client plays by them**: QVR_SVC_RULES now carries flags (1: every rule not sent is at its
  default), names and values as strings, chunked to 1000 bytes; a join sends those off their defaults, then each change.
  The client keeps his own values, sets the server's into his cvars (so his own game's reads are the server's), writes
  his own to his config meanwhile (`VR_ServerRulesConfigWrite`, as the scenarios' swap) and gets them back in
  `CL_Disconnect` (`VR_ServerRulesLeft`). The menu: rows locked as before (`locked`), their help giving his own value;
  the Gameplay Presets' buttons locked (`Item::settingRules`; `vr_gameplay_preset` and My Settings refuse with why).
  `vr_serverrules [cvar...]` prints `mp_rules:`/`mp_rule:` lines (Debug > Reports > Server Rules).
- **Another player's look** (`view::lookOf`, `SkinLook`): your hands and body take your settings, another player's hands
  and body his calibration (`mpavatar::lookOf`: tone, gloves and their colour, nails and, new in it, their colour and
  gloss: 29 fields, `vr_avatarcal` version 3); the skin tone, glove, cuff and nail inputs of the shader come from it, one
  code path. Their skin masks and glove layers are found for them too, outside the draw loops
  (`render::precacheSkinMasks` from `VR_AddMpAvatars`). `vr_mp_avatars` prints `mp_look:` and `mp_look_own:`.
- `QVR_NET_VERSION` 15.

Open (B6b):

- Holster positions seen by others: his holstered weapons drawn on his body at his holster places, and `holsterpos` for
  another player (B3f's moved-on item); seated or standing (his body seated); a flat-screen player's body at
  `vr_flat_body_height`; his pauldrons' style and size and the armour's plates on his body; the magazine's box in his
  drawn weapon hull.
- Locomotion's balance settings (speeds, `vr_crouch_speed_scale`, jumps) are not rules: the page mixes them with comfort
  and controls, each player's own. Make the few balance ones rules by name when they matter.
- Not seen in an image yet: A's view of B's gloves (the numbers are the shader's inputs).
- A rule changed from the console while joined changes the server's copy only (his own comes back when he leaves).

### B6b results (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 23 of 24 pass, `mp_flow` 15/1 (Load Autosave's list without the coop autosave once), then 16/0 alone)

| Scenario | Result |
|---|---|
| `mp_cosmetics` (grown) | 14/0: as B6's, then B is given a shotgun in holster 2 (QC impulse 192): A draws it 0.06 units from where B drew it; the server's holsterpos for B is 0.08 from B's holster point; B's pauldrons (steel, size 1.3; A's own leather, 1) on A's copy of his body: skin 4 and scale 29 both sides as B's own, 0.04 units off; B seated (`vr_bodycal_seated 1`): A's copy `seated=1`, pelvis and head 0.00 from B's own after 12 reports (B's standing height relearnt); B's nailgun held: the server's Box3D hull of it is "12 convex pieces and its magazine". Failed first: no holster, pauldron or seated lines (none drawn or sent) |
| `mp_flatbody` (new) | 6/0: B flat-screen, Height 1.8, `vr_flat_body_height 1.5`: A's copy of his body posed at 1.5 m after 1 report |
| the others | Unchanged: pass (`mp_flow` as above; `mp_ground` 15/0, `mp_melee` 29/0, `mp_bandwidth` 7/0) |

Single player on the branch: scenario_check PASS (147 scenes, 0 failed), load_models_test 4/0, vr_run_check 12/0
(`scratch/b3_runcheck.sh`); the dev build 0 warnings, the Release-config (ThinLTO) build 0 warnings. Your own pauldrons
are placed by the same code as before (setupPauldrons now fills a PauldronLook from the same settings), your own leg fit
reads seated::active() as before (posedSeated with no subject); nothing new is sent or drawn with one client.

What changed:

- **Holstered weapons seen by others** (`VrDrawnHolsters`, vr_move.hpp): the move carries the holsters as this client
  drew them last (`view::netHolsters`: each holster's point in body::Holster's order, and each weapon drawn in one: model,
  frame, skin, place, turn, mirroring; 2 + 36 + 16 a weapon bytes, multiplayer only). The server relays the weapons (not
  the points) in the avatar after the holds, then a gear byte (the armour he wears, `gearByte`); the avatar is up to 241
  bytes. A client draws them on his entity as his hands' weapons are drawn: his holster places, their seated layout, his
  Holstered poses, the flare gun's skins, all his own by construction.
- **`holsterpos` for another player**: `server::holsterPoint`, his move's point carried with his entity since (QC's
  magnetic holsters fly home to it; '0 0 0' and QC's estimate only until his first move).
- **The avatar calibration, version 4 (37 fields)**: `sitting` (seated::active()), `flatheight` (vr_flat_body_height),
  the pauldrons' `pstyle`, `psize`, `pfollow` and offsets (`pfwd`, `pout`, `pup`). A seated player's copy is posed as his
  own seated body (no leg fit: `avatar::posedSeated`, the local read unchanged); a flat-screen player's at his flat body
  height (the mock's 1.8 m Height left aside, as units::eyeHeight does for him).
- **Pauldrons on others' bodies**: `view::placePauldrons(body, PauldronLook, player, caps, lames)`, the local body's
  setupPauldrons made a caller of it (yours from your settings and armorWorn(), his from his calibration and his gear
  byte, his shoulders by `avatar::otherShoulder`): one code path. `VR_AliasMirrored` knows his holsters and pauldrons.
- **The magazine in his weapon hull**: `vr_heldpose` version 2 carries `flagMag` (his drawn weapon has its magazine
  seated: view::DrawnWeapon::mag); Box3D's remote drawn weapon hull takes `view::magazineModelOf` of the gun, as the
  local one does.
- Reports (`vr_mp_avatars`): `mp_holster:`, `mp_holster_own:`, `mp_holster_point_own:`, `mp_holster_sv:`,
  `mp_pauldron:`, `mp_pauldron_own:`; `seated=` on `mp_body:`. QC test impulse 190 + slot: a shotgun in that holster
  (not deathmatch). `QVR_NET_VERSION` 16.

Open (B6c):

- The armour's plates on another player's body (`vr_player_plates`: QC's QVR_SVC_ARMOR goes to its own client only; his
  plates, their pieces and colour, would come per player and be drawn on his copy with the plates' bone poses).
  Meanwhile his pauldrons show where his plates would cover them (his gear byte has the armour, not the plates).
- Locomotion's balance settings as rules by name (B6's open item).
- His holsters ride his entity between his moves (as his hands do): a fast turn of his body draws them a move late.
- Not seen in an image yet: the holstered weapons, pauldrons and seated body of another player (numbers only).

## Decisions

- Server rules (B6) are found by page, not listed: every setting under a balance page is one unless it is a preference,
  unsaved, or named as a player's own (`ownParts`); a misfiled setting is one word there. A remote server's client plays by
  them in his own cvars (kept and put back), rather than reading `shown()` at each site: every client-side read of a rule
  is then the server's with no site converted.


- Melee sub-tick samples (B5d) are not fed to the tracker: merged moves are routine (a 90 Hz client on a 72 Hz server) and a swing at 30 ms jitter lands with the same strength (the poses' own clock and the chord's turn correction already give its speed); the arc's middle a merged frame skips is a few cm. Each move's hand pose kept per frame goes with B8's rewind of hands for melee, where it is needed anyway.

- Map download goes through the Map Library alone: the server names the package (its sha256), the client installs it
  from its own index's entry with the installer's guards; no file and no URL comes from the server, so no chunked
  fallback over the game connection (no new network code to untrusted hosts). Joining needs the package to be in the
  Map Library (Quaddicted or Slipseer). `vr_mp_download` defaults to 1: the same packages the Map Library installs on a
  click, from the same mirrors.

- Coop saves keep the players' slots rather than matching them by name: the slot is what numbers their edicts, a
  player who stayed connected keeps it through the load, and a player joining later takes the slot he is given (by name:
  B4b). A coop save is format 2 so that older builds refuse it rather than load it wrong.

- The harness is Python with an in-engine inbox rather than stdin or RCON: it drives GUI clients and dedicated servers
  the same way, needs no network control channel, and stays inert unless `-cmdinbox` is given.
- The lag simulator is a proxy outside the engine (no engine cvars): it tests the real code paths unchanged, and it
  also measures each link's bytes.
- The same-IP fix compares the port too; a client back from a crash on a new port leaves a ghost until the timeout
  (QuakeWorld's `qport` is the full answer: B7, with the snapshot protocol).
- Voice chat (B10) may vendor Opus and RNNoise (his approval, 2026-11-15: official sources, version and sha256
  recorded, their BSD licences in CREDITS).
- Another player's climbing (B5c) is sent as his server-side holds (the lip and the face's way), not his feet: each
  client poses his feet as it poses its own (its footholds, or hanging), one code path, 7 bytes a holding hand.
- Other players' bodies (B2d) are posed by the local body's own code with its per-frame state swapped per player, rather
  than the state gathered into an instance struct read through a pointer everywhere: the same results for the local
  body by construction (its reads unchanged; checked pixel for pixel), one code path, a small diff in five files. Seen
  from outside, they always have legs (his body mode 1 or 2 is his own first-person view); his body mode 0 draws his
  player model as before.

- Another player's holstered weapons (B6b) come as his client drew them (place, turn, frame, skin), as his hands' weapons
  do, rather than his holster settings sent and his holsters solved again on each client: his seated layout, Holstered
  poses and holster offsets are his by construction, one code path, about 16 bytes a holstered weapon a move (B7's
  deltas will make an idle player's nearly free).

Next phase: B6c — the armour's plates on other players' bodies (QVR_SVC_ARMOR per player to every client that sees him, his plates drawn on his copy of the body by the local plates' code), then Locomotion's few balance settings as rules by name; tests first: `mp_cosmetics` grown (B with yellow armour and plates: A's copy draws his pieces and colour).
