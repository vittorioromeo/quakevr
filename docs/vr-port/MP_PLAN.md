# Multiplayer: verified state, architecture, phase plan, test harness

The multiplayer programme (the author, 2026-11-14): the whole game playable in coop and deathmatch, campaigns start to
finish in coop (level changes, saves, loads), clients fetching the maps a server needs; every VR feature synced
(bodies, hands, fingers, holsters, weapons, interactions between players: drops and pickups, melee, parries, shoves,
climbing, crouch, prone); flat and VR players together; per-player settings seen by others as chosen, balance settings
dictated by the server; proximity voice chat on the spatial audio; deterministic, synced physics and navigation;
bespoke delta compression with general compression on top; client prediction for VR responsiveness; all of it test
driven, with end-to-end tests of several real games at once under simulated lag and loss.

This file is the plan. MP_DESIGN.md (2026-09-30) is the older audit, file by file; where the two differ, this file
is the newer (each claim below was re-checked on 2026-11-14 at `origin/next` 17de8a86, branch `agent/mp`).

- **[run]**: measured with the harness (below) on 2026-11-14.
- **[code]**: re-read in the code on 2026-11-14 (file:line at that commit).
- **[old]**: from MP_DESIGN.md, not re-checked.

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
| Dedicated server quit | Exits 0 (MP_DESIGN.md's crash on quit, `vr_imgprefetch` workers, is gone) | 6 runs [run] |
| Flat with VR | A flatscreen client (`-flatscreenmode`) joins a VR host, walks (+forward), is seen; the server knows him per player (`.vr_flatscreen`, the flat crouch bit) | `vr_server.cpp:795-819` [code]; listen run [run] |
| Server rules | Every balance setting (1856: the balance pages', the gameplay preset's, the melee timing, Locomotion's crouch speed, its scale and slope walking; B6, B6c) goes to every client (`QVR_SVC_RULES`); a remote client plays by them while joined (his own kept, back when he leaves) and his menu shows them dimmed | `vr_serverrules.cpp:17-27` [code] |
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
| Prediction, lag compensation (interpolation buffer: done in B7b) | Hits judged where the server has things now; your own moves wait a round trip [code] |
| Delta compression against acknowledged state (done in B7b: record-level, a byte mask within a record) | Field-level coding and an entropy stage: B9 |
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
  (ObeseCatLord, GPL, MP_DESIGN.md's reference) ships a shared pmove with CSQC prediction, co-op rules, ICE/WebRTC
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
| Up (moves) | 2.3 KB/s idle, 4-10 KB/s walking (B7; was 45 KB/s, 640 B x 71) | 8-12 KB/s | about 60 B a quantized delta move + two repeats |
| Down, 2 players idle, e1m1 | 7 KB/s (97 B x 72; B7b; was 97 KB/s) | 6-10 KB/s | arm poses culled and delta-coded; acked deltas |
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
| **B6** (done: rules, the look of hands and body, holsters, seated, flat height, pauldrons, magazine hull; B6c the plates and Locomotion's rules) | Server rules and cosmetics: every balance setting a rule (presets, armed humanoids, damage, mutators, stamina, grapple...), rule pages dimmed on a remote client; holster positions, height, seated/standing, gloves, body seen by others | `mp_rules`, `mp_cosmetics` (B's height and gloves seen by A) |
| **B7** (done: moves up, the avatars' lerp; B7b snapshots down and the interpolation buffer) | Snapshots: acknowledged per-client snapshots and deltas for entities, players and avatars; quantized delta moves with repeats; interpolation buffer with quaternion slerp | `mp_loss` (10% loss: positions still continuous), byte counts per scenario |
| **B7b** (done: B7 is done; fast retransmit of the reliable window to B9) | The rest of B7: acknowledged per-client snapshots down (the client acknowledges each server frame in QVR_CLC_MOVES; entities, player states and avatars delta-coded against the acknowledged one); the entities' interpolation buffer (a fixed delay behind, adaptive to jitter, a history of messages, slerped turns) in place of NetQuake's two-message lerp for remote clients | `mp_loss`'s jump counts become checks (0 jumps of A's body and hands at 10% loss); `mp_bandwidth` down idle under 10 KB/s |
| **B8** (the walk predicted done; the rest is B8b) | Prediction and lag compensation: shared movement code, reconciliation, then climbing and grapple; rewind of hit models and hands for hitscan, melee, parry, shove | `mp_predict` (own move off at frame 0 under 150 ms lag; corrections under 1 unit on flat ground), `mp_lagcomp` (a hit on a moving target at 150 ms lands) |
| **B8b** (shots compensated; the rest is B8c) | The rest of B8: lag compensation (the server's last 250 ms of hit models and hands per entity; a client's hitscan, melee, parry and shove judged at his interpolation buffer's time, capped); the jump predicted (QC's PlayerPreThink jump in the module), swimming, crouched and lying boxes, climbing and the grapple | `mp_lagcomp` (a hit on a moving target at 150 ms lands), `mp_predict` grown (a jump, a swim, a crouch: corrections under a unit) |
| **B8c** (mp_loss's jumps and melee's lag compensation done; the prediction's moves are B8d) | mp_loss's last jumps; lag compensation for melee, parry, shove and the grapple (other players' hands kept); the jump, swimming, crouched and lying boxes, climbing and the grapple predicted, a lift rider | `mp_loss` 0 jumps, `mp_lagcomp` (a punch at 150 ms lands), `mp_predict` grown |
| **B8d** (the jump, the crouched box and a lift rider done; the rest is B8e) | The prediction's moves: the jump (QC's PlayerPreThink jump in vr_pmove.cpp, stamina), swimming, the crouched and lying boxes, climbing and the grapple; a lift rider | `mp_predict` grown (a jump, a swim, a crouch, a lift: corrections under a unit) |
| **B8e** (swimming, the water jump's carry and the movers' motion done; the rest is B8f) | The rest of the prediction's moves: swimming and water jumps, the lying box (the crawl), climbing, the grapple, a fall onto a moving lift (the movers' motion in the replay) | `mp_predict` grown (a swim on e1m4, a crawl, a climb, a grapple pull, a drop onto e1m1's lift as it rises) |
| **B8f** (the drop onto a rising lift and Quake's water jump out done; the rest is B8g) | The drop onto a rising lift's one correction (12.7 units, below), a water jump out tested, the lying box's crawl (the stick's share in the moves), climbing and the grapple (drawn as they come today) | `mp_predict` grown (the drop under a unit, a water jump, a crawl, a climb, a grapple pull) |
| **B8g** (the lying box's crawl done; the rest is B8h) | The lying box's crawl (the stick's share in the moves), climbing and the grapple; the hands' climb out of the water (vr_water_jump 0) | `mp_predict` grown (a crawl, a climb, a grapple pull, a hand's climb out: corrections under a unit) |
| **B8h** (rounds used by B and the mantle done; the rest is B8i) | Climbing in his replay (the mantle's timed path first, then the hang's pull from his own moves' hands), the grapple's pull, the hands' climb out of the water | `mp_predict` grown (a mantle, a hang and pull up, a grapple pull, a hand's climb out: corrections under a unit) |
| **B8i** | The hang in his moves (each move's hands for hangPull/hangMove; the grips, `owed` and the phase in the state), the grapple's pull, the hands' climb out of the water | `mp_predictclimb --part hang` (a hang and pull up: corrections under a unit), `mp_predict` grown (a grapple pull, a hand's climb out) |
| **B9** (the review done; #1-#2 unified; the rest B11a-c) | Code quality: one code path for single player and multiplayer (section "B9 review") | the MP suite, SP regressions identical |
| **B11a-c** (done: B11a, B11b parts 1-2, B11c parts 1-2) | The rest of the B9 review: heldpose's own-player view asks into the slot store; vr_pmove for every player (then monsters); one avatar routine for any player entity | SP hash audits + MP suite |
| **B12** (was B9; the poses in the snapshot, the rate and its priorities done; the rest is B12b) | Compression and rate: entropy stage with trained tables, per-client rate, priority accumulator | byte budgets per scenario from section 2.3 (`mp_armed`) |
| **B12b** (done: the entities by their fields and trend, the reliable window's resends; entropy measured, not needed) | The entities' field-level coding (the largest kind left in combat), the fast retransmit of the reliable window, then an entropy stage only if it pays against them | `mp_armed`'s combat budget lowered; a lost reliable chunk resent within a round trip |
| **B10** (done: B10v and B10v part 2, the Voice Chat page; his VR checks open) | Proximity voice chat: capture, Opus, VAD, noise suppression, push-to-talk, a jitter buffer, voices placed by Steam Audio at the speaker's head; VR and flat; menu rows | `mp_voice` (a test tone from A heard at B, louder near, delay under 250 ms) |
| **B11** (part 1 done in B13: `mp_dm`, bots, frags, respawn, the fraglimit; the rest is B11 part 2) | Deathmatch and bots: frags, respawns, DM item rules with VR weapons, FrikBot with VR-aware bodies | `mp_dm` (two clients and two bots, a fraglimit reached) |
| **B11 part 2** | The rest of B11: deathmatch 1 and 2's item rules with VR weapons (a level weapon taken and respawned, or staying; `vr_item_objects`, force grab), the VR melee and armed systems between players and bots in deathmatch (a blade's hit, a disarm), bots' bodies and hands as the clients draw them, FrikBot's walk through the shared movement (`vr_pmove`), names in the harness | `mp_dm` grown |
| **B12** (B14 part 1 done: the moves' results client against server, the thread audit; the world's hashes are B14b) | Determinism: Box3D, navmesh and pathfinding hashes equal across runs with several clients; the server-thread audit in multiplayer | hash scenarios |
| **B13** (done: the browser, the net HUD, B13b's 16 players at real time, B13c's rewind at the places sent and send rate; ICE/NAT on his OK) | Polish and performance: ICE/NAT traversal (optional), server browser, latency HUD, CPU of many players | soak tests |
| **B15** (done) | In and out of a coop game in progress: a late joiner's world as the others have it; a leaver's things kept for his return by name; a crashed game's ghost closed when he connects again; the server's quit seen by its clients | `mp_rejoin` |

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
  seen and, on a server, its clients (`mp_server: map=... spawned=N`); each client's moves read (`mp_netmoves:`) and
  snapshots sent (`mp_netsnap:` sent, full, delta, cut, bytes, the acknowledged one); on a client its snapshots read
  and its interpolation buffer (`mp_snapcl:` decoded, undecodable, its delay, frames held past the newest).
- `vr_netmove_selftest`, `vr_netsnap_selftest`: the moves' and the snapshots' codecs.
- `vr_mp_predict_report [reset]` (Debug > Reports > Network: Prediction Report; also in `vr_mp_status`): a remote
  client's prediction (`mp_predict:` states read, frames predicted, moves replayed a frame, corrections all and on flat
  ground); `vr_mp_predict_probe <label>`: `mp_predict_moved:` when his own drawn entity first moves (moves sent, frames,
  ms since); with `vr_mp_watch 1` each correction over a unit prints `mp_predict_corr:` (the state, both places, notes).
- A dedicated server started with `QVR_TEST_HIDDEN` opens no console window (`sys_sdl_win.c`).
- **No test talks to the internet** (B24): every harness game runs with `-loopbackonly` (net_main.c: names other than
  "localhost" never looked up, nothing resolved to or sent outside 127.0.0.0/8, no reverse lookups, IPX or add-on
  downloads; each refusal prints `NET SAFETY:`); the harness refuses to start a game, or send it a command, that points
  it elsewhere (vr_mp_room on with a non-loopback vr_mp_rendezvous, a `connect` elsewhere, a room-code join through a
  service elsewhere); each scenario ends with the check "every game kept to loopback" (each live game's vr_mp_room and
  vr_mp_rendezvous asked, every log read for `NET SAFETY:`). `mp_netsafe` (first in the suite; the suite stops if it
  fails) tests all three.
- **Under load** (B25): the games run at normal priority (idle while the author plays; `QVR_MP_PRIORITY=below`);
  `--load N` (`mp_load.py`) keeps N busy CPU workers at normal priority for the run, as other builds and tests do
  (`--load 16`: the reference; `--load-duty` each one's share of the time); `suite --only a,b`, `--suite-repeat N`.

Running it (in a worker's checkout `--base` defaults to its own kit base, `bases/<checkout>/qbase`, whose
`id1/autoexec.cfg` the kit's runs leave with their script: emptied automatically; elsewhere pass `--base` and
`--reset-autoexec`; the ports are the checkout's own block from 27600, a busy block skipped to the next free one):

    python Misc/quakevr/mp/mp_test.py suite                  (the default profile; --profile long|all: TESTING.md)
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
| `mp_version` | A client with `+vr_net_test_version 1` is told "This Quake VR server has network version 2, your game 1..."; the server logs it; a right one joins |
| `mp_avatar` | B's copy of A's head and hands within 0.10 units of the server's, the main hand's angles exact, the weapon the same; B has none of himself |
| `mp_coop`, `mp_coop_lag` (150 ms, 2% loss) | Unchanged: pass; B sees A move off in 184-211 ms under the lag; signon over the lossy link with the chunked reliable channel |

What changed:

- `net_dgrm.c`: reliable messages in chunks of at most 1200 B (`DGRM_RELIABLE_CHUNK`), up to 16 in flight
  (`DGRM_RELIABLE_WINDOW`); the receiver is NetQuake's (acks counted in order only; a 1 s timeout resends what is in
  flight, go back N). Works with unchanged peers in both directions.
- The connect request carries "QVR" and `QVR_NET_VERSION` (2, `vr_api.h`): a server with the VR protocol rejects a
  client of another version or none, with the reason; `qsocket_t.qvrNetVersion` keeps it per client.
  `vr_net_test_version N` fakes another (the test).
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

- (Superseded 2026-11-15, his call: no extra reach. `vr_weapon_palm_grab_slack` is gone; next's palm grab from
  agent/palmgrab took its place when B7 merged next: the handle in normal reach -> gun mode, else a prop at the
  contact, `vr_pickup_test 17`.)
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

### B6c results (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 23 of 25 pass, `mp_flow` 15/1 (its known autosave-list flake) and `mp_pickup` 5/1 (A never put at his spot: "None"), then 16/0 and 9/0 alone)

| Scenario | Result |
|---|---|
| `mp_cosmetics` (grown) | 20/0: as B6b's, then A faces B (an image: B's steel pauldrons, green gloves and holstered weapons at A, `A_sees_B_cosmetics.png`); B given yellow armour (`give a 150`): A's copy of his body draws `progs/vrbody_bare.mdl#armor` (the plates made over B's body model there), skin 1 (yellow), pieces 1023 as B's own after 2-3 reports, 33 of 33 bone slots posed by B's body's bones as drawn, the joints' leather on; no pauldron cap under a plate (none on either side, as B's own); A, unarmoured, has no plates; an image (`A_sees_B_plates.png`: B plated in yellow, helmet on); B's armour worn down (`give a 101`): A's copy sheds to B's own pieces (223) after 2 reports. Failed first: no `mp_plates:` line (nothing drawn) |
| `mp_rules` (grown) | 14/0: the server on `vr_crouch_speed_scale 0.5`, `vr_slope_walk 0` and `vr_crouch_speed` at its default; B joins with 0.9, 1 and 0: he plays by the server's, his own kept and back when he leaves, the rows locked; `vr_movement_mode` and `vr_move_curve` (Locomotion's controls) stay his. 1857 rules. Failed first: the three not rules |
| the others | Unchanged: pass (`mp_flow`, `mp_pickup` as above) |

Single player on the branch: plates_model_test 9/0 (the local plates' code now shared), scenario_check 145 scenes run, 0 failed (its FAIL is 2 checklist tags whose scenes `next` set aside: `flat_holster_drag`, `flat_clip_unpaid`, the same on origin/next), load_models_test 4/0, vr_run_check 12/0; the dev build 0 warnings, the Release-config (ThinLTO) build 0 warnings. Your own plates are placed and posed by the same code as before (`placeOnBody`, `fillPlateSlots` split out of setupPlates and plateBonePoses); nothing is drawn for others with one client.

What changed:

- **Other players' plates** (`view::placeOtherPlates`, `view::otherPlateBonePoses`): the server already sent each
  player's plates (QC's `.vr_pp_type`, `.vr_pp_mask`) to every client in `QVR_SVC_ARMOR`; a client now draws them on
  his copy of the body: the plates' model and the joints' leather made over his build's body model
  (`agarmor::playerArmorModel`, `playerBridgeModel`), coloured by his armour, their bones his body's as drawn (with its
  head: his helmet shows), a piece he lacks collapsed, by the code that poses yours. Their bones are made into his
  avatar's buffers (`agarmor::playerBridgePosesInto`: the leather's, not the local frame cache), at his body's scale
  (`mpavatar::plateBody` for `avatar::modelScale`). Your `vr_player_plates` 0 shows nobody's. His pauldrons give way to
  his plates as yours do. No morph: a piece of his comes and goes at once (his debris still flies from QC); B11c part 2
  gives his the morphs yours have.
- **Locomotion's balance settings are rules by name** (`locomotionRules`: `vr_crouch_speed`, `vr_crouch_speed_scale`,
  `vr_slope_walk`); the stick's mode, curve, autorun and speed key stay each player's (the server caps speed by
  `sv_maxspeed` anyway).
- Reports (`vr_mp_avatars`): `mp_plates:` (type, pieces, skin, slots and those shown as last drawn, leather, model),
  `mp_plates_own:`. No protocol change (`QVR_NET_VERSION` stays 16).

Open (after B6):

- His holsters ride his entity between his moves (as his hands do): a fast turn of his body draws them a move late (B7's
  interpolation buffer).
- Another player's plates appear and go without the morph yours have (a morph per player needs his own morph state;
  cheap to add if it shows in VR). **Done** (B11c part 2: `view::morphPlates`, his own state).
- The checklist's two set-aside scenes (`flat_holster_drag`, `flat_clip_unpaid`): the coordinator's, on next too.

### B7 results, part 1 (2026-11-14, dev no-LTO build, loopback; 11 scenarios run, all pass: `mp_loss` 11/0, `mp_bandwidth` 8/0, `mp_coop` 11/0, `mp_coop` at 150 ms + 2% 11/0, `mp_avatar` 17/0, `mp_grab` 13/0, `mp_melee` 29/0, `mp_moves` 19/0, `mp_pickup` 9/0, `mp_teleport` 9/0, `mp_held` 14/0)

| Scenario | Result |
|---|---|
| `mp_loss` (new) | B's link loses 10% each way; A walks in a circle, B traces his copy 300 frames twice (`vr_mp_trace`). The server runs B's moves with at most 2% lost (repeats recover the rest; none undecodable); A's hands ride his body: offset roughness 0.28-0.31 with `vr_mp_avatar_lerp` (14-22 off); B's up link walking 9.1-9.7 KB/s (mean 135 B). Jumps of A's whole body (1-2 in 300 frames) are metrics: B7b's |
| `mp_bandwidth` (grown) | B's link up idle 2.2 KB/s, mean 30 B a packet (was 45 KB/s, 640 B); down idle 20 KB/s as before |
| byte counts | Every scenario prints `mp_bytes:` (B's link over the run) and `suite` ends with the table. Up, mean a packet: coop 54-108 B, avatar 51, melee 62, moves 80, grab 113, held 121 (was 640 B everywhere); down unchanged (270-520 B): B7b |

Single player on the branch: `vr_netmove_selftest` 5/0 (324 values a move; a round trip quantizes back to the same
integers; deltas read back; an idle move 11 B, one from zeros 268 B), `vr_pickup_test 16` 3/0 and 17 5/0 (after the
merge of next). Not run this session: scenario_check, load_models_test, vr_run_check, the release-config build (the
single-player path is clc_move as before; `SV_ReadClientMove` only split into read and `VR_ApplyClientMove`).

What changed:

- **Moves up as quantized deltas with repeats** (`vr_netmove.cpp`, `QVR_CLC_MOVES` 16, `QVR_NET_VERSION` 17): a remote
  client writes each packet's moves as one message: the base (the newest move the server acknowledged in
  `QVR_SVC_MOVEACK`, each datagram), the count and the first sequence number, then each move as the delta of its 324
  integers against the one before (a group mask, a mask byte a changed group, zigzag varints). Places are 64ths of a
  unit from the move's origin (the hands, head, muzzles, ports; the magazine box and pouch absolute, their zero
  kept), angles 65536ths of a turn (not wrapped), speeds 1024ths, the pose clock 10 us; the drawn hands and holsters
  as the avatar sends them (eighths, curls in 16ths, the thumb in 127ths). The moves after the acknowledged one are
  repeated (at most 2 besides the new): the server runs each once, in order (`VR_ApplyClientMove`, then the VR
  block), counts lost, recovered, undecodable (`mp_netmoves:` in `vr_mp_status`). Both sides keep 64 moves; an
  acknowledgement older than that, or none (a new map or connection: `VR_OnClientClearState`), sends the next from
  zeros. The loopback client (single player, the listen host's own) still sends clc_move with floats.
- **Avatars lerped** (`vr_mp_avatar_lerp` 1, Debug > Reports > Network: Smooth Player Hands): each avatar keeps the
  one before; drawn between them at the entities' lerp point (the client's time between the last two messages), places
  lerped, turns slerped as quaternions, curls lerped, holstered weapons too; a hand or weapon that came or went, or no
  avatar in the message before (an unchanged one is not sent: it held until then), draws the last as it came.
- `vr_mp_trace <ent> <frames>`: B's drawn copy of a player frame by frame (`mp_trace:`, then `mp_trace: done`).
- Merged next: `vr_weapon_palm_grab_slack` dropped for next's palm grab (his call).

Open items (B7b):

- The down link: snapshots acknowledged per client and delta-coded (entities, player states, avatars): idle down
  is still 20 KB/s, in play 18-31 KB/s.
- The entities' interpolation buffer: NetQuake's two-message lerp clamps its clock to the newest message, so under
  loss a player's whole body jumps 1-3 times in 300 frames (`mp_loss`'s jump counts; the avatars ride it). A fixed
  delay behind with a history of messages makes these checks (0 jumps).
- Fast retransmit of the reliable window (a lost chunk still waits 1 s).

### B7b results (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: all 25 scenarios pass)

First, what B7 part 1 skipped, on its tip: the suite 24 of 25 (`mp_loss`'s hand roughness 0.68 against 0.5: NetQuake's
lerp under loss, this phase's), scenario_check PASS (149 scenes), load_models_test 4/0, vr_run_check 12/0.

| Scenario | Result |
|---|---|
| `mp_loss` (grown) | 15/0: at 10% loss each way, 0 jumps of A's body and 0 of his hands in 300 of B's frames (NetQuake's lerp: 1-3), hand offset roughness 0.21 (was 0.28-0.68); B read every snapshot that came (164, none undecodable), the buffer's delay 40 ms, its time past the newest message 6 frames of 600 (a gap longer than the delay: held, no jump). Failed first: no jump checks, no `mp_snapcl:` |
| `mp_bandwidth` (grown) | 9/0: B idle on e1m1 down 7.0 KB/s, mean 97 B (was 20 KB/s, 270 B; the budget now 10 KB/s); the server's snapshots to B nearly all deltas against the one he acknowledged. Failed first: 20 KB/s over 10 |
| byte counts | Down, B/s (B7 part 1 -> B7b): coop 30.7 -> 18.9 K, avatar 22.4 -> 9.6 K, body 23.7 -> 14.3 K, grab 23.1 -> 16.1 K, held 27.4 -> 18.0 K, campaign 58.0 -> 25.0 K, flow 67.5 -> 30.4 K, melee 30.7 -> 9.7 K, moves 25.6 -> 15.4 K, pickup 25.4 -> 10.7 K, climb 11.5 -> 4.8 K, loss 18.6 -> 7.4 K. What is left in play is mostly the armed humanoids' poses (`QVR_SVC_ARMPOSE`, scoped, not yet in the snapshot) and the broadcast |
| the others | Unchanged: pass |

Single player on the branch: scenario_check PASS (149 scenes run, 0 failed), load_models_test 4/0, vr_run_check 12/0
(run B checked `3 1 0 1 0`), `vr_netsnap_selftest` 5/0, `vr_netmove_selftest` 5/0; the dev build 0 warnings, the
Release-config (ThinLTO) build 0 warnings. The loopback client gets NetQuake's datagram (no snapshot) and draws with
NetQuake's lerp (the buffer is off without snapshots): single player and the listen host's own view are unchanged.

What changed:

- **Snapshots down** (`vr_netsnap.cpp`, `QVR_SVC_SNAPSHOT` 46, `QVR_NET_VERSION` 18): `QVR_CLC_MOVES` begins with the
  newest snapshot the client read. For a client that sends `QVR_CLC_MOVES` (remote), the server's writers of the player
  state (svc_clientdata, from its first byte: damage and setangle stay events), each entity's update and each avatar it
  sees (`QVR_SVC_AVATAR`, every frame now for it: the keyframe wait is the other clients') cut what they wrote off the
  datagram as records keyed 0, the entity's number, 65536 + the player's entity (`VR_SnapTake`, `VR_SnapEnd`). The
  records, sorted by key, go as one message coded against the acknowledged snapshot (both sides keep 32): unchanged
  records nothing, a changed one a byte mask over the base's bytes or whole (whichever is smaller), a new one whole, a
  gone one its key; past the datagram's room the rest stay as the base's on both sides. The client rebuilds the set and
  parses each record with NetQuake's parsers (`CL_ParseUpdate`, `CL_ParseClientdata`, the VR dispatch), so the rest of
  the client sees NetQuake's whole datagram. A spawn (a new map) or an acknowledgement of 0 starts again from whole; a
  demo begun mid-game acknowledges 0 until a whole one comes (its start). `vr_net_snapshots` 1.
- **The interpolation buffer** (`netsnap::interpEntity`, `vr_net_interp` 1): a remote client keeps each entity's last 8
  places and angles with their messages' server times and draws the others at a time a delay behind the newest message:
  between the two messages around it, turns slerped (`mpavatar::slerpAngles`, now shared), a teleport (over 100 units)
  not lerped. The delay is the longest recent gap between messages plus half a tick (1.5 ticks to 250 ms; the longest
  fades 10 ms a second), its clock eased at most 15% faster or slower to follow (a stall of over 250 ms resets it). Not
  for brush models (doors and lifts stay in step with the player's own entity, which he rides: NetQuake's lerp) nor for
  stepping monsters (the renderer lerps their steps). The other players' avatars are drawn at the same time from their
  own last 8 (B7's two-message lerp without snapshots).
- Reports: `mp_netsnap:` (server, per client), `mp_snapcl:` (client) in `vr_mp_status`; Debug > Reports > Network:
  Delta Snapshots, Interpolation Buffer.

Open (after B7):

- The armed humanoids' poses (`QVR_SVC_ARMPOSE`) and the broadcast are outside the snapshot: in play they are most of
  what is left down (B9: the poses as records, the priority accumulator).
- Field-level coding (a moving entity's record goes as its changed bytes, about 6-10 B) and the entropy stage: B9.
- Fast retransmit of the reliable window (a lost chunk still waits 1 s): B9.
- Your own player, and what you ride, at NetQuake's lerp while the others are a delay behind: B8's prediction.

### B8 results, part 1: the walk predicted (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 23 of 26 pass: `mp_moves` 17/2, `mp_forcegrab` 5/1, `mp_loss` 14/1, below)

| Scenario | Result |
|---|---|
| `mp_predict` (new) | 11/0: B at 150 ms each way. His own move shows 2 moves (client ticks) after his press, 19 ms (without prediction: 23 moves, 328 ms); 22.8 moves replayed a frame (the round trip); over 325 states read while he walks there and back on e1m1 (the start's floor, its ramp and steps down north) every correction 0.000 units (flat ground 117 of them, 0.000); at rest he draws himself where the server has him. Failed first: no `mp_predict:`, no probe |

Single player on the branch: scenario_check PASS (150 scenes run, 0 failed), load_models_test 4/0, vr_run_check 12/0
(run B `3 1 0 1 0`), `vr_netmove_selftest` 5/0, `vr_netsnap_selftest` 5/0; the dev build and the Release-config
(ThinLTO) build 0 warnings. The loopback client sends clc_move: sv_phys.c's walk as before.

What changed:

- **One walk module** (`vr_pmove.cpp`): sv_user.c's SV_AirMove (the head steering, friction, acceleration, the air's) and
  sv_phys.c's gravity, SV_FlyMove, SV_WalkMove (steps, wall friction, the unstick) with a move's own time for
  host_frametime, the room-scale walk and vr_slope_walk's gravity, over a `Body` whose hooks are what only the server
  has (touches, links, props stood on and shoved, the slope's record, close quarters, the teleporter's back-off).
- **The server runs a remote client's moves one by one** (`vr_predict.cpp`, `vr_net_permove` 1): each move read is
  queued with its frame's time (a new field of the move, `frameTime`, 10 us); a frame whose walk is plain (walking,
  alive, not swimming, water-jumping, lying or flat-screen) skips SV_AirMove and runs the queue through the module in
  SV_Physics_Client (once SV_CheckWater and SV_CheckStuck, as before; QuakeC's PreThink and PostThink once a frame as
  before), each move held to a clock credit (the server's time x 1.05, at most 0.25 s ahead). A hanging, climbing or
  teleporting frame takes its own path as before and drops the queue. Single player and a listen host's own client send
  clc_move: none of this runs for them.
- **QVR_SVC_PREDICT** (47, `QVR_NET_VERSION` 19): his state after the newest move run (its number, origin, velocity,
  flags, the ground normal and its age) and the rules it ran by (the speed with stamina, carry, heavy weapon and crouch
  scales, the stick's scale, friction, gravity, step...; the narrowed hull's width, the edge probe), as a snapshot
  record (an idle player's costs a few bytes).
- **His client replays** (`vr_net_predict` 1): each frame, the moves sent after that state through the same module,
  against this map's world and brush models with the server's narrowed box (`hull::clientPlayerTrace`: vr_hull.cpp's
  brushes rebuilt from the client's map; Quake's hull 1 without narrowing) and the server's ledge probe; his entity (the
  view, hands, body) drawn between the places before and after his newest move by the time since it was made (frames
  faster than ticks: no step a tick); a new state that moves the prediction eases in (decays by e in 0.1 s; 64 units or
  more: at once); the door or lift he stands on drawn at its newest place (where his replay met it). Without a plain
  state (the server's flag) he is drawn as it comes.
- Debug > Reports > Network: Predict Own Walk, Moves Run One by One, Prediction Report.

Open (B8b), first the suite's three:

- **`mp_moves`' crouch** (new failures, B8's): the watcher's copy of a crouching player keeps its standing head (0 units
  lower) both ways; with `vr_net_predict 0` on the clients (the server still one by one) A's way passes (25 units lower)
  and B's still fails, his copy read standing at another place (454 -280: still eased from face_off's setpos). Suspects:
  the avatar's head height (`vr_avatar.cpp` standing(), eyeHeight) against the predicted origin, and the stand reading
  taken mid-correction after a setpos. First thing next session.
- `mp_forcegrab` 5/1: "B never put at OPEN_SPOT (the server has him at None)": no B line in the server's status (B3f's
  put_at flake, or his setpos: to check with the prediction on and off).
- `mp_loss` 14/1: one snapshot undecodable of 168 at 10% loss (B7b's check; each snapshot now carries the prediction
  record: to check whether a cut record's base went missing).
- Lag compensation: the server's last 250 ms of hit models and hands, judged at his interpolation buffer's time.
- The jump is QuakeC's (PlayerPreThink): the server's state shows it a round trip later and the correction eases it in;
  predicting it means the jump in the module (QC's rules, stamina). Swimming, water jumps, the crouched and lying boxes,
  climbing, the grapple, a flat-screen player's slide: drawn as they come (not predicted), each a case for the module.
- Other players and monsters are not in his replay's world (the server's walk meets them): a bump into one is a
  correction. Close quarters (vr_closequarters) too.
- A move's time is the client's tick (host_frametime); in slow motion (VR_TimeScale) the server's credit holds him.

### B8b results, part 1: the suite's three, lag compensation (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 26 of 27 pass, `mp_loss` 13/2 (A's copy's body 1 and hands 2 "jumps" at 10% loss, 249 frames a second: 2 runs of 6 today, open); scenario_check 150/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_forcegrab` | 9/0 (was 5/1): B's client had died at the join, "Host_Error: SZ_GetSpace: overflow" from Cmd_ForwardToServer: his own commands (prefs, held poses, calibration) filled the 1 KB cls.message and the harness's typed "name B" overflowed it. The client's own commands now leave 512 bytes (client::roomForCommand; the calibration had no check at all) and cls.message is 4 KB. Any client of any server: on its own commit |
| `mp_loss` | 15/0 three runs of four (was 14/1): the undecodable snapshots were deltas whose only change was B8's predict record (key 0x20000: a key step of 0x20001 from none, over decode's limit of 2 x avatarKey). `vr_netsnap_selftest` 6/0 with the case. The fourth run: 13/2, A's body and hands "jump" twice at 200 frames a second (the others 244-252): the machine busy, not the codec (no undecodable) |
| `mp_moves` | 19/0 (was 17/2): the test's: the stand was read from a copy still at his last place, the crouch by the moves byte (the newest message's) before the head (the interpolation buffer's, a few frames behind) came down; both now waited for |
| `mp_lagcomp` (new) | 9/0: B at 150 ms each way shoots A strafing 160 units off (vr_mp_shoot: at A's middle as B draws him): 12 of 12 counted shots hit judged where B saw him, 4 of 12 judged where the server has him now; B's moves put the others 312-344 ms back |

What changed:

- **Lag compensation** (`vr_lagcomp.cpp`, `vr_net_lagcomp` 1, `vr_net_lagcomp_most` 0.5 s): the server keeps the last 80
  frames of the players' and monsters' places, angles, frames and hit model tracks (while a remote client plays); each
  remote move says what its client saw (`WireMove::viewBack`: his interpolation buffer's distance behind the newest
  message, `QVR_NET_VERSION` 20); a MOVE_HITMODEL traceline of his (PF_traceline: the shots) runs with each target put
  back there (a player between the frames around that time, a stepping monster at the newest message's time as NetQuake's
  lerp draws it, hit model poses at that time: `hitmodel::detail::poseNow`), then puts them back. The world, doors and
  lifts stay. Single player and the listen host's own client: nothing (clc_move, no view behind).
- Debug > Reports > Network: Lag Compensation. `vr_mp_shoot <ent>` (client) / `vr_lagcomp_shot` (its server side): a
  test shot reported (`mp_lagcomp_shot:` plain and rewound hits), nothing hurt.

Open (B8c):

- Melee, parry, shove and the grapple are not compensated yet (their hits are engine sweeps and QC builtins, not
  PF_traceline: hitmodel_segment and vr_admelee's sweeps take the same begin/end); other players' hands (parry) need
  their poses kept too (each move's hand pose: the B5d decision).
- Missiles are not compensated (as in most games: they fly in the server's world).
- `mp_loss`'s "jumps" check (A's copy at 10% loss): 1-2 jumps in 2 runs of 6 (frame rate normal, no undecodable snapshot left): to find first in B8c.
- The prediction of jump, swimming, the crouched and lying boxes, climbing and the grapple, and a lift-rider test
  (`mp_predict` grown: corrections under a unit): not started this session.

### B8c results, part 1: mp_loss's jumps, melee lag compensated (2026-11-14, dev no-LTO build, loopback; `mp_test.py suite`: 26 of 28 pass, `mp_loss` 12/3 and `mp_melee` 28/1 on a busy machine (B at 114 frames a second, other agents' games and builds running); alone on a quiet one 15/0 and 29/0)

| Scenario | Result |
|---|---|
| `mp_loss` | 15/0 in 6 runs of 6 (was 13/2 in 2 of 6): 0 jumps of A's body and hands at 10% loss. The cause: 3-4 packets lost in a row (or late) left the buffer's clock past the newest message (its delay 38 ms, two ticks and a margin): the copy held at the newest place, then the next message's lerp put it 14 ms of motion (4-7 units) further in one frame. The server's own samples of A are uneven too (0 to 670 u/s tick to tick: his moves reach the server in bursts), which the lerp smooths |
| `mp_lagcomp` | 11/0 (grown): 12 of 12 test punches (`vr_mp_punch`: a fist's 32-unit line, radius 4, into A's middle as B draws him, judged as melee's sweep judges: hitmodel_segment, a player's box grown by the line) land where B saw A, 5 of 12 where the server has him now; the shots as before (12/12, the punch sent with each shot so A's legs keep their length) |
| `mp_loss` (6 runs above: 15/0; a seventh on a quiet machine 15/0) | `jumps` now scales the neighbours' steps by the buffer clock's advance each frame (`clock_steps`): a long frame is no jump. It keeps the pre-fix runs' real jumps (1 body, 2 hands) and clears the busy suite run's lerp-on trace (B's frames alternating 4 and 15-20 ms: 3 body jumps before, 0 after); its hands' roughness (1.15) still fails there. At 33-61 frames a second (heavier load later) everything fails: the check wants a quiet machine |
| `mp_melee` | 29/0 three runs of five (the punch, parry and bash each way now run through the scope); the two others on the busy machine: 28/1, "A's sword on B: his crowbar held level ... does not parry it": A's swing struck nothing at all (no dirparry line, no damage); `mp_blade` alone 13/0 twice. Open: whether B's hands put back by a long frame's view time stood in an older pose (a trace of the scope's hand poses) |

Single player on the branch: scenario_check PASS (150 run, 0 failed), load_models_test 4/0, vr_run_check 12/0 (run B
`3 1 0 1 0`), the dev build and the Release-config (ThinLTO) build 0 warnings.

What changed:

- **The interpolation buffer's clock** (`vr_netsnap.cpp`, `cushion` half a tick): within half a tick of the newest
  message it slows (its rate times the room left over the cushion), to a stop at it; the delay's rate catches up after
  (at most 1.15). `holds` counts the slowed frames. `vr_mp_trace` prints the clock (`rt`, `nw`, `dl`) and the traced
  entity's newest sample (`st`, `so`): `scratch/b8c_jumps.py`'s analysis.
- **Lag compensation's QC scope** (`lagcompbegin(e)` / `lagcompend()`, vr_lagcomp.cpp): `VR_Melee_Sweep`,
  `VR_Bash_Find` and `VR_Headbutt_Target` are wrappers of their old bodies (`...Now`) in the scope with self; `VR_Parry`
  runs `VR_Parry_HandFor` in the attacker's (another player's blow: the defender's guard where the attacker saw it).
  Nested scopes (a traceline inside) do nothing. The kept frames now hold each player's hands (`handpos`, `handrot`,
  both hands; pos lerped as the origin); `end` restores each target's present place plus what the scope moved it, and
  keeps a frame, angles or hand turn the scope changed.

Open (B8d):

- The prediction's moves (the jump, swimming, the crouched and lying boxes, climbing, the grapple, a lift rider):
  `mp_predict` grown, corrections under a unit, the worst case never desynced.
- Not compensated: the grapple's hook (a missile: it flies and bites in the server's world, `hitmodel_any` on touch),
  missiles, the directional parry's deferred blows (`VR_DRP_Blow`), tackles (bodies meeting in the server's world).
- Hand poses are kept per server frame (lerped between frames), not per move: a 90 Hz client's merged moves differ by a
  few mm (the B5d decision).

### B8d results, part 1: the jump, the crouched box, a lift rider (2026-11-15, dev no-LTO build, loopback; `mp_test.py suite`: 26 of 27 pass, `mp_predict` 5/1 (its probe's race, below: then 20/0 in 4 runs of 4); scenario_check 150 run, 1 failed (`bench_armed16_enf`, "what it placed is gone": 3 of 3 alone), load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_predict` (grown) | 20/0: B at 150 ms each way. His own jump shows 1-2 moves after his press (was a round trip). Jumping in place and walking (across the start's 16-unit step, under its ceiling): every correction 0.000 over 319-325 states, 250-262 of them in the air, drawn offset 0.000 (failed first: 9.3-10.4 units at the step, below). A crouched walk (box 36, maxspeed 240): 0.000 over 210 states. A ride on e1m1's lift (func_plat *7, 150 units up): 0.012-0.031 over 176-180 states, 50-69 of them with the lift moving under him, drawn offset at most 0.023 (failed first: 3.7 units a state walking on its top, the drawn offset 10.3) |

Single player on the branch: scenario_check 150 run (the one failure a busy machine's: alone 3/3), load_models_test 4/0,
vr_run_check 12/0 (run B `3 1 0 1 0`), the Release-config (ThinLTO) build 0 warnings. Nothing of single player's or
the listen host's own client's path changed (`.vr_movejump` 0 there: QuakeC's jump as before).

What changed:

- **The jump in the moves** (`vr_pmove.cpp`): QC PlayerJump's rule (on the ground, the key released since the last;
  270 up; FL_JUMPRELEASED and FL_ONGROUND cleared) in each move after the air move and before gravity, as a server frame
  runs PreThink; the move's key is clc_move's bit 2. On a frame walked move by move the engine sets the QC field
  `.vr_movejump` and PlayerPreThink leaves the jump and its release to the moves (the server's `jumped` hook plays
  PlayerJump's sound and clears `.button2`); lying down and AD's Jump Boots stay QuakeC's (the field 0 for them: no
  jump in the replay, a correction). The state carries the released bit and the jump speed.
- **The crouched box in the replay**: the state's rules carry his box's height (`hull::crouchedHeight`);
  `hull::clientPlayerTrace` takes it (Quake's 32 wide with `vr_hull_width` 0, as moveBox).
- **A remote client's crouch read from his own head over his own feet** (`vr_server.cpp`): the move's head is placed from
  his client's origin, his prediction, a round trip ahead of the server's; read against the server's origin a jump's rise
  was a crouch (boxes 52 and 44 in the air), and the server's shorter box passed under the ceiling his replay met (the
  9-10 unit corrections). Now the head is moved by the server's origin less his (`move.origin`).
- **A lift rider**: the replay's ledge test meets the brush models too (`MOVE_NOMONSTERS`, as the server's: a lift's top
  was a drop, double friction, 1.8 units a state walking on it); a lift under him at the last replay and now, moved
  since, carried him: not a correction (eased in behind it, each state's 2 units sank him 10-16 units into it).
- `QVR_NET_VERSION` 21. `vr_mp_predict_report`: `air_n`/`air_max`, `ride_n`, `err_max` (the drawn offset), `unwalked`,
  the rules' `maxspeed`/`box`/`jump`. `vr_mp_watch 2`: each move traced (`mp_predict_sv` on the server, `mp_predict_m`
  each replay's), `vr_mp_watch 1` each jump (`mp_predict_jump`, the server's `jumped`/`held` moves).
- `mp_predict`'s probe waited for B's stop with no move seen (stopped at once, a few units off: no `mp_stop`): it waits
  for his move first (the suite's 5/1).

Open (B8e):

- Swimming, water jumps, the lying box (the crawl), climbing, the grapple, a flat-screen player's slide: drawn as they
  come (not predicted).
- A fall onto a moving lift: his replay meets it where it is, not where it will be (1.5-1.9 units on landing, eased in);
  the movers' motion in the replay (each move's time on a pusher's velocity) would take it away.
- A room-scale jump (standing up at once from a crouch) is his own move's bit: predicted as any jump.
- The `mp_melee` flake ("crowbar held level", busy machine): not seen this session (29/0 in the suite); not looked into.

### B8e results, part 1: swimming, the movers' motion (2026-11-15, dev no-LTO build, loopback; `mp_test.py suite` 27 of 27 pass, `mp_predict` 22/0 (21/1 in 1 of 2 runs alone, below); scenario_check 150/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_predict` (grown) | 22/0 in the suite. Swimming on e1m4 (deep water at 336 1300 388; stick forward, aside, the jump key's rise, back, the drift down): every correction 0.000 over 216 states, all 216 in deep water, drawn offset 0.000. The walk, jumps and crouch as B8d (0.000). A fall onto e1m1's lift as it rises (A on its other half starts it; B dropped 60 units over its top) and the ride up: on it, his place on it 0.025 at most over 153 states; the lift's own predicted place moved by up to 2.2 units a state (`lift_max`: which server frame a move runs in, eased in under him with him); in the suite's run the whole drop and ride 0.031 at most (in the air 0.000); in 1 of 2 runs alone one correction of 12.7 units in the fall (open, below) |

What changed (nothing of single player's or the listen host's own client's path: `.vr_moveswim` and `.vr_movejump` 0 there):

- **Swimming in the moves** (`vr_pmove.cpp`): each move finds its own water level and type (SV_CheckWater's three points over his origin, the rules' feet/waist/eye heights; the client from his map's hull 0) and runs SV_ClientThink's branch by it: SV_WaterJump (carried over the edge by `.movedir`, gravity off, ended out of the water or at `.teleport_time`), SV_WaterMove (the head's full angles, `upmove`, the drift down, VR_WaterStickScale's swim and wade scales, VR_AfterWaterMove's glide and cap), or the walk (the shallow scale). Then QC WaterMove's drag (`.vr_moveswim`: QuakeC leaves it to the moves) and PlayerJump's swim rise (100/80/50 by the type; QuakeC plays its sound), gravity by the move's level. The hands' strokes stay the server's, once a frame after the moves (`physics::swimStrokes`): his replay corrects to them, eased in. QC's CheckWaterJump stays once a frame (its start is a correction; the carry after it is predicted). `.teleport_time` (moving back held, a water jump's end) is the moves' too (`holdLeft`).
- **The movers' motion in the replay** (`vr_predict.cpp`): the state carries the movers near him in motion (1024 units, 6 at most: velocity and the time to their stop, SUB_CalcMove's think); his replay meets each where it will be at the move's time (clamped at its stop) and the one he stands on carries him after each move (SV_PushMove's rider, the movers running after the players in a server frame). The lift he rides is drawn where his replay has it. B8d's carry between states is gone (the replay's own now); a correction on a mover is his place on it, the mover's own predicted place moving (`lift_max`) eases in under him and the lift with him.
- `QVR_NET_VERSION` 22; `vr_mp_predict_report`: `swim_n`, `swim_max`, `wj_n`, `land_moving`, `lift_max`.

Open (B8f):

- The drop onto the rising lift (1 run of 3): one 12.7-unit correction early in his fall, his earlier replay landing him lower on the lift than the newer one (to find: the movers' list or the time to their stop at that state). Put over the lift before it moves, his fall starts it (QC's trigger): that correction no replay can foresee (5.6 units).
- A water jump out (CheckWaterJump's start and the carry) not tested yet: the harness needs a pool edge.
- The lying box's crawl, climbing and the grapple: drawn as they come (not predicted).
- The `mp_melee` flake: not run this session.

### B8f results, part 1: the drop onto a rising lift, Quake's water jump out (2026-11-15, dev no-LTO build, loopback; `mp_test.py suite` 25 of 27 pass: `mp_download` 4/1 (B's refusal not seen in 60 s, busy machine) then 8/0 alone, `mp_predict` 24/1 (its landing count, below) then 25/0 twice alone; scenario_check 152/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_predict` (grown) | 25/0 alone, twice. The fall onto e1m1's rising lift and the ride: corrections 0.013-0.030 over 177-178 states (in the air 0.000), drawn offset at most 0.015 (failed first: `--part lift` 4 of 4, 18.7-26 units early in the fall: below). Quake's jump out of the water on the start map's pool (B at its surface 16 units from a ledge, the jump key held and the stick forward, the server on `vr_water_jump 1`): lifted out onto the edge, 6-7 states in the water jump, every correction 0.000 over 143 states, drawn offset 0.000 (failed first: one 59.5-unit correction, the jump out's start). The walk, jumps, crouch and swim as B8e (0.000) |

What changed (nothing of single player's or the listen host's own client's path: `.vr_movewaterjump` is 0 there, as
`.vr_moveswim`):

- **A mover pushes him in the replay** (`vr_predict.cpp` `moversAfter`): the replay carried only the lift he stood on.
  Falling onto a rising lift, his feet came to rest within its 2-unit step over its top at the end of a move; the lift's
  next place then held his box inside it: stuck (allsolid: no floor met, no move) until the next state (the 12.7 and
  18-26 unit corrections, as often as the fall's timing put his feet there). The server's SV_PushMove pushes a box left
  inside a pusher's new place by its move; the replay now does too, after each move (the movers run after the players
  in a server frame). `pushed` counts the replayed moves it did.
- **Quake's jump out of the water in the moves** (`vr_pmove.cpp` `waterJumpTry`): QC CheckWaterJump's WaterJumpTry
  (solid at the waist within 24 units ahead, open at `.maxs_z` less 8) along the move's facing (its aim's yaw, the pitch
  a third, as SV_ClientThink sets `.angles`), waist deep, in each move after the water's drag (PlayerPreThink's order):
  FL_WATERJUMP, 225 up, `.movedir` the wall's normal times -50, `.teleport_time` 2 seconds on (the server's hook writes
  them). PlayerPreThink sets `.vr_movewaterjump` for a remote client walked move by move on Quake's jump out
  (`VR_WaterJumpByHand()` false: a flat-screen player, `vr_water_jump 1` or climbing off) and CheckWaterJump leaves it to
  the moves; the line traces are a Body hook (`line`: the server's MOVE_NOMONSTERS, the client's map hull 0 and the
  brush models where they will be). The rules carry `.maxs_z`.
- `QVR_NET_VERSION` 23. `vr_mp_predict_report`: `pushed`; `land_moving` now counts the replays in which a move lands him
  on a lift or door still moving (it counted landing states whose newest replay stood on a lift still sent moving:
  a fall that met the lift at its top's last moment, pushed onto it as it stopped, counted none -- the suite's 24/1).
  `vr_mp_watch 2`: each replayed move's mover (`on`, `ground`, `lift`, `left`), the server's `groundentity`.
- `mp_test.py predict --part lift|waterjump|swim [--repeat N] [--watch 2]`: one segment alone, traced (the cause above
  found so: `--part lift` reproduced it 4 of 4).

Open (B8g):

- The lying box's crawl, climbing and the grapple: drawn as they come (not predicted).
- The hands' climb out of the water (VR's default, `vr_water_jump 0`: a gripping hand pulled down, QC
  VR_WaterClimb_HandPull): the server's, once a frame: its start is a correction (eased in; a large one snaps).
- The `mp_melee` flake: 29/0 in this suite.

### B8g results, part 1: the lying box's crawl (2026-11-15, dev no-LTO build, loopback; `mp_test.py suite` 26 of 27 pass: `mp_pickup` 8/1 (A's grip on the dropped shotgun, busy machine) then 9/0 alone; `mp_predict` 28/0, `mp_melee` 29/0 (the "crowbar held level" flake not seen); scenario_check 155/0, load_models_test 4/0, vr_run_check 12/0, crawl_test 18/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_predict` (grown) | Crawling lying down on e1m1's open floor (B's head at 0.4 m: the lying box, 16 tall; the stick forward, aside, back, let go): every correction 0.000 over 356-360 states in 5 takes alone (`--part crawl`), all walked move by move, his arms' crawl on (the stick's share 24 u/s, the glide after it), drawn offset 0.000; at rest he lies where the server has him. Before, a lying frame was never walked move by move (drawn as it came). Two takes' 0.12-0.20 a state were B crawling back into A (other players are not in his replay, below): the test crawls aside away from him |

What changed (nothing of single player's or the listen host's own client's path: `VR_AfterCrawlMove` keeps its math,
moved into shared functions):

- **Lying frames walked move by move** (`vr_predict.cpp` VR_PredictThink): the lying box was already the rules' box
  (`hull::crouchedHeight`, its waist for the water's level `VR_HullWaistZ`, the crawl's top speed in `VR_CrouchSpeedMax`).
- **The stick's share of the arms' crawl in each move** (`vr_pmove.cpp` `crawlMove`): VR_AfterCrawlMove's stick want
  (`pmove::crawlStickWant`: where it points along the floor at the stick's share of the crawl's top) and the crawl's
  speed step (`pmove::crawlStep`: reached at `crawlGrip`, never braking, the glide out between pulls, the cap, the
  player's velocity along it kept up on the ground) after the air move; single player's VR_AfterCrawlMove now calls the
  same two functions. The crawl's own speed is the state's (`Body::crawl`, the server's `Crawler::vel`), started afresh
  as VR_AfterCrawlMove's (`physics::crawlState`). The rules carry whether he crawls with his arms (lying down, tracked
  hands, not held under a low ceiling), its top, glide and cap.
- **The hands' pulls stay the server's**, once a frame after the moves (`physics::crawlHands`: the hands' want and the
  newest move's stick, no glide), as the swim's strokes: his replay corrects to them, eased in.
- `QVR_NET_VERSION` 24. `vr_mp_predict_report`: `lying`, `arms`, `crawl_n`. `vr_mp_watch 3` (`mp_test.py predict --part
  X --watch 3`): every correction over 0.05 traced (1 before).

Open (B8h):

- Climbing (vr_climb.cpp, 3,700 lines, the server's): a hanging or mantling frame is not walked (drawn as it comes).
  Next: the mantle first (a timed path from its start: `mantleFrom`/`Mid`/`To`, its start and shift, the state's; the
  replay places him on it at each move's time), then the hang's pull (hangPull's hand motion relative to the body,
  weighted, from his own moves' hands; hangMove's limits; the grips, `owed` and reach the state's).
- The grapple's pull (vr_grapple.qc, 3,200 lines of QuakeC: the reel along the rope, the swing): the server's velocity
  once a frame, his replay corrects to it. To predict it the rope (the hook's point, its length, the reel's speed) goes
  in the state and the reel's rule into the moves.
- The hands' climb out of the water (QC VR_WaterClimb_HandPull): the server's, its start a correction.
- Other players are not in his replay (BSP models only): walking or crawling into one, the server stops him and his
  replay does not (0.1-0.2 units a state at a crawl).
- Getting up from lying at once (the mock's head jumps from 0.4 m to standing) is a room-scale leap (QuakeC's): one
  correction (43 units), as a crouch's.

### B9 review: one code path for single player and multiplayer (2026-11-15)

His note (2026-11-15): instead of "if single player do this, if multiplayer do that", every player (and entity) down
the same code paths. Method: `git diff origin/next...agent/mp` (134 files, +18.3k lines), every added condition on
`svs.maxclients`/`cl.maxclients`, `sv.active`/`cls.state`, `deathmatch`/`coop`, `cl.viewentity`, "local"/"remote"
players (a scan of the added lines: 117 conditions outside the test module `vr_mptest.cpp`, bounds checks included),
grouped by system. Verdicts:
**keep** (network-only: transport, acks, deltas, latency, game rules Quake itself has), **unify** (the same behaviour
should come from one system for every player), **done** (unified in B9).

| # | Where | What branches | Verdict |
|---|---|---|---|
| 1 | `held::fistOf`, `grip::handFrameOf`, `gripfit::fit` (vr_held/vr_grip/vr_gripfit.cpp; readers: Box3D hands, ragdoll parts, foe grabs, physics touches, water pulls, carry, grips) | player 1 of a non-dedicated server: the client's live globals; else `svs.maxclients > 1`: his slot's (sent) | **done**: one per-slot store; the host's own client puts its hands into its slot as it measures them (`clientprefs::ownSlot`, `publishOwn`; at a slot reset too), a remote one's come in his messages; the loopback copies of the host's own no longer sent |
| 2 | `clientprefs::value`/`forQc`, `PF_cvar_hget` | `svs.maxclients > 1` before a slot's preferences | **done**: the check dropped (a slot without preferences gives the game's own value: single player always) |
| 3 | vr_heldpose.cpp (`isLocal`, `isRemote`, `grabSpot`, `reverseGrip`, `magPoll`, `remote`), vr_builtins_held.inc (weapon hotspot, any-grip, help kind: 5 sites), vr_carry2h.cpp:333, vr_box3d_hands.cpp:661, `grip::serverTake`'s `h.local` | the host's own player: the client's view asked live (hotspots, ground spot, reverse grip, magazine presses); others: their slot's drawn pose, spot, presses | **unify** (B11a, medium, 1 session): the host publishes its `heldpose::Hand`, ground spot and presses into its slot as #1; readers read the slot only. Three different "local" tests today (`sv.active && cls.state == ca_connected && num == 1`, `cls.state != ca_dedicated && slot == 0`, `num == 1 && cls.state != ca_dedicated`) become `clientprefs::ownSlot`. Care: the remote path turns hotspots through the player's hand fields (`toWorld`), the local one through the view: single player's numbers change unless published unquantized in the same frame; check with the SP hash audits. **Done** (B11a: `heldpose::publishOwn` before each server frame, the view's world places carried in the slot: "B11a results") |
| 4 | sv_user.c:437 `VR_PredictThink`, sv_phys.c:1377/1414 `VR_PredictWalks` (vr_predict.cpp) | a remote client's walk: his moves one by one through vr_pmove.cpp; the host's own player (and a flat or old client): SV_Physics + QC PlayerPreThink | **unify** (B11b, large, 2 sessions; highest value): vr_pmove for every player, the host's own included (behind a cvar first, SP traces compared frame by frame), so prediction, server and single player are one module; then monsters' step and swim through the same body code. **Done** (B11b parts 1-2: `vr_pmove_own`, default 1 since B11c part 2 (his call, "For him (B11b)"; 0 the fallback); with 1, single player's traced walk, jump, crouch, lift, swim, water jump and crawl are frame-identical; monsters kept: not clean, "B11b results, part 2") |
| 5 | vr_mpavatar.cpp (remote bodies, `bonePoses`), vr_view_diag.cpp `remoteRigSkin`/`remoteHandSkin`, vr_avatar.cpp:1430 `otherDrawn` (`cl.maxclients <= 1`), :2392 | another player's body, hands, holsters, plates drawn from his avatar block; the own from the live rig (vr_view_body, vr_view_hands) | **unify** (B11c, large, 2 sessions): one routine draws any player entity from an avatar state (the own filled from the live rig, others from snapshots), the flat player's own body (third person, vr_flattp) from it too. **Part 1 done** (B11c: the body, its plates and pauldrons through `view::drawBody` for yours and every copy; the hands, held and holstered weapons left: part 2). **Done** (B11c part 2: `view::drawHand`, `drawGear`, `drawHands`; the plates' morphs per player: "B11c results, part 2") |
| 6 | vr_teleport.cpp:72, vr_trace.cpp:159, vr_hull.cpp:5083 | no server edict for the own player (a remote client): the client's state (`cl.onground`, `cl.worldmodel`, the prediction's hulls) | **unify** (small): one "own player" accessor (on ground, in water, the world to trace) answering from the edict when hosting, from the prediction's state otherwise. **Done** (B11c: `progs::ownSource`, `ownStanding`) |
| 7 | vr_predict.cpp:750/1155, vr_netsnap.cpp:736/774 | only `cl.viewentity` predicted; others interpolated, the own not delayed | keep (network: latency hiding is the owner's alone) |
| 8 | vr_lagcomp.cpp:245/291/296, pr_cmds.c:754 | rewind only with remote clients (`anyRemote`), per shooter | keep (network: the host's own latency is 0, the rewind an identity) |
| 9 | vr_netmove.cpp:441, sv_user.c:683, net_dgrm.c:978, vr_client.cpp:553 (hands/body in the move only with `cl.maxclients > 1`), vr_server.cpp:565/1563/1601, vr_mpavatar.cpp:672/772, vr_mpdownload.cpp, every `cmd_source != src_client \|\| !sv.active \|\| !host_client` guard (clientprefs, heldpose, mpavatar, lagcomp) | the QVR protocol, moves, version, bandwidth, string commands' guards | keep (transport) |
| 10 | vr_serverrules.cpp (`remote`, `shadowed`, `svs.maxclients <= 1`) | a remote client's own settings shadowed by the server's rules | keep (network: whose settings govern) |
| 11 | host.c:786/1225/1296, host_cmd.c (save/load, `coopclients`), menu.c:2212, vr_menuui.cpp:805, vr_coopsave.cpp, vr_progs.cpp:932 | coop saves and autosaves, Quake's "no deathmatch saves", a dropped coop slot pauses | keep (game rules; versioned format) |
| 12 | QC combat.qc:3412 (player vs player damage in coop/deathmatch), weapons.qc:4677/4736 (test impulses off in deathmatch/coop) | game rules, cheat gating | keep |
| 13 | QC client.qc:1713 | (the comment names a remote client; the condition is every player's) | already one path |
| 14 | vr_backend_mock.cpp:563, vr_mptest.cpp, the `vr_mp_*` report commands (`mp_drawn_own`/`mp_drawn_sv`, `mp_prefs_own`/`mp_prefs_sv`) | tests and reports | keep |

### B9 results (2026-11-15, dev no-LTO build, loopback)

- **Done**: #1 and #2. The server-side readers of a player's client-owned hand state and preferences no longer ask
  "is this the host's own player?" or "is this multiplayer?": 9 player-kind branches in readers (3 `num == 1 &&
  cls.state != ca_dedicated`, 3 `svs.maxclients > 1` before the slot stores, 3 in the preference reads) become 0. What
  remains is one transport decision, `clientprefs::ownSlot()` (the host's own hands go into its slot directly, a
  remote player's by message), used by the three setters, a slot's reset and `send` (the host no longer sends its own
  rig and frames to itself over the loopback). The scan: 117 condition lines before, 114 after (bounds checks
  included). Single player: the same values (the slot holds exactly what the setters were given, refreshed at the
  slot's reset), same functions.
- Tests: after #1 `mp_test.py suite` 27 of 27 (merge of origin/next included); after #2 `prefs`, `grab --listen`,
  `held --listen`, `rules`, scenario_check and load_models_test (numbers in ROUND25's "mpN B9").
- **Left to unify** (the table): #3 heldpose's own-player view asks (B11a, medium, 1 session), #4 vr_pmove for the
  host's own player and then monsters (B11b, large, 2 sessions; the biggest win: prediction, server and single player
  one movement module), #5 one avatar routine for any player entity (B11c, large, 2 sessions), #6 an own-player
  accessor for the client-side fallbacks (small). Fast retransmit and compression stay later (the old B9 row: B12).

### B10 results: his player-to-player features (2026-11-15, dev no-LTO build, loopback; `mp_disarm` 17/0 dedicated and 17/0 `--listen`)

(The voice chat row above keeps its own id for later; this B10 is his overnight list, OVERNIGHT_1115.md.)
- **Grab-disarm** (QC `vr_playerdisarm_grab.qc`): an empty hand whose grip closes on another player's held weapon (within
  `vr_player_disarm_reach` 8 cm of its melee line butt..tip or a hotspot of it, as the server places them: lag
  compensated for a remote taker) no faster than the armed humanoids' `vr_agrunt_grab_speed` holds it; pulled away from
  his chest at `vr_player_disarm_pull` 1.5 m/s or more (the hands' relative speed) it is the taker's, in that hand; a
  gentler pull slips off at `vr_player_disarm_slip` 15 cm. No tug, no ragdoll. `vr_player_disarm_grab` 1 (Armed Grunts page
  rows, beside Grab: Fastest Hand; server rules by page). `VR_Player_Disarm(v, hand, taker, takerHand, why)` is the
  entity-generic "out of a player's hand" (the drop's own path, then the catch of VR_AG_Yank): the "player disarm by
  shots/melee" work expected on next should call it (it had not landed at this merge).
- **Giving weapons**: a toss (an underhand swing let go at its top) lands by the other and his grip takes it (one code
  path with drops: `mp_pickup`). **Ammo**: a shell taken from A's pouch and let fall is taken up by B's hand (the
  carry). **Open** (B10b: the test's, not the game's): let go at B's own pouch it does not go back into B's reserve (metric `ammo_into_b_reserve` 0):
  `VR_Reload_LetGo`'s AtPouch reads B's hand hotspot field, which his client seems not to send as the pouch while the
  hand carries a round (A's empty hand at his pouch does take a shell); magazines and rockets untested for the same reason.
- **Holsters**: B's holsters as A draws them equal B's own (slot, model, within 1 unit) at the start, with a shotgun
  given in every slot (slots 2-5 drawn by default), one drawn out by hand, after his death and respawn.

### B10b results: one way out of a player's hand; rounds handed over (2026-11-15, dev no-LTO build, loopback; `mp_disarm` 27/0 dedicated and 27/0 `--listen`, vr_pd_test 10/0; `mp_test.py suite` 26 of 28 pass: `mp_ground` 14/1 (the sword retake "None") and `mp_pickup` 8/1 (A's grip on the dropped shotgun), each then all pass alone; scenario_check 138/0, load_models_test 4/0, vr_run_check 15/0, Release (ThinLTO) build 0 warnings)

- **Merged next's player disarm** (shots, projectiles, blows: `vr_playerdisarm.qc`) with B10's grab into one path:
  `VR_Player_Disarm(v, hand, taker, takerHand, dir, speed, why)` (vr_playerdisarm.qc) takes the weapon out of any
  player's hand, knocked out (taker world: the grip-lost drop, pushed along `dir`; next's `VR_PD_Disarm` adds its
  sparks, ricochet and flash, then calls it) or taken into another player's hand (the grab, now
  `vr_playerdisarm_grab.qc`, `VR_PDG_*`). The grab's cvars renamed beside next's: `vr_player_disarm_grab` 1 (and
  `vr_player_disarm`, the master switch: off, no way disarms a player), `vr_player_disarm_pull` 1.5,
  `vr_player_disarm_reach` 8, `vr_player_disarm_slip` 15; their rows moved under Armed Grunts > "Disarming: Players'
  Weapons" with next's two. The victim gets the buzz and a line ("B takes your weapon!") either way.
- **B10's open item was the test's**: `vr_mock_hand_to <hand> nearest <classname>` asks the server's edicts, which a
  remote client has not: B's hand never went to the dropped shell, stayed at his pouch and took a pair of his own, so
  the "takes up" check passed on the wrong round and the "return" looked broken. New `nearestmodel <part> [<units>]`
  (the client's drawn entities; a remote client's too). The pouch path itself (VR_Reload_LetGo, VR_Reload_Refund) was
  right: the same code for every player.
- **Rounds handed over** (`mp_disarm`'s ammo part): a shell, a nail magazine (24 nails) and a rocket from A's pouch, let
  fall, taken up by B from the floor (his reserve unchanged at the take) and let go of at his own pouch: his reserve up
  by exactly what A's went down; then a shell B to A (the listen host's own player: the single player's case).
  Not covered yet: "used by B" (a dropped magazine or rocket loaded into his own gun; his gun needs room first, a
  magazine out or a rocket fired): next phase.

### For him (B10)

- Grab-disarm feel: is 1.5 m/s the right pull (a deliberate yank, not a reach)? Should the victim feel it (a buzz) or
  hold on harder with both hands on it (today: two hands on it change nothing)?
- Small interactions not built (each needs his call): a high-five or fist bump (two hands meeting at speed: a sound and
  a buzz), handing an item hand to hand (both holding it, one lets go), a helping hand up a ledge (the climb's hold on
  another's hand), reviving a downed coop player.

### B8h results, part 1: rounds used by B; the mantle predicted (2026-11-15, dev no-LTO build, loopback; `mp_test.py suite` 28 of 29 pass: `mp_ground` 14/1 (the sword retake "None": it landed out of B's reach, his hand stepped 0.6 m to 2.6 m down after it; the known B10b flake, on the engine before B8h too; then 14/1 and 13/2 alone, the pre-B8h engine 15/0 once); `mp_predictclimb` 8/0, `mp_disarm` 35/0, `mp_predict` 28/0; scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_disarm` (grown: `--part use`) | A's dropped nail magazine and rocket taken up by B from the floor and loaded into his own gun at its loading port (his off hand to `vr_mock_hand_to off lport`, as his client draws the port): "a magazine of 24 seated (hand 1)", "1 into the gun (hand 1)", his gun's magazine 0 -> 24 and 0 -> 1, his reserve unchanged. It worked at once: no game fix (the shared path, VR_Reload_LoadInto, is every player's). 13/0 `--part use`; the whole scenario 35/0 `--listen` |
| `mp_predictclimb` (new; `--part mantle`) | On vrclimb, B (150 ms each way) takes the ledge with both hands, pulls down and pushes over the top (mp_climb's overtop): 19 states mantling, each walked move by move; every correction 0.000 over 285 states (20 of them in the mantle), drawn offset 0.000; on the top at rest he is where the server has him (118 179.6 72) |

What changed:

- **The mantle runs in his moves** (`pmove::Mantle`, `pmove::mantleStep`, vr_pmove.cpp): its way (from, up to mid, over
  to, at an even speed along the two) and the time it has run. Single player's mantle frame (vr_climb.cpp
  `mantleStep`) steps the same function by `host_frametime` (the time run is now `Climber::mantleAt`, not a start
  time: the same numbers); a remote client whose moves run one by one has `VR_ClientClimb` leave the mantling frame to
  the walk (`VR_PredictWalks`), and VR_PredictPhysics runs it in each move (`climb::mantlePath` before, the brush
  model's or prop's motion added as before; `climb::mantleMoved` after: its time kept, at its end he stands and the
  climb lets go, as before). The move that ends it ends there; the moves after it walk.
- **The state carries it** (bit `bitMantle`, the way's three points, its time run and its length) and his replay
  carries him along it at each move's time: the server's clock is his moves', so the two agree to the bit.
- No moves, no mantle: a client whose moves stop (a stall) freezes on the way until they come, as his walk does; one
  that stops sending per-move moves falls back to the frame's mantle (its time run carries over).
- `vr_mp_status`'s `mp_client inv=` gains a ninth value: the main hand's gun's magazine (its weapon record's `wi_clip`).
  `vr_mp_predict_report`: `mantle_n`. `QVR_NET_VERSION` 25.

Open (B8h, the rest):

- **The hang** (a hanging frame is the server's, drawn as it comes; the mantle's start is a correction, the drawn body
  snapping from the server's place to the predicted one). Next: the hang in the moves, each move's hands (the moves
  carry them: `WireMove::vr`) for hangPull's motion relative to the body, hangMove's limits; the state the grips (the
  holds, each hand's last relative place, light or not, the top), `owed` and the phase (slipping). keepWithinReach,
  settleOwed, reachCheck, tryMantle and the stamina the server's once a frame after the moves (corrections, eased in).
- The grapple's pull (the rope in the state, the reel in the moves) and the hands' climb out of the water (vr_water_jump
  0): not started.
- Other players are not in his replay (B8g's open item), getting up from lying at once (B8g's): unchanged.

### B8i results, part 1: the hang predicted; mp_ground's sword (2026-11-15, dev no-LTO build, loopback; mp_test.py suite 28 of 29 pass: mp_disarm 6/1 (its gentle pull's setup: A's shotgun not yet in his hand at the server), then 35/0 twice alone; mp_ground 15/0 in the suite and 5 of 5 alone; scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0, prop_climb_test 12/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_ground` (fixed: the test's) | The sword retake's failures (3 of 4 runs today) were the test's: B's hand put in place by `vr_mock_hand` (the mock reports that jump as 21 m/s for 3 frames) and let go about 140 ms later, inside the throw estimate's window: every drop was thrown down at about 10 m/s (`thrown: velocity ... -383`), bounced and slid 14-28 units, out of his reach. Not flipgrip's, not a game bug: single player's drop of a still hand is 0 m/s. The drops now let go once the hand has been still THROW_SETTLE_S (0.5 s of the server's time; `let_go_still`, as `mp_pickup` already did), and the shotgun's ground spot is waited for at its grip hotspot (dropped straight, it lies under the hand, whose view first found another hotspot). 5 of 5 runs 15/0, `mp_grab` 13/0 twice |
| `mp_predictclimb --part hang` (new) | On vrclimb B (150 ms each way) takes the ledge with both hands, pulls down 0.25 m (his body rises 10 units), lets up 0.15 m, lets go and falls: 249 states hanging walked move by move, every correction 0.000 over 358 states (the let-go and the fall included: the moves' grips end the hold in his replay), drawn offset 0.000; at rest on the floor where the server has him. 12/0 in 3 runs (with the mantle part) |

What changed:

- **The hang's step is one function** (`pmove::hangStep`, `pmove::moveBody`, vr_pmove.cpp: vr_climb.cpp's hangPull and hangMove,
  moveBody and slideBody moved there over `pmove::Body`): single player's hanging frame runs one step of the frame's time on
  its edict (`ClimbBody`: the same traces, the same numbers; prop_climb_test 12/0), a remote client's moves one step each on
  the server (VR_PredictPhysics through `playerMove`: `Body::hang`) and in his replay. `pmove::Input` carries each move's hands
  relative to the body (the move's hand places less its origin: the integers sent) and its grips.
- **The frame's part stays the server's** (`climb::hangMoved`, single player's `hangFramePart`): kept within reach, the owed
  settled, the reach checked, the mantle's test, the stamina, from the steps' sums (`pmove::HangSums`).
- **The state carries the hang** (bit `bitHang`: each hold's place, its hand's last place relative to the body, light, its top;
  the owed, slipping, the box's bottom). Only holds on the world (`holdsStill`): a hold on a brush model or prop moves with
  it, that frame stays the server's (drawn as it comes, as before). A grip let go in a move ends its hold there in the replay
  (the server's climb lets go at that move's frame).
- **What the replay cannot know is counted apart** (`trans_n`, `trans_max` in `vr_mp_predict_report`): a hold taken and the
  hang's mantle begun are the server's searches (ledge maps), once a frame: the replay learns of them a round trip late.
  Their corrections ease in as any other, their part of the drawn offset decays apart (`transError`) and they stay out of
  `corr_max`. The mantle's start is about 43 units at 150 ms each way (it was the same jump before, from an unpredicted hang).
- `QVR_NET_VERSION` 26.

Open (B8i, the rest):

- The mantle's start and a hold's take predicted (the client's ledge search and mantle test in his replay: the map's ledge
  maps are his too), so that `trans_max` falls under a unit.
- The grapple's pull (the rope in the state, the reel in the moves) and the hands' climb out of the water (vr_water_jump 0):
  not started.
- A hang from a brush model or prop (the holds' motion in the replay), other players in his replay (B8g's), getting up from
  lying at once (B8g's): unchanged.

### B8j results, part 1: the mantle's start predicted (2026-11-15, dev no-LTO build, loopback; `mp_test.py suite` 27 of 29 pass: `mp_predict` 27/1 (the drop onto the lift's landing, 1.354: a walk, no climb code; then 28/0 alone) and `mp_disarm` 34/1 (A's pouch taking the shell; then the toss 34/1 and 35/0 alone: flakes of its hand placement, no climb code); `mp_predictclimb` 14/0; scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_predictclimb` (grown: `trans_check`) | On vrclimb, B (150 ms each way): the hold's take (both hands, standing) and the hang's mantle start, the states the replay used to learn of a round trip late (`trans_n`, `trans_max`), now checked under a unit: the mantle's start 43.3 units before, 0.093 now (2 such states); the hold's take 0.000; every other correction 0.000 (355 states, 21 mantling), on the top at rest where the server has him. 14/0 |
| single player's climbs | mantle_height_test PASS (the same heads over the top: 15.3-15.4 at Mantle Height 8, 20.0-20.1 at the default), prop_climb_test 13/0, slopes_test 6/6 (the sloped mantle and its no-room cases) |

What changed:

- **The mantle's test is one function run after each hang step** (`pmove::hangMantle`, `pmove::findMantle`, vr_pmove.cpp:
  vr_climb.cpp's tryMantle, findMantleLevel and findMantleSloped moved there over `pmove::Body`'s traces, a new
  `Body::traceWorld` for the floor test's MOVE_NOMONSTERS box): single player's hanging frame runs it right after its step
  (before the frame's part, skipped on the frame the mantle begins: B9's one path; the same traces, the same numbers),
  a remote client's moves on the server after each move's step (the moves after it carry him along the mantle;
  `climb::hangMoved` then mantles the climb with the way and the time run), and his replay the same. The flat mantle's
  search calls it too. `pmove::Input::headZ` (the head's height over the body, the integers sent) gives the head's height
  over the top.
- **The search that found no room is kept on the steps' clock** (`HangGrip::missed`, `missAge`, the body, hold and top
  there: `mantleRetry` 0.15 s, `mantleRetryMove` 1 unit, as before less the box keys) and travels in the state: the replay
  skips the searches the server skipped.
- **The state's hang block** gains each grip's ledge way out, its pull down's start, the ledge map's depth at the five
  spots along it (`pmove::mantleSides`: the replay needs no ledge map for the mantle) and its miss (when missed); the
  box's sides and the mantle's rules (time, Mantle Height, slope). `QVR_NET_VERSION` 27.
- Not the same as the server yet: what only the server's traces see (a monster, another player or a prop on the top: the
  replay's world is the BSP's) can make the replay mantle where the server finds no room; the next state corrects it,
  eased in (never a desync: each replay starts from the server's state).

Open (B8j, the rest):

- **A hold taken in the air** (a jump at a ledge, a fall past one): the take is the server's search (gripHold: the ledge
  maps, the hand's leniency, the holsters and busy hands, the stamina), learnt a round trip late; standing, the take
  moves nothing (0.000), in the air it is the fall since. Next: the ledge maps built from the client's world too
  (vr_ledges.cpp reads sv.worldmodel) and gripHold's world part in the moves.
- The grapple's pull (AD's hook, QC: the rope in the state, the reel in the moves) and the hands' climb out of the water
  where no ledge is (QC VR_WaterClimb_HandPull's water jump: a ledge's hold already takes the climb's way, predicted now
  but for the take): not started.
- A hang from a brush model or prop (the holds' motion in the replay), other players in his replay (B8g's), getting up
  from lying at once (B8g's): unchanged.

### B8j results, part 2: a hold taken in the air predicted (2026-11-15, dev no-LTO build, loopback; `mp_test.py suite` all 29 pass; `mp_predictclimb` 19/0; scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_predictclimb --part airgrab` (new) | On vrclimb B (150 ms each way) jumps at the ledge, both hands under its lip in front of the face, the grips pressed 20 ms into the jump (no hold yet: the press keeps looking in the air) and the ledge taken as the hands pass the lip, his body 13.6 units up ("pressed 14 ms before, in the air"); held, let go, falls. His replay took it at the same move (`mp_predict_take: replay`): the take's state 0.000 (`trans_max`; it was the fall since, a round trip's), every correction 0.000 over 354 states (151 in the air, 121 hanging); on the floor at rest where the server has him |
| single player's climbs | climb_jump.py (the air grab's retry, 30 press times) PASS 26/30; prop_climb_test 12/0 twice, slopes_test all pass, mantle_height_test PASS (the same heads: 20.0-20.1 at the default) |

What changed:

- **The take is one function** (`takeTry`, vr_climb.cpp): a hand's press (its edge, the air grab's retry on an age kept
  on the moves' clock: `climb::TakeMemory`), the gates (`climb::TakeRules`: climbing on, the regrab delay left, the
  holsters that win per hotspot standing and hanging, the busy hands, too tired, the leniency radii, Lowest Ledge, the
  hang's box and mantle rules: the server's once a frame) and the search (findHold over a `Seeker`: the server's player,
  with the brush models, props and ladders round the hand, or a replay's body with the world alone and its own lines).
  Single player's frame runs it as one move (`gripHand`, the same numbers: the retry's age is the frames' times summed);
  a remote client's moves run it after each move's step on the server (`climb::moveTake` in VR_PredictPhysics: a hold on
  the world hangs the next moves at once, one that moves stops the frame's moves for his frame's hang) and in his replay
  (`climb::replayTake`: the grip's part of takeHold, `gripTaken`, and hangState's on the world alone).
- **The ledge maps are the client's too** (`ledges::world()`: the server's world model, else the client's: the same BSP
  built by the same code, so the same map and hash).
- **The state carries the take** (bit `bitTake` while climbing is on: the press memory and the rules, 65 bytes; a
  delta-coded record, unchanged when idle: the regrab delay is clamped at 0). Each move's take inputs ride in
  `pmove::Input` (the controllers' own points, the throw velocities, the head, the hotspots, the buttons: the integers
  sent, relative to the body). `QVR_NET_VERSION` 28.
- Not the same as the server yet: what the replay's world lacks (a ledge on a brush model or prop, a monster's or another
  player's body over a hold, AD's ladders and their off switches) and a re-press in the server frame a full let-go came
  in (the server's climb lets go a frame later; the replay applies the regrab delay at the move): eased corrections,
  never a desync.
- A remote client's grip that takes a hold in his moves is hidden from QuakeC from the next frame (VR_ClimbPreThink runs
  before his moves): its first frame's press reaches PlayerPreThink, as a press at no hold does. Single player and the
  listen host's own client unchanged (their take is still before the QC, in VR_ClimbPreThink).

Open (B8j, the rest):

- The grapple's pull (AD's hook, QC: the rope in the state, the reel in the moves) and the hands' climb out of the water
  where no ledge is (QC VR_WaterClimb_HandPull's water jump, vr_water_jump 0): not started (a session each: both are QC's
  per-frame pulls; they need their QC halves moved into the module, as the walk's jump and drag were).
- mp_predict's occasional lift-landing 1.354 correction: not looked at.
- A hang from a brush model or prop, other players in his replay, getting up from lying at once: unchanged.

### B8j results, part 3: the hands' climb out of the water predicted (2026-11-15, dev no-LTO build, loopback; `mp_test.py suite` 28 of 29 pass: `mp_climb` 10/1 (A's mantle: no hold taken by his play, no climb code touched) then 11/0 alone; `mp_predictclimb` 23/0; scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_predictclimb --part waterclimb` (new) | On the start map's pool (vr_water_jump 0, climbing on) B (150 ms each way) swims west to its edge (a bank the ledge map has no lip on: the climb's hold does not take it), raises his main hand 0.4 m ahead over the top, grips and pulls it down at 0.9 m/s: lifted out (the server's `waterjump: climbed out by hand 1, the top 34.0 above the feet, rising at 253`), his replay climbed him out at the same move (`mp_predict_wclimb: replay`, 21 replays), every correction 0.000 over 499 states (228 in the air); on the top at rest where the server has him. 9/0 |
| single player | The same climb through QC's builtin (start map's pool, the mock's hand pulled down): `climbed out by hand 1, the top 33.2 above the feet, rising at 251`, as QuakeC's own did (the same traces and numbers, transcribed) |

What changed:

- **The climb out by hand is one function** (`pmove::waterClimbTry`, vr_pmove.cpp: QC VR_WaterClimb_HandPull moved
  there, over Quake's WaterJumpTry along the hand's way, `waterJumpAlong`): a hand of the frame's empty ones
  (`Rules::climbHands`, QC `VR_WaterClimb_FreeHands`) gripping (`Input::grabs`), ground under it, pulled down at
  `waterClimbPull` (0.3 m/s; `Input::handVel`, the move's hand velocity) lifts him out, high enough to clear the top
  (`Rules::minsZ`). Single player's (and the listen host's own client's) frame calls it once through QC's new builtin
  `waterclimb(hands)` (CheckWaterJump: B9's one path); a remote client's moves run it after each move's drag, waist
  deep, on the server and in his replay. The feel and the print are QuakeC's (`VR_WaterClimb_Climbed`, through the
  Body hook `waterClimbed`).
- PlayerPreThink sets `.vr_movewaterclimb` (bit 0 on, bits 1-2 the empty hands) for a client walked move by move with
  the hands' climb (vr_water_jump 0, climbing on, VR); CheckWaterJump leaves it to his moves. The state carries it (a
  byte after the bits) and `Rules::minsZ`. `QVR_NET_VERSION` 29. `vr_mp_predict_report`: `wclimb` (1 + the hands),
  `wclimb_n` (replays that climbed him out); `vr_mp_watch 1`: `mp_predict_wclimb` (the server's move, his replay's).
- Not the same as the server yet: a hand's busy kind changing within a frame (the empty hands are the frame's), the
  replay's world (a prop or a monster on the top: BSP only): eased corrections, never a desync.

Open (B8j, the rest):

- **The grapple's pull** (vr_grapple.qc, the hook on the world: VR_Grapple_ServiceAnchor's HoldPlayer and Wind, run in
  PlayerPreThink's VR_Grapple_PlayerFrame once a frame): not started. The plan: the rope's first corner from him
  (`gr_pa`, the engine's `ropepivot`), its path beyond (`gr_ba`), the length (`gh_len`), the reel's speed and ramp
  (`gh_speed`, `gh_reel`, the stamina's share), the payout and the shortest length in the state; each move's reel and
  unreel buttons (VR_Grapple_ReelHeld: the hand's secondary, or the other driver's) and HoldPlayer's hold and Wind's
  winding in the moves (one function, the anchor's QC calling it through a builtin for single player, B9); the taut
  twang and the overstretch's loosen are the server's (hooks). A hook on a prop, a monster or a lying gun: the
  server's (their own physics).
- mp_predict's occasional lift-landing 1.354 correction: not looked at.
- A hang from a brush model or prop, other players in his replay, getting up from lying at once: unchanged.

### B8j results, part 4: the grapple's pull predicted (2026-11-15, dev no-LTO build, loopback; `mp_test.py suite` 28 of 29 pass: `mp_predictclimb` 27/1 (the grapple's at-rest check: the swing dropped him on the long ledge's top, 48 up, drawn where the server has him; the check now takes either floor) then 28/0 alone; scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings)

| Scenario | Result |
|---|---|
| `mp_predictclimb --part grapple` (new) | On vrclimb B (150 ms each way) takes a grappling gun in his main hand (the test give, impulse 151), fires it up (the server steers the hook into the ceiling over him: `vr_grapple_test_aim`), reels with B for half a second (the server: `reel off after 0.67 s: rope 488 -> 144`, him 224 units up), hangs on the rope a second, lets go (the quick release, impulse 45) and falls: every correction 0.000 over 248 states (184 in the air), 133 of them with the rope in his moves (`grapple_max` 0.000); the hook's coming off is the server's (1 state, 51 units, eased in apart: `grapple_off_max`); on the floor at rest where the server has him. 10/0 |
| the same, the rope left to QuakeC (`--permove-skip 2`) | 7/3: corrections up to 100.3 units (drawn offset 70.8): the test sees the rope unpredicted |
| `--part waterclimb --permove-skip 1` (B8j part 3's check) | 7/2: with the climb left to QuakeC's frame a remote client does not climb out at all (no climb line; he sinks to the pool's floor): the test fails, on the climb's checks rather than its corrections (Open) |
| single player | The reel through the builtin (vrclimb, the mock's gun up, B held 0.67 s): `reels you: 487 away ... closing 0 (reel 60) ... 373 away (reel 300) ... reel off: rope 488 -> 132`, the ramp and speed as QuakeC's |

What changed:

- **The rope's hold and reel are one function** (`pmove::grappleStep`, vr_pmove.cpp: QC VR_Grapple_Wind and
  VR_Grapple_HoldPlayer for a hook in the world, transcribed: the reel's ramp (GH_RAMP), the stall, the brake as he
  arrives, the hold on the rope's length less its path beyond the corner, swept and slid, the lift off the floor; the
  taut's twang and jolt through the Body hook `grappleTaut`, QuakeC's VR_Grapple_Taut). Single player's (and the listen
  host's own client's) VR_Grapple_ServiceAnchor calls it once a frame through QC's new builtin
  `grapplehold(h, hand, at, beyond, reeling, shortest)` (B9's one path; HoldPlayer removed from QC); a remote client's
  moves run it after each move's jump (where PlayerPreThink's grapple frame sits), on each move's B/Y
  (`pmove::Input::reels`), on the server and in his replay.
- ServiceAnchor leaves the hook to his moves when the engine says so (`.vr_movetakes` bit 1: walked move by move) and no
  other hook took them this frame: `.vr_movegrapple` (the hook), `.vr_movegrapple_at` (the corner), `.vr_movegrapple_rope`
  (the path beyond, the shortest, the reeling hands and the gun's hand). The moves write the hook's `.gh_len` and
  `.gh_taut` back; the reel's ramp (`reelAge`) is kept on the moves' clock while the hook is the same.
- The state carries the rope (the climb byte's bit 3, then 37 bytes: the corner, the path beyond, the length, the reel's
  speed and age, the shortest, the payout, the drivers, the hand, the taut). `QVR_NET_VERSION` 30.
  `vr_mp_predict_report`: `grapple`, `grapple_n` (states with it), `grapple_max` (their longest correction),
  `grapple_off_n`, `grapple_off_max` (the hook's coming off, counted and eased apart as a climb's transition).
- `vr_net_permove_skip` (Debug > Network: Moves Left to QuakeC; 0): hosting, the hands' climb out (1) or the grapple's
  rope (2) left to QuakeC once a frame, as before their phases: `mp_test.py predictclimb --permove-skip N` checks a part
  fails without its prediction. The water climb's QC gate reads `.vr_movetakes` bit 0.
- Not the same as the server yet: the hook's coming off (the trigger let go after the grace, the quick release, the
  overstretch's loosen: the server's, once a frame), the unreel's payout (its ramp, armed by a press in the air, and
  the rope's lengthening: QuakeC's once a frame, the moves pay out at the frame's speed), the corner moving within a
  frame (the rope wrapped round an edge: the frame's corner), a second hook holding him (QuakeC's, once a frame): eased
  corrections, never a desync. Hanging from a ledge or mantling, his moves leave the rope alone (QuakeC's own frame held
  him there too, before the climb's).

Open (B8j, the rest):

- **The water climb left to QuakeC's frame fails for a remote client** (`--permove-skip 1`: no climb at all). Its moves'
  path (the default) climbs; the frame path is what single player and the listen host use and climbs there. Likely the
  remote client's frame-level grip or hand velocity as QuakeC sees it (B8j part 2 hides a grip the moves take); not
  looked at.
- The grapple's let-go (the trigger's grace, the quick release) predicted: the move's trigger and impulse in the rope's
  state; the unreel's payout in the moves.
- mp_predict's occasional lift-landing 1.354 correction: not looked at.
- A hang from a brush model or prop, other players in his replay, getting up from lying at once: unchanged.

### B11a results: the host's own held-weapon state through its slot (2026-11-15, dev no-LTO build, loopback)

First, the two short looks the plan asked for:

- **The water climb left to QuakeC's frame** (`mp_test.py predictclimb --part waterclimb --permove-skip 1`): it climbs
  now (`waterjump: climbed out by hand 1, the top 34.3 above the feet, rising at 254`; the server has him on the edge's
  top, 128). The test fails as it should with the climb unpredicted (6/3: `wclimb 0`, no climb in his replay,
  corrections up to 57.7): the check sees a missing prediction. Part 4's "no climb at all" did not come back (part 4's
  `.vr_movetakes` gate, or a flake of that run); closed.
- **mp_predict's lift-landing 1.354**: `predict --part lift --watch 3` 9/0 (corrections at most 0.027, `lift_max` 2.06
  eased): not reproduced alone; it stays an occasional flake of the whole scenario (Open, below).

What changed (the B9 review's #3; one path, the readers no longer ask "is this the host's own player?"):

- **`heldpose::publishOwn`** (vr_heldpose.cpp), called by the new `VR_ServerFrameBegin` (host.c, Host_ServerFrame
  before the clients run, every tick): hosting (`clientprefs::ownSlot`), this game's own hands' `Hand` (measure()), its
  ground spots (`twohand::groundSpotNow`, with vr_weapon_grab_hotspots/anywhere as before) and its holster presses
  (`magholster::poll`) go into its slot, as a remote player's `vr_heldpose`/`vr_grabspot`/`vr_magpoll` put his; at a
  reset of its slot too. Nothing while it is not connected. A hosting client no longer sends its own up (send()).
- **`Hand::placed`**: the publisher's view drew them this frame, so the slot carries their world places too (the
  hotspots `spotWorld`, the carried weapon's place, angles and tip, the hand's own turn and vr_revgrip for the reverse
  grip). The readers take those when the slot has them, and place a remote player's (a round trip late) on the hand of
  the move the server runs, as before. That keeps single player bit for bit: the same view values, unrounded, from the
  same frame (the view draws after the server in a frame; QC read the last drawn either way).
- Readers on the slot alone: `weaponhotspot`/`weaponhotspotinfo`, `weapongrabspot`, `weaponanygrip`,
  `weapondrawnpose` 0..5 (the carried weapon), 43 (the reverse grip; no player given: the own slot), 44..46 (the held
  weapon's axes, flip, may reverse, flippable), `magholsterpoll`, carry2h's held-out-of reach, Box3D's drawn weapon hull
  (a placed slot not drawn yet: no body, as the local path; a remote one's not come: the muzzle capsule, as before).
  `heldpose::isLocal`, `isRemote`, `remote` are gone (`slotHand`); grip's unused `Held::local` dropped. measure() now
  fills the hold's kind, axes, flags and held-out-of whatever the drawn pose (a remote client sent zeros without one).
- The three "local" tests the review named (`sv.active && cls.state == ca_connected && num == 1`, `cls.state !=
  ca_dedicated && slot == 0`, `num == 1 && cls.state != ca_dedicated`) are gone from these readers; what is left is
  the one transport decision, `clientprefs::ownSlot()`.
- Still the host's view alone (not in #3, client-side drawing tools for tests and the armed humanoids): weapondrawnpose
  21/22, 30..32, 39, 47 (the flip grip's choice), 49/50 (the leaving turn), 51/52 (the drawn hand).

Tests: `mp_test.py suite` 27 of 29 pass (mp_climb 7/2: A never put at the ledge, the setup; mp_predict 23/1: timed
out waiting for B's prediction put in the water, the setup), then 11/0 and 28/0 alone; `--listen`: grab 13/0, held
14/0, disarm 35/0, ground 15/0 (the host's own hands now through its slot). Single player, the old exe against the new
(same seeds, fixed frames, the filtered console output compared line by line): revgrip (vr_embed_test 42) 24/0,
flipgrip (44) 39/0, embed 24 (19/2 in both: embedfeel's arm-stretch cases), gun_wall_test, crate_grip_test 10/0:
identical (crate: entity numbers aside). magholster_test time/walk: the same steps and numbers, but the bound weapon's
instance and edict numbers differ (old 25/280 in two runs, 24 or 23 in walk; new 23/276 in two runs; the flight home
22.89-22.90 m in both): not explained (the old varies run to run too); watch it in B11b's frame-by-frame comparison.
scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings.

Open (B11a):

- The magholster test's entity numbering difference above.
- `PF_holsterpos` (the host's own holsters from its live rig, another's from `server::holsterPoint`) and the
  weapondrawnpose kinds listed above still ask the host's view: not in the review's #3; B11c (one avatar state for
  any player) is where they meet.

### B11b results, part 1: the own player through the moves, behind a cvar (2026-11-15, dev no-LTO build, loopback)

**A bug found first (fixed, its own commit):** `netmove::clientUses()` asked whether the client's socket was
"LOCAL", but that is the loopback's *server* side; the client's is "localhost" (net_loop.c). So since B7, single player
and a listen host's own client sent QVR_CLC_MOVES over the loopback, the server walked them move by move as a remote
client (vr_pmove), and since B8 they predicted themselves from QVR_SVC_PREDICT. Single player on agent/mp was not
next's path (every "single player identical" check since B7 compared two exes that both had it). Now `clientUses` is false while
this game hosts (`!sv.active`: its client is the loopback's; comparing with "localhost" instead failed mp_climb's
mantles on a dedicated server 3 runs of 3, so a client of 127.0.0.1 may carry that name too) and the loopback sends
clc_move again: single player is sv_phys.c's SV_WalkMove and QuakeC's PlayerPreThink, as on next (the trace's `m=0`).

What changed:

- **`vr_pmove_own`** (default 0, 1 since B11c part 2: his call; Debug > Network, "Movement: Own Walk Through Moves"): this game's own player
  (`clientprefs::ownSlot()`) walks through the remote client's code path (VR_PredictThink, VR_PredictPhysics,
  vr_pmove.cpp): one move a frame, built from his clc_move (`host_client->cmd`, the jump and attack bits) and his VR
  block (`server::clientMove`) as a WireMove, through the same `inputOf`, for the frame's time. Everything keyed on
  `VR_PredictWalks` follows (the jump, QC WaterMove's drag, the water jump and the hands' climb out, the crawl, the
  mantle, the hang, holds taken in a move, the grapple). The state record (QVR_SVC_PREDICT) and the move queue stay a
  remote client's (`remoteMoves`).
- **`vr_pmove_trace`** (debug; "Movement: Trace Walk Each Frame"): `pmtrace: e t o v fl wl ge m` per player at the end
  of SV_Physics_Client; `Misc/quakevr/mp/pmtrace_cmp.py` compares two runs line by line.

The frame-by-frame comparison (single player, `sv_random_seed 1`, fixed frames, vr_pmove_own 0 against 1; the scripts
`Misc/quakevr/mp/pmtrace_all.sh` with pmtrace_sp.sh and pmtrace_cmp.py; the walk and magholster runs in ROUND25):

| Scenario | Frames | Result |
|---|---|---|
| e1m1 walk: forward, jumps, strafe, back, crouched walk | 493 | identical for 380; then the crouched walk 0.0001 apart (one print digit) |
| e1m1 lift (func_plat ride, walk off) | 330 | identical for 274; then velocity 0.001 apart, origins equal |
| e1m1 crawl lying down (stick forward, aside, back) | 380 | identical |
| magholster_test time + walk | 772 | identical, and the magholster lines too (weapon 24 bound, 14.78 m flight home) |
| e1m4 deep water, swimming (stick, jump up) | 345 | one frame apart from entering the water, then the same velocities every frame (a constant 0.565 lower) |
| start's pool: Quake's water jump out | 181 | jumps out 3 frames earlier, then the same walk; lands 0.017 lower once (128.015) |

Explained:

- **Rounding (walk, lift):** sv_user.c and sv_phys.c multiply by `host_frametime`, a double (the acceleration,
  friction, gravity: `sv_accelerate.value*host_frametime*wishspeed` is computed in double); vr_pmove in float with the
  move's float `dt` (as the remote client's replay must). Values that round differently (the crouched speed cap's
  odd numbers) differ in the last bit. Bit for bit would need vr_pmove's time in double (both sides of the prediction
  then use it: still one code).
- **The water's level (swim, water jump):** SV_ClientThink chooses walk or swim by the level the last frame's physics
  found, and QC CheckWaterJump (PlayerPreThink) tries the jump out with the last frame's level (2 only); vr_pmove finds
  each move's level where it starts (B8e: so the replay finds it as the server does). Entering water the moves swim a
  frame sooner; at the pool's edge the jump out is tried from where he is now (level 1 the frame before), 3 frames
  sooner. Neither is a bug of the moves; it is the old path being a frame late.
- **The magholster numbering difference (B11a's open item):** not a code change: without `sv_random_seed` the weapon's
  instance number varies run to run on one exe (24, 26, 27 in four unseeded runs of today's exe; the old 23-25 too).
  Seeded it is 24 in every run, old path and new. Closed.

Not explained: the water jump run's one landing at 128.015 instead of 128.031 (0.017 lower, kept while he walked);
walking across the same spot plainly, both paths stay at 128.031 (identical for 70 frames). Only after the moves'
earlier jump out, the jump key still held.

With `vr_pmove_own 1` built in as the default (then reverted): prop_climb_test 12/12 (heavy, stack, side, light,
close, knock, hang, off, lift, movelight, liftlight, lip: mantles, hangs, holds taken through the moves),
mantle_height_test PASS (the same shallowest pulls, 0.30 m and 0.45 m), crawl_test 18/0.

Tests (the default 0, after the fixes): `mp_test.py suite` 28 of 29 pass (mp_climb 10/1: A's mantle, its known suite
flake; then 11/0 alone, twice); scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO)
build 0 warnings.

Open (B11b):

- The default. Turning it on changes single player slightly (a frame entering water, last-bit rounding): the brief
  says single player stays identical, so it stays 0 until he decides (or vr_pmove's time goes double and the water
  level question is settled).
- The 0.017 landing above.
- Monsters' step and swim through the same body code: not started.

(All three closed in part 2, below.)

### B11b results, part 2: the own player frame-identical (2026-11-15, dev no-LTO build, loopback)

What changed (vr_pmove.cpp/.hpp, vr_predict.cpp):

- **The move's time is a double** (`pmove::Input::dt`). The pieces of sv_user.c and sv_phys.c that multiply by
  `host_frametime` (friction, acceleration, the air and water moves, gravity, SV_WalkMove's step down) compute in double
  and round to float where theirs do; SV_FlyMove's time stays a float as its parameter is; the VR pieces (the crawl, the
  hang, the mantle, the grapple, the swim's glide) and QuakeC's WaterMove drag keep their float frame time
  (`static_cast<float>(host_frametime)`, QuakeC's `frametime`). The own player's move takes `host_frametime` itself; a
  remote client's its float `frameTime` widened, the same on the server and in his replay.
- **The water's level: single player's frame order kept.** SV_ClientThink (walk or swim, the stick's water scales) and
  QC PlayerPreThink (WaterMove's drag, CheckWaterJump, PlayerJump's swim branch) run before SV_Physics_Client's
  SV_CheckWater, so they go by the level the last frame found; gravity and SV_WalkMove by this frame's. Each move now
  does the same: its walk or swim, drag, jump out and swim-jump go by the last move's level, then `checkWater` (after
  SV_CheckVelocity, as SV_Physics_Client) finds its own for gravity and the walk. Decided for single player's: Quake's
  order is what every map and mod was tuned with, the difference is one frame, and the moves' "level where he is now"
  was not clearly more correct, only different. So the moves changed, single player did not.
  The last move's level and type are now part of a move's state: the server keeps them per slot
  (`ServerSlot::waterLevel`, the edict's when the last server frame was not walked by moves: told by the server's
  clock, `waterNext`, not host_framecount, which a dedicated server's frames skip or double: with the frame count,
  mp_predict's water jump out had one 6-unit correction in 2 runs of 2) and QVR_SVC_PREDICT carries them
  (the climb byte's bits 4-5 the level, 6-7 the type: none, water, slime, lava), so a remote client's replay starts its
  first move with the server's. The server's once-a-frame strokes and crawl pulls go by the level the last move's swim
  went by (`Body::thinkLevel`).
- `pmtrace_all.sh walk`: the e1m1 walk scenario (forward, two jumps, strafe, back, a crouched walk at 240, strafe),
  scripted in the file now.

The frame-by-frame comparison (single player, `sv_random_seed 1`, fixed frames, vr_pmove_own 0 against 1,
`bash Misc/quakevr/mp/pmtrace_all.sh`):

| Scenario | Frames | Part 1 | Part 2 |
|---|---|---|---|
| e1m1 walk: forward, jumps, strafe, back, crouched walk | 463 | 0.0001 apart from the crouched walk | identical |
| e1m1 lift (func_plat ride, walk off) | 330 | velocity 0.001 apart after 274 | identical |
| e1m4 deep water, swimming (stick, jump up) | 345 | a frame apart entering the water | identical |
| start's pool: Quake's water jump out | 181 | out 3 frames sooner, a landing 0.017 lower | identical |
| e1m1 crawl lying down | 380 | identical | identical |

**The 0.017 landing:** a consequence of the jump out 3 frames sooner (a different arc onto the bank, so a different
landing slide); with the jump out on single player's frame the whole run is identical, the landing included. Closed.

**Monsters through the same body code: not done (B9's rule, not clean).** A monster's walk is sv_move.c's
SV_movestep (a discrete step tried and stepped down, no velocity, friction or acceleration) and its swim the same with
FL_SWIM; only its fall (SV_Physics_Step: gravity, SV_FlyMove with SV_Impact's touches) resembles the player's. The one
shareable piece would be flyMove behind a Body adapter built for every falling monster every frame: more code on a hot
path for no behaviour gain, with its own touch and contents differences to prove identical. Kept as it is.

Tests (the default 0): `mp_test.py suite` 27 of 29 with the fix (mp_climb 10/1: A's mantle, its known suite flake, then
11/0 alone; mp_predictclimb 26/2: a mantle's states and the grapple's 1.157-unit correction, then 26/2 (the grapple
1.157) and 28/0 alone: dry-land timing flakes, no water involved and the remote moves' arithmetic unchanged (a float
time widened to double rounds the same)); the run before the fix 27 of 29 (mp_ground's known sword retake, mp_predict's
water jump out: fixed, then 28/0 twice); scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0, Release
(ThinLTO) build 0 warnings.

### B11c results, part 1: one body routine; the own-player accessor; B11a's leftovers (2026-11-15, dev no-LTO build, loopback)

- **One routine draws any player's body** (`view::drawBody`, vr_view_body.cpp; `BodyDraw`/`BodyEntities`, vr_view.hpp):
  the model (build, bare forearms or bracers, the fallbacks), the pose (`avatar::pose` for this game's own, whose motion
  state is the avatar module's live one; `avatar::poseOther` for a copy), the placement, the shading, the armour's
  plates and their joints' leather, the pauldrons. Per-entity data in: yours filled from the live rig and your settings
  (`setupBody`), another's from his avatar and calibration (vr_mpavatar.cpp `setupBody`). The own view's specifics are
  data: legs (vr_body_mode 3; a copy always), the drawn view's ride (flat play's bob), the shading (`showPlayerState`:
  your armour skin, wounds, power-ups; a copy plain), the plates' morphs (yours dissolve in and out; a copy has none
  yet: the same fields would give his too). `placeOtherPlates` and the separate `setupPlates`/`setupPauldrons` passes
  are gone; a copy's pauldrons now follow your vr_player_plates as yours do (before: whether his plate entity showed).
- **#6, the own-player accessor** (vr_progs.hpp): `progs::ownSource()` (Edict: this game hosts him; Client: a remote
  client's own, from the prediction and the client's map; None) and `ownStanding()`; the teleport's standing rule, the
  world trace (vr_trace.cpp) and the prediction's brush trace (vr_hull.cpp) ask it instead of their own
  `sv.active`/`cls.state` tests.
- **B11a's leftovers**: QC `holsterpos` reads the player's slot (`heldpose::holsterPoint`: this game's own placed by
  `publishOwn` from its body now, a remote player's from his move); weapondrawnpose 47 (the flip grip's choice), 49/50
  (the leaving turn: now `view::leavingTurn(slot, mirrored, flip, ...)` on the server's hand) and 51/52 (the drawn hand)
  from the slot's `Hand`. The carried grip, the leaving flip and its slot go up with a remote player's drawn pose
  (`vr_heldpose` version 3, 66 values), so his flip grip and his reversed weapon's drop now work as the host's do; 51/52
  (tests only) are the host's placed values, not sent. Kept, by verdict: 21/22, 30-32, 39 (armed humanoids' and
  ragdolls' weapons and arms as this machine's view drew them: monsters, not player state; a dedicated server answers
  '0 0 0' and QuakeC keeps its estimate).
- **Single player identical**: seeded e1m1 with your body, red plates and pauldrons (`vr_mp_avatars`' own body skeleton,
  looks, pauldrons, plates and holsters, from the eyes, a camera off the head and flat third person): every number
  equal before and after; the eyeshot and flat strips 4 and 62 pixels apart by at most 8 levels (run noise). The
  builtins checked in place against the old reads (a temporary comparison, removed): revgrip (vr_embed_test 42) 24/0,
  flipgrip (44) 39/0 (47: 1 read, 49: 2, 51/52: 8, all equal), magholster_test (147 holsterpos reads, all equal).
- MP: mp_body 12/0, mp_cosmetics 20/0 (twice), mp_flatbody 6/0, mp_held 14/0, mp_disarm 35/0. `mp_test.py suite` 29 of 31
  pass: mp_predictclimb 24/4 (B's mantle: 1 mantling state; the grapple's corrections 1.8) then 28/0 alone, mp_disarm
  34/1 twice (the magazine into B's gun, then the shell at his pouch: the known hand-placement flakes) then 35/0 alone.
  scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings.

### B11c results, part 2: the hands, held and holstered weapons of any player; his plates' morphs (2026-11-15, dev no-LTO build, loopback)

- **One set of routines draws any player's hands and weapons** (vr_view.hpp, vr_view_hands.cpp): `view::drawHand` (a
  jointed hand: its fingers posed and skinned, the rig placed, its skin and whether it shows) and `view::drawGear` (a
  weapon in a hand or a holster: model, place, frame, skin, mirrored). Yours call them at the end of the live rig's
  solve (`setupRigHand`, `setupWeapon`, `setupHolsters`), with what is the own view's alone (the grasp and the palm's
  fit, a gun's kick, shake and turns, the reverse grip, the holsters' places on your body, a wound's blood skin) already
  in the place, pose and skin given; every copy calls them through `view::drawHands`, from his snapshot (his
  `VrDrawnHand`s and `VrDrawnHolsters` from his entity as drawn, his fingers shaped by your rig's settings as before).
  Gone: mpavatar's `placeEntity` (a copy of the view's `place`), `view::remoteHandSkin` and `remoteRigSkin`.
- **His plates morph as yours** (`view::morphPlates` on a `view::PlateMorphs` state: yours and one per copy): a piece
  he gains comes in over your `vr_player_plates_morph_time`, another armour's pieces go out where the new one's come in;
  taken as they come when his copy is first drawn (or after 36 frames unseen) and on a new server. A copy now has the
  settled plates' entity and one per morph, their bones each, as yours. The alias renderer reaches a copy's entities
  through `mpavatar::find` (a plate's morph value, VR_AliasMorph; mirrored, VR_AliasMirrored: `mpavatar::mirrored` gone).
- Reports: `vr_mp_avatars` prints `mp_hands:` / `mp_hands_own:` (each hand, held and holstered weapon as drawn: place,
  angles, frame, skin, the rig's bones summed); `mp_plates:` and `mp_plates_own:` add `begun=`, `coming=` (and the
  copy's `morph=`, the newest coming in's shader value).
- **Single player identical** (scratch `b11c2_sp.sh`: seeded e1m1, a sword in the off hand and a shotgun in the main
  (grip mode 1), three holstered weapons, gloves, the body, pauldrons and red plates given mid-run so they come in by
  their morph; from the eyes, a camera off the head and flat third person): every number equal before and after (hands,
  weapons, holsters, bones' sums, plates' entities and morph values, body, looks), the eyeshot strip 0 pixels apart,
  the flat one 215 pixels by at most 8 levels (run noise). revgrip (vr_embed_test 42) 24/0, flipgrip (44) 39/0,
  magholster_test the same draws, distances and times as part 1's, handroll 17/0, flathands 76/0, screenhands 4/0.
- MP: mp_cosmetics 22/0 (grown: A's copy of B's plates comes in by a morph, begun 1, all 1023 pieces coming, its
  shader value 2.001 at the first report; settled after 75 reports with A's morph time at 2 s), mp_body 12/0, mp_held
  14/0, mp_flatbody 6/0, mp_disarm 35/0.
- **The shared movement path on by default** (his call, 2026-11-16: "For him (B11b)"): `vr_pmove_own` 1, so single
  player and a listen host's own player walk through vr_pmove.cpp as a remote client's moves and his prediction do; 0
  stays as the fallback (Debug > Network). The one known change: tracked hands' swim strokes and crawl pulls push one
  frame later (after the move, not in SV_ClientThink before it). With 1 the default: `pmtrace_all.sh` (0 against 1, frame
  by frame) walk 463, lift 330, swim 345, water jump out 181, crawl 380 frames all identical; prop_climb_test 12/12,
  mantle_height_test PASS (the shallowest pull 0.3 m at 8, 0.45 m at the default, as before), crawl_test 18/0.
- Final checks (vr_pmove_own 1, everything above built in): `mp_test.py suite` 29 of 30 pass: mp_climb 7/2 (A never
  put at the ledge for B's mantle: the known suite flake of its setup) then 11/0 alone; scenario_check 138/0,
  load_models_test 4/0, vr_run_check 12/0, Release (ThinLTO) build 0 warnings.
- Open: a copy's plates and hands have no wounds or power-up shading (yours: `showPlayerState`; his would need his
  wounds sent). The copies' ViewEntities are still found by their own lookups (mpavatar::find, bonePoses, lookOf) beside
  yours (view::find): one registry for every player's view entities would fold them, a small follow-up.

### B12 results: the poses in the snapshot, a rate and its priorities (2026-11-15, dev no-LTO build, loopback; `mp_armed` 16/0)

First the measurement (`vr_net_bytes`, new: what a client read, by kind; the harness prints it per scenario beside the
proxy's totals, and `suite` ends with the table). B's link down before this phase (B11b's suite, B/s): coop 19.3 K,
campaign 24.7 K, flow 29.8 K, melee 10.9 K, bandwidth 40.1 K (its cull 0 window included). The kinds showed the armed
humanoids' poses (`QVR_SVC_ARMPOSE`, whole every frame after the broadcast) as the largest by far wherever one was in
sight: `mp_armed` (new, below), six armed grunts in sight, idle: poses 37-42 KB/s of 46-47 KB/s; in combat 37-41 KB/s of
50-56 KB/s, entity updates 9-14 KB/s next. Up stays where B7 put it (2-13 KB/s by scenario).

| Scenario | Result |
|---|---|
| `mp_armed` (new) | Six armed grunts ahead of B in vrtesthall (`vr_physics_spawn monster_vr_grunt`). Idle: B's link down 46.3 KB/s with their poses outside his snapshot (`vr_net_snapposes 0`, as before) -> 6.2 KB/s in it (poses 37.1 -> 0.9 KB/s); in combat 50.5 -> 22.9 KB/s (poses 36.8 -> 9.4 KB/s); budgets 10 and 35 KB/s (section 2.3). Under `vr_net_rate 12000` in combat: his link 12.0 KB/s, a snapshot in every datagram, the nearest two grunts' poses 137 and 109 entries in 5 s against the farthest two's 54 and 44; none undecodable. Failed first: no poses in the snapshot, no rate |
| `vr_netsnap_selftest` | 8/0 (was 6): a pose record new then changed by a byte (6 B); a rate's budget of 40 B: the player state and the highest priority sent, 37 of 43 waited, read back as the server keeps them |

What changed:

- **The poses in the snapshot** (`vr_server.cpp` VR_SnapEnd, `vr_net_snapposes` 1): each pose of an armed humanoid in
  a remote client's sight is a record (`netsnap::poseKey` + its entity; a frame's poses of one entity together),
  delta-coded against the one he acknowledged like the rest; a pose skipped this frame (its level of detail) is gone,
  so the client parses exactly the poses NetQuake's datagram would have carried, before the broadcast instead of after
  it (the poses' parser only stores them). The loopback client and single player get them as before.
- **The broadcast's room first**: the snapshot's budget leaves the frame's broadcast (sounds, temp entities) room, at
  most half the datagram, so events still go before poses as when the poses came last.
- **A rate** (`vr_net_rate`, B/s, 0 none; a per-player preference up with the others, `QVR_NET_VERSION` 31; Debug >
  Reports > Network: Rate Cap): the server fills a bucket at his rate (at most 0.1 s of it, a datagram at least) and
  charges each datagram whole; his snapshot's body gets what the bucket holds after what is written and the broadcast.
- **The priority accumulator** (`netsnap::encode`): when the changed records do not all fit (his rate, or a full
  datagram), his own state (player state, walk) and gone entries go first, then the highest priorities that fit; a
  record's priority is its weight (256 / (256 + distance), twice ahead of his view, twice another player's avatar) times
  1 + the frames it has waited; one that waits stays as the base's on both sides. Without a cap and with room, nothing
  changes.
- `vr_net_bytes [reset|poses]` (client): bytes read, reliable and unreliable, the unreliable by kind (the snapshot's
  entries by class and its header; poses, avatars, entities outside it; sounds, temp entities; the three biggest of the
  rest), `poses`: each armed humanoid's pose entries and mean distance. `mp_netsnap:` adds the rate, the records that
  waited and the snapshots with any.

Final checks: `mp_test.py suite` 27 of 30 pass: `mp_climb` 10/1 (A's mantle setup: no hold taken, its known flake; 11/0 alone), `mp_disarm` 33/2 (the rocket into B's port, "None": its hand placement; 35/0 alone), `mp_predictclimb` 2/2: B crashed joining (below; 28/0 twice alone, once with the fix; `mp_climb` 11/0 with it too); scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0 (run B `3 1 0 1 0`), vr_netsnap_selftest 8/0, vr_netmove_selftest 5/0, the dev build and the Release (ThinLTO) build 0 warnings.

Found and fixed (multiplayer only, its own commit): `CL_KeepaliveMessage` sent its nop during a long map load without `NET_CanSendMessage`; with B2's chunked reliable window a send while a chunk was in flight restarted `sendChunkBase` past the unacked chunks and the 1 s resend's offsets wrapped (a Q_memcpy crash of B, 150 ms behind, loading vrclimb: 2 runs of 3). The keepalive now waits while a reliable message is in flight (that keeps the connection alive itself).

Not done (measured, left for B12b): an entropy stage. With the poses in, what is left down in combat is entity updates
(8.4 KB/s: NetQuake's variable-length updates of moving grunts and their weapons, whole whenever their bits change) and
the poses' deltas (9.4 KB/s); the rest (time, move acknowledgements, prediction, header) about 4 KB/s. Field-level
coding of the entities comes before an entropy stage; the fast retransmit of the reliable window is still open.

### B12b results: the entities by their fields, the reliable window's resends (2026-11-15, dev no-LTO build, loopback; `mp_armed` 17/0, `mp_retransmit` 7/0)

First the measurement (`vr_net_bytes`' new `ent_ops`: the snapshot's entity entries by op). In `mp_armed`'s combat the
entities' entries were byte by byte against the base (op 2) far more often than whole: 11299 deltas of 10.5 B each
(head included) against 608 whole ones of 21.7 B in one window. Not the layout changes the B12 notes guessed: a moving
thing's fields (origin, yaw, frame, the lerp byte counting down every frame) change every frame, and the byte delta
paid a group mask and whole changed bytes for each.

| Scenario | Result |
|---|---|
| `mp_armed` | B's changed entity entries in combat by their fields: 3537 of 3650, 6.0 B each (byte by byte: 10.5); idle 4.1 B (5.3). Combat's entity bytes: 9.9 and 27.2 KB/s before (the fight decides: a grunt shot by another dies, its gibs and weapon fly), 3.8-14.6 KB/s after; B's link in combat 17.1-29.8 KB/s (budget 35), idle 5.4-5.7 KB/s (budget 10). New check: the changed entries mostly by fields, at most 8 B each. Failed first: 0 by fields. One suite run with two grunts killed by the others' fire (before the trend coding): 47.2 KB/s, entities 33.4 KB/s, 8.5 B an entry: the flying pieces, which the trend now codes |
| `mp_retransmit` (new) | B 100 ms away each way: a one-chunk reliable message's dropped chunk resent 0.51 s after the drop (its timeout: twice the measured round trip and 50 ms; NetQuake's: a second), a five-chunk message's dropped first chunk resent 0.22 s after (two acks of later chunks: about one round trip); both arrive whole; `net_stats` counts 1 window resent at its timeout and 1 fast |
| `vr_netsnap_selftest` | 12/0 (was 8): a grunt (a long entity number) and its weapon stepping by fields 24 B (byte by byte 30); the grunt's layout changed (a pitch sent, no lerp) 28 B (whole 53); a record that is no update (no U_SIGNAL) byte by byte as before; a gib flying and tumbling against its trend 3 B (against the base 14) |

What changed:

- **An entity's update by its fields** (`vr_netsnap.cpp` fieldsEncode/fieldsDecode, op 3; `QVR_NET_VERSION` 32): the
  update is split at NetQuake's fields (its bits, number, model, frame, colormap, skin, effects, origins and angles in
  the protocol's sizes, alpha, scale, frame2, model2, lerpfinish) with Quake VR's own (`U_QVR_*`) as one tail; the entry
  is a varint mask (the trend's bit, then the changed fields: lerp, frame, the origins, yaw in its first byte), the bits
  xor the base's when they changed, each change modulo its field's size zigzagged (a yaw across 0 degrees: small), the
  tail byte by byte or whole when it changed. A field the base lacks counts from 0. The server sends it only when its
  own decoding gives the update back exactly (else byte by byte or whole as before), so it is lossless whatever the
  bytes mean; the smallest of the three goes. The client rebuilds NetQuake's update and parses it as before.
- **The trend**: each snapshot remembers the one it was coded against (`baseSeq`), and both sides keep the base's own
  base while it is in the ring; a moving thing's origins and angles may be coded against where their last change
  carries them (the base's value plus that change times frames ahead over frames between, in integers, the same on both
  sides) when that is smaller (the mask's first bit). A flying gib or tumbling weapon then costs its head and a byte.
- **The reliable window's resends** (`net_dgrm.c`): the timeout is twice the chunks' smoothed round trip plus 50 ms
  (Karn's rule: a resent chunk's ack not measured), at least 0.15 s, doubling with each timeout in a row up to
  NetQuake's second (a peer not reading, a map loading: never more often than that allows). Two acks of later chunks
  (the receiver keeps only the next in order, so one before them was lost) send the window again at once, once until
  the next ack in order. `net_stats` prints the windows resent by timeout and fast. Loopback and single player unchanged.
- `vr_net_reliable_test <entity> <bytes>` (server): a reliable message of that size to a player's client, which prints
  `mp_reliable: <bytes> bytes` when it arrives. `mp_proxy.py` drops the next reliable chunks down on request
  (`drop_reliable_down`, at least a size) and times their resend.

Not done (measured, left): an entropy stage. Against `mp_armed`'s budgets it does not pay now: combat 17.1-29.8 KB/s of
35, idle 5.4-5.7 of 10. What is left down in combat is the poses' byte deltas (8-12 KB/s, the largest kind now), then
the entities (mostly new things whole: gibs, shots, dropped weapons), the walk state (1.2-1.5 KB/s). A pose record
coded by its fields and trend (as the entities now) is the cheaper next step if the budgets ever tighten. The combat
budget stays 35 KB/s: the fight sets the spread (grunts killing each other), and lowering it would make the scenario
flaky, not the code better.

Final checks: `mp_test.py suite` after the fields and resends (before the trend): 27 of 31 (`mp_armed` 14/3 in a gib
storm, `mp_pickup` 8/1, `mp_predict` 23/1, `mp_disarm` 34/1: their known flakes; `mp_predictclimb` 28/0, B12's
keepalive fix holding); after the trend: 30 of 31 (`mp_armed` 16/1, the rate's nearest-over-farthest check). That check
now wants the nearest two's share half way to their weights' ratio (the fixed 1.2 failed with the six standing close
once the entities cost less): 1.20 against 1.17 wanted. `mp_armed` alone after it: 16/1, combat 40.1 KB/s over the 35
budget in another gib storm (a grunt gibbed at B's feet: dozens of `vr_smallgib`, new entities whole). Once (1 run of
5) B's link stopped 20 s in during such a storm (the proxy idle, B's log silent until the harness quit him; nothing
undecodable, no error): open, below. vr_netsnap_selftest 12/0, vr_pd_test 1 11/0 (the merge). Not run this phase:
scenario_check, load_models_test, vr_run_check (single player's loopback untouched: the codec and the resends are a
remote client's only).

Open (B12b):

- **Gib storms**: `mp_armed`'s combat goes over 35 KB/s when a grunt is gibbed in front of B (40-47 KB/s; the new gibs
  whole, then flying). Their spawn could go as one record per burst, or the gibs' updates could be culled by size and
  distance; or the scenario's grunts made unable to kill each other, so it measures six armed humanoids as it says.
- **One stalled run**: B's link stopped mid-storm once (above). Not reproduced in 4 more runs; look at B's frame time
  and the server's sends to him (`vr_net_stats`) when it recurs.

### B12c results: the stalled run (a button), the gib storms' fragments, a rate's starved records (2026-11-15, dev no-LTO build, loopback; `mp_armed` 18/0 in 4 runs of 4)

**The stall was no network fault.** In the stalled run (`mp_armed_20261115_201930`) A and B both logged `mp_event: left`
at the same millisecond, 2 s into the combat window; the server then printed "Run setup: a new game", removed both
clients and loaded `start`. vrtesthall's NEW GAME wall button runs `campaign 1; map start` (a command button,
`buttonEffect 3`), and `button_touch_any` let anything thrown at a button press it (`vr_button_throw`): a grunt's weapon
or limb flung by a grenade's blast hit it hard enough. Host_ShutdownServer's `svc_disconnect` reached both clients
(Host_EndGame prints only with `developer`), so the clients sent nothing more and the proxy went idle. Fixed in single
player too (its own commit): a command button runs its command only when a player pressed it (his hand, his step,
what he threw, his shot), never for a monster (`vr_monsters_press_buttons`) nor for a thing no player threw.
`vr_systems_test 17` (vrtesthall): failed first (a monster and a flung weapon both pressed NEW GAME), now 1/0. The
harness says why a link stopped (`B left the game`, the server's map loads) instead of only "fewer packets".

**The gib storms.** Measured first (`vr_net_classes`, new: the server's snapshot entity entries by classname): in a storm
the new entities' whole entries are small (5-33 KB of 90-165 KB in a window: the "burst's new entities", templates or
baselines, would not pay); the flying fragments' updates are: `vr_smallgib` 31-53 KB and `vr_explosion_debris` 24-25
KB of 157-165 KB (35-50%), 7.5-8.8 B an entry with the trend, every frame each. Then `vr_limb`, the grunts' weapons,
their corpses (`(freed)`: things gone by the report).

| Scenario | Result |
|---|---|
| `mp_armed` | B's link in combat with the poses in his snapshot: 29.4, 29.5, 33.0 and 29.4 KB/s in 4 runs (storms with 790-3270 fragment records waiting), under the 35 KB/s budget; before: 33.3-35.9 KB/s in 3 runs (B12b: 40-47 in heavy storms). New check: the fragments' bytes a datagram within their budget (72-104 B of 128). Under `vr_net_rate 12000` the nearest two humanoids' poses against the farthest two: 1.12-1.56 (wanted 1.06-1.25); before the fix 0.01 and 0.22 (1 and 5-10 entries for the nearest in 5 s). The check now counts only the humanoids alive at the window's end (`edict`'s health): one killed in it read as the rate's fault |
| `vr_netsnap_selftest` | 13/0 (was 12): every record changed, the odd ones cosmetic with 30 bytes: every other record sent, the cosmetic ones within 30 B, their highest priority among them |
| `vr_systems_test 17` | 1/0 (new; single player): above |

What changed:

- **The cosmetic fragments' budget** (`vr_net_cosmetic_bytes`, 128 B a datagram, 0 none; Debug > Reports > Network:
  Fragments' Bytes): a remote client's snapshot carries at most that of `vr_smallgib` and `vr_explosion_debris`
  entries (nothing touches them: no gameplay), chosen by the B12 priorities (near, ahead, waited longest); the others
  wait as a rate's do (the base's record on both sides). 9.2 KB/s at most at 72 Hz however many fly. `mp_netsnap:`
  adds their bytes (`cosmetic=`) and their waits (`cwaited=`).
- **The client's places only from changed records** (`interpNote`): an unchanged record adds no sample, so a thing whose
  update waited (a fragment's budget, a rate) is drawn between the places that came, not stopped a frame then jumped;
  one unchanged longer than 4 frames rested, and sets off from its place a frame before its next (single player and
  the loopback have no interpolation buffer: unchanged).
- **A rate's starved records** (`encode`): a record larger than a frame's share of `vr_net_rate` never went while
  smaller, lower ones took each datagram's room (the nearest humanoids' large poses). The highest priority now goes past
  what the bucket holds when it fits the datagram and the bucket is not in debt; the next datagrams carry only what must
  go until it is paid back (the rate held: 12.0 KB/s under 12000).
- `vr_net_classes [reset]` (server): the snapshot entity entries' bytes, entries, whole ones and things by classname,
  "mp_netclass:" lines; `mp_armed` notes it for each window.

Not done: the poses by their fields and trend. With the fragments budgeted, combat is 29-33 KB/s of 35 and the poses'
deltas are the largest kind (9.5-12.2 KB/s): the first step if the budget tightens or the storms grow. Not run here (the
coordinator's): the full suite's flakes are listed in its results below.

### B10v results: proximity voice chat, Opus and RNNoise (2026-11-15, dev no-LTO build, loopback)

(The phase table's voice row, "B10"; "v" keeps it apart from B10's player-to-player features.)

- **Vendored** (his approval, 2026-11-15): Opus 1.6.1 (the official release tarball, SHA-256 `6ffcb593...572411a1`,
  matching xiph's SHA256SUMS.txt) as its float build without the optional DNN parts or SIMD, and RNNoise 0.2 (the
  GitHub release's tarball, `90fce4b0...2b11c0d37`, its model built in: no separate download), under
  `Quake/vr/external/opus` and `rnnoise` (unchanged sources, a README each with the version, hash and what was kept;
  BSD-3 licences in CREDITS.md and shipped as `licenses/opus.txt`, `licenses/rnnoise.txt`). Static libraries always
  optimised: `opus.vcxproj`, `rnnoise.vcxproj` (in the solution), `vr.cmake` (`qvr_opus`, `qvr_rnnoise`), `vr.mk`.
  RNNoise's release lacks the `os_support.h` its plain-C and NEON paths include: Opus's is on its include path. The
  Quest: all 147 sources compiled with the NDK's clang for arm64 (NEON) and a program calling both linked
  (`scratch/voice_android.sh`; agent/quest's CMake takes them from vr.cmake).
- **Sending** (`vr_voicechat.cpp`): in a multiplayer game with `vr_voicechat` 1 (default), the microphone Voice Notes
  uses (`vr_note_device`, opened on a worker, 48 kHz mono; never with `-nomic`, which the harness passes), RNNoise in
  10 ms blocks (`vr_voicechat_denoise` 1; its voice probability is the voice activation's, `vr_voicechat_vad` 0.6),
  Opus VoIP at `vr_voicechat_bitrate` 24000 in 20 ms frames, in-band FEC at 10% expected loss. Push-to-talk
  (`vr_voicechat_mode` 0, default): `+vr_voicechat`, or the off hand's upper button at the mouth when Voice Notes are off
  (with them on, that gesture stays the notes'); voice activation (1): a frame before the voice's onset and 0.4 s after
  it. A talk spurt's first frame is flagged and its end sent as an empty marker. Frames go after the move
  (`QVR_CLC_VOICE` 17), each in two datagrams (this and the next): NetQuake drops an unreliable datagram that arrives
  after a newer one. The host's own player goes the same way, through his loopback client (B9's rule).
- **Relaying** (`vr_voicerelay.cpp`): the server keeps each speaker's last 16 frames (newer sequence numbers only: the
  copies dropped; at most 60 a second taken) and writes the others' into each client's datagram before his snapshot
  (`QVR_SVC_VOICE` 48), new frames first, then the last datagram's again while his budget lasts:
  `vr_net_voice_bytes` 8000 B/s, at most half his `vr_net_rate`, a quarter of the datagram; frames waiting over 0.3 s
  are dropped. `QVR_NET_VERSION` 33. `vr_voicechat_relay` (server) prints each slot's counts.
- **Receiving**: per speaker, a jitter buffer of 32 frames by sequence number; a spurt starts after 40 ms + 3 x the
  measured jitter (RFC 3550's, at most 200 ms); a frame missing at its turn is recovered from the next one's FEC, else
  concealed (Opus PLC, at most 8 in a row); a frame that comes late adds 20 ms of concealment (the delay raised, at most
  to 200 ms); copies are counted, not played. Decoded 40 ms ahead of the mix's read position into a 2 s looping sound
  (`qvr_voicechat/<slot>`, made in memory, 16-bit at the mix's rate) started on the speaker's entity and moved to his
  head each frame (his avatar's head): Quake's and Steam Audio's spatialisers both place it, attenuated by
  `vr_voicechat_attenuation` 0.5 (silent at 2000 units); `vr_voicechat_radio` 1 full volume anywhere. Without sound
  (-nosound) the same play-out runs on the clock (the tests' path). `vr_voicechat_volume`, per player
  `vr_voicechat_player_volume <n|name> <0-4>`, `vr_voicechat_mute <n|name> [0|1]` (by name: a new player in his slot
  starts unmuted), `vr_voicechat_players`. `((o))` floats over a talking player's head and `TALKING` sits low in your
  view while you send (`vr_voicechat_indicator`).
- **Single player**: nothing runs (no capture or sends without `cl.maxclients > 1`); the only shared change is
  `voicenotes::deviceBusy()` (the meter waits while voice chat's microphone opens: SDL opens one device at a time).
- **Menu**: Debug > Reports, the Network rows: Voice Chat, Voice Activation, its threshold, Noise Suppression,
  Microphone Gain, Volume, Fade With Distance, Radio, Talking Indicator, Players, Report, Self-Test, Voice Budget.

Tests:
- `vr_voicechat_selftest` 13/0: Opus loopback of a voiced-like signal (180 Hz and harmonics, vibrato, syllables) at
  22.4 kbit/s, 26-80 bytes a frame, SNR 9.2 dB (bound 6: SILK codes the spectrum, not the waveform), an encode 0.08 ms;
  a lost frame from the next one's FEC at 10.1 dB; five concealed frames bounded; RNNoise: white noise down 80 dB, its
  voice probability on it 0.00, a block 0.09 ms; the jitter buffer: 100 frames in swapped pairs, 10 lost: 90 played,
  10 recovered, none late, the spurt ended at its marker.
- `mp_test.py voice` (a 440 Hz tone in place of A's microphone, `vr_voicechat_test_tone`): 20/0 dedicated, 19/0
  `--listen` (A hosts). First frame heard at B 109-131 ms after A captured it (bound 250); 152 of 153 frames played,
  none late; the tone's share 1.000 at 440.0 Hz; gain 0.952 at 96 units, 0.649 at 702, 1.000 with the radio; his
  volume 0.5 -> 0.488; muted: frames come, none played; B's voice bytes 6.0 KB/s (a frame and its copy) under 8000,
  1.55 KB/s under a 1500 budget (frames dropped), 3.99 KB/s under `vr_net_rate` 8000 (half: 4000); push-to-talk released
  ends the spurt; voice activation sends the loud tone and stops on silence; RNNoise on: frames flow; no microphone:
  told, nothing opened, not sending. With B 50 ms away, 30 ms jitter and 5% loss each way (`mp_voice_lossy`, the jitter
  and loss from after his join): 196 ms, 132 played, 17 recovered by FEC, 4 concealed of 153, none late, the tone's share
  0.69-0.86, 4 late (each raising the delay a frame; the first spurt starts at 40 ms, its jitter not yet
  measured: at most 8 allowed) (before the copies: 61 of 153 lost to NetQuake's overtaken-datagram drop).
- `mp_test.py suite` (33 scenarios with `mp_voice` and `mp_voice_lossy`): 28 pass; `mp_avatar` 15/1 (B draws A's
  weapon: None), `mp_climb` 7/2 (A never put at the ledge), `mp_predictclimb` 27/1, `mp_disarm` 34/1 (the shell into
  A's reserve), `mp_voice_lossy` 20/1 (1 late frame, the bound then 0): each passes alone (17/0, 11/0, 28/0, 35/0,
  21/0 with the bound 8). scenario_check 139 of 141 (`classic_drops`, `magthrow`: "Grunt Burst Rifle" is not a toolgun
  Weapons entry, the toolgun calls it "Burst Rifle": data from next, not B10v), load_models_test 4/0, vr_run_check 12/0,
  the dev and the Release (ThinLTO) builds 0 warnings (ThinLTO needed the engine's libopus import library dropped: the
  music's libopus-0.dll stays, loaded by libopusfile-0.dll; nothing of the engine's called it).

### For him (B10v)

- In VR with a second player: hold the off hand at your mouth and press its upper button (Voice Notes off) to talk;
  his voice should come from his head, fade with distance, and show `((o))` over him. Try Voice Activation and the
  Radio (Debug > Reports, the Voice rows), and `vr_voicechat_mute <name>`.
- Untested headless: a real microphone (and its device choice, vr_note_device) and the two spatial mixers playing the
  looping voice sound. (Flat play's key: B10v part 2's Push-to-Talk key, CAPSLOCK.)

### B10v part 2 results: the Voice Chat page, a Push-to-Talk key (2026-11-16, dev no-LTO build, loopback)

- **Voice Chat page** (`vr_menu_voicechat.inc`; VR Settings > HUD, Haptics and Sound > Sound > Voice Chat, also linked
  from Debug > Reports, whose Voice rows moved here): your line (voice chat off / not in a multiplayer game /
  push-to-talk / voice activation, waiting / TALKING, with your microphone's level), Your Voice (Voice Chat, Talk With:
  Push-to-Talk or Voice Activation, Activation Threshold, the Push-to-Talk key, Noise Suppression, Microphone Gain,
  Microphones), Voices You Hear (Volume, Fade With Distance, Radio, Talking Indicator), **Players**: a row for each
  other player (his name, his volume in %, `((o)) talking` lit green while heard, `muted` red, `muted, talking` while a
  muted player's frames arrive: whom to hear again) with Mute/Unmute, Quieter, Louder buttons (25% steps, 0-400%), and
  developer rows (Players in the console, Report, Self-Test, Voice Budget). The buttons and the console commands
  (`vr_voicechat_mute`, `vr_voicechat_player_volume`) share `voicechat::setMuted`/`setPlayerVolume`; the page is
  rebuilt when a player joins or leaves (`voicechat::players()`). Settings stay per name for the game, as before.
- **Push-to-Talk key** `vr_flat_key_voicechat` (default CAPSLOCK, free in the default binds; Flat Keys and the page):
  held, push-to-talk (flat play owns it: its bind is off while flat play runs; read in VR too).
- `vr_menu_press "<page> > <row text's start> / <button>"` presses a button row's button (the Voice Chat page's players,
  the Checklist's items); a script line's trailing carriage return no longer breaks the label.
- `vr_voicechat_report` prints each listed player's page row (`mp_voice_row:`).

Tests: `mp_test.py voice` 22/0 (A named VoiceA): his row on B's page "VoiceA 100% ((o)) talking"; Quieter twice: gain
0.488, "50%"; Louder twice and Mute: frames come, none played, "100% muted, talking"; Unmute: heard again; the
Push-to-Talk key (vr_mock_key CAPSLOCK) held: heard at B, let go: not sent. vr_menu_path_check 0 missing;
menu_paths.txt regenerated. Merge of origin/next: flat ladders kept on the moves' climb (findHold's Seeker, the mantle's
pmove step, the flat ladder's mount at its end), and a headset's slide (vr_slide, experimental) walks through SV_AirMove
while its input is held or it slides (`slide::headsetEngaged`; vr_pmove_own skipped it: vrslide_test 1/0 again;
flatladder_test 2/0, flatslide_test 26/0). `mp_test.py suite` (34 scenarios): 32 pass; `mp_armed` 17/1 (a snapshot in
every datagram under the cap: 304 of 360) and `mp_dm` 13/3 (the bot's shot, the parry, the fraglimit) pass alone (18/0,
16/0). scenario_check 138/0, load_models_test 4/0, vr_run_check 12/0 (run B: `vr_run startgun 3` with Melee Only,
`scratch/b10v2_runcheck.sh`), the dev and the Release (ThinLTO) builds 0 warnings.

### For him (B10v part 2)

- In VR with a second player: VR Settings > HUD, Haptics and Sound > Voice Chat: his row lights while he talks; Mute,
  Quieter, Louder change what you hear of him. Your line shows TALKING and your microphone's level while you talk.
- Flat play: hold Caps Lock to talk (Flat Keys > Push-to-Talk to change it).
- Still untested headless: a real microphone, the two spatial mixers.
- Open: a headset client's slide on a remote server is not in his moves' replay (his prediction walks it: corrections
  while sliding; vr_slide is experimental, off by default).

### B13 results: the join through reordering (selective acks); B11 part 1, deathmatch with bots (2026-11-16, dev no-LTO build, loopback)

- **The stall's root cause** (found with a trace of every chunk, ack and resend on both ends): NetQuake's receiver acks
  every data packet but takes only the next one in order, so a chunk that overtook a late one was dropped and acked.
  B2's window counted an ack only in order, assuming the ack of a dropped chunk always came before the late one's;
  under jitter the acks reorder too: the late chunk's ack came first, then the dropped one's, which now read as next
  in order. The server took serverinfo (12 chunks) as delivered while B held 6 of them; it waited for his `prespawn`,
  he for chunk 6: only keepalives. Not the chunking, B12b's timings or B12's keepalive.
- **The fix, selective acks** (`net_dgrm.c`): a Quake VR receiver holds the chunks that come before their turn (a
  window's worth, every chunk in flight fits: `held` in the socket) and acks a chunk only once it has it; the sender
  marks each chunk acked, moves on past the ones acked in order, and resends (at the timeout, or fast at two acks of
  later chunks) only the ones not acked. A peer that is not Quake VR still acks the chunks it drops, so it gets a window
  of one chunk (NetQuake's stop and wait, always safe): the server knows from the connect request's network version,
  the client from the accept reply, which now carries the server's after the port (NetQuake clients read the port
  alone). `QVR_NET_VERSION` 34. Also: a stale unreliable datagram no longer ends the read for the frame (NetQuake's
  `break`): the acks and chunks behind it waited a frame each.
- **Tests**: `mp_retransmit` stages the stall (the proxy holds a two-chunk message's first chunk back 60 ms and the
  second's ack twice as long): the old window never delivered it nor the next message (7/1), now 8/0 (read in 0.27 s).
  `mp_voice_lossy` joins through its 30 ms jitter and 5% loss (no longer turned on after the join); `coop --lag 50
  --jitter 30 --loss 0.05` joined 4 of 4 (before: 1 of 3).
- **B11 part 1, `mp_dm`** (new, 9/0): a dedicated deathmatch server (deathmatch 1, fraglimit 3, timelimit 8 as QC's
  safety) on dm4; A and B (through the proxy) join at two deathmatch starts; A's `impulse 100` twice adds two FrikBot
  bots: both clients list four players (`mp_frags`), each bot seen moving; A, idle, is killed by a bot and his trigger
  respawns him (PlayerDeathThink); the bots reach the fraglimit (Bishop 3, Kryten 1): the intermission at both clients,
  their scoreboards the same. Nothing needed fixing: deathmatch, FrikBot's client slots and the scoreboard work over
  the network as they are. New client watch lines (`vr_mptest.cpp`): `mp_frags` (a scoreboard slot named, its frags),
  `mp_event: intermission=`, `died`, `respawned`.
  In the suite, waiting for the bots to kill A and to reach the fraglimit by themselves timed out once (one bot's
  suicide in 7 minutes): the deaths are now the server's `vr_hurt 1000 <slot> <bot entity>` (the bot credited; the
  command takes an attacker now and runs in deathmatch on a dedicated server's console, which its players can't send),
  A respawned by his trigger each time: 10/0, the bot at 4 frags.
- `mp_test.py suite` (34 scenarios): 31 pass; `mp_predictclimb` 26/2 (the mantle) and `mp_lagcomp` 9/2 (a view time
  of 1000 ms) pass alone (28/0, 11/0), `mp_dm` as above. scenario_check PASS (141 scenes), load_models_test 4/0,
  vr_run_check 12/0, the dev and the Release (ThinLTO) builds 0 warnings.

### B11 part 2 results, part 1: deathmatch's item rules, player disarm PvE/PvP with bots (2026-11-16, dev no-LTO build, loopback; `mp_dm` 12/0)

- **Player disarm split** (his call, 2026-11-16): `vr_player_disarm` (PvE: enemies' shots and blows) now defaults to
  **0**; new `vr_player_disarm_pvp` (PvP: another player's or a bot's shots, blows and grab) defaults to **1**. One path
  (`VR_Player_Disarm`); `VR_PD_ByOn(by)` decides by who did it (a client, a bot included: PvP; a monster, anything else:
  PvE): `VR_PD_Shot` (the shooter), the projectiles (their owner), `VR_PD_Blow` (the blower), the grab (PvP's). Menu rows:
  Armed Grunts > "Enemies Can Disarm You", "Players Can Disarm Each Other" (menu_paths.txt regenerated). vr_pd_test 1 and
  2 (PvE, which they turn on) 11/0 and 8/0.
- **`vr_dm_test 1`** (new, `vr_dmrules_test.qc`, run by `mp_dm` on its dedicated server; also Debug > Tests): a weapon
  pickup a bot touches (the engine's body touch, the fake hand) is its, gone, back in its 30 s as it was placed
  (the wait pulled forward); the weapon the bot's hand let go of for it is a thrown weapon removed in deathmatch after 10 s;
  in deathmatch 2 the pickup stays and the bot having it does not take it again (no ammo twice); A's shot across a bot's
  held weapon knocks it out with the disarm sounds (PvP); PvP off: nothing; a monster's shot: nothing with PvE off (the
  default), counted with it on; a bot's shot across A's weapon knocks it out ("Your weapon is knocked out of your hand!"
  at A's client). 17/0. Nothing in deathmatch's item rules needed fixing.
- **Fixed (dedicated server only)**: its QC asked for the client's own cvars (`r_wateralpha`, `v_i*_level`), which a
  dedicated server never registers: unknown handles (read as 0) and every QC test there failing on them. Registered at
  their defaults when the QC asks (`vr_builtins.cpp dedicatedClientCvar`).
- **mp_dm's flakes fixed** (the test's): B's start often sees neither bot (the move is now seen by A or B); two bots on
  dm4 can reach fraglimit 3 by themselves in under a minute (the rules' test runs first); A killed by a bot's own shot
  just before the scenario's `vr_hurt` waited for a second death (that death now counts).
- **Not done (next part)**: parry between a player and a bot (a bot's blows are FrikBot's, not VR swings), the grab
  disarm with a bot (bots never grab; a player's grab on a bot's weapon is the same code as on a player's, untested with
  a moving bot), bots' bodies through the shared avatar routine (the server would make a bot's avatar block from its
  edict: head, body yaw, its two hand fields, its weapon; bots' hands both sit at one point today, `bot_setvrfields`),
  FrikBot's walk through `vr_pmove` (FrikBot runs its own copy of Quake's physics in QC with a `phys_obj`; the moves'
  module would take its stick as an Input instead).
- **The checks** (before the merge of origin/next): `mp_test.py suite` (34 scenarios): 32 pass; `mp_loss` 13/2 (A's
  body and hands jump at 10% loss: its known busy-machine check, other agents' runs going) and `mp_lagcomp` 9/2 (a view
  time of 775 ms, its known suite flake); both re-run alone after the merge (below). `mp_dm` 12/0 (dmtest 17/0).
  scenario_check PASS (141 scenes), load_models_test 4/0, vr_run_check 12/0, the dev and Release (ThinLTO) builds 0
  warnings.

### B11 part 3 results, part 1: origin/next merged; bots' hands apart; the grab on a bot's gun (2026-11-16, dev no-LTO build, loopback; `mp_dm` 12/0, dmtest 22/0)

- **origin/next merged** (22 commits: the teleporters' self view, looping sounds, wet shine, trailer titles, the
  installer): one conflict, `vr_server.cpp init` (both commands kept: `vr_net_reliable_test`, `vr_snd_loops`). After
  it, B11 part 2's suite flakes alone: `mp_loss` 15/0, `mp_lagcomp` 11/0.
- **mp_dm's frag credit** (the test's): its first kill's check wanted a bot's frags at 1 or more at B; the bots' own
  suicides had put Servo at -2, and A's death took it to -1 (11/1 once). Now a frag up from the bot's count before
  (`dm_frag_gained`).
- **Bots' hands apart** (`bot_setvrfields`, FrikBot): both hands sat at one point in front of its chest; now the main
  hand 7 units to its right, the off hand 7 to its left, each weapon along its aim from there (the shots' start, the
  disarms' and the grab's lines of its guns). Single player's Melee Only bots test (`vr_pickup_test 15`) 13/0.
- **The grab on a bot's gun** (`vr_dm_test 1` grown, 22/0): the bot held 40 units in front of the human for a second
  (the lag compensation's past places of it there too), his empty main hand closed at the middle of its gun's line:
  a running bot's (320 u/s) is too fast to take hold of (the grab's 1.25 m/s is against the other's body too: a hand
  must keep up with a running player's gun); a still one's is taken hold of, and the pull away from it (past
  `vr_player_disarm_pull`) puts the bot's gun in his hand. Same code as between two humans: nothing needed fixing.
  The test keeps the human and the bot alive while it runs (the bots' own fight killed him between the hold and the
  pull once), takes whichever gun the bot holds (its own Super Nailgun often) and waits for the human's own hand to
  empty on the bot's shot (the disarms' count also counted a bot's late roll); the harness now waits for A's "knocked
  out" line instead of looking once. mp_dm 12/0 in 3 of 3 runs after that.
- **The checks**: `mp_test.py suite` (34 scenarios): 31 pass; `mp_flow` 15/1 and `mp_climb` 7/2 (A's mantle setup,
  its known flake) pass alone (16/0, 11/0); `mp_dm` 11/1 (the pull, fixed above). scenario_check PASS (142 scenes),
  load_models_test 4/0, vr_run_check 12/0, vr_pickup_test 15 13/0, the dev and Release (ThinLTO) builds 0 warnings.
- **Not done, part 2 of B11 part 3: bots' bodies.** A bot has no moves, so no avatar: the clients draw its player model
  (its own gun in the model's hands). The avatar needs the hands as drawn (`VrDrawnHand`: the jointed hand's place
  on the gun's grip, its curls, the weapon's place by its Weapon Offsets), which only the client's own drawing makes
  today (`view::setupWeapon`, `setupHand`: the local player's `hands::State`, its view entities). The way: the server
  sends a bot's controller poses (QC's `handpos`/`handrot`, apart now) and its weapons in the avatar with a "solve
  here" flag, and the receiving client runs the same weapon and hand placement for it (`setupWeapon`/`setupHand`
  taking a subject: a State and entities of their own instead of the local player's), then poses its body as any
  other avatar's. One code path for your hands and a bot's; a sizeable refactor of the view's hands, so its own step.
- **Not done**: FrikBot's walk through `vr_pmove` (its stick as an Input), parry against a bot's blows (FrikBot's axe
  is the player's `W_FireAxe` trace, not a VR swing the parry sees).

### B11 part 3 results, part 2: bots' bodies (2026-11-16, dev no-LTO build, loopback; `mp_dm` 14/0)

- **origin/next merged** (24 commits): three conflicts, all on the player disarm's PvE/PvP split (next made
  `vr_player_disarm` off by default with its own label; ours keeps the two settings and their labels, his call of
  2026-11-16). next's PATCH_NOTES/RELEASE_NOTES name the menu row "You Can Be Disarmed": on this branch it is "Enemies
  Can Disarm You" (to be reconciled when agent/mp lands).
- **The wire** (`QVR_NET_VERSION` 35): a bot (its client slot's entity a "player" whose QC `ishuman` is 0, the slot not
  active: FrikBot) now has an avatar. Its hands are `VrDrawnHand::solveBit` records: the controller (QC's `handpos`,
  `handrot`, which `bot_setvrfields` sets every frame) and the weapon's model and frame, 11 B a hand; its head at its
  eyes (`view_ofs`, `v_angle`), its body pose a standing player's of the default height (`units::modelEyeHeight`).
- **The client** (`view::solveHands`, vr_view_hands.cpp): the same `setupWeapon` and `setupHand` that place your hands
  place a bot's, with its own drawing state swapped in for the time (`SubjectHands` per player entity: the weapons' and
  jointed hands' entities, `rigHands`, the curls and their biases, `drawnAs`, the carry flag, the ghost's place,
  `wholeTurn`), then `netHands` writes them as yours are written for the others, and the avatar draws them and poses
  the body on them exactly as a human's (`view::drawHands`, `avatar::poseOther`). Its curls are a flatscreen player's (a
  hand holding a weapon or a fist grips; an empty one is open; the grasp then wraps what it holds).
- **What a bot's hands leave out** (`view_detail::otherSubject`, read only inside setupWeapon/setupHand/heldBy/gripFlip):
  everything that is the local player's alone -- the hand-off's carry, the reverse grip, a blade in a body, props,
  limbs and the torch, the hands on each other's weapon (steadyOther, pushOut), the reload's parts, the hotspots, the
  draw from a holster, the morph, the spent gun's shake, the nail pistol's slide, the Box3D record of the gun, the ammo
  text. Your own hands run every phase as before (`own` true: each gate is `own && <as before>`).
- **Check** (`mp_dm`, grown): A or B reports a bot's hands as placed (`mp_solved:` in `vr_mp_avatars`): both jointed
  hands drawn about 5.2 units from their controllers (the rig's origin from the grip), its rocket launcher in its main
  hand (5.2 units from the hand's origin), its body posed on them (`mp_body:` for the bot: `progs/vrbody.mdl`). 14/0.
- **Not done**: FrikBot's walk through `vr_pmove` (its stick as an Input) and parry against a bot's blows (FrikBot's axe
  is the player's `W_FireAxe` trace, not a VR swing the parry sees). Bots' fists close as a flat player's guard does
  (gripping), not punching motions; their hands do not swing (a bot's melee has no hand motion to send).

### B11 part 3 results, part 3: bots walk through vr_pmove; a bot's swing parried; bots' bodies at their height (2026-11-16, dev no-LTO build, loopback; `mp_dm` 16/0)

- **origin/next merged** (4 commits, no conflict).
- **FrikBot's walk through the moves players' walks run** (`vr_pmove_bots`, default 1; Debug > Reports, "Movement: Bots
  Walk Through Moves"; 0 the fallback: FrikBot's own copy of Quake's physics, `bot_phys.qc`). In BotFrame, after its
  key emulation (CL_KeyMove), `botmovethink(self.movevect)` (`predict::botThink`) starts the bot's slot's frame as a
  client's message read starts his (`beginFrame`: the QC move flags cleared) and runs `VR_PredictThink`, which queues
  its stick as one move (`botInput`: clc_move's forward, side and up; its aim steering it; its jump key; no hands
  holding). FrikBot's SV_ClientThink then leaves its walk (as sv_user.c leaves a client's), PlayerPreThink sees the
  same `.vr_movejump`/`.vr_moveswim` a client's does, and its SV_Physics_Client calls `botmovewalk()`
  (`predict::botWalk`): `VR_PredictPhysics` (gravity, walk or swim, steps, slopes, the jump, the water jump, the
  close-quarters slowing, the carry and heavy weapons' speed) and the link with its touches, in place of FrikBot's
  gravity and its phys_obj's SV_FlyMove. PlayerPostThink and BotAI still run after it from the phys_obj's think (as
  PostPhysics ran them: not inside the bots' loop, the runaway counter). A wall's hold (the walk's `MoveNotes`: a wall
  met, short of the stick's want as SV_Accelerate reaches it) is told to `frik_obstructed`, as PostPhysics measured
  its own; FrikBot's step search (botCheckForStep, BruteForceStep) is not needed: the walk steps up 18 itself.
  One path: the same `VR_PredictThink`/`VR_PredictPhysics` as a remote client's and this game's own player's moves.
- **A bot's swing parried**: a bot's melee (W_Fire_Melee's button swing, player_axe*: its blow at the third frame) now
  swings its main hand (`.vr_botswing`; `bot_setvrfields`): raised on its right, swung level across its front (160
  degrees, 18 units out, straight ahead at the blow, 0.1-0.3 s), the weapon pointing out along the arc. The clients
  draw it so (its controllers, B11 part 3 part 2's solveBit). Its weapon's line is kept each frame
  (`VR_HandLinesKept`: tracked hands or a bot's, VR_Parry_GuardFrame) and its W_FireAxe blow on a player is judged by
  that line as a tracked hand's swing (`vr_melee_blowhand` set: VR_DRP_PvpApplies), so a player's weapon held across
  its way parries it (the directional parry), else the guard as before. A flat-screen player's button swing is as
  before (no lines kept: the guard's).
- **Bots' bodies at their height**: the server sent a bot's body pose at the model's eye height (1.646 m) and the
  client sized a player without a calibration at that too: bots were drawn smaller than humans (scale 26.2 against
  29.6). Now its standing eye height (`DEFAULT_VIEWHEIGHT - mins_z`: 46 units, 1.75 m at world scale 1), and a player
  with no calibration yet is sized by his body pose's standing height (a bot's; a human's until his calibration comes).
- **Disarm labels as next's**: Armed Grunts > "Disarming: Your Weapon" > "You Can Be Disarmed" (PvE, next's wording;
  "Off by default: still being tuned") and "You Can Be Disarmed by Players" (PvP); menu_paths.txt regenerated, the
  calibration board's paths 6 found, 0 missing. PATCH_NOTES/RELEASE_NOTES already say next's.
- **Tests**: `vr_dm_test 2` and `3` (new, `vr_dmrules_test.qc`, run by `mp_dm`; Debug > Tests "Deathmatch: Parry a
  Bot's Swing"): a living bot held 40 units in front of A with a sword, A's crowbar given into his gripping main hand
  (another weapon there let go of first: a weapon swapped in a hand is let go of at once), the bot's swing begun as its
  button's. 2: A's crowbar low at his side: the blow lands (20 damage; "dirparry: player's blow lands: score 0.00, 76
  deg off square, gap 13-18 units"). 3: upright across its way (mp_melee's UPRIGHT): "dirparry: player's blow parried by
  hand 1: score 1.00 (13 deg off square, gap 4.2 units, 0.014 s ago)", no blow lands. Both check that the bot walked
  through the players' moves that frame (`.vr_movejump`). `mp_dm` 16/0 (two new checks).
- **The checks**: `mp_test.py suite` all 34 scenarios pass (a first run lost every scenario after mp_bandwidth to port 27500 taken by another run: "Unable to bind"; rerun on `--port 27640`); `vr_pickup_test 15` (Melee Only bots, single player) 13/0. scenario_check PASS (145 scenes), load_models_test 4/0, vr_run_check 12/0 (with Melee
  Only's `3 1 0 1 0`), the dev and Release (ThinLTO) builds 0 warnings.
- **Not done**: bots never grab (the grab disarm with a bot: the player's on its gun only); a bot's fists do not punch
  (a bot cannot punch: its melee is the axe or its melee holster's weapon); FrikBot's AI is unchanged (its jumps and
  routes were tuned to its own physics: watch for a bot stuck where it used to pass; `vr_pmove_bots 0` is the old walk).

## For him (B11b)

**His answer (2026-11-16): on.** `vr_pmove_own` defaults to 1 since B11c part 2; 0 stays as the fallback (Debug >
Network, "Movement: Own Walk Through Moves"). The checks with the new default are in "B11c results, part 2". What he
was asked (as of B11b):

`vr_pmove_own` stays 0 (Debug > Network, "Movement: Own Walk Through Moves"). If it were 1, single player would walk
through vr_pmove.cpp (the module a remote client's moves and his prediction run) instead of sv_phys.c's SV_WalkMove and
QuakeC's PlayerPreThink parts. Exactly what would change:

- **Nothing traced:** walk, jumps, crouched walk, a lift, swimming, Quake's water jump out and the crawl are identical
  frame by frame (origin to 0.0001, velocity to 0.001, flags, water level, ground entity); with 1 built in as the
  default in part 1, prop_climb_test 12/12, mantle_height_test and crawl_test passed (mantles, hangs, holds taken).
- **The hands' swim strokes and crawl pulls** (tracked hands, not the stick) push once a frame after the move instead
  of in SV_ClientThink before it: a stroke's push reaches the walk one frame (about 11-14 ms) later. Not visible in a
  trace without tracked hand motion; the only behaviour change known.
- **The room-scale walk** goes through the moves' copy of VR_ClientRoomscaleMove: same code shape, not traced (the mock
  headset does not walk in the room).
- **Flat-screen play is unaffected** (a flat player never walks through the moves).
- What 1 would buy: one walk code for single player, a listen host, a dedicated server's clients and prediction (a fix
  in one is in all). What it costs: the strokes' frame above, and single player then depends on the moves' module
  staying exact (pmtrace_all.sh is the check).

## Decisions

- Melee's lag compensation (B8c) wraps the queries (melee's sweep, a bash's find, a headbutt's target, a guard's check
  against another player's blow) in QC's scope, not the whole melee frame: the blow is dealt, pushes and deaths happen,
  where the targets are (no ragdoll or effect at a past place for the others), as Source and Overwatch deal a rewound
  hit. Its end keeps what the scope did to a target (moved by the push, a death's frame) on top of its present place.
  The interpolation buffer's clock (B8c) slows to a stop near the newest message rather than extrapolating: no
  overshoot to take back, at the cost of a slowed copy during a long gap.

- Lag compensation (B8b) rewinds only the targets of a MOVE_HITMODEL traceline from a remote client, for that trace,
  rather than his whole frame's QC: the AI, triggers and his own walk keep the server's world; the view time is his
  move's own (newest message time less his buffer's delay), capped at vr_net_lagcomp_most (0.5 s: a 150 ms link's
  round trip and buffer is already 0.33 s, so the plan's 250 ms was too short).

- A remote client's walk (B8) runs move by move on the server through `vr_pmove.cpp`, a port of sv_phys.c's walk with
  hooks, while single player and the listen host's own client keep sv_phys.c's path untouched: the two the prediction
  must agree on (his server and his client) run the same code to the bit, and single player cannot change by
  construction. Moving single player onto the module too is possible later with the hash audits. (Since B11c part 2,
  his call: single player and the listen host walk through the module too, `vr_pmove_own` 1, frame-identical in the
  traces; sv_phys.c's path is the fallback, 0.)

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
- Voice chat's push-to-talk gesture (B10v) is the Voice Notes' (the off hand's upper button at the mouth) only while
  Voice Notes are off: his notes keep it. No key is bound to `+vr_voicechat` by default.
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

- Snapshots (B7b) are delta-coded per record, a record being the bytes NetQuake's own writer wrote, rather than field by
  field: every entity extension (`U_QVR_*`, the avatars, the player state) is covered with no codec of its own, and the
  client parses with NetQuake's parsers (the rest of the client unchanged). Field-level coding and entropy are B9's.
- The interpolation buffer (B7b) leaves brush models and stepping monsters on NetQuake's lerp: the player's own entity
  is not delayed (no prediction until B8), and a lift he rides must stay in step with him.

- Player disarm is two settings (his call, 2026-11-16; B11 part 2): `vr_player_disarm` is PvE (enemies' shots and blows
  on your weapon), **off** by default until it is polished (single player and multiplayer); `vr_player_disarm_pvp` is PvP
  (another player's or a bot's shots, blows and grab), **on** by default. One path (`VR_Player_Disarm`) for both; what
  decides is who did it (`VR_PD_ByOn(by)`: a client, bots included, is PvP; anything else PvE). The grab's settings are
  PvP's. Menu: Armed Grunts > "Disarming: Players' Weapons", "Enemies Can Disarm You" and "Players Can Disarm Each
  Other". When next's own PvE default change lands, keep these names.

- The reliable window under reordering (B13) uses selective acks (the receiver holds early chunks and acks only what
  it has) rather than NetQuake's receiver with the window shrunk: a 30 ms jitter reorders most of a window, and
  go-back-N resent most of it each time. A peer that is not Quake VR keeps a one-chunk window (its acks are NetQuake's).

- A bot's hands (B11 part 3, part 2) are placed on each client by your hands' own code with the bot's drawing state
  swapped in (`view::solveHands`), as B2d poses another's body, rather than setupWeapon/setupHand rewritten to take a
  subject struct everywhere: the local path is unchanged by construction (its phases gated `own && ...`), and a bot
  has none of the local player's interactions (props, reloads, the hand-off: left out under `otherSubject`). The server
  sends a bot's controllers, not hands it cannot draw (no view code on a dedicated server).

- Voice chat's per-player settings (B10v part 2) are on a page of their own, a row a player with buttons, rather than
  a cycle or slider per player on a fixed page: the rows follow who is in the game (rebuilt when one joins or leaves),
  and the console commands share the buttons' code. Flat play's push-to-talk is a flat key (CAPSLOCK, free in the
  default binds) rather than a default bind of `+vr_voicechat`: the Flat Keys page rebinds it, and flat play owns it.

### B14 results, part 1: a headset's slide predicted; the moves' determinism; the server-thread audit (2026-11-16, dev no-LTO build, loopback)

- **A headset's slide in the moves** (B10v part 2's open item): vr_slide's slide ran in SV_AirMove only (his frames left
  the moves while he held its input or slid), so a remote headset player's replay walked where the server slid him:
  corrections of tens of units while sliding. Now `slide::move` (vr_slide.cpp) is the one slide: SV_AirMove's
  (`VR_SlideMove`, flatscreen play and a headset's unwalked frames) and the moves' (vr_pmove.cpp `slideMove`, from
  `airMove`, before the walk's friction) both call it with a `slide::MoveIn` (input held, ground, the jump key, the floor
  under him when it is needed, his stamina) and a `slide::Tuning` (the vr_flat_slide_* numbers); the stamina's parts are
  `slide::payParts` (the server's QuakeC pays them: `Body::slidePaid`); a slide on the ground steps up in the moves'
  walk as in SV_WalkMove (`slide::stepsUp`). The slide's state rides in the moves' `Body` (`slide`, a clock `slideClock`:
  the server's time on the server, 0 at the state in his replay), the move's slide bit in `pmove::Input::slide`, the
  server's `Rules::slide` (not flatscreen, not lying down) and numbers. His state record carries a slide byte (on, held,
  grounded, airborne, the rule) and, once his moves sent the slide's input (`slideSeen`), its last height, his stamina
  and the server's numbers; sliding or cooling down, the paid parts and its times against the clock after his moves (an
  idle record stays the same). Before the server's numbers reach him, his replay slides on his own settings without
  stamina. `vr_mp_predict_report`: `slide_n` (states sliding).
- **Fixed on the way**: `slide::setHeadsetInput` cleared the slide's `held` on any move read without the bit; a remote
  client's resent older move (5% loss) cleared it before the frame's moves ran, and the next held move was a press on
  the server alone (a slide his replay never saw: an 18-unit correction). The press's edge is each move's own now.
  `slide::headsetEngaged` is gone (the moves no longer skip a headset's slide).
- **The moves' determinism** (client vs server): on each new state his client compares the last replay's result for the
  move the state is after with the server's, bit for bit (`exact_n`, `exact_miss`, `exact_max`, `vel_miss`) and folds
  both into FNV-1a hashes (`hash_sv`, `hash_cl`: equal while every result is); `vr_mp_watch` prints a miss
  (`mp_predict_exact_miss`). `mp_predict` and `mp_predictslide` check it (`predict_exact_check`: at most 1% off, none by
  0.001 unit): walking 327 of 327 equal, sliding 178 and 181 of 178 and 181, hashes equal. One run showed one state off
  by 4e-6 in z on e1m1's stairs (the client's trace against SV_Move's at a step: an ulp; open).
- **The server-thread audit** (multiplayer; the server frame and the client both run on the main thread):
  no data race on server state found. Box3D's step on the pool (vr_box3d_threads, 300 bodies awake) runs
  shouldCollide/preSolve on workers reading edicts and QuakeC fields while the main thread waits (read only: a callback
  that wrote an edict, ran QuakeC, SV_Move or Con_Printf would race). Its one timing-dependent result, the hands' press
  hits (an atomic counter's order fed notePress's plane solve and deepest), is sorted now (a separate commit). Still
  timing-dependent, no race: `vr_hull_async`'s monster box trees (a monster collides with Quake's hulls until its tree is
  built: the frame it switches is the CPU's), the 128-hit cap's overflow, the held-gun cuts' entry order (looked up by
  key: harmless). Deterministic by design: the navmesh's builds (land on a fixed server frame: vr_nav_build_frames,
  vr_nav_job_frames; 0 would make them timing-dependent), the map-load parallel work (merged in index order), the
  background save (a copy). Thread-local: qcvm, pr_global_struct, com_token and friends.
- **Not done** (B14b): the world's hashes (vr_physics_hash, the navmesh's, the paths') equal across runs with several clients:
  the harness's servers run on the wall clock (vr_fixed_frames 0) and its clients' moves land in server frames by their
  timing, so two runs differ by design; it needs a fixed-step server in the harness first (the moves applied by number,
  not by arrival). Bots grabbing and punching: not small (left).
- Tests: `mp_test.py predictslide` (new) 13/0: B 150 ms behind and 5% lost each way runs north on e1m1 and slides
  (held to its end, then ended by a jump): 69 and 36 states sliding, every one walked move by move, corrections 0.000,
  at rest where the server has him; before the fix his frames left the moves while sliding. `mp_predict` 29/0.
- A state whose frame held one of his moves to the server's clock (a burst of moves past his credit, a busy machine:
  `mp_predict_sv ... held`) is the server's time, not his moves': its record's slide byte says so (bit 128) and the
  check skips it (`exact_held`). Seen once in the suite: an 11-unit miss at e1m1's step in the air, 12 moves in one
  server frame, 5 held.
- Checks: `mp_test.py suite` (35 scenarios) 33 pass; `mp_avatar` 16/1 (a finger curl 0.0630 against 0.0625: no hand
  code touched) and `mp_predict` 23/2 (the held burst above, then e1m4's water not reached in 20 s on a busy machine)
  then 17/0 and 29/0 with the held states skipped; `mp_predictslide` 13/0 (three runs). Single player: vrslide_test
  15/0, flatslide_test 26/0 (25/1 once, then 26/0), flatladder_test 2/0, scenario_check 138/0, load_models_test 4/0,
  vr_run_check 12/0; the dev and the Release (ThinLTO) builds 0 warnings.

### For him (B14)

- In VR with vr_slide on, on a remote server: sliding should feel as it does alone (no snaps back while sliding, nor
  at its end or a jump out of it).

### B14b results: a lockstep server; the world's hashes equal across runs (2026-11-16, dev no-LTO build, loopback)

- **The lockstep** (harness only; vr_mptest.cpp, vr_netmove.cpp): a dedicated server with `vr_mp_lockstep N` steps its
  world only once N clients are in (active, spawned, a connection: bots and a listen host's own client don't count) and
  each one's next move by number has been read: `netmove::serverRead` keeps the moves without running them, and each
  stepping frame runs exactly one of each (`serverRunNext`, in SV_RunClients before his think; the first move: the
  oldest read since the slot's reset). The decision is at the frame's start (`VR_LockstepFrameBegin`, so moves read
  this frame are next frame's); a held frame runs no client think, no SV_Physics and no VR_ServerFrameEnd (only reads
  and sends). `vr_mp_lockstep_until S` holds the world after step S ("mp_lockstep: held step=S"): the harness's
  commands (hashes, cvars, spawns) go in at known steps. With `vr_fixed_frames 1` (a step 1/72 s) and
  `sv_random_seed 1`; `sys_ticrate 0.002` lets it run faster than real time. Its clients (`vr_mp_lockstep_ahead K`)
  make at most K moves the server has not acknowledged; a waiting frame (`VR_LockstepWaits`, at _Host_Frame's start)
  reads the inbox, runs the commands and reads the server's messages (cl.time held) and nothing else: no time passes,
  no move is made, so a command a waiting client runs lands on its next move, the same one every run. The harness
  waits for each client's newest move to be the server's last of his plus K (`vr_mp_lockstep_status`).
- **What differed once it ran**: only the poses' clock (`.vr_posetime`, the melee history's `mh_hdt`, `mh_ptime`): the
  headset's clock at a client's first move is the load's wall-clock time, and the swing speeds over its differences
  went apart by an ulp. Under the lockstep the client sends the connection's moves times the frame's time instead. The
  rigid bodies, the navmesh (with the obstacles' batches on vr_nav_job_frames) and the monsters' paths were the same
  from the first run (vr_hull_async 0: its trees' switch frame is the CPU's, B14's audit).
- **New commands**: `vr_nav_hash` (FNV-1a of each class's tiles' bytes: polygons, their flags as movers and props left
  them, links; and of the agents' paths: polygons, corners, link under way; Debug > Navigation: Print Navmesh Hash),
  `vr_mp_lockstep_status` (Debug > Reports: Network: Lockstep Status), `vr_mp_predict_tracelog <move>` (every trace
  of that move's run on the server and of its replays on the client, `%.9g`: where a replay leaves the server's
  result). `vr_bench_statehash fields` names what two runs differ in (the harness prints the fields).
- Test: `mp_test.py lockstep` (new, in the suite): two lockstep runs on e1m1 (A and B, the mock headset), held at steps
  36, 360 and 720: B walks north, A's impulse 241 makes a rottweiler (vr_test_spawn 7) that hunts them, A strafes:
  `vr_physics_hash` (52 bodies), `vr_bench_statehash` (301 edicts) and `vr_nav_hash` (135 tiles; 1 agent, 10 paths
  found) equal in both runs at each step; B's prediction misses the same in both (19/0, about 2 minutes).
- **B's one miss, explained** (`--tracelog 316`): the server's move 316 started from velocity (-0.4, 201, 24.6) in the
  air where his move 315 ended at rest on the ground: the rottweiler's bite (QuakeC's knockback, between his frames)
  pushed him into the north wall. Not a prediction error: knockback isn't predicted (the correction is the server's).
- **The step's ulp (B14, 4e-6 in z at e1m1's stairs): not reproduced.** Under the lockstep B's 700 walking moves (to the
  north wall) had no miss but the bite; that ulp came with the arrival-timed moves' varying lengths. Open: walk
  mp_predict's stairs under the lockstep (`--tracelog` on the move it names) when it shows again.
- **Bots grab and punch** (B11): not started (not small: a bot's hand targets, grip timing and the punch's swing in
  bot_setvrfields; B11 part 3's notes).
- **Client-reported fields** (open): a client's moves carry values it computes from its own drawing -- the ammo pouch
  (its body's pose, eased over its frames since the load), the light on him (`stl_light`: lightmaps and dynamic lights
  at its clock) -- and on a busy machine those differed between runs (in the suite: `ammopouchpos`, `stl_light`). The
  check compares every edict field but those (`LOCK_CLIENT_FIELDS`, listed when they differ). Once also the melee
  history (`mh_hdt`, `mh_hi`) from step 360 on, after the pose clock's fix had passed four runs: open (a hand's history
  fed by something else of the client's own; logging VR_Melee_Track's inputs is the next step).
- Checks: `mp_test.py suite` (36 scenarios) 34 pass: `mp_lockstep` 14/4 (the fields above, before the client-reported
  ones were set apart; then 15/2 alone: `mh_hdt` at steps 360 and 720) and `mp_dm` 14/1 (A's intermission not within
  30 s; again 14/1 alone: not yet checked against the build before B14b, whose suite passed it; with the lockstep's
  cvars 0 every hook returns at once). The lockstep alone 17/0, 17/0, 19/0 earlier. The dev and the Release (ThinLTO)
  builds 0 warnings. Single player checks (scenario_check, load_models_test, vr_run_check) not run this phase.


### B14c results: mp_dm's fraglimit, the lockstep's client fields, a lockstep deathmatch with bots (2026-11-16, dev no-LTO build)

- **Single player checks** (B14b skipped them): scenario_check PASS (138 scenes run, 0 failed), load_models_test 4/0
  (352/0), vrslide 15/0, flatslide PASS (its sections, 26/0 the last), flatladder 2/0, vr_run_check 9/0 (revolver +
  glass cannon on e1m1, saved; Melee Only on e1m2 `3 1 0 1 0`; the save loaded back).
- **mp_dm's intermission timeout: not a regression.** B14b's code is all behind the lockstep's cvars (0 there). The logs
  of every mp_dm run today (B14's included) show dm4's lava killing a FrikBot bot every 3-4 s ("turned into hot slag",
  "visits the Volcano God"), a frag off each time: the runs that passed reached the fraglimit within 13 s, the two that
  failed ran a minute (17 lava deaths) and the killer bot's frags never got to 3 by A's 5 deaths. No bisect build was
  needed: the same rate before and after 76e305078. The test now: A killed by a bot twice and respawned (as before),
  then A frags the bots (`vr_hurt 1000 <bot's slot> <A>`) until his frags reach the fraglimit: the intermission at both,
  the scoreboards equal. `vr_hurt` takes a bot's slot (FrikBot's edict of a slot with no client).
- **Open (not B14c's)**: the bots' lava deaths on dm4 themselves (FrikBot's walk through the players' moves,
  vr_pmove_bots, at the lava's edges: worth a look with B11's items); `vr_dm_test 3` (the bot's sword parried by A's
  upright crowbar) failed in 2 of 9 runs today: once parried 0 with damage, once neither read nor landed.
- **The lockstep's mh_hdt**: the poses' clock counted the client's own moves, but the first move the server runs is
  the oldest read since the slot's reset, a matter of timing; the clock's 10-microsecond steps then fell differently
  and the melee history's time steps went apart by one. The server now counts the clock from that first move, a frame a
  step (vr_netmove.cpp serverRunNext; the client's own count removed).
- **stl_light deterministic**: under the lockstep the client reads each move's light afresh with an empty lightmap
  cache (vr_stealth.cpp lightAt), not a 0.1 s sample at its clock since the load with the cached lightmap spot (both
  phase-dependent). No longer set apart in the check.
- **The ammo pouch: still set apart** (`LOCK_CLIENT_FIELDS`): its position follows the client's avatar (avatar::Follower,
  its smoothing over the client's frames since the load), which no move number fixes. Seen once: 1/64 unit in z
  (-61.42 against -61.41) at step 72 as B starts walking, equal again at 360 and 720.
- **New**: `vr_bench_statehash values <prefix> <ent>...` prints those edicts' fields' values (`%.9g`); the lockstep
  checks note the players' values of `ammopouch*`, `stl_light*`, `mh_*`, `vr_posetime` where two runs differ.
- **New test** `mp_test.py lockstep_dm` (in the suite, 37 scenarios now): two lockstep runs on dm4, A adds a FrikBot bot
  at step 36 and another at 72, B walks from 36 and stops at 360, A strafes from 360: the physics (27-31 bodies), the
  edicts' (4149 fields, 247 edicts) and the navmesh hashes equal at 36, 72, 360 and 720, the four players named and
  scored the same: 20/0. The bots' random() on the server's seeded stream and their waypoint navigation are the same.
- Checks: mp_lockstep 17/0 three times (stl_light compared from the second), mp_lockstep_dm 20/0, mp_dm 17/0 (16/1 once: vr_dm_test 3's parry, above).
  Dev and Release (ThinLTO) builds 0 warnings. Thrown props under the lockstep: not started.

### B14d results: the bots out of dm4's lava, vr_dm_test 3's flake (2026-11-16, dev no-LTO build)

- **The bots' lava deaths: vr_pmove_bots'.** Counted on a dedicated dm4 server with A and two bots
  (`scratch/b14d_lava.py`): 21 in 150 s walking through the players' moves, 0-2 per 45-150 s with FrikBot's own physics.
  Fixed three ways (ROUND25 has the details): botWalk's wall report to FrikBot measured along the wish (a turn read as
  a wall cleared frik_obstacles' ledge flag each frame); frik_obstacles reads lava/slime/sky below a drop past its 256
  units, on blind routes too (dm4's stairs beside the pit); botWalk holds a bot at the edge when its walk (not a jump)
  takes it off its floor over lava or slime. Now 3 in 120 s; mp_dm's run 0.
- **vr_dm_test 3**: the other bot's shots and A's pose step made the blow's outcome a matter of timing: the test holds
  every other bot's attacks off and the bot's spot follows A until he has stood still through the hold.
- **Not started** (budget): thrown props under the lockstep, the ammo pouch's determinism, the step's ulp on
  mp_predict's stairs, B11's open items.
- Checks: ROUND25's B14d section (the full MP suite and the SP checks).

### B14e results: a lockstep client's moves the same every run, thrown props under the lockstep (2026-11-16, dev no-LTO build)

- **Why mp_lockstep went 15/2 again** (mh_hdt, the hands' throw fields, stl_light at 360/720; no idle priority needed:
  it failed at below-normal priority with his game closed): not the server. The server now logs every move it runs
  (`vr_mp_lockstep_movelog 1`: "mp_movelog:" lines, each value as the world gets it, its pose clock the server's), and
  two runs' moves differed from the first: A's throw positions, ammo pouch, standing height and holsters at move 0 (the
  frames from his game's start to his first move, as many as the machine's speed made them, eased those states), and
  then in both players `time`/`viewBack` (cl.mtime), the origin and every place relative to it, the light, the drawn
  hands: what a client had read when it made move N depended on timing -- a datagram of the step after (read in the
  same frame after its move), a held server frame's repeats (each read moves `cl.mtime` and the lerp), the reliables
  sent when the last one's acknowledgement came.
- **The fix (test-only paths, behind vr_mp_lockstep / vr_mp_lockstep_ahead)**: a lockstep client's frames wait (no
  time passes) until it is in the game (signon replies still sent: `CL_ReadLockstep`, `Host_SendPrespawn`); a waiting
  frame reads the server's messages up to the one that lets its next move go and no further (`VR_LockstepReadsMore`),
  nothing relinked; a moving frame reads none (`VR_LockstepReadHeld` in CL_ReadFromServer). The server sends a lockstep
  player one datagram when he comes in and then one a step (`VR_LockstepSkipsDatagram` in SV_SendClientMessages), and
  steps only with every player's reliable messages acknowledged (a step's reliables go right after its datagram, read
  before the next step's). Result: every value of every move the same in both runs (1440 moves), B's prediction exact
  719/0 in both (the rottweiler's bite no longer a miss), every edict field compared: the ammo pouch, the light and the
  melee history included (`LOCK_CLIENT_FIELDS` gone). The ammo pouch's determinism (B14c's open item) is this.
- **New test** `mp_test.py lockstep_throw` (in the suite): two lockstep runs on e1m1, held at 36, 72, 144, 216, 432:
  B grips his main hand low and is given a shotgun into it (impulse 154), A his off hand and an axe (impulse 172);
  both swing the hand up and let go in one move (the mock's hand that fast: a hard throw); the props become rigid
  bodies (52 -> 54), fly and land: held before the throw and not after (the server's avatars), every hash equal at
  each step, every move the same, the physics hash new at each step from the throw (29/0). `lockstep` and
  `lockstep_dm` check every move too ("slot=N: every move the same in both runs"); `lockstep_dm` was missing from the
  suite's list: added.
- **The step's ulp on mp_predict's stairs: not seen again** (mp_predict 29/0: walking 340 of 340 equal, jumping 324,
  crouched 217, crawling 357, swimming 215; the lift's ride 9 of 178 off, its mover's clock, as before). Under the
  lockstep the walks have no miss at all. Left open (B14's one run).
- Checks: ROUND25's B14e section.

### B11 part 4 results: bots punch and grab as players do (2026-11-16, dev no-LTO build, loopback; `mp_dm` 20/0)

- **One code path, the AI only deciding.** A bot's off hand is now a player's: FrikBot's AI picks what it does
  (`bot_offhand_ai`, frikbot/bot_fight.qc, from BotAI), and `bot_setvrfields` moves the hand, sets its grip and its
  velocity once a frame; everything after that is the players' code.
- **The fist punches through the melee's sweep.** An enemy within 60 units (origin to origin) and the off hand empty:
  the fist is drawn back 4 units (0.08 s), thrust at 6 m/s at his middle, 10 units deep (at most 36 units from its
  chest), held 0.06 s, back in 0.15 s, every 0.5 s. `VR_Melee_Track` and `PlayerVRMeleeImpl` take hands that move
  themselves, `VR_HandLinesKept` (tracked, or a bot's) where they took `VRHandsTracked`. A bot's poses have no clock of
  their own (`.vr_posetime` -1, so the melee reads them over the server's frames). The blow is a player's punch: the
  sweep, `W_FistMelee`, PvP's parry and damage.
- **The button swing keeps its traced blow.** A bot's sword or axe swing (B11 part 3, part 3) stays W_FireAxe's trace,
  read by the directional parry. Its main hand's sweep strikes nothing during the swing (`VR_HandButtonSwing`,
  vr_util.qc), so there is one blow, not two. Tried first: the swing's blow from the sweep alone. It landed 20 but the
  directional parry did not read it (a sweep's blow on a player has no PvP line yet; mp_melee's swords are the same).
- **Grabs through the hand's touch and grip.** The bot's walk runs SV_Physics_Client's premove (`VR_ClientPreMove` in
  `predict::botWalk`: the hands' and weapons' touches, the hands in water) as a client's frame does. Its grips' presses
  and let-gos go through `VR_HandGrabUtil_UpdateState` (the `ishuman` return is gone: a bot's grip bits are its AI's,
  the previous frame's kept as Prev). Its hands' `.handvel`/`.offhandvel` are measured from their places against its
  origin, as a client's controllers send theirs (the grab's speed and pull read them).
  - **Things lying near:** a dropped gun (`wpnthrow_handtouch`; not a melee weapon, and not the one its own hand just
    let go of) or a backpack, within 48 units of its chest. The open hand goes there at 1 m/s and closes once it
    arrives (open at least 0.15 s: the press counts 0.125 s after a let-go). The hand's touch takes it, exactly as a
    player's grab. A gun in its off hand fires with its main trigger (`button3`) and is let go of once spent.
  - **The disarm** (`vr_player_disarm_pvp` and `_grab`): the enemy is a player with a weapon in hand and within reach.
    The hand closes on the weapon's drawn line (its middle, the line `VR_PDG_Gap` measures). The bot stands still while
    it takes hold (keys 0: the grab's speed counts both bodies), then pulls the hand away from the player's chest at
    2.5 m/s. `VR_PDG_Hold` takes the weapon into its hand. A reach given up waits 1 s.
- **Tests** (`vr_dm_test 4-6`, vr_dmrules_test.qc, run by `mp_dm`; Debug > Tests "Deathmatch: A Bot Punches You",
  "... Takes a Gun by Hand", "... Grabs Your Gun"):
  - 4, the punch: the bot is held 40 units in front of A, PvP off. Its AI punches him and its off hand's blow lands
    through the sweep.
  - 5, the grab: a nailgun is laid 20 units in front of the bot. Its hand takes it ("weapon: Nailgun taken into the
    off hand"), and the gun is gone from the floor.
  - 6, the disarm: PvP on, A's shotgun in his gripping hand. "pdisarm: <bot>'s hand 0 takes hold of player's weapon
    (hand 1), 0.0 units off it, at 0.00 m/s", then the shotgun is in the bot's off hand and A's hand is empty.
- **Harness:** `frag_table` now reads an `mp_frags:` line that an obituary printed without its newline runs into. A bot
  that reached the fraglimit itself read as 1 frag. The knock-out check takes the line at A or B: the test's human is
  the first one alive, which is B when a bot's shot has just killed A.
- **Checks:**
  - The MP suite: 36 of 38 scenarios clean. mp_dm 20/0, mp_lockstep_dm 22/0 (every hash and move equal in both runs,
    with the bots' new hands), mp_lockstep_throw 29/0, mp_melee 29/0, mp_disarm 35/0.
  - The two failures, both seen in earlier phases: mp_climb 10/1 (a mantle) passed 11/0 alone on the second try;
    mp_lockstep 18/1 (8 of slot 0's 720 moves differed) passed 19/0 alone. Neither has a bot in it, and a human's
    code path is unchanged.
  - SP: Melee Only bots (`vr_pickup_test 15`) 13/0, scenario_check PASS (138/0), load_models 4/0, vr_run_check 9/0.
  - Dev and Release (ThinLTO) builds: 0 warnings.
- **Not done:** a gun the bot's off hand took is not in its inventory (FrikBot picks its main weapon from its items):
  it fires it alongside, then lets it go. A running bot's disarm still stops it to take hold. Bots punch only with an
  empty off hand (the main hand holds its weapon).

### B13 results: the server browser, the net HUD, the soak with 16 players (2026-11-16, dev no-LTO build, loopback)

**Server browser** (`vr_serverbrowser.cpp`; menu.c's server list, Multiplayer > Join Game > Server browser, or
`menu_servers`): one list of the favourite servers (`vr_mp_favourites`, a star), the recent ones (`vr_mp_recent`, the
last 8 joined, newest first: any join, from the browser, the Join Game address or `connect`) and the LAN's. A search
broadcasts NetQuake's server-info request and asks each favourite and recent server by its address too (net_dgrm.c),
so a server on the internet answers as a LAN one. A row: name, map, players, ping (the reply's round trip; the search
now reads every frame, not every 0.1 s); a listed server that did not answer shows its address and "no reply". Enter (A,
a click) joins; Y or F (or a click on the star) makes it a favourite or not; Del forgets a recent one; X, Space or the
last row searches again. Replies are listed at the address they came from (NetQuake: the address the server wrote in
its reply, its LAN's behind a router). A server on this PC is listed (NetQuake dropped any reply from its own IP; now
only this game's own server's). Commands: `vr_mp_browse [join|favourite|forget <row>]` (`mp_browser:` rows when the
search ends), `vr_mp_favourite [<address>]`.

**Net HUD** (`vr_nethud.cpp`, `vr_net_hud` 0; Debug > Reports > Network: Net HUD): joined to a remote server, the top
right (under the clock and the frame rate) shows `ping` (his moves' acknowledgement round trip: the way there, the
server frame that ran the move, the datagram back; a server without Quake VR's moves: the reliable chunks'), `loss`
(unreliable datagrams whose sequence was skipped, of all expected in the last second) and `fix` (his walk's prediction
corrected by half a unit or more, a second); 2 adds the bytes each way and the reliable resends. `vr_net_hud_report
[<n>]` prints the values (after n whole windows).

**The CPU of many players** (`mp_soak`: a dedicated deathmatch server on dm3, A, B and 14 FrikBot bots, a window of 10 s
of the server's game time; `vr_profile` now runs on a dedicated server too; `vr_mp_edicts` counts edicts by class and
Box3D's bodies):

| Found | Fix |
|---|---|
| **Telefrags of the dead.** Quake VR's corpses, dead players and gibs are SOLID_CORPSE (stock: SOLID_NOT), so a spawn's `teledeath` touched them: each respawn at a start "telefragged" the dead players lying there again (an obituary, a frag, their weapons dropped again) and struck every gib in reach 50000 each frame. With 16 players on 6 starts: 933 telefrags in 3 s of game time, the edicts 1067 to 7047, then `ED_Alloc: no free edicts` (the server crashed) | `tdeath_touch` strikes only the living (`health > 0`). Also single player's (a teleporter's destination with gibs or corpses): its own commit |
| **Starts taken.** A start counted as free with no "player" within 32 units, the dead included; one 33-49 units away was still in the spawn's telefrag box | A start is taken by a living player within 64 units; a respawn with every start taken waits up to 2 s for one to clear (`VR_DMSpotClear`), then spawns as stock Quake did |

Results after the fixes ((`mp_soak` 8/0, dev no-LTO and ThinLTO builds): 16 players listed at both clients, no crash, the edicts ~800 -> ~1200-1750 over the window (were 7047 in 3 s, then the crash); telefrags 933 in 3 s -> 116-206 in 10 s. The server's pace: 0.74 of real time (dev, the best run), 0.18 in the final ThinLTO run (956 bodies, 792 awake: 413 gibs and 330 props awake; the server process at 0.62 of a core) -- the soak's state varies run to run with the bots' fights. MP suite: 39 of 40 scenarios clean, mp_browser 10/0 and mp_nethud 8/0 new; mp_climb 10/1 (its mantle, seen in earlier phases) passed 11/0 alone. SP: scenario_check PASS (138/0), load_models 4/0, vr_run_check 12/0. Dev and Release (ThinLTO) builds: 0 warnings.)

Open items (B13b):

- **The server's pace with 16 players**: a dev build ran 10 s of game time in ~13.6 s of wall clock (0.74 of real
  time). Its frame: Box3D ~61% (~700 bodies, ~550 awake: ~290 gibs and ~190 props awake, the dead bodies' gibs and the
  dropped weapons of every death), QuakeC ~27% (`VR_Gib_Touch`, `forcegrabbable_touch`, `VR_Melee_SweepNow`,
  `VR_Prop_Flung`, `VR_Gib_TraceHeld`: the gibs and props again). Next: deathmatch caps on loose gibs and dropped
  weapons (the oldest put to sleep or removed), the gibs' touch and trace only while moving; then measure an LTO build
  with `--exclusive`.
- **B's link with 16 players**: 40-65 KB/s down (snapshot entities ~28 KB/s, sounds ~7 KB/s, the rest ~21 KB/s:
  NetQuake's broadcast datagram, particles and sounds to everyone, not culled by sight). `vr_net_rate` (0: no cap)
  limits it per player; the default cap and culling the broadcast are B13b's.
- **Living telefrags remain** (~100 in 10 s): bots stand at the starts; the 2 s wait then spawns over them.
- **LAN discovery** was not tested by the harness (its games are bound to 127.0.0.1: no broadcast reaches them):
  the broadcast path is NetQuake's, unchanged but for the own-address filter. His check: a server on another PC.

**ICE/NAT traversal (a written design only; no third-party code):** a player behind a home router cannot host without
forwarding a port. The plan: (1) a tiny rendezvous service (any host both can reach; ours or the player's own) that
keeps each hosting server's public address as seen from outside (its UDP source address and port) under a short code;
(2) the joining client asks it for the code, the service tells both sides each other's public address; (3) both send
UDP probes to each other at once (hole punching: each router opens a mapping for the outgoing probe, so the other's
probe gets in), over the game's own port and socket (net_dgrm.c's control socket), a CCREQ-style "punch" packet
ignored by servers that do not know it; (4) on the first probe through, the client sends its connect request to that
address as today. Routers with symmetric NAT (a new mapping per destination) defeat it: then a relay (the service
forwards the datagrams, a bandwidth cost) or the port forward. IPv6 needs no punching where both have it. The browser
would list such servers by code with the favourites. Needs his OK for hosting a service.

### B13b results: the CPU and the link of 16 players (2026-11-16, dev no-LTO build, loopback; `mp_soak` 14/0)

`mp_soak` (dm3, A, B through a proxy, 14 bots; 10 s of the server's game time), before -> after (both in low-impact
runs, his game open):

| | B13 | B13b |
|---|---|---|
| the server's pace | 0.18-0.74 of real time (0.38 at the start of this phase) | 0.99 (it keeps real time) |
| its frame | Box3D 130 ms, QuakeC 50 ms (VR_Gib_Touch 12%, forcegrabbable_touch 12%, VR_Gib_TraceHeld 4%) | Box3D 1-2 ms, QuakeC ~4 ms (the melee's sweep first) |
| Box3D at the window's end | 1046 bodies, 856 awake (451 gibs, 345 props) | 274-311 bodies, 125-131 awake (14-20 gibs, 28-45 props) |
| edicts | 1731 (667 classless: gibs; 274 weapons, 308 records) | 576-613 (~30 loose gibs, 21-32 weapons) |
| telefrags in the window | ~100-200 of the living | 0 |
| B's link down | 37 KB/s (35 datagrams/s at the slow pace) | 24 KB/s (77 a second at real pace); 72% of the broadcast's known bytes culled |

- **Deathmatch caps** (`vr_dmloose.qc`, StartFrame): Most Loose Gibs `vr_dm_gibs_max` 64 and Most Dropped Weapons
  `vr_dm_weapons_max` 32 (0: no cap; Debug > Network rows): past them the oldest go first (a gib by `.vr_gib_made`, a
  weapon by its deathmatch removal time), never one held, skewered or being force-pulled; small gibs and limbs keep
  their own caps. Deathmatch only: single player and coop as they were (`vr_dm_test 7` checks both). `.vr_gib` is now in
  the engine's edict index (`findflags`), so the cap walks the gibs only, and `VR_Gib_TraceHeld` (a pellet's skewered
  gib) walks them instead of every edict.
- **The gibs' touch only while it can matter** (vr_physics.cpp `gibTouchIsNothing`): a rigid gib's own touch is no longer
  called when it can do nothing -- not thrown, past its grace with what it was made in (`vr_gib_spawn_harmless`, the
  born-in chain), slower than any flung prop hurts (and its 220 u/s splat), the toucher not a BSP mover. The player's hand
  touch still goes. Behaviour-preserving in single player too: `vr_prop_touch_verify 1` runs each skipped touch and
  checks no entity, global or message changed (1251 checked on e1m1 with gibs and a walk, none changed; the gib props
  table 3/3 monsters with it on). Its "trace": the held-gib walk above.
- **The broadcast culled by sight** (`vr_net_cullbroadcast` 1, vr_server.cpp `VR_WriteBroadcast`): a remote client gets
  each known message of the unreliable broadcast only if its point is in his fat PVS (particles, temp entities, Quake
  VR's particles and tracers: either end) or, a sound (and a gore sound), nearer than its falloff reaches at half
  `vr_snd_falloff` (2000 units at ATTN_NORM; ATTN_NONE always). Messages not known, and what follows one to the next
  boundary, go to everyone as before; the loopback client gets all of it (single player and a listen host's own view
  unchanged). `vr_net_broadcast_report [reset]` prints the bytes sent, culled and not known.
- **A default rate cap**: `vr_net_rate` 80000 B/s (was 0, no cap). 40000 was tried first: the server sends a remote
  client a datagram every one of its 72 frames, so even dm4 with two bots reached it, his records waited and
  `vr_dm_test 3`'s parry failed 2 runs of 3 (the lag compensation read an older place); with 0 it passed, with 80000
  it passed alone and failed once in the suite (its known flake, B14d). The rate slider now goes to 160000.
- **Respawns beside a taken start** (client.qc `VR_DMBesideSpot`): every start with a living player at it (deathmatch,
  not CTF's team starts), the respawn takes a place 48-144 units beside one (8 ways, the start's facing first): the
  player's box clear of the level all the way from the start and of solid things there, a floor within 40 units under
  it, no liquid, no living player within 64 units. Only when there is none does it wait (up to 2 s) and then telefrag as
  stock Quake did. `vr_dm_test 8`: the human on the only start counted, the bot respawned 96 units beside it, nobody
  telefragged.
- **mp_soak's checks** (14): 16 players at both clients, edicts under 2048, loose gibs under the cap (+32 for a frame's
  deaths) and weapons (+16), frags scored, B's link under 45 KB/s, the server's pace at least 0.8 of real time, the
  broadcast culled, at most 10 telefrags in the window, nobody left, clean quits. ThinLTO run (not low impact): 14/0,
  0.99 of real time, the server process at 0.32 of a core, Box3D 1.05 ms and QuakeC 3.6 ms a frame, 238 bodies (90
  awake), B 33 KB/s, 63% of the broadcast's known bytes culled, 0 telefrags.
- **mp_dm** 22/0 with `vr_dm_test 7` and 8.
- **Checks:** MP suite (ThinLTO): 36 of 40 scenarios clean; mp_avatar 15/1 (B's draw of A's weapon), mp_disarm 34/1
  (the toss's take), mp_dm 21/1 (`vr_dm_test 3`) each passed alone right after (17/0, 35/0, 22/0); mp_climb 10/1 (its
  known mantle flake). SP: scenario_check PASS (138/0), load_models 4/0, vr_run_check 12/0, the gib props table with
  `vr_prop_touch_verify 1`. Dev and ThinLTO builds: 0 warnings.

Open items:
- **The rate's pressure on the lag compensation**: under a cap a client's records wait, and the lag compensation judges
  his parry at a place he had not been sent yet (`vr_dm_test 3` at 40 KB/s). Its rewind should use the places he was
  actually sent (the acknowledged snapshot's), not the frame's.
- The server still sends every remote client a datagram each of its 72 frames (~40 KB/s for a small game): a send
  rate of its own (36 Hz with the interpolation buffer) would halve it.
- An LTO soak with `--exclusive` for the pace's headroom (this phase's runs were all low-impact).

### B13c results: the rewind at the places sent, a send rate, the exclusive soak (2026-11-16, dev no-LTO and ThinLTO)

- **The lag compensation rewinds to the places a client was sent** (vr_lagcomp.cpp `sentAround`/`drawnAt`): each remote
  client's snapshots are logged (vr_netsnap.cpp `logSent`, 80 of them, 1.1 s: their time and the players' and
  monsters' entity and avatar records, each whether it was written there, new or changed: encode marks `sent`'s records
  `fresh`). A rewind for his shot, blow or parry places each target as his interpolation buffer drew it: between the
  two places he was sent around his view time (not the server's frames between), held past the newest or before the
  oldest, rested until a frame before a place that came after a long wait (interpNote's rest gap), no lerp across a
  teleport; the hands from the avatar's records, which wait on their own. A record that waited under his rate
  (`vr_net_rate` 80000, or a gib storm) is then judged where his game still drew it. NetQuake's datagrams (no
  snapshots): the frames, as before.
- `vr_net_test_hold <n>` (Debug > Network: Hold Players' Updates, a test aid): the other players' updates to a remote
  client wait n snapshots in each n + 1, exactly as the rate's waits leave them. `mp_lagcomp` (14 checks) grew: B's test
  shot sends where he drew A; the server reports how far from that the rewind put A (`err`) and how far the server's
  frame at his view time is (`err_frame`, the rewind before B13c). Free: 0.03 units on average; A's updates held 3 and 6
  snapshots: 0.04 (the frames' rewind 4.1-6.7 units off on average, up to 23).
- **A send rate** (`vr_net_sendhz` 72; Debug > Network: Send Rate): a remote snapshot client's datagrams a second; off
  his beat the frame's broadcast (culled by his sight) is held for his next datagram (vr_server.cpp `VR_HoldBroadcast`;
  `vr_net_broadcast_report` counts held and dropped); his state, entities and voice go as they are then. Measured, 72 ->
  36 Hz: B idle on e1m1 3.2 -> 2.0 KB/s; mp_lagcomp (150 ms lag) 7.5 -> 5.2 KB/s; mp_nethud walking 6.0 -> 3.6 KB/s;
  mp_soak (16 players) B 27.7 -> 14.6 KB/s, A 33.4 -> 21.4; hits and blows unchanged (mp_lagcomp 14/0 at 36 Hz, `err`
  0.04), no prediction corrections either way; the cost: the others drawn about a frame later (his buffer's delay
  follows the longest gap between datagrams, 2 frames; 25.6 ms at 72 Hz). **Default kept at 72**: not clearly better --
  the rate cap already holds a link, and the others' latency is seen in VR; 36 is the setting for a host with many
  players or a thin uplink. (mp_nethud's loss check at 36 Hz can read 0% in its one-second window: fewer datagrams.)
- **The ThinLTO soak, exclusive** (`mp_soak`, 16 players on dm3): 14/0 at 72 and at 36 Hz, the server 0.99 of real time
  both, Box3D 241 bodies (127 awake) at the window's end.
- **B13b's suite flakes**: `mp_avatar`'s "B draws A's weapon" was the test (B's report could describe the frame drawn
  before A's avatar with the weapon came): it now waits for the drawn weapon (17/0 in the suite). `mp_disarm`'s toss
  retake: the toss's landing (real-time mock play) varies, and on some landings A's hand pushes the shotgun along the
  floor 1.5 cm short of the fist's grab surface ("against this hand", allowed 0 cm) -- a grab quirk a real hand may meet
  too (a weapon lying against the hand not taken): open (35/0 in this suite). `mp_dm`'s `vr_dm_test 3` (the bot's blow
  parried) failed once in the suite again with B13c's rewind (22/0 alone): B14d's open flake, not the rate's waits.
- **Checks:** MP suite (dev): 37 of 41 clean; mp_ground (its reverse grip cosine), mp_dm (`vr_dm_test 3`), mp_browser
  (a ping of 698 ms right after the soak) passed alone (15/0, 22/0, 10/0); **mp_flow fails, alone too**: "Load Autosave
  lists the coop autosave" -- the server's coop~e1m2.sav is written (608 KB) but A's `vr_autosave_list cold` lists only
  the older coop~e1m2@2 and @3 (vr_continue.cpp's `inspectAutosave` rejects the newest; no save code changed in B13c:
  from origin/next or a save still being written when read -- open). SP: scenario_check PASS (138/0), load_models 4/0,
  vr_run_check 12/0. Dev and ThinLTO builds 0 warnings.

### B13d results: B13c's open items (2026-11-16, dev no-LTO)

- **mp_flow's coop autosave listing: the harness, not a regression.** Single player on a clean origin/next (f6fbfdcaf):
  `vr_autosave_now`, then `vr_autosave_list cold` and Continue list the new autosave/e1m1 at once; a list read in the
  same frame as the save's write misses it until the next refresh (as ever: nothing cached). The cause: the harness took
  the server's coop~e1m2.sav as whole at its last edict's brace (the save thread flushes each edict), before its edict
  count and its close; A's list in that window skipped it (`Sys_GetFileTime` opens with `FILE_SHARE_READ`: a file open
  for writing is not listed). Now it waits for the `// N edicts` trailer and lists again (up to 5) while the file may
  still be open: 4 runs of 4 clean (it failed 1 run in 4 before). `vr_autosave_list cold` now names each save passed
  over and why (`inspectRefusal`: unreadable, version, format, map, campaign, package).
- **A hand against a lying weapon takes it, for every player** (vr_physics.cpp `weaponAgainstHand`): it was entity 1's
  alone -- on a dedicated server the first client's grip took a gun his hand pushed along the floor ("against this
  hand"), a later client's pushed it away untaken (mp_disarm failed 1 run in 2 with A entity 2, 0 in 8 with A entity
  1). Where it is carried stays his view's choice (`heldpose::grabSpot`). Single player unchanged (the player is entity
  1; `vr_pickup_test 17` 5/0). mp_disarm: A joins second (`join(a_second=True)`: entity 2, every run), the toss retaken
  by its handle (`spot -1`: gun mode with vr_weapon_grab_handle_leniency; its forend's hotspot could lie under the gun,
  3.8 cm out of the lowest fist's reach, and hotspots get no slack -- his "no extra reach"): 35/0 twice.
- **The suite's flakes:** `mp_dm`'s `vr_dm_test 3` (B14d's): the held bot's *off hand* punched A while its sword's blow
  was parried, the punch's damage read as the blow's landing; tests 2 and 3 now hold the bots' off hands too
  (`VR_DMT_HoldOffHands`: no punch, no reach) -- 22/0 twice. `mp_ground`: the reverse grip cosines (server -0.6, B
  -0.84, the same hand angles) are compared once they agree within 0.2 (the sword as B draws it catching up) -- 15/0.
  `mp_browser`: the least ping of up to three searches (698 ms once after the soak: a hitch, not the round trip) -- 10/0.
- **Checks:** MP suite (dev) 39 of 40 clean: mp_flow 16/0, mp_disarm 35/0, mp_dm 22/0, mp_ground 15/0, mp_browser 10/0 in it; mp_armed failed a rate-cap check under `vr_net_rate 6000` (a snapshot in every datagram: 318 of 360), alone 18/0 then 17/1 on another of its rate checks (nearest poses 1.30x the farthest's, wanted 1.35): its real-time rate checks under a busy PC, not B13d's code -- open. SP: scenario_check PASS (138/0), load_models 4/0, vr_run_check 12/0, autosavenow 4/0, vr_pickup_test 17 5/0. Dev and ThinLTO builds 0 warnings.

Open items: mp_armed's rate-cap checks under `vr_net_rate 6000` flake (two different checks, a busy PC); LAN discovery
and ICE/NAT need his machines and his OK.

(B13d's next-phase line named B11 part 2: already done, in B11 parts 1-4.)

### B15 results: in and out of a coop game in progress (2026-11-17, dev no-LTO build, loopback; `mp_rejoin` 15/0)

**Why this phase:** with B2-B14 done, what a first public beta meets first and no test covered was players coming and
going mid-game: a friend joining a game already under way, a player whose game crashes or whose link drops coming
back. NetQuake handles both badly in coop: a returning player starts over with a shotgun and 25 shells, and a crashed
game's slot stays a ghost for `net_messagetimeout` (300 s) while he comes back as another player beside it.

- **A late joiner's world** (`vr_mp_world`, a client report: each entity of the last message after the players' --
  model, frame, skin, place -- then the model and sound precaches, static entities and a hash of the light styles):
  after A's shotgun was let fall on e1m1's floor and B was given weapons and hurt, C joins and has every entity at rest
  in B's view as B has it (the shotgun among them, within a unit), and the same precaches, statics and light styles.
  No code change was needed: VR_WriteClientSpawnState already sends the world texts, tips, rules, armour, x-ray and
  avatar calibrations, and the snapshots start whole.
- **A leaver's things kept for his return** (vr_coopsave.cpp `VR_CoopNoteLeave`/`VR_CoopRejoinParms`/
  `VR_CoopRejoinSpawn`; `vr_mp_rejoin_keep` 1, Debug > Network: Keep a Leaver's Things): a spawned player leaving a coop
  (non-deathmatch) game in progress -- `disconnect`, a crash, a timeout -- has QC's SetChangeParms run on him (what a
  level change carries: weapons and their magazines, ammo, health clamped to 50-100, armour; keys and powerups dropped)
  and his level-start parms kept, by name. A player of that name connecting again gets his old slot if free (his entity
  number and colours), spawns with what he had, and a respawn gives his level-start things (`setspawnparms`), not those.
  Kept through level changes, forgotten when the server shuts down (a new game, a load). A listen host's own client
  and single player never take this path (`svs.maxclients > 1`, `sv.active`: a shutdown's drops are not leaves).
- **A crashed game's ghost** (net_dgrm.c): a connect request from a known address on another port (the same machine,
  or the same NAT) whose name is that connection's, while that connection has been silent over 3 s
  (`QVR_GHOST_SILENCE`: a game in play sends a move every frame), is him back: the old connection is closed as
  NetQuake closed a same-port one, his retry 2.5 s later joins (and gets his things back). Two players behind one
  router with the same name are told apart while both play (neither is silent). Without a name or within 3 s: as before.
- **The server's quit** reaches its clients: C sees it ("mp_event: left"), is out of the game, his game still running.
- `mp_rejoin` (15 checks): the late joiner's world (3), B's leave and return (his slot, weapons, nails, health 70;
  shells not fewer) and his suicide's level-start things, B's crash (killed behind his proxy: no port unreachable reaches
  the server, as over a NAT) and return on another port (the ghost closed in the "back from" line, his slot and things,
  no ghost in the list), the server's quit.
- **Open:** shells after a return can be up to a magazine more than before (17 -> 25: a fresh spawn's shotgun magazine,
  filled from the reserve, seems counted back into it by the round trip through the parms; nails, rockets, cells and
  weapons exact): not chased, a level change may do the same -- check with a changelevel. On loopback a crashed client
  is dropped at once anyway (Windows answers the dead port: the server's read fails); the ghost path is for NATs and
  other machines.
- **Checks:** MP suite (dev) 39 of 41 clean: mp_rejoin 15/0 in it; mp_armed 17/1 (its rate-cap pose ratio under
  `vr_net_rate 6000`, B13d's open flake) and mp_climb 10/1 (A's mantle: no hold taken by his play, B8j's flake), then
  alone 18/0 and 11/0. SP: scenario_check PASS (136/0), load_models 4/0, vr_run_check 12/0, autosavenow 4/0. Dev and
  ThinLTO builds 0 warnings.

**What a first public multiplayer beta still needs** (in order): (1) his VR checks in a headset of the Multiplayer
menus' host and join flow, the server browser and voice chat (B10v), never seen by him; (2) a player-facing guide (how
to host: UDP port 26000 forwarded, `maxplayers`, coop or deathmatch, `vr_mp_download`, what is single-player only) and a
"multiplayer beta" section in the release notes with the known issues; (3) a client that notices a vanished server:
`net_messagetimeout` is 300 s, so a crashed or unplugged server freezes its clients' worlds for 5 minutes -- a "server
not responding" notice after a few seconds and a shorter client timeout; (4) two machines over the internet (his):
latency, NAT; the ICE/NAT rendezvous needs his OK; (5) mp_armed's rate-cap flake.

Next phase: B16 -- the client's side of a vanished server (a "not responding" notice in the net HUD and the view after
3 s of silence, a client timeout of its own well under net_messagetimeout's 300 s, back to the menu with the reason;
`mp_rejoin` grown: the server killed, C told within the bound), then the player-facing multiplayer guide and the
release notes' beta section; the shells open item above. B10v's VR checks and the ICE/NAT rendezvous service are his.

### B16 results: a vanished server at its client; mp_armed's rate flake; the players' guide (2026-11-17, dev no-LTO build, loopback; `mp_vanish` 8/0)

- **A vanished server** (vr_nethud.cpp `silenceCheck`, cl_main.c `CL_CheckServerSilence`, net_main.c
  `NET_QvrSilence`): after each frame's reads, a client joined to a remote server measures the server's silence as the
  lesser of the game time (`realtime - cl.last_received_message`) and the wall clock since the connection's last
  message, so neither a hitch of the client's own (a level's load counts once) nor a fast-mode test's frames time it
  out early. After 3 s in the game: the console ("Server not responding (3 s): waiting up to 30 s"), a centre print,
  and a first line at the top right "SERVER NOT RESPONDING n s" whether or not the net HUD is on; it goes when the
  server is heard again ("Server responding again"). At `vr_mp_client_timeout` (new, 30 s, archived; Debug > Reports >
  Network: Server Timeout; 0: NetQuake's net_messagetimeout alone, 300 s) the client leaves the game (Host_EndGame:
  `clc_disconnect` sent, the connection closed) with the server browser open, "Server stopped responding" under its
  list (menu.c `M_ServerLost`: m_return_reason) and the reason in the console. While joining (`cls.signon < SIGNONS`:
  the server may be loading its level; it sends a nop every 5 s meanwhile) the bound is three times as long and there
  is no notice. A listen host's own client, single player and demos never take the path (a loopback connection:
  `NET_QvrSilence` 0, and `remote()` needs `!sv.active`).
- `mp_vanish` (new, 8 checks): A hosts a listen server, B joins through the proxy, both `vr_mp_client_timeout 8`; the
  proxy's new `blackout` cuts B's link down: B's notice after 3.1 s (console, the HUD's line, its report's
  `notice_line`), the link back and B plays on, A untouched (no notice, remote 0, still in); A's game killed: B's
  notice after 2.7 s, out at 7.7 s (from the kill's return) with the reason, the server browser open (`vr_mp_status`'s
  new `menu=` field), his game running. Its waits are on B's console lines; the silence is real time by design.
- **mp_armed's rate flake** -- the test's, not the code's: (1) "a snapshot in every datagram" counted the proxy's
  packets down, reliable chunks included (a fight's many: 318 of 360); the proxy now counts the unreliable datagrams
  (`NETFLAG_UNRELIABLE`, `window()`'s `datagrams`) and the check is snapshots sent >= datagrams (357 datagrams of 360
  packets, 371 snapshots); (2) "the nearest poses more often" ordered the humanoids by distance alone, while
  `weightOf` doubles a record within 60 degrees of the player's view: the failing run's 0.72 was a grunt at 145 units
  beside or behind B (0.64) against one ahead at 279 (0.96). The client now reports each humanoid's mean weight as it
  read its poses (`mp_netprio:` `weight=`, from its own view), and the check orders them by it (highest two 1.68x the
  lowest two, wanted 1.37, weights 1.74); weights within 1.15 of each other aren't told apart.
- **Docs:** docs/vr-port/MULTIPLAYER.md is now the players' guide (hosting and UDP 26000, joining and the server
  browser, coop, deathmatch and bots, voice chat, single-player only, when something goes wrong, known issues); the
  server/client design it held moved to MP_DESIGN.md (references updated, the round notes left as history).
  PATCH_NOTES_1.1.1.md (new) has a self-contained "Multiplayer beta" section.
- **Checks:** MP suite (dev) 38 of 42 clean: mp_vanish 8/0 and mp_armed 18/0 in it (and alone 18/0); mp_ground 12/3
  (B's grip hotspot on the dropped shotgun: -1), mp_rejoin 14/1 (A's shotgun never let fall: its grip's setup, "entity
  0"), mp_disarm 34/1 (the pouch's shell) and mp_dm 20/2 (vr_dm_test 3's parry, 6's fist 14.5 cm off): the known hand
  placement flakes (B10b, B8j, B14d), none in this phase's code; alone mp_ground 15/0, mp_rejoin 15/0, mp_dm 22/0,
  mp_disarm 34/1 (another check: the nail magazine's port) then 35/0. SP: scenario_check PASS (136/0), load_models
  4/0, vr_run_check 12/0, autosavenow 4/0. Dev and ThinLTO builds 0 warnings.
- **Open:** the shells after a return (B15's); the notice is a 2D canvas line and a centre print (in VR the centre
  print may go to the wrist hologram): his look in a headset; LAN discovery and the ICE/NAT rendezvous need his
  machines and his OK.

Next phase: B17 -- B15's shells item (a level change's parms with a shotgun magazine: counted twice?), a dedicated
server's comforts (a server.cfg example in the guide, `status`'s pings), then the ICE/NAT rendezvous as a design for
his OK (no service run). B10v's VR checks, the guide's read-through and internet play with two machines are his.

### B17 results: the suite clean; a return without a level change's floors; dedicated servers' comforts; a NAT proposal (2026-11-17, dev no-LTO build, loopback; the full MP suite 3 runs of 3 clean)

- **The suite's hand-placement flakes had no one shared cause in the code: they were the harness acting on state the
  load delayed**, each a different race, plus one fragile mock placement:
  - `vr_mock_hand_to` stepped the hand from its last *drawn* place. That place lags the commands by a frame or two
    and is held out by walls and floors: in mp_ground B's view still drew the shotgun held when the server had it
    fallen, "the weapon lying nearest" was another 25 m away, and the hand, sent there and stopped by the walls, came
    back a unit a command (30 tries: not enough); in mp_disarm a hand that had reached the floor for a shell went to
    the pouch 8 units low (its drawn place held up by the floor), the shell let go outside it. The harness's clients
    now place hands from the play space's place (`vr_mock_hand_to_absolute 1`, the mode the SP grab tests use), and
    mp_ground waits for B's own view to have let the shotgun go.
  - mp_rejoin's `lying()` took the report *before* the let-go reached the server ("none", speed -1) for a weapon at
    rest: "entity 0 at the origin". It now wants a weapon.
  - mp_dm: A and B connected at once; under load B came first, and the server's QC tests take the first human as
    theirs, while mp_test poses A's hands: B's crowbar fell from his open hand (vr_dm_test 3's "parry: none"), the bot
    reached for his shotgun (6). A connects first now (checked: A's entity is the lower).
  - Found on the way: mp_ground's sword placed exactly 6 cm over its handle met it lying on its edge (7.5 cm into it,
    the grip not yet pressed: shoved 2 m away): 12 cm now. mp_predict's swim waited for a big correction's line, which
    never came when B's prediction started on e1m4 after the put: it waits for his own place now. mp_climb's mantle
    play (real-time keyframes) took no hold once under load (B8j's flake): up to 3 plays, as a player tries again.
- **A return without a level change's floors** (vr_coopsave.cpp `VR_CoopNoteLeave`): B15's "a magazine more" was id's
  SetChangeParms raising shells to 25 (and health to half the capacity) on a level change: a free top-up for leaving
  and coming back. The leaver's parms keep his shells and health as they were (the health cap still applies; a dead
  player's new parms untouched). `mp_rejoin` checks both exactly (B hurt to 30, 17 shells, before his crash).
- **Dedicated servers** (host.c, host_cmd.c, vr/vr_svstatus.cpp new): `server.cfg` in the game folder exec'd by itself
  (after autoexec.cfg, before the command line's +commands); `pausable 0` its default; a console status line every
  `vr_sv_status` seconds (new, 60; 0 off) and when a player comes or goes ("status: e1m1 (coop, skill 1) | 2 of 8
  players: A 42 ms, B 85 ms | map 0:12:03, up 1:02:03"; `vr_sv_status_now`); `status` prints each player's ping.
  `mp_dedicated` (new, 8 checks): the lines at the start, A's join and leave and every 2 s; status's ping; pause refused.
  MULTIPLAYER.md's "A dedicated server" (command line, a server.cfg example, the console), PATCH_NOTES_1.1.1.md.
- **NAT traversal: a proposal for his OK** (MP_DESIGN.md, "NAT traversal"): (1) one server socket for every client
  (NetQuake opens one per client on a new port, which only port-preserving routers map: a forwarded host behind
  another kind can't be joined today), (2) a plain-UDP rendezvous with room codes and hole punching (~200 lines of
  our own, no libraries), (3) the service run somewhere (his) and the menus, (4) a relay only if needed. Not ICE/WebRTC
  (DTLS libraries, TURN servers). Nothing built.
- **Checks:** the full MP suite (dev) 3 runs of 3 clean (43 scenarios each, mp_dedicated in), after a first set
  (1 clean of 2: mp_armed's combat link 36620 B/s against its 35840 budget once, and the climb flake fixed above);
  SP: scenario_check PASS (136/0), load_models 4/0, vr_run_check 12/0, autosavenow 4/0. Dev and ThinLTO builds 0 warnings.
- **Open:** mp_armed's combat budget overran by 2% once (a real-time fight's variance; not chased); mp_predict's drop
  onto the rising lift (2.08 units in the air, once alone: B8's mover prediction edge, not chased); `server.cfg`'s
  auto-exec has no harness test (the shared base can't hold one safely); the ICE/NAT steps wait for his decision.

Next phase: B18 -- NAT step 1 if he agrees (one server socket for every client, mp_proxy's NAT mode), else the lift
prediction edge and mp_armed's combat budget; his: B10v's VR checks, the guide's read-through, internet play with two
machines, the rendezvous service decision.

### B18 results: the lift's contact move, the combat budget, server.cfg tested, mp_fuzz (2026-11-17, dev no-LTO build, loopback; NAT step 1 skipped: his OK)

- **The drop onto the rising lift (mp_predict's 2.08 / 1.35 / 1.005-unit flake)** is one lift step, reproduced 1 in 8
  runs of `predict --part lift --watch 3`: the server runs the movers once a tick and a client's moves as they come;
  one of B's moves bunched with another in a tick (the server's trace `k=0/2`, `k=1/2`) inside the unacknowledged
  window puts the lift a step (150 u/s / 72 = 2.083 units) off where an older replay met it: one replay lands on it,
  the newer is pushed by it or lands a move later. A pusher-relative correction (Past.on for a push too) was tried and
  made it worse (2 of 10 failing: a push's world-space result does not depend on the lift's place, a land's does), so
  `vr_predict.cpp` now counts the correction of a move that met a mover (landed on it moving, or pushed by it) in
  either replay apart (`contact_n`, `contact_max` in vr_mp_predict_report; out of corr_max and air_max), and
  mp_predict checks it within one lift step; every other correction stays under a unit, the drawn offset under the
  larger of a unit and the contact's. 10 of 10 runs: contact 0.000 in 8, 1.026 and 2.083 in 2 (exactly a step).
  The fix proper is a server-side move buffer (at most one of a client's moves a tick, a backlog of two run together):
  it trades up to a tick of latency on bunched arrivals for the alignment; not done (it touches every move's timing,
  lag compensation and the lockstep).
- **mp_armed's combat overrun (36.6 KB/s against 35)** was a real cost: a grunt gibbed in the 5 s window, the
  fragments 105 B a datagram (within their 128) on top of a heavy fight's ~320 B. `vr_net_cosmetic_fill` (new, 352 B;
  Debug's network rows "Fragments' Fill"): the fragments take only what the snapshot's other entries leave under it,
  a quarter of `vr_net_cosmetic_bytes` always (vr_netsnap.cpp encode; its self-test 14/0). With it the combat windows
  21-27 KB/s, the fragments 7-31 B a datagram with a gib storm in them.
- **server.cfg's harness test** (mp_dedicated, 10/0): a server.cfg in a base folder of the run's own (a second
  `-basedir`; id1, as quakevr is searched in the one base folder with the game code): exec'd after autoexec.cfg, its
  hostname held, the command line's `+vr_sv_status 2` over its 97.
- **Chosen for a public beta: robustness against broken or hostile packets** (needs no headset nor his decision; a
  public server takes strangers' packets and the browser joins strangers' servers). `mp_fuzz` (new, in the suite):
  the proxy corrupts B's game packets (`Proxy.corrupt`: bits flipped, cut short, random bytes, garbage after; the
  header's length rewritten), 30% of them until 30 were, up then down, B joining again whenever his link ends. Found:
  a corrupted entity update's colormap closed the client (`Sys_Error ("i >= cl.maxclients")`, a crash report); it
  and svc_lightstyle's, svc_updatestat's, CL_ParseTEnt's and CL_NewTranslation's are Host_Errors now (cl_parse.c,
  cl_tent.c: the game ends with the reason; single player's demos too). After it 3 of 3 (60 each way, B dropped by
  the server 33 times: unknown command char, a malformed QVR_CLC_MOVES, badread; B's game left 35-39 times on
  CL_EntityNum, bad server message, svc_updatename...), then 14/0 in each suite run. The server's side held from the
  start.
- **Checks:** the full MP suite (dev) 3 runs: 50, 50 and 52 of 52 clean. The failures: mp_armed's rate-priority check
  (the nearest two's poses against the farthest two's under vr_net_rate 12000) in runs 1 and 2 -- it fails with the
  fill off too (A/B, 4 runs each: off 1 failure, on 0), the real-time rate flake of B13d/B16, open; mp_disarm's toss
  and placement (B10b's), mp_lockstep_throw's "8 moves differ" once (passed in the other two). SP: see ROUND25.
- **Open:** the server-side move buffer above (the lift contact's step); mp_armed's rate-priority flake; NAT steps,
  B10v's VR checks, internet play: his.

Next phase: B19 -- NAT step 1 if he agrees (one server socket for every client); else the server-side move buffer
(one of a client's moves a server tick; mp_predict's lift contact under a unit, the lockstep and lag compensation
unchanged) and mp_armed's rate-priority check made robust; his: B10v's VR checks, internet play with two machines,
the rendezvous service decision.

### B19 results: the server-side move buffer; broken packets; the suite's flakes (2026-11-17, dev no-LTO build, loopback; NAT step 1 skipped: his OK)

- **The move buffer** (vr_netmove.cpp `serverRunBuffered`, called by SV_RunClients after a client's messages;
  `vr_net_movebuffer`, new, 2; Debug's network rows "Move Buffer"): a remote client's QVR_CLC_MOVES are kept as read
  (and acknowledged at once: the delta base and the net HUD's ping as before) and each server tick runs the oldest kept,
  in order, then more only while over `vr_net_movebuffer` wait. The whole move waits (its hands, buttons and walk
  together), so `serverLast` is the newest *run* and his predict state's move number needs no change. A bunched pair
  (B18's `k=0/2`, `k=1/2`) now runs a tick apart: each move meets the lift where his replay met it (the replay's first
  move against the newest message's movers, each later one a tick on). 0 runs as they come (what was kept runs at
  once). The lockstep keeps its own by-number run (the buffer is off under it); lag compensation goes by the absolute
  times his moves carry (unchanged by when they run).
  - Measured, `predict --part lift --watch 3` (B on the proxy, 150 ms each way): before, 10 runs: the lift's own
    predicted place moved under him (`lift_max`) 2.07-4.01 in every run, the replay's result for a state's move against
    the server's (`exact_max`) 1.94-2.12 in 9 of 10; with the buffer at 1, 10 runs: `lift_max` 0.05-0.21 in 8, 2.1 in 2,
    `exact_max` 0.03-0.11 in 8; at 2 (the default), 8 runs: `lift_max` 0.054-0.138 and `exact_max` 0.033-0.156 in all 8.
    `contact_max` 0.000 in all (B18's 1.026 and 2.083 came 2 runs in 10). The buffer's ticks over a run (224 moves):
    at 1, 4 ran two and 5 none (the transitions that put the lift a step off); at 2, 1 and 1; 0.7 moves waiting on
    average (about 10 ms).
  - The cost: at most `vr_net_movebuffer` ticks (2: 28 ms) more before the server runs his move, ~10 ms on average on
    the proxy's link; nothing for himself (he predicts), the others see him that much later.
  - `mp_netmoves:` (vr_mp_status) gains `tick_one`, `tick_more`, `tick_none`, `wait_sum`. mp_predict's lift check: the
    contact and the lift's predicted place under a unit when the buffer ran one move a tick through the segment, else
    within one lift step (`b_predict_lift_buffer_off`). mp_blade's "moves came bunched" counts the moves the buffer kept.
  - Still open: `exact_miss` stays ~9 a lift run, by hundredths (0.03-0.16): the client's mover place (its message's
    origin plus its speed times the moves' time) against SV_PushMove's own steps; under the corrections' notice.
- **mp_armed's rate-priority flake (B13d, B16, B18)**: a record's priority is its weight times one plus the frames it
  waited, so it waits whole frames, and when the fight's other updates leave room for most poses every frame or two the
  share between the highest and lowest weights flattens towards 1; only a tight budget gives shares near the weights'
  ratio, and the fight's traffic is real time (B18's four failures: shares 1.10-1.36 against 1.16-1.49 wanted). Nor are
  the entries B read a measure of the priorities at all: a pose goes only when it changed, and a humanoid idle in the
  window changes less (a B19 suite run: weights 0.517 and 0.498, 58 and 132 entries: the order itself failed). The
  check is now the server's own: `vr_net_posewaits <slot>` (new, a server command, vr_netsnap.cpp) tallies one slot's
  pose records as they went, the frames each waited plus one and its weight then; the highest two weights' mean wait no
  longer than the lowest two's, shorter once those wait at all (3 runs: 2.09 against 5.12 and alike). The client's
  share is a metric (`armed_rate_share`).
- **mp_fuzz found two more client faults (SP-relevant, own commits):** a corrupted entity update's U_MODEL2 high byte
  (model numbers up to 65535) read past `cl.model_precache` (an access violation in CL_ParseUpdate); the same for a
  baseline's model and STAT_WEAPON (clientdata, svc_updatestat; view.c indexes the models by it): Host_Errors now
  (cl_parse.c). And a Host_Error inside a snapshot's record (vr_netsnap.cpp `apply` points net_message at each) left
  net_message there: the next connection's request overflowed it (SZ_GetSpace: B never joined again). Host_Error and
  Host_EndGame put it back on its own buffer (net_main.c `NET_RestoreMessage`). mp_fuzz then 14/0 in 5 runs alone.
- **mp_dm** read B's player list once the first bot's name reached him, not the second's (3 of 4 listed once): it
  waits for both.
- **mp_predictclimb's hold take not counted (trans_n 0, once)**: the client's flag for a climb's transition (a hold
  taken, the hang's mantle begun; the hook come off) was set by each state read, so two states read in one frame, the
  second already hanging, lost it. Both flags now last until a replay takes them (vr_predict.cpp).
- **mp_disarm's toss (B18 run 1)**: the play's release falls at a real-time point of B's swing (the throw's age
  0.06-0.27 s): the earliest is the highest lob (60 u/s up against 47-55), and it slid on to lie 71 units from A by the
  start room's south wall, past the 64 asked; "A never put at" followed: the spot 20 units short of it was in the wall
  (SV_CheckStuck put him back). The check is within a step and a reach (96; `toss_lands_from_a`), and A is put 20, 32
  or 44 units short.
- **mp_lockstep_throw's "8 moves differ" (B18 run 2)**: explained from its log: moves 0-7 of A differed only in both
  hands' throw estimate (`.throwPos`, `.throwAge`: fields 37-40 and 66-69), one run's from a place 4000 units off and a
  sample older. The hands' throw history is sampled every client frame, and the frames before the lockstep's first
  move (loading, joining) are as many as the machine makes them. A lockstep client (`vr_mp_lockstep_ahead`) now clears
  that history at its first move (vr_client.cpp; play outside the harness unchanged).
- **Checks:** the full MP suite (dev) 3 runs of 3 clean (52 scenarios each), after sets that found the faults above
  (mp_fuzz's crash and stranded rejoin, mp_armed's order, mp_dm's list, mp_predictclimb's flag: each fixed, then its
  scenario 3-5 runs alone); SP: scenario_check PASS (136/0), load_models 4/0, vr_run_check 12/0, autosavenow 4/0. Dev
  and ThinLTO builds 0 warnings.
- **Open:** NAT steps, B10v's VR checks, internet play: his. The buffer's latency on a real internet link (its
  `wait_sum` in a server's `vr_mp_status`) is worth a look when he plays with two machines.

Next phase: B20 -- NAT step 1 if he agrees (one server socket for every client); else the remaining exact misses of a
mover's place (the client's mover against SV_PushMove's steps) and a soak of the buffer under mp_proxy's jitter
(`--jitter 30`: its latency and the lift's contact); his: B10v's VR checks, internet play with two machines, the
rendezvous service decision.

### B20 results: the move buffer under jitter; the lift's leftover misses; a 20-minute soak (2026-11-17, dev no-LTO build, loopback; NAT step 1 skipped: his OK)

- **Jitter measured first** (`predict --part lift --watch 3`, B 150 ms behind each way; `--part` now takes `--jitter`
  and `--loss`): with B19's buffer (depth 2), at `--jitter 30` the lift's predicted place moved 2.2-4.3 units in 3 of 4
  runs (the buffer's ticks off one a tick 50-68 a run); at 60, B's corrections in the air 9-27 units and he missed the
  lift in 4 of 4. The server's `mp_netmoves` gave the cause: **B's moves lost with no packet lost** (6-11 a run at 30 ms,
  48-71 at 60). NetQuake's datagram layer drops an unreliable datagram that a newer one overtook ("a stale datagram");
  each move rides in three (`repeats` 2), and the proxy's uniform jitter reorders past that.
- **The server reads a remote client's overtaken datagrams** (net_dgrm.c: `qsocket_t.qvrAcceptStale`, set by
  SV_ReadClientMessage for a client that sends QVR_CLC_MOVES while the buffer is on; `net_stalemessage`): only their
  QVR_CLC_MOVES (each move idempotent by its number; the snapshot acknowledgement in it is older and ignored), anything
  else in such a datagram dropped as before. A client's own reading (snapshots) is unchanged.
- **The buffer waits for a gap and its depth follows the client's jitter** (vr_netmove.cpp `bufferDepth`): a missing
  move before the oldest kept (a reordered one) is waited for while the moves kept are within the depth (`tick_gap`);
  the depth is the spread of his moves' lateness (the buffer's tick less the move's number, as each first came) over
  the last 2 s, from `vr_net_movebuffer` (2) up to `vr_net_movebuffer_max` (new, 10; Debug's network rows "Move Buffer
  Most"; at or under vr_net_movebuffer a fixed depth). The level settles at what the jitter needs and no more: a
  steady link's depth is the least again 2 s after a spike (the moves over it then run together).
  - Measured (lift runs; latency = `wait_sum`/moves run): **jitter 30**, 4 runs: corrections 0.000-0.006, the lift's
    place 0.03-0.11, exact misses 0.01-0.04, moves lost 0-1 (the join's first ticks), depth 5-6, waiting 2.2-2.8 moves
    (31-39 ms). The same with the fixed depth 2: the lift's place 2.1-4.3 in 3 of 3, depth's ticks off 76-82 a run,
    waiting 0.9 (13 ms). **Jitter 60** (cap 10), 3 runs: corrections 0.003-0.027 (before: 9-33), lost 0-3 (before
    48-71), depth 9-10, waiting 4.5-5 moves (63-69 ms); the fixed depth 2 there: corrections 4-14, lost 38-41. **30 ms
    and 5% loss**, 3 runs: corrections 0, the lift's place 0.04-0.08, lost 0-1. **No jitter**: depth 2 after the join's
    first 2 s (its startup burst measures 5-6), as B19.
  - `predict --part lift` at 60 ms still fails "B rode the lift up": he lands beside it at the bottom, his prediction
    agreeing (the scenario's drop timing at 360 ms and more round trip), not a fault.
  - Decided: adaptive on by default (clearly better from 30 ms up; the same as B19 on a steady link); the cap 10 covers
    a 60 ms jitter's spread (9-10). The cost is the others seeing a jittery player later by about half his jitter's
    range; his own play is predicted.
- **`mp_jitter`** (new, in the suite; 13/0): B at 150 ms give or take 30 falls onto the rising lift: moves lost 2 at
  most, the depth to 3 at least, the lift's predicted place under a unit. `mp_netmoves` gains `tick_gap`, `depth`,
  `depth_max`, `spread_max`; `mp_netmove_skip` lines under vr_mp_watch 3.
- **The lift's leftover exact misses (0.03-0.16)**: tried sending the movers' exact origins with the predict state
  (the entity update's coordinates are to 1/16 unit): no change (3 runs: exact_max 0.04-0.12, 7-10 misses), so it was
  reverted (no protocol change). The misses' pattern (`mp_predict_exact_miss` on a ride): every other move his replay
  has him 0.033-0.042 units above the server, the others within 0.0002: about DIST_EPSILON (0.03125), a trace's
  stand-off from the lift's top. The server's SV_PushMove carries a rider by the lift's move exactly; the client's
  replay, on alternate moves, lands him on it by a trace. Under every notice (the drawn offset 0.02); open, low value.
- **The 20-minute soak** (`soak --soak-minutes 20`, new: sampled each minute; QVR_MP_LOW_IMPACT=1, idle priority):
  16 players on dm3, the server's pace 0.999-1.0 every minute, its edicts 484-557 and Box3D bodies 186-246 (no
  drift), B's link down 9.9-19.3 KB/s, up 2.7-2.9 KB/s, B's moves 86,680 run and none lost, the buffer at depth 2-3
  all along. The processes' private bytes moved in ~100 MB steps either way (A +173 then -83 at minute 7, B +87 and the
  server +96 at minute 15; A and B +100 MB from minute 2 to 20): mimalloc's segments and the GPU driver's, not a
  trend between the steps (0.6-0.9 MB a minute). The check now measures the heap itself (mimalloc's committed after
  `vr_heap collect`); its 20-minute run (21/1): A's heap 1529 -> 1538 MB and B's 1550 -> 1556 MB over minutes 2-20
  (flat: 0.3-0.5 MB a minute, while their private bytes still stepped ~100 MB), **the server's 1228 -> 1379 MB**
  (FAIL: 48 at most): 2.6-8 MB a minute over minutes 2-7, flat (0.2-0.6) over 9-17, then +37 and +17 at minutes 18-19
  after the run's peak of Box3D bodies (299 at minute 16); 66 MB of it at minute 8 was a `vr_alloc_sites 720` trace
  I asked of the running server (its own storage). That trace (10 s): Box3D's `b3GrowAlloc` from
  `b3ComputeMeshManifolds` the busiest site (3.45 mallocs a frame against 0.31 frees at its own site; the other frees
  go through the contacts' and islands' destruction), 215 more mallocs than frees in 720 frames. Not found: whether
  the server's heap grows without bound or settles at the busiest fights' peak (Box3D's pools keep their most);
  the next phase's first item.
- **Checks:** the full MP suite (dev) 3 runs: 53, 53 and 52 of 53 clean (run 3: mp_disarm's nail magazine port, 34/1,
  B16's known hand-placement flake; alone again: see ROUND25). mp_jitter 13/0 in each. SP: scenario_check PASS
  (136/0), load_models 4/0, vr_run_check 12/0, autosavenow 4/0. Dev and ThinLTO builds 0 warnings.
- **Open:** the lift's DIST_EPSILON misses above; NAT steps, B10v's VR checks, internet play: his. Real links' jitter
  is rarely uniform: the depth's 2 s window and the cap are worth a look with his two machines (`depth`, `wait_sum`
  in a server's `vr_mp_status`).

### B21 results: the server's heap over a 40-minute soak; Box3D's memory (2026-11-17, dev no-LTO build, loopback, low impact; NAT step 1 skipped: his OK)

- **Box3D is not the server's growth.** `vr_physics_mem` (new; Debug > Profiling and Memory): Box3D's live bytes,
  its world's, the contact manifolds' pools and long arrays, the engine's hull caches; the soak's rows carry it each
  minute (`mp_box3dmem`). 16 players on dm3, 30 minutes: Box3D 5.1 -> 8.8 MB, the manifold pools 1.4 MB, the longest
  manifold array 8 (upstream's per-count pools would hold ~5 MB at that). B20's busiest site, `b3GrowAlloc` from
  `b3ComputeMeshManifolds`, is the mesh contacts' triangle caches, freed with their contacts.
- **The server's heap** (mimalloc's committed after a collect): 1211 MB at minute 2, 1447 at minute 30: steps of
  +55 to +60 MB at minutes 9, 13 and 17, +10 to +13 at minutes 20-22, 0.2-1 MB a minute between; A and B flat
  (1509 -> 1518, 1519 -> 1523). **The map again at minute 30** (`soak --soak-change 30`: changelevel dm3, A and B
  back): Box3D 8.8 -> 3.1 MB, the heap 1447 -> 1383 MB: 64 of the 236 MB came back. Not pools settling at the
  fights' peak (the steps keep coming, the change returns little): a server-side leak or a cache outliving the map,
  in ~55 MB lumps; not Box3D. FrikBot's bots don't follow a changelevel: minutes 31-40 had A and B alone (flat).
  The run's two FAILs: the heap check (1211 -> 1383) and "nobody left" (the change's own leaves: now excluded).
- **Found on the way, single player too** (commit 8cee842ae; on next's base: branch `agent/b21-box3dmem`, 24b7ce728):
  the prop hulls' cache (`propHulls`, keyed by each thing's drawn box) grew all map long: 1912 hulls by minute 30 on
  the server, 3940 over the arena's 32 waves (`vr_arena_test 6`: Box3D 5.3 -> 10.5 MB); it is trimmed past 1024 now
  (`trimPropHulls`; the actors' hulls stay), and the same arena soak holds at ~5.7 MB. Box3D's manifold arrays
  longer than 4 come from the heap (a guard: upstream's per-count pools are quadratic in the longest; Box3D
  README). On next's base: scenario_check PASS (136/0), load_models 4/0, vr_run_check 12/0, ThinLTO 0 warnings.
  Arena test 6 fails "a wave clear's drops go vr_arena_item_waves clears on" (18 > 15) before and after the change.
- **The client's overtaken snapshots** (item 2): not done. A snapshot overtaken on the way is one sample between two
  the interpolation already joins with a line; its datagram's other contents (svc_time, sounds, temp entities,
  prints) cannot be replayed out of order, and its entities' samples would need inserting into the histories by time.
  Small gain (a midpoint for curved motion under reordering), not cheap: left.
- **The lift's DIST_EPSILON misses** (item 3): not trivial (the client's replay against SV_PushMove's carry): left.
- **A client crash under mp_fuzz** (suite run 2): a corrupt packet's svc_time made `R_TextureAnimation`'s frame
  negative ("infinite cycle" Sys_Error; the light styles read out of bounds too). svc_time is checked as read now
  (Host_Error) and the texture animation draws frame 0 for such a time (6707b6baa, single player's code too).
- **Checks:** the full MP suite (dev) 3 runs: 53, 51 and 52 of 53 clean (run 2: mp_rejoin 14/1, "C has the shotgun
  A let fall", and mp_fuzz's crash above, then 14/0 three times; run 3: mp_held 13/1, a hotspot 0.62 off against
  0.5, the hand-placement kind of flake). Dev and ThinLTO builds 0 warnings (agent/mp and agent/b21-box3dmem).

Next phase: B22 -- NAT step 1 if he agrees; else the dedicated server's ~55 MB heap steps (16 players, dm3; not
Box3D; 64 of 236 MB back at a map change): a live-bytes-by-size snapshot of mimalloc's heaps (`mi_heap_visit_blocks`)
before and after a step, then the allocation sites of the grown size (vr_alloc_sites on the server's other threads
too), suspects the server-only stores (the snapshots' sent logs, lag compensation, netmoves, the profiler's and
QuakeC's per-function records while vr_profile is on) and the bots' clients; the soak's change could re-add the bots
(impulse 100) so its minutes after a change stay busy. His: B10v's VR checks, internet play with two machines, the
rendezvous service decision.

### B22 results: the server's heap was idle workers' empty pages; B21's two flakes (2026-11-17, dev no-LTO build, loopback, low impact; NAT step 1 skipped: his OK)

- **What grew:** `vr_heap live` (new: the live blocks by block size, mimalloc's pages held, made and empty, and whose)
  each soak minute. 16 players on dm3, 22 minutes: mimalloc's committed 1237 -> 1432 MB (steps: +131 MB at minute 6,
  +16 at 10, ...) while the live blocks grew 877 -> 897 MB and the working set 657 -> 681: committed memory nobody used.
  In single player after e1m1's load: 285 empty pages, 120-150 MB, every one owned by another thread than the main one.
- **Why:** mimalloc's pages belong to the thread that made them, and only that thread gives back a page that frees on
  other threads emptied. The job workers make blocks (a load's hulls, nav tiles, a fight's jobs: the hit model's
  skinned bounds, the held weapons' meshes, Box3D's hulls) that the main thread frees; a worker that then went idle,
  or never allocated in that size again, kept its emptied pages committed for good. A changelevel did not help (the
  pages are the workers', not the map's): B21's "64 of 236 MB back".
- **The fix (single player too; commit 120427f2c, on next's base `agent/b22-heaptidy`):** each worker tidies its own
  heap: mimalloc's normal collect after a job at most once a `vr_heap_thread_tidy` (new, 1000 ms; ~6 us), and a forced
  one once it has had no job that long (Zancle's `ThreadPool::setIdleHook`, new: a worker's timed wait; ~0.5 ms on an
  idle thread). e1m1: committed 1382 -> 1095 MB, empty pages 0. The server over 16 minutes: committed 1016 -> 1029 MB
  (was 1237 -> 1432 over 20), live 861 -> 878 MB, flat over the last 4 minutes.
- **The live blocks' slow growth** (+16 MB in 16 minutes, tapering): `vr_heap_sites 1000 70000` (new: every thread's
  blocks of a size range kept by call stack until freed) from minute 2 to 16: 9.5 MB still live at 32 sites, the most
  the hit model's skinned bounds (`refit`, 155 blocks, 3.2 MB: `skinTrees`, at most 256 then all begun again), the
  held weapons' drawn meshes (751 blocks, 1.4 MB) and the ragdolls' skinned vertices (155, 2.6 MB): caches filling,
  bounded. No leak found.
- **mp_rejoin "C has the shotgun A let fall"** (B21 run 2): `lying()` took the weapon lying nearest A's hand before the
  let-go reached the server: another weapon anywhere on the map (2600 units off in the failure; in the passing runs a
  weapon 340 units off, within B's view by luck). It now waits for the hand empty (`drawn=0`) and a weapon within 96
  units of it.
- **mp_held "the carried shotgun's hotspots"** (B21 run 3, 0.62 > 0.5): compared as soon as the hand-off showed in both
  reports, before the shotgun settled into the off hand; the wait now also waits for the hotspots within the slack (a
  shotgun that never settles still fails).
- **The soak** (`mp_test.py soak`): `soak_live.txt` (the server's live blocks by size each minute and just after the
  change), `--heap-sites MIN:MAX` (soak_sites.txt), the bots added again after `--soak-change`'s change.
- **Checks:** the full MP suite (dev) 3 runs, 3 clean (mp_rejoin 15/0 and mp_held 14/0 each). The 40-minute soak
  (`soak --soak-minutes 40 --soak-change 30`, the bots again after the change) 23/0: the server's committed heap 1020
  MB at minute 2, 1040 at 20, 1044 at 30 (B21: 1211 -> 1447), 1009 after the change, 1035 at 40 with the bots back;
  its live blocks 857 -> 883 -> 849. On next's base (`agent/b22-heaptidy`, 6e5fc7980): scenario_check 136/0,
  load_models 4/0, vr_run_check 12/0, vr_alloc_test and vr_heap stress PASS, ThinLTO 0 warnings; agent/mp dev 0
  warnings.

Next phase: B23 -- NAT step 1 if he agrees; else the lift's DIST_EPSILON misses (the client's replay against
SV_PushMove's carry, B21 item 3) and a 60-minute soak with the change twice (the caches' bounds: `vr_heap_sites`);
his: B10v's VR checks, internet play with two machines, the rendezvous service decision.

### B23 results: NAT traversal: one server socket, room codes, the room service (2026-11-17, dev no-LTO build, loopback; his OK of 11-17)

- **Step 1, one server socket** (net_dgrm.c `ReadShared`/`ReadDatagram`, `_Datagram_ControlRequest`; QVR_NET_VERSION
  36): every client on the listening socket, demultiplexed by address and port into per-client queues (256) and a
  control queue (64); a v36 client keeps the address it reached (the accept's port is the server's inside one). The
  drivers' CheckNewConnections return the listening socket whenever listening (no peek). Windows: SIO_UDP_CONNRESET
  off and SO_RCVBUF 1 MB on it (net_udp.c: the buffer).
- **Steps 2-3, room codes** (net_dgrm.c "room codes"; MP_DESIGN.md's proposal, status line added): `vr_mp_rendezvous`
  (`vittorioromeo.com:26100`), `vr_mp_room` (1); `connect @CODE`, Join Game's "Join by room code", Host Game's
  "Room Code" (C copies), `vr_mp_roomcode [copy]`. The service: `Misc/rendezvous/` (qvr_rendezvous.py, its test,
  systemd unit, deploy.sh, README with the relay design note).
- **Tests**: `mp_test.py nat` (12 checks; in the suite: 47 scenarios); `mp_nat.py` NAT routers through `-natgateway`;
  `Misc/rendezvous/test_rendezvous.py` 20/0. The harness: `vr_mp_room 0` for every game.
- **Checks**: MP suite x3: run 1 two fails (mp_armed's combat field share once, alone 18/0; mp_avatar's off-hand curls
  raced: the wait now includes them, 2/2), runs 2 and 3: both clean (46 scenarios, mp_nat 12/0). SP: scenario_check 136/0, load_models 4/0 (vr_run_check not run: no canonical script; nothing SP-side changed but the Join Game page). ThinLTO 0 warnings.
- **Open**: real routers (his two machines; a symmetric NAT gives the message, no relay); IPv6; a public room list
  (`vr_mp_public`) not built; the menus not seen in a headset.
- **Later (2026-11-21, "portcheck"): the port check, rendezvous protocol version 3.** A registered host sends
  PROBECHECK (version 3; REGISTER and JOIN stay version 2) with his game port (`net_hostport`); the service sends one
  PROBE from a second socket (another source port) to his outside IP at that port, then PROBING. The probe got in:
  reachable; PROBING and no probe in 2 s: not reachable; a version 2 service's BADVERSION, none, or no answer: unknown.
  Limits: one probe per registration, only from the room's address and to its IP, 64 bytes of replies to a 64-byte
  request. Shown on Host Game (above the room code), the net HUD while hosting (`port 26000 reachable`), the console
  once and `vr_mp_roomcode` (net.h `NET_PortCheck`/`NET_PortCheckText`). Tests: `test_rendezvous.py` 44/0 (the probe,
  its limits, version 2 served as 2, `--newest 2`), `mp_test.py nat` 18/0 (off: unknown; cone NAT without a forward:
  not reachable; forwarded: reachable; a version 2 service: code still given, unknown), roomui (the HUD line).
  The relay: BACKLOG.md, low priority (his decision).

Next phase: B24 -- his: deploy the room service and try a room code between two real routers; ours: the lift's
DIST_EPSILON misses (B21 item 3) and a 60-minute soak with the change twice; a relay only if real punches fail often.

### B24 results: no test on the internet; the lift exact; a 60-minute soak; the room-code pages (2026-11-17, dev no-LTO, loopback)

- **No test talks to the internet** (a B23 run's dedicated server had sent REGISTER datagrams to vittorioromeo.com):
  three fences, "Test harness" above. The engine's `-loopbackonly` (net_main.c `NET_LoopbackRefused`/`NET_LoopbackAddr`,
  net_wins.c, net_udp.c, net_wipx.c, host_cmd.c's Download); the harness's `net_safety_problem` /
  `command_net_problem` (launches and commands; a name other than localhost counts as off the machine, never looked
  up); every scenario's "every game kept to loopback" check. `mp_netsafe` (new, first in the suite, 8 checks): the
  rule's cases, a game not started, a dedicated server hosting a room at 169.254.0.1 (link-local: never routed) and
  at `qvr-netsafe.invalid` refused by the engine.
- **The lift's residual (B21 item 3) was two clocks, not DIST_EPSILON**: a replay's lift went on from the entity
  update's place (to 1/16 unit: up to 1/32 off) by B's move times (0.0139-0.0147 s against the server's ticks of
  1/72 +- the 1 ms tolerance: 150 u/s x 0.0002 s = 0.03). Now the predict state carries each nearby mover's exact
  origin and the server's tick (QVR_NET_VERSION 37, 16 bytes more per mover in motion near him), the replay's movers go
  on by that tick a move (`moverAt` from the exact place; the drawn lift too), and a dedicated server's ticks are
  exactly host_netinterval (host.c `Host_FixedTicks`: no tolerance without a headset). `predict --part lift`:
  exact_max 0.058 -> 0.000004 (misses 10 -> 2, both float rounding), lift_max 0.000.
- **The 60-minute soak** (`soak --soak-minutes 60 --soak-change 20,40`, low impact; `--soak-change` takes a list now):
  25/0. The server's committed heap 1049 at minute 2, 1070 at 20 (-> 1051 after the change), 1062 at 40 (-> 1032),
  1060 at 60; its live blocks 884 -> 908 MB, flat from minute 10 (B22's fix holds). A and B +10 and +9 MB. Pace
  0.999-1.0 every minute, B's link down 7.5-26 KB/s, B's moves 258,855 with 3 lost, Box3D back to 3.1-3.7 MB at each
  change, edicts 474-586.
- **The room-code pages, seen** (`mp_test.py roomui`, not in the suite; `menu_joingame [CODE]` and `menu_hostgame`,
  new console commands): a flat listen host with a local service, his Host Game page ("Room Code UUYLWC (C: copy)")
  and a player's Join Game page with the code typed in, 960x540 screenshots; the player then joins by it.
- **mp_armed's combat field share**: the encoder sends each entity update the smaller way, so the share by fields
  follows the fight (80-97%, B23's fail 79.9% at 6.5 B a record); the check is now on the bytes (mean under 9 B) with
  the share over half.
- **Checks**: the MP suite (48 scenarios) x3, then x3 again after a fix. First batch: mp_download failed in all three
  (its local HTTP mirror refused by the first `-loopbackonly`: downloads from a literal loopback host are now allowed,
  every connection checked; mp_download alone 9/0) plus mp_disarm's magazine round once (alone 36/0). Second batch: run
  2 one fail, runs 1 and 3 several unrelated ones (mp_lagcomp's rewind-frames comparison twice, mp_dm's bot checks,
  mp_ground's sword grip, mp_disarm twice), none of them in the first batch on the same engine code (only the download
  check changed), so read as load on the machine; alone: mp_lagcomp 15/0, mp_dm 23/0, mp_disarm 36/0. mp_netsafe 8/0,
  mp_predict and mp_jitter passed in all six. SP: scenario_check 136/0, load_models 4/0, vr_run_check 12/0
  (`scratch/b10v2_runcheck.sh`, run B `3 1 0 1 0`); dev and ThinLTO builds 0 warnings.
- **Open**: his: deploy the room service, a room code between two real routers; IPv6; a public room list.

Next phase: B25 -- his: deploy the room service (`Misc/rendezvous/deploy.sh`) and try a room code between two real
routers (the screenshots in B24 show the pages); ours: a relay only if real punches fail often; IPv6 or a public room
list (`vr_mp_public`) if he wants them; B10v's VR checks are his.

### B25 results: the suite under load (2026-11-17, dev no-LTO, loopback)

B24's suite had 0 clean runs of 6 while other work ran on the machine; each failing scenario passed alone. Per
scenario, the cause (`--load N` reproduces it here: below):

- **The harness's priority** -- the games ran below normal: every build and fast-mode kit test of other workers came
  first and these real-time games went without (under 32 busy workers a dedicated server printed nothing in 60 s).
  Now normal priority as the kit's runs (idle still while the author plays; `QVR_MP_PRIORITY=below` the old way).
- **A game bug (MP only): a hitched player kicked by another game of the same name.** The B15 "back from a crash" rule
  took a connect request from the same address by the same name, his connection silent over 3 s, as him back and
  closed his connection. Every harness game is "player" when it joins, so under load a joiner (B) closed A's live
  connection and got no answer itself ("Server not responding for 90 s", mp_lagcomp's join); for real, two players
  behind one router with the default name, one hitching. Now the request carries the game's own key
  (`vr_mp_clientkey`, new, archived: made up at the first connect, so a restarted game sends the same) and the rule
  needs it too. mp_rejoin: a new check (another game by B's name while B's link up is cut 4 s: B kept, it a slot of its
  own; `Proxy.blackout_up`, new), and B2 (the restarted game) keeps B's key.
- **mp_dm**: the bots play by themselves meanwhile: once a bot walked into dm4's exit (the level started again, the
  bots gone: 7 checks failed, the B24 and B25 runs), and they could reach the fraglimit in the QC tests' steps. Now
  `noexit 1` and `fraglimit 0` until A's frags (then above everyone's frags so far); A's own list of the bots waited
  for too (A listed three once).
- **mp_lagcomp**: the "frames' rewind off under the hold" check used the mean of 6 shots, but a held shot finds that
  rewind off only when B's view fell between two of A's held updates -- wall-clock phase: now the worst of 12 (four held
  legs: 3, 6, 3, 6 snapshots).
- **mp_ground**: the turned hand waited 5 units over the sword (`vr_mock_hand off ... -0.35`) and touched it, the
  sword on its edge tipped (cosine -0.91 -> 0.68 at the same hand): turned at head height now, and `agreed()` also
  waits for the server's cosine to stay put; the dropped shotgun now comes to rest before the hand goes to it (under
  load it still slid, the hand's jumps knocked it 1.5 m away).
- **mp_disarm**: the held-out shotgun waited for until the server draws it (`spots`), not only in his inventory; the
  launcher's trigger held until the server's magazine goes down (0.1 s of the server's time let B's game run
  +attack and -attack in one frame: no shot).
- **mp_download** (B24's first batch): its local mirror vs the first `-loopbackonly`, fixed in B24.

The load generator: `mp_test.py ... --load N [--load-duty D]` (`mp_load.py`, new): N busy workers at normal priority
for the whole run (each its share D of every 50 ms; they stop with the run or when the harness is gone); `suite --only
a,b` and `--suite-repeat N`. `--load 32` (every CPU) is past any real day: game start-ups ~100 s (10x), and real-time
gestures (mp_disarm's yank and toss, the ammo pouch) fail -- not a target; `--load 16` is the suite's reference load.

- **Checks**: dev and ThinLTO builds 0 warnings. Under `--load 16`, other workers' jobs running too: the five B24 scenarios plus mp_rejoin, then the full suite (49 scenarios), 47 clean: mp_lagcomp 15/0, mp_disarm 36/0, mp_dm 23/0, mp_download 9/0 in it. Its 2 fails were this phase's own first tries, changed since: mp_ground's head-height turn sent the hand to another weapon (reverted); mp_rejoin's same-name step left B's later return wrong (now `rejoin --part samename`, alone). A plain full run (scratch/b25_full_plain1.txt) was still going at the report: the 3 clean runs plus 3 under load are not done yet. SP: the engine change is the connect request's (net_dgrm.c, multiplayer only); no SP test run.
- **Open**: under `--load 32`, mp_disarm's gestures (vr_mock_play's real-time keyframes: the yank read as 0 m/s);
  mp_rejoin's same-name step in the suite's run left B's crash return with health 0 and his shells twice (now `--part samename` alone; why not understood); the full 3 + 3 runs.

Next phase: B26 -- his: deploy the room service (`Misc/rendezvous/deploy.sh`) and try a room code between two real
routers (the screenshots in B24 show the pages); ours: a relay only if real punches fail often; IPv6 or a public room
list (`vr_mp_public`) if he wants them; B10v's VR checks are his.

### B25b results: B25's acceptance; a coop join's telefrag; mp_dm's restart (2026-11-18, dev no-LTO, loopback)

- **mp_ground** (B25's revert of the head-height turn): 16/0 alone, then in every full run below.
- **mp_rejoin's crash return after the same-name step: a game bug, not the harness.** D's join (B's name) moved the coop
  starts' turn on; B's return then took the start C stood on and telefragged him ("C was telefragged by B"; B died in
  the burst and took C's backpack: health 0, shells 34). Stock Quake's coop `SelectSpawnPoint` takes the next start in
  turn whoever stands on it: any coop join or return in progress can telefrag a player at a start. Now
  `VR_CoopSpawnPoint` (client.qc): the next start in turn if no living player is within 64 units (deathmatch's
  `VR_DMPlaceBlocked`), else the next clear one round (every coop start and the player start), else a clear place
  beside one (`VR_DMSpotBeside`), else stock. The same-name step is back in the default run, plus a check that nothing
  in mp_rejoin was telefragged (18 checks). Coop only: single player never takes this path.
- **mp_dm under load: vr_dm_test 7 set `deathmatch` 0 for 3 frames** (its caps out of deathmatch); a player's respawn
  in them took respawn()'s single-player way, `restart`: dm4 started again and 7 checks failed. Now a test flag
  (`vr_dml_test_off`, vr_dmloose.qc) turns the caps off; the game stays in deathmatch.
- **mp_melee's jittered swing**: the clean reference is vr_mock_play's real-time gesture; one clean take under load was
  4.98 against 3.92-3.98 in every other run. The clean swing is taken again once when the two differ.
- **mp_disarm's magazine load** (use part): each placement now waits for B's echo, and the whole is tried again from the
  start pose twice. Not enough: the magazine stopped ~2.5 units from the port (leniency 2) in 4 of 18 full runs (2 with the retries) and
  every retry stopped there too; `disarm --part use` alone passed 4 of 4 under load. Open.
- **Runs (full suite, 47 scenarios)**: before B25b's fixes, plain 2 clean then mp_lagcomp (B's move view times 418-556
  ms, the machine busy beyond --load 16: every other run 350-415) and mp_disarm; load 1 (melee, disarm), then 2 clean.
  After the disarm/melee changes: plain 3 of 3 clean; load 1 clean, mp_dm (the restart above), 1 clean. After the
  dm fix: plain clean 1 of 3 (mp_predictslide one move 1.65 units off; mp_disarm + mp_fuzz), load 2 of 3 clean
  (mp_disarm). So: 3 consecutive clean plain runs once (b25b2), never 3 consecutive under load (2 at most).
- **A crash: mp_fuzz, B's game exited 0xC0000005** after the server's corrupted packets down, during a rejoin's signon
  (its log ends at "signon prewarm"), 1 run of 18; no dump. Open, first for B26.
- **Dedicated servers' load window never closes** (next's code, not B25b's): host.c passes `cls.signon == SIGNONS` to
  VR_TimeFrameEnd, never true on a dedicated server, so its map load stays open (the file cache and the heap's load
  hold on for the whole game; since startuplog a "still in map load" line every 2 s). Probably
  `|| (isDedicated && sv.active)`; not changed here (it shifts every dedicated scenario's timing: wants its own runs).
- **Checks**: dev and ThinLTO 0 warnings; scenario_check 136/0, load_models 4/0, vr_run_check 12/0 (after the coop
  spawn change).

Next phase: B26 -- ours first: mp_fuzz's client crash on corrupt packets down at signon (reproduce with
`suite --only mp_fuzz --suite-repeat N`); mp_disarm's magazine stopping 2.5 units off the port (reload debug 2:
`QVR_RELOAD_DEBUG=2`); the dedicated server's load window; then 3 consecutive clean runs plain and under --load 16.
His: deploy the room service (`Misc/rendezvous/deploy.sh`) and try a room code between two real routers; a relay only
if real punches fail often; IPv6 or a public room list if he wants them; B10v's VR checks.

### B26 results: B25b's crash found and fixed; a dedicated server's load window; the magazine's placement (2026-11-18, dev no-LTO, loopback)

- **The crash: svc_setview took any number (a game bug, single player's code too).** B25b's "no dump" was wrong: the
  report and .dmp were in the harness base's `quakevr/crash` (`2026-11-18_04-53-44`): an access violation in
  `qvr::hands::current` reading `cl_entities[cl.viewentity]`. cl_parse.c's svc_setview took the server's short as sent;
  a corrupt packet (or a hostile server) put the view entity outside the array, read by the next frame. Now a Host_Error
  as CL_EntityNum's (0 to cl_max_edicts - 1); the start-sound entity check `>=` (was `>`, one past the array); Rogue's
  team swatch reads the scoreboard only for a player's own view. Commit 2c2563ee3 on origin/next (branch
  `agent/mp-next-fixes`), merged into agent/mp.
- **Reproduced, deterministically**: mp_proxy.py `inject` (given bytes after the next packets' messages) and a fifth
  corruption, a whole command after the message (a code the parser takes, its fixed argument bytes: the parser's own
  checks meet the values, the message ending right after, as the crash needed); the corruptions' own seed, printed
  (`fuzz --seed N`). `fuzz --part inject`: every svc code (but disconnect and serverinfo) and every svc_quakevr
  sub-command with all-0xFF and all-0x7F arguments, 246: before the fix B crashed at the address of B25b's report;
  after it 246 of 246, B running (144 left with a reason and joined again). The suite's mp_fuzz sends FUZZ_HOSTILE (the
  three setviews, the scoreboard and stat indices) too. `fuzz --part signon` (--repeat N): B's joins under corrupt
  packets down (60 joins: every one ended, in the game or left with its reason). The harness now copies any crash
  report written during a run into its folder and prints `mp_crash:` (the exception line, the .dmp's path).
- **The dedicated server's load window**: host.c's VR_TimeFrameEnd now ends a dedicated server's load at its first frame
  with `sv.active` (was `cls.signon == SIGNONS`, never: the file cache and the heap's load hold kept for the whole game,
  the start-up trace's "still in map load" every 2 s: 129 lines in one mp_fuzz server log before, 0 after; "map load
  (map e1m1) end (took 328 ms)"). Commit 4d44dedcf on `agent/mp-next-fixes`; single player and listen servers unchanged.
- **mp_disarm's magazine (4 of 18 in B25b): the placement's, not the seating's.** The server's reload rows (debug 2)
  show the gun's port moving 0.6-1.3 units as B's off hand comes to it (once 1.2 sideways and back), and the round
  landing up to 3.1 units from where B's game aimed it (along his facing, at the start of the placement); an open-loop
  placement then misses the leniency (port radius + magazine radius, 2: unchanged) and each new placement moves the gun
  again. A player watching the magazine and the well corrects: the test now does, from the server's last row after
  each placement at the port (`vr_mock_hand_to off lport 0 dx dy dz`, the mock's new world offset), up to 4 at the
  port, the gaps noted. Under --load 16, 4 of 4 seat (one at the 4th placement: gaps 3.7, 3.7, 3.1, 1.0: the old test
  failed there). Open: why the gun moves as the off hand nears it, and the 3-unit gap between B's view of the held
  round and the server's (the remote client's drawn held round against the server's grip offset?): in VR a remote
  player may see the magazine at the well 1-3 units before it seats.
- **Runs (the acceptance)**: full suite (47 scenarios) 3 consecutive clean plain runs, then 3 consecutive clean under
  `--load 16` (scratch/b26_c1_chain.txt: 6 of 6, 0 checks failed; ~21 min each). The magazine seated at the first
  placement at the port in all six (gaps 0.3-1.5), mp_fuzz's hostile commands survived in all six, no crash report.
- **Checks**: dev and ThinLTO 0 warnings; scenario_check 136/0 (a first run 135/1: bench_crowd_ad "what it placed is
  gone", B8d's known flake, while a --load 16 run went), load_models 4/0 (a first run without a verdict under the same
  load, then 4/0 alone), vr_run_check 12/0.

Next phase: B27 -- ours: the held magazine's gap between a remote client's view and the server (up to 3 units) and the
gun moving as the off hand nears it (mp_disarm's debug-2 rows); a wider structured fuzz up (clc commands with fixed
argument sizes, as B26's down). His: deploy the room service (`Misc/rendezvous/deploy.sh`) and try a room code between
two real routers; a relay only if real punches fail often; IPv6 or a public room list if he wants them; B10v's VR checks.

### B27 results: the held round agrees with the server; a guest's commands up (unfinished) (2026-11-18, dev no-LTO, loopback)

- **The gun moving as the off hand nears it: the drawn pushes (single player's code too).** B's carry and body-collide
  traces (`QVR_RELOAD_DEBUG=3`): the held props' meet (vr_held.cpp meetFrame) pushed the round and the gun apart by up
  to 2.8 cm each at the 6-under pose (a rocket; a magazine 1.4), and the off forearm blocked the gun by 5.2 cm
  (vr_selfcollide.cpp betweenHands, eased by the hand's distance to the port: a magazine's length under the well). Looks
  only, but the gun's drawn port is the one the move sends and the server tests against, while the server holds the round
  unpushed. Now `held::roundPortEase`: both eased out by the round's own point (heldRoundRef) from twice the old reach of
  the port; Collision Leniency 0 still keeps them apart (reload_test.sh's check). Commits f943eee34 + 04da5cffe on
  origin/next (branch `agent/mp-next-fixes-b27`), merged into agent/mp.
- **The round's 0.5-3 unit gap: the remote client's guess of its place in the hand.** For 0.1 s after taking it B took
  its place from where the server had it and this frame's controller, the server's hand a network delay older: a round
  taken up on the move was drawn off by the hand's travel in that delay (0.55, 1.35, 2.75 units in three plain runs of the
  same take; more under load), for as long as it was held. Now the server sends each remote client his hands' held things
  and their places in his hands (QVR_SVC_HELDPLACE, a snapshot record after the predict state: unchanged, free;
  integers), and holdFrame takes it from the first frame (and its changes: a regrip, Held Object Offsets). Single player
  and a listen host: as before. Network version 38.
- **mp_disarm**: a new check, B's drawn round and port against the server's rows at each still pose (under 0.5): round
  0.08 / 0.25, port 0.04 / 0.06 (magazine / rocket), the same under `--load 16` (were 0.5-2.9 and 0.6-1.3). The magazine
  seats at the first placement at the port (gap 0.0-0.1; the rocket 0.2-0.3: what is left of the meet's push at 6 under).
- **Fuzz up (unfinished)**: `fuzz --part injectup` (FUZZ_HOSTILE_UP: the string commands a client may send with values
  their parsers must check, kick and ban; `fuzz_sweep_up`: every clc code but disconnect with 0xFF/0x7F x 0, 12, 64
  bytes, 136 in all). Its run: the server and A untouched, but only the first clc 0 drew a drop and B's "kick #1" did not
  reach A: the bytes appended after one of B's packets' messages up are not what the server parses yet (B26's down
  injection works). Not in the suite. NetQuake lets any coop client `kick` the others and `ban` addresses (host_cmd.c
  Host_Kick_f, net_dgrm.c NET_Ban_f: refused only in deathmatch): a room-code stranger could empty a coop game.
- **Runs (the acceptance)**: full suite 3 consecutive clean plain runs, then 3 clean under `--load 16`
  (scratch/b27_c3_chain.txt: 6 of 6, 0 checks failed; ~21 min each); mp_disarm's view gaps in all six: round 0.05-0.25,
  port 0.04-0.06. (Two earlier chains void: the SP tests beside the first, then its orphaned games holding the port.)
- **Checks**: dev and ThinLTO 0 warnings; reload_test 116/0, front 34/0, magcollide 11/0, pouch 29/0,
  scenario_check 136/0, load_models 4/0, vr_run_check 12/0. (A first chain was void: the kit's run.sh writes its script
  into the same base's autoexec.cfg (`bases/mp/qbase`), and the SP tests ran beside it: never run both at once.)

Next phase: B28 -- ours: why bytes injected up are not parsed (mp_proxy's append on B's packets: their reliable or
unreliable framing), then `fuzz --part injectup` into the suite; a guest's kick and ban refused (src_client, not the
listen host's own: conservative, a server setting to allow them). His: deploy the room service
(`Misc/rendezvous/deploy.sh`) and try a room code between two real routers; a relay only if real punches fail often;
IPv6 or a public room list if he wants them; B10v's VR checks.

### B28 results: a guest can't kick or pause; the hostile commands up parsed (2026-11-18, dev no-LTO, loopback)

- **Why B27's bytes up went unread: they followed acks.** mp_proxy appended the next command to the next game packet
  either way; up, most of B's packets are acks of the server's reliable chunks, whose bytes after the header
  Datagram_GetMessage never reads (a reliable chunk's would join the next chunk's). Now only after an unreliable
  datagram's message (B sends one each frame in the game). And fuzz_inject_up judged a drop by B's own game, which is
  told only when his connection can take the svc_disconnect then (SV_DropClient): now by the server's slots
  (vr_mp_status's mp_client rows). With both: `fuzz --part injectup` 138 sent (35 hostile string commands, every clc
  code with 0xFF/0x7F x 0, 12, 64 bytes), B dropped 98 times, each with the server's reason (unknown command char 74,
  badread 12, malformed QVR_CLC_MOVES 6, malformed QVR_CLC_VOICE 6); the server and A untouched, no crash.
- **Its first run: a guest's `kick # 1` kicked A** (NetQuake's Host_Kick_f refuses a client only in deathmatch; B27's
  `kick #1` was one token, a player named "#1"). The audit of every client-executable command (Cmd_AddCommand_ClientCommand:
  status, god, notarget, fly, name, noclip, setpos, say, say_team, tell, color, kill, pause, spawn, begin, prespawn,
  kick, ping, give, vr_scene_clean/count and the vr_* state reports; cmd.c refuses any other from a client, `ban`
  too: map, changelevel, restart never reach the server from a client): **kick, pause and vr_scene_clean** act on
  everyone's game. Now `VR_GuestMay` (vr_server.cpp): a guest (every player but a listen host's own, the loopback's;
  a dedicated server's every one) is refused them and told so ("kick: refused, only the host may (this server's
  vr_mp_guest_admin is 0)"), unless `vr_mp_guest_admin 1` (default 0, archived; Debug > Network: Guests May Kick and
  Pause); the server's console always may. A client's `ban` (the console's own command) is answered now (cmd.c).
  **Talk**: say, say_team and tell from a guest under QuakeWorld's flood guard (more than 4 in 8 s: silent 10 s, told).
  **Self-only cheats** (god, notarget, fly, noclip, give, setpos) stay NetQuake's in coop (refused in deathmatch):
  they change only the player's own game; sv_cheats (the rerelease's) is not consulted.
- **setpos (single player's code too)**: `setpos 1e30 -1e30 1e30` kept the player at 1e30 (his origin sent to every
  client; `setpos nan` was reset by SV_CheckVelocity); a huge angle overflowed the integer casts writing it. Now
  refused, told why, unless finite, within 4096 units of the world's bounds and 36000 degrees. Commit 1e3594c65 on
  origin/next (branch `agent/mp-next-fixes-b28`), merged into agent/mp (with origin/next's 4 new commits).
- **Tests**: new suite scenario `mp_guests` (a listen host and a guest: the guest's kick, pause, vr_scene_clean and ban
  refused and told, A in, the game not paused; the host pauses and unpauses; the guest's 4 messages then silenced,
  the host's own talk not held; `vr_mp_guest_admin 1`: the guest's pause goes; the host kicks the guest): 13/0. The
  suite's mp_fuzz now sends the 138 commands up too (after the hostile commands down, ~3 min).
- **Runs (the acceptance), unfinished at the session's end**: the chain (`scratch/b28_all.sh`: 3 plain suites, then 3
  under `--load 16`; tallies in `scratch/b28_all.txt`) was left running. Its first plain suite (48 scenarios): all
  clean but mp_guests' last check, "the host kicks a guest" judged by B's "Kicked by" line, which B's game gets only
  when his reliable channel is free (SV_DropClient): the check now reads the server (B's slot gone, A's kept); the
  second suite ran the old check, the rest the new. mp_fuzz (with the 138 up) 17/0 there.
- **Checks**: dev and ThinLTO 0 warnings; scenario_check 136/0, load_models 4/0, vr_run_check 12/0.
- **Open (minor)**: `kick # N` with no message prints "Kicked by X: " and an empty message (NetQuake's parse of the
  number's tail); a dropped client whose connection was busy learns of it only at his timeout (vr_mp_client_timeout).

Next phase: B29 -- ours: a dropped client told even when his reliable channel is busy (an unreliable notice beside
SV_DropClient's svc_disconnect: now he learns at vr_mp_client_timeout); name and colour change floods (each broadcast
to every client: the talk guard's for guests); a structured fuzz of B's join up (his signon messages corrupted, as
B26's down). His: deploy the room service (`Misc/rendezvous/deploy.sh`) and try a room code between two real routers;
a relay only if real punches fail often; IPv6 or a public room list if he wants them; B10v's VR checks; whether a
guest's self-only cheats (god, noclip, give...) in coop should follow sv_cheats too.

### Bots phase 1 results: a flat-screen player's hands for bots; coop bots (2026-11-18, dev no-LTO, loopback)

- **What**: bots' hands posed by vr_flatplay.cpp's own code (`poseBot`, QC `botflatpose`): one gun or two, the
  overhead swing (its blow the melee's sweep), the flat guard raised by skill (`vr_bot_parry_chance`, -1 by skill);
  dual wield (`vr_bot_dual_wield` 1); coop bots back on the next level with what they had (`VR_CoopNoteBots`,
  `botcarryparms`). Details: ROUND25.md, "MP bots phase 1".
- **Tests**: mp_dm gains vr_dm_test 9 (bot parries bot) and 10 (dual wield); test 3's guard is level (`LEVEL_GUARD`,
  square to an overhead blow) and the bot is placed by the human's head yaw; tests 4-6 with `vr_bot_dual_wield 0`, and
  a bot's own second gun put away, not dropped (dropped, its reach took it instead of test 5's nailgun: 1 in 6 suites).
  New scenario mp_coopbots (13/0). mp_dm 25/0 three runs after that fix; mp_lockstep_dm 23/0.
- **Runs (the acceptance, B28's owed one too)**: ThinLTO 0 warnings; the chain (`scratch/bots1_all.sh`, tallies in
  `scratch/bots1_all.txt`; 49 scenarios): plain 1 failed only vr_dm_test 5 (above, fixed in the test), plains 2-3
  clean, then 3 clean under `--load 16`; a 4th plain on the fix: clean (49 of 49): so 3 consecutive clean plain suites (2, 3, 4) and 3 under load.
- **SP**: scenario_check 136/0, load_models 4/0, vr_run_check 12/0, flatparry 43/0, flathands 76/0, flattwohand 9/0,
  flatmelee 12/0, flatplay 31/0; flatswing 4/1 twice (its knight trials 7 and 8 of 11 land, 9 wanted: the knight moved
  off its spot (56, 29 units) or past the default swing's reach at 28; not from this phase's code, whose changes are bot
  only; agent/mp's own earlier divergence from next (vr_pmove_own?) unverified).

Next phase: B29 as planned (a dropped client told when his reliable channel is busy; name and colour floods; B's
signon fuzzed), and bots phase 2 if he wants it (reloads, doors and lifts in coop, a bot's two-handed grip).

### mpready results: the open items before next (2026-11-19, LTO build, loopback)

- **flatswing 4/1: not agent/mp's.** On today's agent/mp (b67c7f8eb, origin/next merged) and on origin/next
  (1dc69cc10) the knight table is the same, line for line, run after run (`sv_random_seed 1`): 10 of 11 land (raised 22:
  the knight walked off to 43.5; default 24: one blow, 25 damage), 5/0 on both. Bots phase 1's 8 of 11 was the random
  sequence of the tree before origin/next's merges; nothing MP-only moves the knight. The trials stay random against
  every change that draws a random number elsewhere (a margin of 1 today): no bisect (it does not fail on agent/mp).
- **vr_dm_test 5: the test's.** The rv1119 suite's failure ("Shotgun taken into the off hand, 8.9 units from its
  handle"): the bot's reach takes the nearest gun lying within 48 units, and a shotgun lay nearer than the test's
  nailgun (one its off hand had taken earlier, dropped at its feet by the test's hold, or A's knocked out by test 1).
  The test now takes away every other gun and backpack within 160 units of the bot as it lays the nailgun (printed:
  "3 other things lying near the bot taken away" in a run that would have failed). Two more mp_dm flakes seen while
  repeating it, both the test's: **vr_dm_test 3** (2 passed 3 failed: "the bot's sword swing begun" false: A's crowbar
  brought into his pose moves fast enough to read as a headset's swing, the held bot raised its flat guard (chance by
  skill) and W_Fire_Melee refuses a swing while the guard is up; tests 2 and 3 now hold the bots' guards down, test 9
  tests the guard) and **the grip step** ("A's main grip on the server" timed out: a bot's nailgun killed A after
  test 1; `respawn_if_dead` brings him back by his trigger first, a check). mp_dm 26/0 in 3 of 3 after.
- **REVIEW_MP_2026-11-18**: every finding has its status line (1-16 done; 17's items done by their groups and the
  integrator). Nothing small is left open there. Still open from the phases (not the review's): B28's `kick # N` with
  no message ("Kicked by X: " and nothing), B29 (a dropped client told when his reliable channel is busy; name and
  colour floods; B's signon fuzzed), the S_MixJoin calls and the voice ring's zeroing not run against the real mixer
  (the harness is `-nosound`), the drawn hands, holsters and pouch outside the move guard (others' drawing only).
- **Runs (not accepted)**: ThinLTO and dev 0 warnings. The chain (`scratch/mpready_all.sh`, tallies in
  `scratch/mpready_all.txt`): no suite clean. **mp_guard failed in all 6** (11/1: "A's honest moves: none held", one
  speed held in ~410 moves every time; 12/0 in the review's runs and bots phase 1's 49-of-49 chain): a regression since,
  most likely from origin/next's two merges (b9c0ee46c, b67c7f8eb); not bisected (out of budget). The first thing to
  do next: which value (vr_moveguard.cpp counts speeds without naming them) and which merge. The rest look like load and
  timing (another worker's tests ran beside the first suites): plain1 mp_jitter's lift; plain2 mp_spoof 18/2 (A sees B
  move in 750/732 ms, 500 wanted; B's own round trip 467/623 ms through the proxy); plain3 mp_armed, mp_lagcomp
  (807 ms rewinds), mp_disarm (port 23.3 units at one pose), mp_voice/voice_lossy (late frames); under --load 16
  mp_nethud, mp_bandwidth, mp_melee, mp_predict, mp_coop_lag, mp_grab, mp_disarm. mp_dm clean in all 6.
- **SP**: scenario_check 136/0, load_models 4/0, vr_run_check 12/0, flatswing 5/0, flatparry 43/0, flathands 76/0,
  flatmelee 12/0, flatplay 31/0 (LTO build).

### mpguard results: mp_guard's held speed and a clean suite (2026-11-20, nothing else running)

- **mp_guard 11/1 was the test's honest move, not a next commit.** `vr_mp_guard_debug 1` (new, a server's debug cvar)
  prints each value the move guard holds by name ("mp_guard_held: slot=N throwVel[1] 41.089 of 40.000"); a share
  under 1 also prints values over that share of their bound ("mp_guard_near:"). The held value: A's main hand's throw
  estimate. The test jumped his hand in one frame from the mock's standing pose to over his head (1.48 m: the mock's
  scripted velocity reads it as 1.48 m in 50 ms, 29.6 m/s) and turned the launcher 80 degrees, its flick at the
  launcher's length on top: 35.4, 39.4, 35.6 m/s on agent/mp before next's merges (b9c0ee46c^1, dev build), 39.4 and
  41.1 after, over 40 in 3 runs of 4 on the LTO build. No single commit: the estimate rides the frame times, and next's
  frames came faster (its render changes; no change to vr_throw, the guard or the mock's motion in the range). The fix
  is the move: A's arm now carries the launcher pose to pose in a 0.4 s play (`GUARD_ARM_PLAY`, at most 3.7 m/s),
  his largest value 18 m/s (the off hand's jump to the grip). 12/0 x3 (dev), then in every suite below.
- **mp_predictslide** (plain1: corrections 4.96, 2 results off by 4.5): a move the server never had (5% lost each way;
  `mp_netmoves lost=1` in that segment): his replay ran it, the server did not, one move's correction. The checks now
  take the server's lost count for the segment and allow that many states off by a move's travel at most
  (`LOST_MOVE_MOST`). Open (a design question, not a bug): the server could run a lost move as the previous one.
- **mp_dm test 9** (plain3: "parry: none: player at -157 degrees, 14 units", "player is stuck" spam): the swinger was
  put 36 units ahead of the defender, inside A. The defender is now turned 45 degrees at a time until that place is
  free of walls and other players (`VR_DMT_PairFree`; case 1 keeps case 0's place).
- **mp_voice_lossy** (load3: A 0.38, C 0.56 of frames heard): the two talkers' shares are now taken over 250 of A's
  frames, not 100 (the counts are read one game after another, frames in flight at the ends counted on one side);
  0.47/0.48, 0.44/0.51 since. The mute check (B received 3 of A's frames after the mute, 1 run) counts from 8 frames
  held back at the server (the frames on B's 50 ms link at the mute arrive first).
- **mp_fuzz's crash (B26's open one) found**: both fuzz failures of the confirming runs were B's game aborting in
  `rope::parseCorners`: a corrupted message cut short, `MSG_ReadByte`'s -1 as the corner count, a resize to SIZE_MAX.
  A count under 0 now ends the parse (vr_rope.cpp).
- **Runs** (the suite after these fixes, nothing else running; the kit's `--lto` step relinked nothing, so these ran
  the dev build): plain 2 of 3 clean (plain1: mp_fuzz's crash above, mp_rejoin "B back from his crash has what he
  had": items 4097 -> 4129, a rocket launcher picked up as he came back, 1 run), load 2 of 3 clean (load2: mp_fuzz's
  crash). mp_guard 12/0 in all 6 (and 3 of 3 in the first set). Not yet 3 + 3 consecutive: the rope fix is newer than
  them: with it, mp_fuzz clean 2 of 2. ThinLTO (the first set's build) and dev 0 warnings.
- **SP** (dev build): scenario_check 136/0, flatparry 43/0, flathands 76/0, flatmelee 12/0, flatplay 31/0.
- Seen, not this work's: every mp_dm run's A logs a QC assertion in `VR_AmmoUtil_GetAmmoCountByAmmoId` (a bad ammo id).

### Ready for next?

**What still blocks the merge (his):**
- His headset checks: B10v and B10v part 2 (voice in VR: the off hand at the mouth, `((o))`, the Voice Chat page, a
  real microphone and both spatial mixers, none run headless), the room-code pages in a headset (B23-B24), a remote
  player's body, hands and held things seen from a headset (B2d-B12), melee and parry between two headsets.
- **A room code between two real networks**: deploy the room service (`Misc/rendezvous/deploy.sh`; protocol 2 since
  the review's finding 11: the deployed service must be this one before a build with it ships) and join a host behind
  another router. Only simulated routers (`mp_nat.py`, `-natgateway`) and loopback have been run.
- **The NAT decision**: whether a relay is built for symmetric NATs (today they are told, no relay: README's design
  note), and IPv6 or a public room list. Until he decides, `vr_mp_room 1` and `vr_mp_rendezvous vittorioromeo.com:26100`
  are the defaults: every multiplayer host registers with his service (a dead service costs nothing: the lookup is
  off the game thread with back-off, finding 6).
- Optional: whether a guest's self-only cheats in coop follow sv_cheats (B28), bots phase 2.

**What the merge touches** (372 commits; 195 files and about 31k lines outside the vendored code, plus Opus and
RNNoise vendored: 237 files, 213k lines, two new VS projects): the datagram layer (`net_dgrm.c` one server socket,
room codes, per-connection secrets: QVR_NET_VERSION 39: builds before it and after it refuse each other in multiplayer, told why),
`sv_main.c`/`sv_user.c`/`host_cmd.c` (snapshots, moves, guest gating, command budgets), `host.c`, `menu.c` (Host/Join
Game), the QC (client.qc, combat.qc, weapons.qc, the player disarm grab, FrikBot's hands and walk), and about 60
`Quake/vr` modules (new: netsnap, netmove, predict, pmove, lagcomp, moveguard, clientprefs, heldpose, mpavatar,
coopsave, voicechat/voicerelay, serverbrowser, netlookup...; changed: climb, view_body, avatar, held, slide, physics,
the cvar table's QVR_RULE/QVR_PREF/QVR_OWN flags on 880 rows).

**Risks for single player** (each checked by the SP runs above, none seen failing; his play is the real test):
- `vr_pmove_own 1` (his call, B11b): single player walks through `vr_pmove.cpp`, not sv_phys.c's walk. Frame-identical
  in the traces (pmtrace_all.sh); the hands' swim strokes and crawl pulls push one frame later; the room-scale walk
  not traced (the mock does not walk). Fallback: Debug > Network, "Movement: Own Walk Through Moves" off.
- The own player through per-player state: the body, hands and held weapons posed through per-slot swaps (B2d,
  B11c), the own player read from `progs::hostSlot` (finding 3), `cvar_hget` stopping at a player `self` (finding 13:
  a touch function whose `self` is the player no longer reads `other`'s preference).
- Settings: the server-rule and preference lists now come from the cvar table's flags (finding 14); a setting tagged
  wrong would be overwritten by a host's value only in multiplayer, never in single player.
- `setpos`, `svc_setview` and the start-sound entity checks are stricter (on next already); the dedicated server's
  load window (host.c) changes no listen or single-player path.
- Size and start-up: Opus and RNNoise are linked into the exe (RNNoise's model is a 141k-line table); voice chat opens
  nothing unless used.

## Review fixes (mpfix, 2026-11-20): areas A (transport) and C (avatars, voice)

The deep review before agent/mp merges into next (`qvr-kit/scratch/mprev_A.md`, `mprev_C.md`). Finding -> state.

- A1 (high) fixed: a connect request gets a slot only with the server's cookie for its address and port
  (CCREP_CHALLENGE 0x86, 9 bytes, smaller than any request; SipHash-2-4 of the address and the 10 s time step, the key
  drawn once a run; network version 40). A server's new client keeps one reliable chunk in flight until it acks one
  (qvrHeard) and is dropped after QVR_UNHEARD_TIMEOUT (15 s) without an ack (300 s before). The version reject still
  answers before the cookie (one packet each, so an old client is told why). mp_spoof step 5.
- A2 (medium) fixed: a ghost connection closed by the driver is marked (qvrClosing: NET_GetMessage -1, its slot drops
  and frees it), never freed under its slot; mp_spoof step 5 sends the rejoin pair in one frame (clients 3, 2, 3).
- A3 (medium) fixed: one automatic Map Library install per `connect` the player started (vr_mpdownload.cpp
  autoInstalls); a package already installed, or the stock game, still mounts and rejoins (rejoinedFor guards loops).
  No size prompt (the first install is still automatic, as designed). Not harness-tested (needs a server switching
  packages).
- A4 (medium) fixed: a host punches a joiner 5 times over 1 s (80 over 8 s before); the joiner's requests go on 8 s.
- A5 (low) fixed: ReadShared reads 1024 datagrams a call at most.
- A9 (low) fixed: an empty server name becomes "*" before the duplicate-name suffix.
- A12 (low) fixed: vr_mp_clientkey from DgrmRandom.
- A6, A7, A8, A10, A11, A13, S1-S3, O1, O2: not done (budget); lows, listed in BACKLOG.md.
- C1 (high) fixed: heldpose decode clamps slot/leaveSlot to the weapon table, helpKind, spotType, carriedGrip,
  leaveFlip to their domains; weapons::value returns 0 for a slot past the table. mp_fuzz sends such a record.
- C2 (medium) fixed: the voice jitter estimate updates only on a frame newer than the newest, by seqDiff (the copies and
  the 16-bit wrap no longer read as jitter); vr_voicechat_selftest 4b (0.0 ms on a perfect link with copies).
- C3, C4 (low) fixed: an uncalibrated avatar's heights clamped to the calibration's height range; calibrations from the
  server clamped by calClamp on the client.
- C5 (low, privacy) fixed: push-to-talk ends when the off hand's button is up (whatever ate the release), a menu or the
  console is up, or voice chat is inactive. Not harness-tested (needs a held button eaten by a page).
- C6 skipped: the 8 bytes are what mp_avatar / mp_held check the weapon over the wire with (mp_test.py "wpn").
- C7, C8: not done (budget); BACKLOG.md.

## Review fixes, half 2: snapshots, prediction, lag compensation; QC, menus, harness (mpreview B and D, 2026-11-20)

The deep review before agent/mp goes into next (`qvr-kit/scratch/mprev_B.md`, `mprev_D.md`); branch agent/mpfix2.

| Finding | What | Done |
|---|---|---|
| B1 (medium) | The predict record's 700-byte reserve was less than its largest (744): a datagram overflow ended the server | Fixed: `VR_WritePredict` writes the record into its own 4 KB buffer and copies it into the datagram only when all of it fits (a field added later can't overflow it either); `SV_SendClientDatagram` sets `allowoverflow`/`overflowed` (stack garbage before) |
| B2 (medium) | Lag compensation rewound every target once per pellet (a super shotgun: 14 full rewinds) | Fixed: one scope per shot in the two hitscan funnels every caller goes through, `FireBulletsImpl` (pellets; ended before ApplyMultiDamage: the damage dealt where they are, as melee's) and `LightningDamage` (its three lines); the tracelines inside find the scope open and skip. Test: `vr_dm_test 12` in mp_lagcomp (new builtin `lagcompcount`: one scope begun per blast and bolt, the 14 and 3 tracelines inside it) |
| B3 (medium) | The prediction replay looped over every client entity in each trace | Fixed: the four loops go over `worldtrace::brushModelEntities()` (vr_trace.cpp's per-message brush list, now shared), same entities in the same order |
| B4 (medium) | Two player walks (vr_pmove.cpp and sv_phys.c) | Left: a large refactor, BACKLOG.md section 9 |
| B5 (low) | A reused edict was rewound to its previous occupant's place | Fixed: `entryAt` skips frames older than the edict's `freetime` |
| B6 (low) | The room-scale budget clamps the server's walk, not the replay's | Done (onecopy, 2026-11-22): the budget left in QVR_SVC_PREDICT (`QVR_NET_VERSION` 44), his replay runs `moveguard::budget` on a copy; mp_guard's B6 checks |
| B7 (low) | Per-client snapshot encode work | Done (onecopy, 2026-11-22): the write pass writes the first pass's verified field encodings; `isCosmetic` once per entity a frame |
| B8 (low) | The 100-unit teleport cut at low send rates | Fixed: `netsnap::teleportCut(span)` (100 units, or 4000 u/s times the samples' gap), shared by interpEntity and lag compensation's drawnAt |
| D1 (low) | Single player ran the PvP disarm projectile scan | Fixed: `VR_PD_Can` counts PvP only in coop or deathmatch |
| D2 (low) | "mp_event: joined/left" printed in every game | Fixed: only with `vr_mp_watch` (the harness sets it on every game) |
| D3 (low) | menu_paths.txt stale | Fixed: regenerated (`vr_menu_dump_paths`, 10,033 entries, +29) |
| D4 (low) | Dead `M_Search_Draw`/`M_Search_Key` | Fixed: removed with their switch cases (the enum value kept) |
| D5 (low) | Deathmatch's caps rescanned every edict per removal | Fixed: `VR_DMLoose_Cap` chains the candidates in one pass (edicts' order, same tie-break), then removes the oldest from the chain |
| D6 (low) | A bot's reach kept going to a removed thing | Fixed: `bot_reach_valid` also needs the thing's `modelindex` |
| D7 (low) | Two copies of the grapple's reel and wind (QC monsters/props, vr_pmove.cpp players) | Left (BACKLOG.md 9) |
| D8 (low) | A >4 KB inbox line stalled the harness inbox | Fixed: dropped up to its newline with a console line; checked in mp_lagcomp (a 5,000-byte line, then a command that runs) |

Checks: dev and ThinLTO builds 0 warnings (LTO exe relinked); mp_lagcomp + mp_dm alone 17/0 and 27/0 (vr_dm_test 12 3/0, the inbox check); SP scenario_check 136/1 (bench_crowd_ad, the known AD infighting), flatplay 31/0, flatparry 43/0, flathands 76/0, flatmelee 12/0; suite plain 54 of 57 (mp_coop_lag, mp_lagcomp's move view-time check, mp_voice_lossy: real-time checks, the other fixer's runs alongside); those three alone 12/0, 17/0, 28/0. --load 16 (the other fixer's suite running too: round trips up to 1 s): 33 of 57, every failure a latency or timing check, no Host_Error or overflow in any server log; to rerun on a quiet machine.

## Ready for next (mpfinal, 2026-11-20): the two review halves combined, A/B against the pre-review base

Verdict: **ready**. agent/mp is now half 1 (A, C) + half 2 (agent/mpfix2: B, D) + origin/next (Discord, README, aglod;
the two doc conflicts kept both sides; menu_paths.txt regenerated: unchanged, 10,037 entries). Dev and ThinLTO builds
0 warnings (the LTO exe relinked). SP on the merged head: scenario_check 137/0 (bench_crowd_ad passes), flatplay 31/0,
flatparry 43/0, flathands 76/0, flatmelee 12/0.

The MP suite four times on the head (A), interleaved with four on the pre-review base 8af21793d (B: its own exe,
harness and game code; a second `-basedir` at its tree, so the engine searches its quakevr), in the order A1 B1 plain,
A1 B1 `--load 16`, A2 B2 plain, A2 B2 `--load 16`, about 30 minutes each, with two other workers testing alongside:

| Run | Head (A): scenarios failing | Base (B): scenarios failing |
|---|---|---|
| plain 1 | 2: mp_lagcomp (the moves' view time 560 and 512 ms over its 500), mp_coopbots (vr_dm_test 11: the grunt at 19 health at the 864-frame give-up) | 5 (below) |
| load 1 | 0 | 5 |
| plain 2 | 0 | 5 |
| load 2 | 0 | 5 |

- The brief's named checks pass in all four head runs: mp_nat's symmetric NAT is told within 13-14 s (limit 30; A4's
  1 s punch), mp_spoof 27/0 (16 cookie-less requests challenged, 9 B each; a wrong cookie challenged again; the echoed
  one gets a slot; the unheard drop at 15.1 s), mp_lagcomp's one rewind per blast and bolt (vr_dm_test 12),
  mp_coop_lag 12/0, mp_voice_lossy 28/0.
- mp_lagcomp's moves' view time is 350-397 ms in the other three head runs, the base's 345-411: a load burst in
  plain 1, not the review.
- mp_coopbots: 6 more takes each, alone: head 6/6, base 6/6 (head 9 of 10 overall). The fight against the grunt is
  the bot's aim over 12 s of real time; one give-up in ten, not tied to a review change (D1, D5, D6 don't reach a
  bot's fight with a monster). Left as a rare flake (BACKLOG.md section 1).
- The base fails the same 5 in every run, none of them on the head: mp_predict and mp_predictclimb (the start map:
  the clients' `Host_Error: Model *43 not found` under the A/B's two base dirs, the setup's), mp_predictslide (no
  sliding states), mp_melee (the two upright-crowbar dirparry checks), mp_flow (Load Autosave's coop listing). Base
  only, so not chased.

## The slide pose and the dead body as others see them (mpslide, 2026-11-20)

His words: "the sliding player is just crouched" from another client; "their ragdoll is the default Quake ranger model,
not their own customized player model".

- **The slide.** What was wrong: vr_avatar_pitch.cpp's slide source read only the local player's slide
  (`flatmove::slidePose`, `slide::headsetPose`) and skipped another player's body outright, and nothing about a slide
  was sent: his copy got the crouched box's crouch only. The fix: the server's slide (`slide::onServer`, flat or
  headset) is bit 128 of the avatar's gear byte (`avatarGearSliding`; the armour stays its low two bits; no new message,
  no protocol version change: an older client would only misread a slider's armour colour), `Avatar::sliding` ->
  `Subject::sliding`, and the same pose code takes it (`BodyPitch::sliding`, the neck back:
  `flatmove::slideNeckBack(other)`). Tests: `mp_slide` (flat B: his own body sliding 0.92, A's copy 0.57, leant
  back), `mp_slide_vr` (a headset B's vr_slide: A's copy 0.51). Open: a remote flat client's own slide pose is his
  client's guess (crouch held, on the ground, fast: `flatmove::sliding`), longer than the server's slide, so his copy's
  ease peaks lower; a headset client of another server poses no slide of his own (`slide::headsetPose` is the host's).
- **The dead body.** What was wrong: the body a player leaves (QC vr_deathdoll.qc's player_body, or Quake's body
  queue) is `progs/player.mdl` in his colours, drawn as that model (its ragdoll on a listen host and in single player,
  its death animation on a client of another server); his avatar body (build, look) is only drawn on his living
  entity, which the others are not sent while he is dead. The fix (`mpavatar::addDolls`, every frame, single player
  too): an entity drawn with `progs/player.mdl` (or its ragdoll's skinned copy) whose colormap is a player's and which
  is not his own entity is his dead body; it is drawn as his body by the routines that draw a living player's copy
  (`setup`, `setupBody`, `view::drawBody`, `avatar::poseOther`), with `Subject::doll` (`avatar::DollPose`): the
  trunk, head and legs from the doll's rig bones (`ragdoll::drawnPart`, or `ragdoll::bonePose` of its frame on the
  entity), each body bone's bind turned onto its rig bone's rest direction then with it (`solveDoll`), the legs
  reaching its ankles scaled to his legs, bending to its knees; the arms by `solveArm` to his jointed hands drawn open
  at its wrists. His calibration (build, bracers, height, scale) and look (tone, gloves, nails) as sent; your own
  dead body your own settings (`calOf`: `CalState::own`). Headless while the Immersive death view is in its head
  (`ragdoll::headHidden`). The ragdoll's physics, grabs and hits are unchanged (only the drawing). Off with
  `vr_mp_body 0`. Test: `mp_deathbody` (20/0): a VR listen host and a flat client, each dying: host-own (ragdoll),
  client-other (animation), host-other (ragdoll), client-own (animation): the dead player's build's model, his tone and
  gloves, both hands, its pelvis 7.7-7.9 units from the doll's origin. Single player: `vr_hurt 110 0` drew
  `mp_doll` with the brawny build and tone -0.50 (a screenshot from `vr_mock_camera`).

### Still the ranger after a respawn (mpbugs3 found it, mpbugs4 fixed it, 2026-11-21; the avatar ragdoll rig planned)

His words: "Killed bot/player ragdolls are still the Quake ranger model. Could it be possible to rig our own custom
model (the one we use for the IK) and use that as a corpse ragdoll?"

- **Measured** (one listen host, coop, a FrikBot bot, `vr_hurt 110 1`, then `vr_mp_avatars` and `vr_mp_near`): while
  the bot lies dead its body is drawn as its avatar (`mp_doll: ent=2 model=progs/vrbody.mdl ... ragdoll=1`), on a VR
  host and on a flat one (`-flatscreenmode`), and the dead host's own body too. **Once the bot has respawned** (500
  frames later) its old body still lies there (a dead player's body stays until VR_Knockdown_MakeRoom takes it) and is
  drawn as `player.mdl#rag`, the ranger, with no `mp_doll` line. In deathmatch, where everyone respawns at once, every
  corpse he looks at is one whose owner is up again: hence "still the ranger".
- **Why**: `drawn.of[]` holds one drawn copy per player, keyed by his entity. `addDolls` skips a body whose player's
  copy was drawn this frame (`drawn.of[p].frame == host_framecount`: his living copy, added first by `addEntities`), so
  only a body whose owner is dead (not sent) gets the copy. Not bots, flat players, ragsync or the colormap: those all
  work (the bot's body carries its colormap, `f + 1`).
- **The fix (mpbugs4, built)**: 8 doll slots past the players' (`drawn.of[firstDollSlot + k]`, `avatar::dollSlots`;
  `Drawn::owner` the body's owner). `assignDollSlots`: the bodies in sight nearest first; a body takes its owner's own
  slot while his copy is not drawn (he lies dead: the nearest of his bodies), else the doll slot it had last (its pose's
  state goes on), else a free one (`avatar::forgetOther`, its plates resynced); past 8, the ranger. `setup`,
  `setupBody` and `drawPowerups` take the slot and the owner apart: the calibration, look and armour the owner's; the
  posed body's state (`avatar::poseOther`'s `others[]`, now `avatar::otherKeys` long), its skeleton and pauldrons the
  slot's; a doll has no torch, and the power-up lights and arcs are keyed by the slot (none past the players'). `lookOf`
  and `weaponOwner` read `Drawn::owner`; the look masks and the `mp_doll` report (`slot=`) walk every slot. Harness:
  `mp_deathbody` (23/0): B respawns, A (the host, his ragdoll: doll slot 17) and B himself (a client: his own slot)
  still draw B's old body as B's build.
- **The avatar's own ragdoll (planned, not built)**: today the body is the ranger's ragdoll (player.mdl's rig,
  player_bones.json) with the avatar's mesh posed onto its bones each frame (`solveDoll`). For the avatar's own:
  1. A rig2 for `progs/vrbody.mdl` (`quakevr/rigs/vrbody.rig2.txt`, `make_rig2.py`): its 20 bones from the md5mesh's
     skeleton (pelvis, spine, chest, neck, head, clavicles, upper arms, forearms, hands, thighs, shins, feet), the
     weights from the md5's own (each vertex's bone), the joint limits copied from `vrgrunt.rig2.txt` (a human of the
     same proportions); checked in the Rig Studio (`vr_ragdoll_rig_spans`: no spanning triangles).
  2. The server's body (vr_deathdoll.qc) made with `progs/vrbody.mdl` (the build's model: the calibration the server
     keeps per slot, `cal.slot`) instead of player.mdl, flagged as a player's body (a field the engine reads, not the
     colormap and the model name), so Box3D builds the avatar's ragdoll from its rig2 at the avatar's scale (his
     height: the body's `scale`).
  3. ragsync sends it as any ragdoll (`QVR_SVC_RAGPOSE`: its part count is the rig's, 20); a client draws the vrbody
     parts directly (`ragdoll::publishNet`), skinned with the dead player's look (tone, gloves, nails: `view::lookOf` by
     the body's owner); his hands at its hand bones. `addDolls`/`solveDoll` are then only the fallback (an older
     server's player.mdl body, `vr_mp_body 1`).
  4. Single player and the Immersive death view: the view in the vrbody's head bone (`ragdoll::headHidden` as now).
  Costs: a rig (an afternoon with the Rig Studio), the QC body model switch, the engine's owner field and look; risk:
  the vrbody mesh's proportions per build (a rig per build, or one rig scaled). Its value over the fix above: the
  corpse's limbs are the avatar's own lengths (no stretched ranger proportions), grabs and gibs on the real shape.
- **Built (avatardoll, 2026-11-21)**, a little apart from the plan: the ragdoll code rigs a Quake .mdl (fromAuthored needs
  PV_QUAKE1 vertices), so each build's body is written as one (`Misc/quakevr/make_vrdoll.py`: `progs/vrdoll_lean.mdl`,
  `vrdoll.mdl`, `vrdoll_brawny.mdl`, `vrdoll_fem.mdl`, the md5mesh's bind pose, feet at -24, a quarter-size palette skin)
  with its authored rig (`quakevr/rigs/vrdoll*.rig2.txt`: 19 bones from the md5 skeleton, the forearm's twist helpers its
  forearm, the wrist its hand; the md5's own weights; the armed grunt's limits; the elbow and knee axes from the straight
  bind pose). Seed tables without seeds (authored only, never derived). The server: QC `VR_DeathDoll_Avatar` asks
  `avatarbody(p)` (`mpavatar::serverBody`: his build and size from the calibration the server keeps per slot -- his world
  scale times his eye height over 1.646 m, as setupBody draws him; single player your own settings; no calibration, a
  bot: the default build at the default world scale; vr_mp_body 0 or his body mode none: none) and makes the body his
  build's model, `model_scale` his size (the feet kept on the floor), no death animation. Box3D builds its ragdoll from the
  rig at that scale; ragsync sends it as any (19 parts, its scale); every client still draws the dead player's avatar on
  it through mpbugs4's doll slots (`addDolls` takes `progs/vrdoll*` too; `dollPose` finds the same bone names, so the drawn
  body follows the physics bones one to one, his look, hands and arm lengths his own). id's ranger stays the fallback
  (vr_mp_body 0, body mode none, a worldspawn without `VR_DeathDoll_Precache`, data without the models). Test:
  `mp_deathbody` 34/0 (a short brawny A at 1.55 m, a tall lean B at 1.95 m: each body his build's vrdoll, 19 parts at
  host and client, the leg 21.8-21.95 model units at its scale, B's scale over A's 1.258 = 1.95 / 1.55; the slots and
  looks as before); `vr_mp_avatars`' `mp_doll_rig:` line (parts, scale, the left leg's drawn length). Not done: the arm
  lengths and shoulders in the physics (the rig is scaled uniformly; the drawn arms are his), a flat player's body at
  his flat-screen height (the server takes his VR height), a client of an older server (the ranger as before) and one
  with `vr_net_ragdolls 0` (the doll's single frame: standing).

## Sync audit 2026-11-21: every player feature, seen by another player or not

His words: "Player flashlights are not synced, they should. Can you do an audit of all player features/interactions
and find everything that should be synced but isn't?" No fixes here: the gaps, measured, and the batches to fix them.

**How it was measured.** `vr_mp_near <ent> [radius] [seconds]` (Debug > Reports > Network: Drawn Near Me) lists what a
game draws round a player (each model: whose -- its own view entity, another player's copy, an entity --, alpha,
effects, the powerup glow), its dynamic lights there and the sounds it started in the last seconds by him or near him
(a ring of the last 96, filled at `S_StartSoundPitch`). `mp_test.py syncaudit [--flat B]`: a dedicated server, A and B
200 units apart on e1m1, both with sound on through SDL's dummy driver (nothing reaches a speaker); B does a thing, then
B's report of himself and A's of B are compared, each against the idle baseline: "B has, A not" is a gap. Probes: idle,
quad (impulse 255), ring (impulse 254), a punch in the air, a shotgun shot, a walk. Two runs, B in VR (mock) and B
flat; both 12/0 (every report came). Status: **S** synced, **G** a gap, **E** being done elsewhere, **L** local by
design (right as it is), **N** no such feature. Evidence: [run] syncaudit, [test] an existing mp_test scenario, [code].

| Feature | Status | What another player gets (how) | Evidence |
|---|---|---|---|
| Head, hands (place, turn), finger curls, thumb | S | QVR_SVC_AVATAR's drawn hands, at the snapshot rate | mp_avatar [test]; hand_rig x2 both [run] |
| Body: IK, torso yaw, lean, crouch height, legs | S | the avatar's VrBodyPose and his calibration; vr_avatar.cpp's own IK per copy | mp_body [test]; vrbody both [run] |
| Crouch, prone and crawl, swim, in the air | S | the avatar's moves byte | mp_moves [test] |
| Slide pose | S | the avatar's slide state (agent/mpslide, merged: section above) | mp_slide, mp_slide_vr [test] |
| Climb, hang, mantle (the holds) | S | the moves byte's hold bits and places | mp_moves, mp_climb [test] |
| Two-handed holds, hand-off, reverse grip, parry guard | S | both drawn hands and the drawn weapon's place and turn | mp_held, mp_melee [test] |
| Punches, swings, shoves, bashes, parries between players | S | judged by the server per player; their whooshes are server sounds | mp_melee [test]; knight/sword1-2 both [run] |
| Height, build, gloves, skin, nails, seated, handedness | S | QVR_SVC_AVATARCAL, reliable on change | mp_avatarcal, mp_cosmetics [test] |
| Pauldrons, armour plates | S | calibration and the gear byte (B6c) | vrpauldron(_arm) x2 both [run] |
| Held weapon: model, frame, skin, place | S | the drawn hand's weapon | v_shot, v_axe both [run] |
| Held weapons' parts: the revolver's crane, cylinder and shells, magazines and their rounds, SSG open, the ammo screen's clip, the Widowmaker's and Rotary's spin, ammo type (its model), held and holstered | S | the avatar's VrGunLook and holsters' flags and clip, drawn by your own guns' routines (agent/wpnsync: "Held weapons' state and player lights") | mp_wpnsync, mp_wpnsync_flat [test] |
| The auto pump's stroke and its clacks (`vr/autopump_back/home`) | S | his shot's QVR_SVC_FIRED (already broadcast) starts the stroke on each client, timed as yours; its clacks heard at his copy's gun (autopump::otherFired, syncfix5); his copy's fore-end is drawn still | syncaudit shoot [run] |
| Flashlight, torso torch (belt, hand, gun, head; on/off, where it points) | S | VrDrawnLamp in the move and the avatar: his torch's model and its spot, spill and glow lights (agent/wpnsync); not its visible cone or cord | mp_wpnsync [test] |
| Holstered weapons | S | VrDrawnHolsters (B6b) | mp_cosmetics [test]; holstered v_shot both [run] |
| Hip and leg holster sheaths | S | VrDrawnGear (syncfix batch 2): his sheaths as he drew them; a bot's (and its holstered weapons, what it carries) placed by each client on its body as drawn (view::holsterOnOther) | syncaudit idle, bot [run] |
| Wrist gadget and its straps (VR) | S | VrDrawnGear (syncfix batch 2); its screen's two glow lights (r10/r30) lit at his copy's gadget by your gadget's code (gadget::otherGlow, syncfix2); its screen's contents stay his own | syncaudit idle [run] |
| Ammo pouch and the rounds in it | S | the pouch: VrDrawnGear (syncfix batch 2); the rounds: what it holds (the server's .vr_pouch_kind/.vr_pouch_count, +2 B in the avatar while drawn) posed in his copy's pouch by your pouch's code (view::posePouchItems, syncfix2) | syncaudit idle [run] |
| Grenade pouch (the back) | S | VrDrawnGear (syncfix batch 2) | syncaudit idle [run] |
| Ring of Shadows | S | the gear byte's ring bit (syncfix batch 1): his whole copy faded as he sees himself (view::powerupLookOf's alpha 0.3), deathmatch too | syncaudit ring [run] |
| Quad, Pentagram, Biosuit looks | S | the gear byte's power-up bits (syncfix batch 1), drawn by the code that draws yours (view::powerupLookOf): the glow on his hands and body, its two hand lights, Quad's arcs | syncaudit quad [run] |
| Muzzle flash of another player's shot, his ammo screen's light, his barrel's heat | S | the shot's QVR_SVC_FIRED (already broadcast) drawn at his copy's gun: your flash model (size, time, an AD gun's colour) and the shot's sparks and smoke (gunfx::otherFired, syncfix2; Quake's EF_MUZZLEFLASH light was always seen). His held gun's ammo screen casts your screen's light (r20, emissive::otherWeaponScreenLight); his barrel heats from his shots as yours and glows and lights (r30) on his copy's gun (gunfx::otherBarrelHeat, vr_hotblade.cpp; syncfix5) | syncaudit shoot [run] |
| Spent casings from another player's gun | S | QVR_SVC_EJECTOTHER (the QC's eject, also sent to the others who see him: 8 B a shot each, reliable) thrown out of his copy's gun by your casings' code (vr_shells.cpp ejectFrom; his auto pump's timing, flicks and the revolver's crane are his own) | syncaudit shoot [run] |
| Shot sounds, Quad's attack sound | S | server sounds; vr_gunsfx's layers made on each client | guncock, gun_thump, gun_tail, damage3 both [run] |
| Wounds on another player's body and hands | S | the server's wound event on him (QVR_SVC_WOUND, already sent to every client) painted on his copy's body and jointed hands, placed by capsules round them as yours, in the pool's masks (no new data; up to 3 masks a player, the monsters' pool); kept on his dead body, forgotten when he lives again or leaves (syncfix5). His arms share their skin's texels: a wound on one arm shows on both (yours has a mask a side). Blood thrown onto his hands from what he hits (your spatter) is still yours only | syncaudit hurt (B's own body and hands painted and A's copy of him: wounded:vrbody_bare.mdl, hand_rig.mdl; syncfix6: vr_hurt's broadcast had been dropped) |
| Footsteps | S | the old trigger's server sound in multiplayer (the legs' own steps are single player's: no one else sees the feet's phase) | foot4-7 both [run]; vr_playersteps.cpp:9-12 [code] |
| Mantle grunt | S | a server sound already (climb::grunt, CHAN_VOICE: the audit read the test command's `S_LocalSound`) | mp_climb [test] |
| Slide sound | S | each client plays it on another player's entity from his copy's slide (the server's slide in his avatar: mpslide), the scrape following his speed (slidesound::othersFrame, syncfix5); no new data | mp_slide [test] |
| Voice chat and the talking indicator | S | B10v | mp_voice [test] |
| Chat, scoreboard, frags | S | Quake's | mp_dm [test] |
| Name tags over heads | N | none in the game (the scoreboard names players) | [code] |
| Emotes | N | none | [code] |
| Tracers, impacts, decals, gore and gibs on monsters | S | QVR_SVC fire/tracer FX, hand impacts, wounds, gore sounds | vr_protocol.hpp [code] |
| Drops, pickups, hand-overs, force grabs, disarms, thrown weapons | S | server entities, judged per player | mp_pickup, mp_forcegrab, mp_disarm [test] |
| Grapple rope | S | QVR_SVC ropes | [code] |
| Carried props, ragdolls, all physics bodies | E | queued ragsync | |
| Dead player's ragdoll model | S | drawn through his avatar: his build, tone, gloves (agent/mpslide, merged: section above) | mp_deathbody [test] |
| Death, respawn, gibbing | S | server entities | mp_flow (B4b) [test] |
| The flat player's aiming pose (ADS) | E | queued adspose | |
| Navmesh follow (`vr_nav_follow`) | S | shipped, handled | |
| Flat bodies seen by VR players, and back | S | one code path (mpavatar::addEntities) for either: B flat drew the same hands, body, weapons and pauldrons on A as on himself | flat run [run] |
| Force grab's glow and sparkle on the target, teleport arc | L | the aimer's own cues; others see the item fly or him arrive | |
| Bullet time, time scale | L | single player only (`svs.maxclients == 1`): stays off in multiplayer | vr_bullettime.cpp:247, 1050 [code] |
| Toolgun, Setup, holster setup | L | single player only (section 1.3) | [code] |
| HUD, the gadget's screen contents, haptics, menus, voice notes | L | his own | |

Counts (43 rows): 34 synced, 0 gaps, 4 elsewhere, 4 local by design, 2 not features (the slide pose and the dead body synced by mpslide since the audit; batches 1-2 by syncfix, batch 2's rest and batch 3's flash and casings by syncfix2, the gun lights, the auto pump, wounds and the slide sound by syncfix5, wounds seen in a run by syncfix6; the mantle grunt was a server sound: ROUND25.md "syncfix", "syncfix2", "syncfix5", "syncfix6").

**Side finding.** The Debug menu's Cheats rows (Quad, Pentagram, Ring, Biosuit: `impulse 190-193`) give a holstered
shotgun in coop instead: the single-player test impulses (`VR_TestImpulse`) are skipped in coop, and weapons.qc's
harness "holster give" (4784) takes 190-195 next. `impulse 255` and `254` still give quad and the ring.

**Fix batches**, most noticed first:

| Batch | What | Effort | Risk |
|---|---|---|---|
| 1. Powerups on the copy | Each player's powerup bits in the avatar's gear byte (spare bits; delta'd, on change only); the copy drawn faded under the ring (and his holstered and held weapons with it) and with the glow, hand lights and quad arcs, through the code that draws yours (powerupGlow / the arcs read the player's items, not `cl.items`) | S | low (the ring's alpha in deathmatch: a gameplay fix) |
| 2. Worn gear on the copy | Gadget and straps (his gadget arm is in his calibration), holster sheaths (his holster points are sent), grenade pouch, ammo pouch and its rounds (a count byte): placed on the copy's body by the view's own placement code, per instance as B2d made the body | M | low-medium (a dozen more entities per copy: draw cost with 16 players) |
| 3. Another player's shot effects | His muzzle flash (model and light) and casings from the shot his client fires, on the others: from the weapon events every client gets already (the shot sound on CHAN_WEAPON, or a bit in the drawn hand) | M | low (no new message if the shot's sound is the cue) |
| 4. Blood and wounds on other players | His wound events painted on his copy's body and hands (vr_wounds.cpp's own-player path per copy), bloody hands and drips | L | medium (wound layers per copy: GPU memory, 16 players) |
| 5. His own sounds the others don't hear | The mantle grunt and the slide sound as server sounds (or played on the copy from its moves byte), never twice for the player himself | S | low |
| 6. Name tags (a new feature, if wanted) | A name over each other player's head, faded with distance; a setting | S | low |

Harness: `mp_test.py syncaudit` (B in VR) and `syncaudit --flat B`; each batch's fix turns its "B has, A not" lines
into "both". Tables of the two runs: `scratch/mpaudit_table_vr.txt`, `scratch/mpaudit_table_flat.txt` (not committed).

## Held weapons' state and player lights (wpnsync, 2026-11-21)

His words (11-20): "The revolver cylinder open/close state is not synced in mp. Please sync it and see if other similar
things need syncing (e.g. selected ammo type, state of the magazine in a weapon, etc...)"; (11-21) "player flashlights
are not synced, they should."

Before: another player's held and holstered weapons were drawn as one model at his frame and skin (view::drawHands:
VrDrawnHand's model, frame, skin, place). Everything your own view draws on top of that from your stats (the revolver's
crane, cylinder and shells; the super shotgun's barrels broken open; the magazines and the rounds they show; the ammo
screen's button and clip) was never drawn for anyone else, and the torch was entirely yours (its model, its lights).

Now: the server writes each player's guns as it has them (QC's fields: what his own stats carry) into his avatar
(`VrGunLook` a hand, flags and clip a holstered gun), his client sends his torch as drawn in his move (`VrDrawnLamp`), and
the other clients draw them by the routines that draw their own: `view::drawGunParts` runs the same steps and parts
(`ssgStep`, `revolverStep`, `magSlideStep`, `heldMagazines`, `ssgHeldParts`/`ssgHolsterParts`, `revolverParts`,
`idleAttachments`) on the avatar's look that `setupWeapon`, `setupMagazines`, `setupSsgParts`, `setupRevolverParts` run
on yours (`view::ownGunLook`, from the stats); `flashlight::drawOther` places his torch with your torch's `place` and
lights it with your beam's lights (`beamLights`). `QVR_NET_VERSION` 41 (the avatar and the move grew): the room service
must be redeployed with the new version.

| State (a held weapon, its hands, the player's lights) | How it goes now | What another player sees | Check (mp_wpnsync) |
|---|---|---|---|
| Weapon model, frame (firing animation, the revolver's hammer), skin | VrDrawnHand (his client's drawn entity) | as he draws it | (B2b) |
| Alternate ammo (lava nails, multi rockets, plasma): the other ammo's model | the model index in VrDrawnHand | the other model | (B2b) |
| Reverse grip (blade down), a gun carried by its foregrip, mirrored left-hand models | his drawn entity's angles and mirror bit | as he holds it | (B3) |
| Heavy revolver: crane swung out / home, cylinder's turn, ejector rod, each chamber live / spent / empty | **new**: VrGunLook (QC .vr_rv_hud, .vr_rv_cyl), drawn in its parts by revolverParts | the crane swinging out and home, the cylinder turning, the shells | swing >= 90 after B/Y; shells 6 -> 0 after the eject |
| Super shotgun broken open, its loaded chambers (in the hand and holstered) | **new**: VrGunLook flags (QVR_WPNFLAG_SSG_OPEN), clip; a holster's flags and clip | the barrels dropping open, the chambers' shells | open after B/Y |
| Magazine in or out (QVR_WPNFLAG_NOMAG), the rounds it shows (its frame), in the hand and holstered | **new**: VrGunLook flags and clip, the player's reload-mode bits | the magazine and its top rounds, none when dropped | in with rounds; gone after B/Y |
| A magazine sliding out (the nail pistol's eject), sliding home | **new**: VrGunLook magOut bit; the seat from the flags' change | the slide out and home | (not in the scenario) |
| Ammo screen's clip and its button (vr_weapon_screen_idle) | **new**: VrGunLook clip and clip size, idleAttachments within vr_weapon_world_attach_range | his clip, as a lying gun's screen shows it (not his reserve) | button drawn |
| Widowmaker's and Rotary Shotgun's barrels turning, the AD crossbow's string | **new**: the nearest of the drawn pose's two frames in VrDrawnHand's frame | the barrels in one of 27 poses (no blend) | - |
| The torch: on the belt, in a hand, on a gun, on the head; its model; lit; where the beam points | **new**: VrDrawnLamp in the move (place, turn, shown, lit, reach), passed on in the avatar | his torch's model where he has it; lit, its spot, spill and glow lights from its lens along its axis (unshadowed) | lit: 3 lights, 0.x units off his, cos 0.99+; off: none |
| Torch's visible beam cone in the air, its lit lens glow, its cord | not sent | not drawn (one cone: yours) | - |
| The auto pump's stroke after a shot (vr_autopump) | not sent (his client's own animation of his shots) | the shotgun whole, the fore-end still | - |
| The Widowmaker's motor sound, the lava guns' glow light, the chainsaw's cord handle | not sent (his client's) | not heard / not lit / no handle | - |
| Reloading in progress (a round in the other hand, the pouch) | the round is a carried prop (an entity) | the round in his hand | (B10b) |
| Held grenade (flat-screen quick grenade) | the grenade is an entity | as it is | - |
| Laser sight | none exists (the lasers are the menus' and the arena cards' pointer) | - | - |

Bytes per player (the avatar, down to each other client): +3 B for a player with empty hands and an unlit torch on
his belt left out (each hand's look byte, the torch's flags), +4 B a gun in a hand (flags, clip, its size) and +4 more
for a revolver (its chambers and cylinder), +2 B a holstered gun, +12 B the torch placed (place, turn) and +2 B lit (its
reach): about 25-35 B for an armed player with his torch out. A snapshot client gets the avatar delta-coded against
what it acknowledged, so these bytes cost only when they change (mp_wpnsync's B link down: snap_av 620 B over the
whole run). Up: the torch is 9 values of the delta-coded move (nothing while it is still).

Checks: `mp_wpnsync` (dedicated, both VR mock) 16/0 and `mp_wpnsync_flat` (A flat-screen) 16/0: B is given the heavy
revolver: A draws it in its parts, crane home, 6 shells (A's look of it equal to B's own stats); B's B/Y swings it
out: A's crane at 95 deg (7 reports, the swing's animation included); B's B/Y again throws the shells out: A draws
none; the super shotgun broken open by B/Y: A's barrels down in its parts (3 reports); the nailgun: A draws its
magazine (B's clip 24) and its button, then none once B/Y drops it (1 report); B's torch given to his off hand and
lit: A draws its model 0.10 units from B's own, its beam the same way (cos 1.000), its 3 lights alive; switched off:
none. Debug aid: `vr_mp_avatars` (Debug > Reports > Network) prints `mp_gun:`, `mp_gun_holster:`, `mp_lamp:` for
each other player and `mp_gun_own:`, `mp_lamp_own:` for yours.

Single player is drawn by the same routines as before through wrappers (`magSlideStep(hand)`, `revolverStep(.., hand)`,
`ssgStep(.., flags)` read the stats into the look): vr_magslide_test 1 63/0; vr_rv_test 1 45/1 (below).

The rest: the plain `mp_test.py suite` (dev no-LTO, the author's game running: low impact) 58 of 59 clean, mp_fuzz 16/1
("B joins again after the hostile command up clc 17": not in 60 s) then 17/0 alone; scenario_check 140/0;
flatreload 54/0; flateject 16/1 (the revolver's open pose: the check wants -15 15 -30, moved 0 3 -1, the tunables'
defaults are -35 0 -30, moved 0 -2 1 since "His values as defaults" a49166efa: the test's, not this change's).

Weapon handling sounds (the coordinator's note from the sync audit, 2026-11-21): the sounds QC plays (the revolver's
crane, its shells, magazines in and out, the super shotgun's break and load, the pouch's rounds: VR_RV_Sound,
VR_Reload_* through `sound`) are the server's and heard by everyone from the player. The auto pump's two clacks are
not: the stroke is his client's own animation after his own shot (`weaponfx::playerFired` -> `autopump::fired`,
played with `audio::playAt` at his gun), so nobody else hears or sees it. Left for a next step: an avatar's shot event
per hand (the server's fire message for another player) starting an autopump stroke per avatar hand, its fore-end
drawn by `setupPumps`' parts in `view::GunParts` and its clacks at his gun; `mp_test.py syncaudit` (not on next yet at
this writing) to show the row synced.

Open:
- `vr_rv_test 1`'s "a grunt's body: dead to 2 slugs" fails (twice): the server's damage roll on the grunt, which
  this change doesn't reach (client drawing and avatar fields only); to compare against origin/next.
- Left (his client's own animations, no state on the server): the auto pump's stroke, the Widowmaker's motor sound,
  the lava guns' glow light, the chainsaw's cord handle, the torch's visible beam cone and cord. Each would need its
  own event or field; none changes what the gun is.

## Ragdolls and physics bodies in multiplayer (ragsync, 2026-11-21)

His words (2026-11-20): "Ragdolls don't seem to be synchronized. Especially in coop, they should be, for active
ragdolls, etc." and "Can you check if props and other physics entities are synchronized? They also should."

**How it was measured.** `vr_mp_bodylog <seconds> [radius]` (vr_netragdoll.cpp) logs each tick the physics things
near the player: a listen host the server's truth (each entity's origin and angles, each ragdoll part's joint and turn
at its step's time), a remote client what it draws (the entities' drawn places at its interpolation clock, a ragdoll's
drawn parts, or for one drawn animated the rig bones its frame places). `mp_test.py ragsync`: a VR listen host A and a
flat client B side by side in vrtesthall; the same play twice (a grunt knocked down: an active ragdoll; killed: a dead
one; a crate and a knight's sword thrown tumbling; another grunt knocked down and gibbed), the ragdolls out of B's
snapshot (`vr_net_ragdolls 0`, as before) and in it; each of B's samples against A's at the same server time
(interpolated between A's ticks); "moving": A's moved more than 0.05 units or 0.1 degrees that tick. Real-time
loopback (the harness's), interpolation delay 0.12-0.25 s.

**The audit.** Box3D runs on the server only (2.2, item 1). Status: **F** fine already, **G** a gap (fixed here),
**L** local by design.

| Kind | Simulated | Reaches a client | Turn interpolated | B against A while moving, p95 [run] | Status |
|---|---|---|---|---|---|
| Dead monster ragdolls | server Box3D (parts) | before: nothing (the corpse's death animation at its origin); now QVR_SVC_RAGPOSE | before: no; now each part slerped | before 24.3 units, 153 deg (mean 11.2, 76); now 0.18 units, 0.21 deg (max 1.0, 8.4) | **G**, fixed |
| Active ragdolls (stagger, knockdown and get-up, crawl, ledge climb, grabs, struggles) | server Box3D (parts, ragdoll::publish) | as dead ones | as dead ones | included above (the knocked-down grunt) | **G**, fixed |
| A dead player's body (vr_deathdoll) | server Box3D (parts) | as dead ones; mpslide's body drawn on it | as dead ones | as above | **G**, fixed |
| Props: crates, barrels, held, thrown, stood on, shoved | server Box3D | entity origin (int32 1/16 units) and angles (shorts), snapshot, delta-coded | yes (netsnap interpEntity) | crate 0.04 units, 0.01 deg | **F** |
| Dropped and thrown weapons, embedded blades | server Box3D (props; a blade on the part it skewers) | entity | yes | sword 0.04 units, 0.35 deg; dropped shotgun 0.04 (with ragdolls sent too, ragsync2; 1.75-2.3 before it) | **F** (a skewering blade sat on a part B drew elsewhere: fixed with the ragdolls) |
| Gibs, heads, cut limbs | server Box3D (props) | entity; small gibs under vr_net_cosmetic_bytes (B12c) | yes | gibs 2.8-7 units, 11-25 deg (their budget's waits); heads 0.05 | **F** (cosmetic budget by design) |
| Grenades, explosion chunks, rubble | server Box3D (props; chunks cosmetic) | entity | yes | not in this play (as gibs: entities) | **F** |
| Pushable corpses (bosses, not ragdolls) | server Box3D | entity | yes | (as props) | **F** |
| What a remote player's hands hold | server, placed in his sent hands | QVR_SVC_HELDPLACE (B27) | yes | (mp_held) | **F** |
| Blood, casings, decals, blood streams, sparks | each client (events) | events (QVR_SVC_EJECT, WOUND, BLADEWOUND...) | n/a | n/a | **L**: cosmetic, nobody can tell |

**The design (built).**
- **The server's ragdolls are authoritative**; a client simulates none (its game has no Box3D world of a remote server).
- **One record per ragdoll** in each remote client's snapshot: key `netsnap::ragKey + entity` (a sixth key class),
  QVR_SVC_RAGPOSE: its part count, the authored-rig bit, the cut bones, bleeds, scale, its step's time (ms), the root
  part's place (int32 eighths) and every part's turn (smallest three, 15 bits each: 0.005 degrees), the others' places
  from the root (int16 eighths). 18 + 12 bytes a part: a grunt (20 parts) 270 B, a player's body similar.
- **PVS-culled** (the entity in the client's sight: scopedInSight), **delta-coded** with the other records (a part's
  bytes in 8-byte groups), and **a settled ragdoll costs nothing**: its record is unchanged, the time too (a republish,
  asleep, keeps the last real publish's: ragdoll::serverPose). Measured: 0 B over 2 s once settled.
- **The priority accumulator**: the moving ones' changed records under `vr_net_ragdoll_bytes` (400) a snapshot, nearest,
  ahead and longest waiting first (netsnap holdRagdolls: the rest held as the base's, their wait raising their
  priority). **They take from the ragdolls only** (ragsync2): in the encoder a ragdoll's changed record is chosen after
  every other entry, whatever its priority, in the room they leave in the 1400-byte datagram, never by the first-finite
  rule (no bucket debt), and under his `vr_net_rate` only from what his bucket would spill by his next datagram (what it
  cannot hold anyway): the others never wait behind them, in that datagram or the ones after. A heavy fight that fills
  his link starves the ragdolls (drawn between the poses they have), not the entities.
- **The client** keeps each ragdoll's last 24 poses with their server times (a third of a second: past the
  interpolation buffer's longest delay) and draws it between the two around the clock its entities are drawn at, each
  part slerped, through the listen host's own drawing (ragdoll::publishNet, swapModels, skinned models, the death view's
  hidden head, mpslide's dead player's body). A rig that doesn't fit his (another part count) is drawn animated.
- **Bandwidth**: the play's two ragdolls cost B 3.0 KB/s on average over 16 s (48 KB), a moving grunt about 19 KB/s
  at 72 Hz alone; a crowded coop fight is capped by `vr_net_ragdoll_bytes` (400 B a snapshot: 29 KB/s at 72 Hz, about
  one moving grunt at full rate, more at a share each) and then by his rate.
- **Protocol**: QVR_NET_VERSION 42 (a new message in the snapshot): **the room service must be redeployed** with it.
- **A client's own hands on a ragdoll** (ragsync2): the server's ragdoll follows his sent hands (the grab fit per slot,
  as the listen host's). In his own snapshot a ragdoll he holds carries which of his hands hold it and where the server
  had them at that pose (his hand fields, int32 eighths: 12 bytes a hand, only while held; flag bits in the bleeds
  byte); his client leads its drawing with his hands since, by the listen host's code (leadWithHands, vr_ragdoll_held_local):
  the limb in his hand at his frame rate, not an interpolation delay behind. Not harness-tested (no scenario has a remote
  client hold a ragdoll): his to try in VR.
- **Measured after** (ragsync2, `mp_test.py ragsync`): with the ragdolls sent the others' records that waited over the
  play 443 (662 in the play without them), the dropped shotgun 0.04 units p95 (2.3 before), the crate and sword 0.04;
  the backpacks 0.04 over all samples, one flung backpack 3.2 units p95 while it flew (one 0.8 s burst as a grunt was
  gibbed, likely the gib storm's full datagram; not traced further); the small gibs' cosmetic budget as before.
- **raginterp** (2026-11-21, the regression smallfix found): with the ragdolls sent the crate and sword at B were 6.5 /
  2.9 units p95 in some runs, drawn in steps 1-2 ticks ahead. Not the ragdoll code: the entities' interpolation history
  kept 8 messages (0.11 s) while the adaptive delay may reach 0.25 s (and the clock lag its target 0.25 s more, catching
  up after a hitch); the ragdolls' bigger snapshots arrived less evenly, the delay grew past 7 ticks and the clock fell
  before each entity's oldest message, which interpEntity holds ("before the oldest"): drawn at the oldest, stepping
  ahead. Now 40 messages (vr_netsnap.cpp historyLen, a static_assert ties it to delayMost + snapAfter).
