# Multiplayer: server vs client

State as of 2026-09-30 (branch `agent/mpreport`, base `a6e7469d`). Research only; the one code change is the test
aid `vr_dumpplayer [client]` (vr_server.cpp).

- **[run]**: verified with a two-process run (see "How it was tested").
- **[code]**: read in the code.
- **[inf]**: inferred, not exercised.

## Summary

- **Protocol.** Quake VR is Ironwail's PROTOCOL_RMQ (999) plus `PRFL_QUAKEVR`. The additions:
  - a 291-byte float block in every `clc_move`;
  - `svc_quakevr` (39) with 17 sub-commands;
  - `U_QVR_*` entity bits 24-27;
  - a beam-id byte on `TE_BEAM`/`TE_LIGHTNING*`;
  - `STAT_QVR_*` stats 64-112.
- **What already works.** A remote VR client connects to a listen server or a dedicated server. Its head, hands,
  muzzles and bits land in its own edict's fields [run]. Server systems keep per-client state: climbing
  (`climbers[MAX_SCOREBOARD]`), Box3D hand bodies and standing (`hands`/`stands` resized to `maxclients+1`), carry,
  melee (`mh_*` fields), force grab (`.fg_player`), rope tags and unstick.
- **What doesn't work: other players are invisible as VR players.** They are a plain `progs/player.mdl` yawed by
  their main-hand aim [run + code]. Nothing about another player's head, hands, fingers, body, calibration,
  off-hand weapon, holsters, held prop, climb holds, flashlight or force-grab glow reaches other clients. Stats go
  only to their owner.
- **The main correctness bug: the server uses the host's state for every player.** In roughly 40 places, server code
  reads either the listen host's client state (drawn fist and weapon, measured grip frame, body calibration,
  `cls.state`) or the host's personal cvars (handedness, gun angle, holster/grip/throw/reload modes, world scale,
  weapon weights). In roughly 10 places, client code reads the local server's edicts or traces (`worldtrace::move`,
  `serverInHand`, `restsOnHand`, prop mass, hull width), so those features break or degrade on a remote client.
- **Latency.** There is no client-side prediction: movement, climbing pull-ups, grapple swings and pushed props all
  arrive one round trip late. Box3D runs only on the server. Melee is judged from hand poses sampled at the server's
  tick against monsters where the server has them, with no lag compensation.
- **Tick rate.** Both a listen and a dedicated server tick at a steady 72 Hz (`host_fixedtick 1`, the default since
  2026-10-06; ROUND21.md, "Server tick rate"), and only the last `clc_move` of each tick is kept. Before it, a listen
  server ran at half the headset's rate at 72-90 Hz (36 Hz on a 72 Hz headset, 45 at 90, 60 at 120, 48 at 144) and a
  dedicated one at `sys_ticrate` 0.05 (20 Hz; measured 16-22.5 [run]), which `host_fixedtick 0` brings back.
- **Bandwidth per player.**
  - Up: about 25 KB/s (72 Hz × about 346 B).
  - Down, idle: 18 KB/s from a listen server, 5.7 KB/s from a dedicated one [run].
  - Replicating avatars would add about 3-4 KB/s per visible player at 72 Hz once quantized.

## Transport facts

| Item | Value | Source |
|---|---|---|
| `clc_move` vanilla part | 19 B: cmd, time, 3 angles as shorts, 3 moves as shorts, buttons, impulse | cl_input.c:404-435 [code] |
| `clc_move` VR block | **291 B**, all floats in world space: head angles, vrYaw, 2 × 80 B per hand (pos, rot, vel, throwVel, velMag, angVel, throwPos, throwAge), headVel, 2 muzzles, vrBits0 (short), teleport target, 2 hotspots, roomscale move, buttons, sawCord, handDrop, origin, headPos, 2 shot angles. Non-finite values drop the move. | vr_move.cpp:46-120 [code] |
| Move rate | 72 Hz (`host_netinterval` 1/72, `host_fixedtick` 1), whatever the headset's refresh rate: a move each server tick, several in a frame below 72 fps. Before 2026-10-06 (`host_fixedtick 0`): half the headset's rate at 72-90 Hz (36 Hz at 72), because the float 1/72 was a little over an exact 1/72 s frame [run] | host.c `Host_FixedTicks`, `_Host_Frame` [code] |
| Server → client rate | Listen and dedicated server: 72 Hz (a dedicated server's frame waits for the next tick, at most `sys_ticrate`); `host_fixedtick 0`: a dedicated one at `sys_ticrate` 0.05, i.e. 20 Hz | host.c, main_sdl.c [code] |
| Coordinates and angles | `PRFL_INT32COORD` (1/16 unit, 4 B) and `PRFL_SHORTANGLE` | sv_main.c:1982 [code] |
| Entity extras | `U_QVR_SCALE`, `SCALEORIGIN` and `OFFSET` are 12 B each; `NOROTATE` is 0 B but forces both extension bytes. They are set on **every** update, not delta-compressed. | vr_server.cpp:321-336 [code] |
| Packet limit | 1400 B (`DATAGRAM_MTU`) for remote clients; 64000 locally. The shared datagram (sounds, particles, ropes) is copied into a client's packet only if it all fits. VR particle and rope writers check only `MAX_DATAGRAM`. | sv_main.c:1199, 1211; vr_ropesim.cpp:467 [code] |
| Interpolation | NetQuake linear lerp of origin and Euler angles between the last two messages; no prediction | cl_main.c:576-605 [code] |
| Negotiation | Not per client. With `PRFL_QUAKEVR`, the server reads a VR block from every client, so a vanilla Ironwail or QSS client is misread and dropped [inf]. A flat Quake VR client sends a zeroed block without `HANDSTRACKED`, and QC treats it as untracked. | vr_server.cpp:119-126, 176-178 [code] |
| Per-move handling | Fields are overwritten by each move; the last move of a server frame wins. `bits.received = move.vrBits0` overwrites, so a press and release inside one tick is lost. `sawCord` and `handDrop` are held longer by the client. | vr_server.cpp:140-197 [code] |
| Per-client reset | `clientMoves`, `clientBits` and `climbers` are cleared only on a world reset, not on connect or drop. A reused slot inherits the last player's state until a move arrives [inf]. | vr_progs.cpp:176 [code] |
| Client → server settings | None: no userinfo, setinfo or cvar replication | [code] |

### Measured bandwidth (e1m1 start, 2 players, idle) [run]

| Link | Packets/s | Mean size | Rate |
|---|---|---|---|
| Client → server | 72 | 310 B of payload (+36 B UDP/IP and NetQuake header) | about 25 KB/s (200 kbit/s) |
| Listen server → remote client | 72 | 251 B (range 175-415) | 18.1 KB/s |
| Dedicated server → client | 22.5 | 254 B | 5.7 KB/s |

Load adds to these [code]:

- **Moving props:** about 24 B each per frame. They are resent every frame once they differ from the baseline, even
  when asleep.
- **Grapple ropes:**
  - The rope beam is a 29 B `TE_BEAM` per hook per frame, broadcast with no visibility (PVS) check: 2.1 KB/s per rope
    per client at 72 Hz.
  - `QVR_SVC_ROPE` is at most 108 B, sent on change or every 0.25 s.
- **Climbing:** the hold-position stats change every frame while climbing, so they go out reliably every frame [inf].
- **Haptics** use the reliable channel (vr_server.cpp:483-498).

## Tracking, body, calibration

| System | Runs where | Network | Multiplayer problems | Recommendation |
|---|---|---|---|---|
| Head and hands (the move) | Client builds it (`vr_client.cpp:180-360`). Hand calibration (`vr_handcal_*`, `vr_gunangle/yaw`), the `vr_wofs_whole_*` offset, handpose wall collision, two-hand aim and the weight spring are baked in (`vr_hands.cpp:546-576`). Main-hand aim is written to `cl.viewangles`. | 291 B up. World space, based on the client's lerped origin (`move.origin`). The server shifts hands by the origin lag (`rebaseHands`, vr_server.cpp:446). | Works per client [run: edict 2 had its own handpos/headpos]. At a 20 Hz tick, 2 of every 3 moves are discarded. | Keep it. Send hands relative to `move.origin` as int16 in 1/8 units, and angles as shorts: about 291 → 150 B. Optionally carry the extra samples since the last move for melee (see Melee). |
| Body and arm IK (`vr_avatar`, `vr_body`, `vr_bodycal`) | Client only. It poses the local view entity (`vr_view.cpp:3930-3944`). Module-level "last posed" state allows only one body per frame. | Nothing | Other players see `player.mdl`. Missing to draw them: head pose, hand poses, height, arm, shoulder and elbow calibration, handedness, body yaw, lean and crouch. | Replicate (see Remote avatars). Run the same solver on every client for each visible player. Make `avatar` state per instance. It must stay client-side: cosmetic, per-frame, needs the render skeleton. |
| Body calibration (`vr_bodycal_*`, `vr_body_tweak_*`, `vr_height_calibration`, `vr_body_arm_length`, `vr_body_shoulder_reach`) | Client. **Also server C++:** climb `armReach` and `shoulderOf` (`vr_climb.cpp:945-962`) call `bodycal::armLengthMetres()`, `shoulderShift()` and `units::bodyScale()`. | Nothing | Every player climbs with the host's arm length and shoulder position. A dedicated server uses defaults. | Client → server: a `clc` "body" message on connect and on change (about 20 floats). The server keeps it per client; remote clients receive it for avatars. |
| Hand calibration (`vr_handcal_*`, `vr_gunangle`, `vr_gunyaw`, `vr_offhandpitch/yaw`) | Client, baked into the move. **Also QC:** `VR_Melee_Wrist` reads `cvar("vr_gunangle"/"vr_gunyaw"/"vr_offhandpitch"/"vr_offhandyaw"/"vr_handcal_off_mirror")` (vr_melee.qc:674-676). | Nothing separate | Melee wrist pivot uses the host's gun angle for everyone. | Per-client prefs channel (P0-1), or send the wrist offset in the move. |
| Fingers and grasp (`vr_grasp`, `vr_handrig`, `vr_handpose` fingers) | Client only. Needs no server state. | Nothing | Other players have no fingers. | For avatars, send finger curls (5 bytes per hand) and the held entity plus grip; each client runs the grasp solve for remote hands (deterministic from those inputs). |
| Handpose wall collision (`vr_handpose.cpp:69-98`) | Client. Uses `worldtrace::move`, i.e. `SV_Move` on the **local server** as client 0 (`vr_trace.cpp:13-25`). | Nothing | Remote client: no hand or weapon collision with doors and monsters, world only [code]. | Trace `cl.worldmodel` and client brush entities, plus client-side boxes for monsters. |
| Weapon offsets and sights (`vr_wofs_*`, sights, `vr_gunmodelscale`, `vr_gunmodely`) | Client (drawing and the muzzle in the move). **Server:** `weaponvalue()` Weapon Weights (`w_mass`, `w_throwdmg`, `w_meleedmg`) in QC; `weapons::modelTransform` and `offsetScale` size Box3D hand and weapon bodies (`vr_box3d.cpp:1217-1219`, `vr_weapons.cpp:1051`). | Muzzle and shot angles are in the move. | Weapon mass and damage and the hull scale are the host's for all. | Weapon weights are balance, so make them a server rule. Hull scale comes from the per-client rig (P0-2). |
| Other players' drawing | `player.mdl` via `modelindex_player`; yaw from `v_angle`, i.e. main-hand aim (`sv_user.c:411-416`) | Normal entity | No hands, weapons or body. Yaw follows the gun, not the head or body. | Remote avatars (P1-1). |

## Held things, melee, hits

| System | Runs where | Network | Multiplayer problems | Recommendation |
|---|---|---|---|---|
| Held props, one hand (`vr_carry.qc`, `vr_grip`, `vr_held`) | Server QC and builtins place the prop. The client draws its own held prop in this frame's hand. | Hands and bits up; `STAT_QVR_CARRYMAIN/OFF` to the owner. | **Host shortcuts:**<br>• `vr_grip.cpp:399`: `h.local = ... NUM_FOR_EDICT(player) == 1`. Only player 1 gets the measured palm frame; the others get `defaultFrame`.<br>• `held::fists[2]` (the host's drawn fist) is used by `grabTouch` for every player (`vr_held.cpp:424,608`).<br>• `vr_physics.cpp:131`: fist-grab only for edict 1.<br>• The client reads `sv.qcvm` edicts (`serverInHand`, `grip::serverPlace`, `vr_held.cpp:1078-1119,1262`). A remote client falls back to `msg_origins`.<br>Others' props lag at the server origin. | Keep server authority. Per-client rig (palm frame, fist spheres) comes up from the client (P0-2). Remote clients draw others' props in those players' replicated hands (P1-1). |
| Two-handed props (`vr_carry2h`) | Server builtins keyed by prop (multiplayer-safe). The client repeats the solve. | Nothing extra | On a listen server the client reads `carry2h::serverHold`. A remote client records its own hold, so its grips can differ (`vr_held.cpp:1607`). Uses the host's fist. | As above. Send the grip pair once per grab (CATCHBLEND-sized event) so the client needn't guess. |
| Two-hand weapon aim (`vr_twohand`) | Client | Rotated `hand.rot`, `shotRot`, `VRBITS0_2H_AIMING` | `weaponhotspot`/`weaponhotspotinfo` builtins return `{}` unless `NUM_FOR_EDICT(player)==1 && cls.state==ca_connected` (`vr_builtins.cpp:751`); melee's two-hand check (`vr_melee.qc:411`) is lost for others. | Keep aim on the client. Send the hotspot (weapon-local grip point) in the rig or the move. |
| Throwing (`vr_throw`) | Client estimates the throw; QC releases it | `throwVel/angVel/throwPos/throwAge` in the move | Client-trusted velocity; no absolute speed clamp (`vr_carry.qc:444`). | Keep on the client (needs 72+ Hz tracking). Add a server speed cap. |
| Melee (`vr_melee.qc`, `combat.qc`, `vr_meleehud`) | Server QC per player (`mh_*` fields). Speeds are recomputed from pose deltas over **server time** (`vr_melee.qc:695-755`). | Hands up; `STAT_QVR_MELEE`, `QVR_SVC_HANDIMPACT`, haptics down | 20 Hz dedicated tick: poses undersampled. Jitter: 0 or 2 moves per tick skew speeds. No lag compensation: blows are judged against where the server has the monster. Host's `vr_gunangle` wrist (above). The server's melee timing rules (`vr_melee_speed`...) apply to all: decided 2026-10-06, see "Server rules". | Keep judgement on the server. Either send client timestamps and all samples since the last move (process every move, not only the last), or detect swings on the client and have the server validate. Rewind monster hit models by the client's latency. The motion recorder stays listen-only (a dev tool). |
| Hit models (`vr_hitmodel`) | Server C++ (per entity) | Nothing | Copies the client lerp using the **host's** `r_lerpmodels/r_lerpmove` (`vr_hitmodel.cpp:373,377`). "What you see is what you hit" holds only at zero latency. | Make the lerp flags a server setting. Add rewind (as above). |
| Model collision (`vr_modelcollide`) | Client, drawing only | Nothing | `hosting` branch reads `EDICT_NUM` flags and `box3d::restsOnHand` (`vr_modelcollide.cpp:378,389,865`). A remote client guesses by model. | Send the needed flag (e.g. "rests on hand") as an entity bit, or accept the fallback. |
| Weight spring (`vr_weight`) | Client (spring). Server builtins `weightdamage/weightleniency` use server cvars. | Spring-moved hands; `handDrop` bits | Prop mass: listen reads `EDICT_NUM(ent)` / `box3d::propMass` (`vr_weight.cpp:151-164`); remote uses `props::estimateMass`, so the feel differs from the server's damage mass. Weapon drop is client-decided. | Keep the spring on the client (felt latency). Send the mass: an entity extra, or a byte in the carry stat. |
| Fatigue (`vr_fatigue`) | Shake is client-side; `VR_StaminaSpeedScale` is server-side per player | `STAT_QVR_MELEE`, `STAT_QVR_CLIMB` | Only the debug commands (`vr_stamina_set`, `vr_debug_stamina_hold`) use `svs.clients[0]` (`vr_fatigue.cpp:56`). | Fine. |
| Climbing (`vr_climb`, `vr_ledges`) | Server C++ from `SV_Physics`, per client. The client pins the drawn hand to the hold. | Last move per tick up; `STAT_QVR_CLIMB` + 6 hold stats (reliable, every frame while climbing [inf]) and haptics down | Host arm length, shoulder and handedness (above). Host `vr_climb_leniency`. Pull-up felt one round trip late. Others' hands on holds are not drawn. 20 Hz dedicated tick. | Keep authority on the server. Per-client body data (P0-2). Predict the pull-up on the client (P2). Replicate holds in the avatar data. |
| Holsters and pouch | Client computes hotspots; QC and climb use them | `hotspots[2]` up; 24 holster stats down (owner only) | Others' holstered weapons are invisible. Hotspot is client-trusted. Host `vr_holster_mode`. | Prefs channel. Replicate holster weapon ids in avatar data (6 bytes). Optional server distance check. |
| Flashlight (`vr_flashlight`) | Client only (model, dlight, cord) | `QVR_BUTTON_*BUSY` only | Others never see it or its light. Its beam trace is `worldtrace::move` (listen-only). | Keep the local lamp on the client. Put an on/hand bit in avatar data and draw remote lamps. Use a client-side trace. |
| Chainsaw cord (`vr_chainsaw`) | Client (cord, handle, pull detect); QC (start chance, fuel, cuts) | `sawCord` byte (pulls held 0.15 s) | Others see only the model frame, not the cord or handle. | Fine. The cord is cosmetic; optionally add a "handle out" bit to the avatar. |
| Wall torches (`vr_walltorch`) | Server QC entities; the client draws flame, light and crackle over any entity with the stick model | Fire level is the entity frame | Works for others' torches [code]. `restoreAfterLoad` reads the server's `vr_walltorch` (correct). | Fine. |
| Flick reload (`vr_flick`) | Client | `RELOADFLICKING` bits | A flick bit shorter than one tick can be lost (overwrite). | OR edge bits across moves (P0-5). |
| Loose rounds loading by contact (QC `vr_reload.qc` `VR_Reload_LooseFrame`; the slide-in, `vr_collectfx.cpp`: `server::sendCollect` to the holder alone, `server::sendCollectSeen` to the PVS for a gun lying about) | Server QC loads the round; each client draws the slide | The collect event | **Known limitation (2026-10-09):** a round loading by contact into a gun someone **holds** slides in only on the holder's client; the other players see the round vanish. Into a gun **lying about** the slide is sent to every client with it in its PVS, so all see it. | Send the held gun's slide to the PVS too (the holder's hand-relative path drawn from the others' view of that hand). |
| Haptics | QC → `QVR_SVC_HAPTIC` to `self`'s client, **reliable** | 19 B each | Continuous buzzes (saw, torch pull) fill the reliable channel. | Send them unreliable; keep one-shot events reliable. |

## Grapple, physics, hulls

| System | Runs where | Network | Multiplayer problems | Recommendation |
|---|---|---|---|---|
| Grapple (`vr_grapple.qc`) | Server QC (hook, reel, constraint), per player; tags from a global allocator | Hook entity; rope beam `TE_BEAM` 29 B every frame, broadcast | Swing and reel felt a round trip late. Others' ropes start at the server `gg_at`, not their drawn gun (`vr_client.cpp:938-970`). Host `vr_grapple_trigger_release`. | Keep on the server. Send the beam only on slack change and with a PVS check. Start remote ropes at the replicated muzzle (P1). |
| Rope corners (`vr_ropesim`) | Server C++, casts against Box3D, when the server's `vr_grapple_rope_sim` is on | `QVR_SVC_ROPE` ≤108 B on change or every 0.25 s, broadcast unreliable | The client gates drawing on **its own** `vr_grapple_rope_sim` (`vr_rope.cpp:1067`): a mismatch ignores the corners. | Keep on the server (authoritative corners). Gate the client on "corners received", not on its own cvar. |
| Rope slack chain (`vr_rope`) | Client, per render frame, traces `cl.worldmodel` | Nothing | Each client's slack differs (cosmetic) | Correct split. Keep. |
| Box3D props (`vr_box3d`, `vr_physics`, `vr_rigid`) | Server only: one world stepped per server frame (`vr_box3d.cpp:5307-5446`); 3 substeps at 20 Hz. Deterministic, single-threaded. | Ordinary entity updates (INT32COORD, short angles); about 24 B per moving prop per frame, resent while it differs from the baseline | Linear Euler lerp wobbles on tumbling props. At 20 Hz a fast throw over the 100-unit lerp threshold snaps. Pushes, bats and standing reactions are one round trip late. | Keep the server authoritative (no client Box3D: two sims diverge and cost CPU). Add snapshot or quaternion interpolation with about 2 ticks of render delay. Delta-compress `U_QVR_*`. Client prediction only for props in the local hand (already done by drawing). |
| Hand and weapon kinematic bodies | Server, for **all** clients (`hands.resize(maxclients+1)`, `vr_box3d.cpp:1708-1715`) | From the move | **Host shortcuts:**<br>• `vr_box3d.cpp:1726`: fist spheres only for `i == 1`, from `held::fist` (the host's drawn hand).<br>• `vr_box3d.cpp:1929`: `if(i == 1 && cls.state == ca_connected)` player 1's weapon body is the drawn model hull (`view::drawnWeapon`); others get a hand-to-muzzle capsule.<br>• Scale uses the host's `vr_world_scale × vr_gunmodelscale`.<br>On a dedicated server everyone gets a 4.5 cm sphere plus a capsule. Jittery remote moves give velocity spikes (capped by `limitPushes`). | Per-client rig (P0-2): fist spheres from the client, weapon hull from the weapon id plus that client's offsets (computed server-side from the model, not from `view::`). |
| Standing on props | Server, per client (`stands.resize(maxclients+1)`, `VR_StandsOn`) | Nothing extra | Felt a round trip late (no prediction). | Fine. It improves with movement prediction (P2). |
| Debris (`vr_debris`: rocks and bricks) | Server (interactable: picked up, thrown) | Entities | In multiplayer at most `vr_debris_mp_max` (default -1: as many as in single player, `vr_debris_max`; the author, 2026-10-06; 0: none). | Decided (2026-10-06): interactable debris stays server-side; VFX-only debris is client-side. See "Debris and effects". |
| Force grab (`vr_wpnforcegrab.qc`, `vr_fgfx`, `vr_drawblend`) | Server QC (target, lock, fly home toward the server's copy of the hand). The client draws glows and the catch blend. | `STAT_QVR_FG*` (owner only); `QVR_SVC_CATCHBLEND` 29 B reliable, once per catch | Others see no glows. Homes on the lagged server hand. World weapons are force-grabbable only in single player (`vr_wpnforcegrab.qc:121,132`). Host `vr_forcegrab_mode` and flick speed. | Keep. Prefs channel. On the client, blend the flying object toward the drawn hand in its last 100 ms. |
| Player and monster narrow hulls (`vr_hull`, `vr_unstick`) | Server C++, compiled from `sv.worldmodel`; unstick per client | Nothing | One `vr_hull_width` for all (a server rule, fine). The client's lean recentering (`worldtrace::playerBoxFits`, `vr_hull.cpp:3064`) uses the narrow box only when `world == sv.worldmodel`; remote clients use the 32-wide hull 1. | Keep on the server. Send the width in serverinfo and let the client compile the hull from `cl.worldmodel` (or accept hull 1). |
| Teleport (`vr_teleport`) | Client aims with `worldtrace::move` (`vr_teleport.cpp:41`, listen-only); the server validates with its own `vr_teleport_range` | `teleportTarget` 12 B plus a bit, every move | **A remote client cannot teleport**: the trace returns nothing [code]. The range is the host's while the aim uses the client's. | Aim with a client-side hull trace (`worldtrace::world`/`hullTrace`). Server range becomes a server rule; the client clamps to the server's range. |
| Crosshair | Client, `worldtrace::move` | Nothing | World-only on a remote client | Client-side trace against client entities. |

## Settings and menus

- **Counts** [code, 2026-09-30; about 2,670 now]: vr_cvars.inc had 1062 cvars. 250 are read by QC (`cvar()`/`cvar_hget`), 134 by server C++,
  368 in total server-side. About 694 are client-only.
- **Per-player preferences the server reads: the host's value applies to everyone** [code]:
  - Server C++:
    - body calibration and `vr_height_calibration` (climb)
    - `vr_world_scale` (`units::metresToUnits` everywhere)
    - `vr_climb_leniency`, `vr_swim_look`, `vr_swim_stroke_assist`
    - `vr_teleport_range`
    - `vr_carry_grab_drawn` and `vr_box3d_hand_push_fist` (edict 1 only)
    - `vr_gunmodelscale`, `vr_gunmodely`, `vr_leg_holster_model_*`
  - QC:
    - `vr_gunangle`, `vr_gunyaw`, `vr_offhandpitch/yaw`, `vr_handcal_off_mirror`
    - modes: `vr_reload_mode`, `vr_holster_mode`, `vr_weapon_grip_mode`, `vr_weapon_cycle_mode`,
      `vr_weapon_throw_mode`, `vr_2h_handoff`, `vr_forcegrab_mode`, `vr_grapple_trigger_release`, `vr_carry_take`
    - assists: `vr_throw_assist*`, `vr_forcegrab_flick_speed`
    - feedback: `vr_holster_haptics*`, `vr_forcegrab_eligible_*`, `vr_grapple_haptics`, `vr_counter_haptic`,
      `vr_explosion_rumble`, `vr_heartbeat`, `vr_headshot_sound`
  - Server rules since 2026-10-06 ("Server rules"): `vr_melee_speed`, `vr_melee_wrist_speed` and the other melee timing
    settings.
  - Could be either a preference or a rule: `vr_grenade_catch`,
    `vr_carry_two_hands_solid`, `vr_gore`, `vr_body_interactions`.
  - The other ~335 server-read cvars are real rules (damage, stamina, grapple, parry, hulls, Box3D, debris).
- **Menus.** Pages mix rules and preferences. The Main page (Left Handed, grip mode, force grab, 2H handoff,
  teleport range), the Immersion modes, Hand/Gun Calibration, Body Arms, Player Calibration, Hip Holsters and Weapon
  Weights all hold values the server reads. On a remote client, rule pages (Climbing 39 cvars, Grapple 33,
  Throwing 28, ...) change nothing, silently.
- **Listen-only tools** (fine as dev tools):
  - `vr_dumpplayer`, `vr_climb_probe`/`try`, `vr_physics_*`, `vr_rigid_place`, `vr_hull_*`, `vr_stamina_set`,
    `vr_carry_check`, `vr_weight` test
  - motion recorder, playback and review
  - the menu's live preview (`vr_menuui.cpp:1242` requires `svs.maxclients == 1`)

## Debris and effects (decided 2026-10-06)

The author's decision: **VFX-only debris** (no gameplay interaction: nothing picks it up, nothing it hits matters, it
deals no damage) is **client-side**: one event, and each client spawns and simulates its own. **Interactable debris**
(grabbed, thrown, burns and spreads fire, blocks, deals damage: a crate's wooden pieces) is **server-side
entities**. Every spawner was audited; each was already on its side, so no spawner moved and single player is unchanged.

| What | Where it is made | Interaction | Side | Network cost |
|---|---|---|---|---|
| Explosion chunks (`vr_explosiondebris.cpp`; moved 2026-10-08, below) | Server C++, from each explosion temp entity QuakeC broadcasts | None: Box3D props of 20 g (ride lifts, blasts throw them, knock props barely), no damage, no sounds or touches | Server | About 26 B a frame for each chunk in sight while it lives (2-4 s, at rest too: no baseline; measured 2026-10-08); at most `vr_explosion_debris_mp_max` (24) in multiplayer |
| Spent casings (`vr_shells.cpp`) | Client, from `QVR_SVC_EJECT` sent to the firing player | None | Client | 7 B event. Only the shooter's client sees them; remote players' casings belong with the remote avatars. |
| Splinters and wood dust (`VR_Crate_Splinters`, a piece bursting) | Client particles (`particle2` presets) | None | Client | Particle event |
| Sparks, blood, dust, smoke, fire particles, shock arcs | Client particles, from events | None | Client | Events |
| Blood decals, gore on walls and bodies (`vr_decals`, `vr_gore`, `vr_bodyblood`, `vr_wounds`) | Client, from wound events | None | Client | Events |
| Crate pieces (`vr_crate_piece`, vr_crates.qc; `vr_crate_piece_max` 48) | Server QC | Grabbed, thrown (they hurt), burn and spread fire, struck to dust | Server | About 22 B a frame for each piece in sight. They have no baseline because they are made after signon. |
| Rocks and bricks lying about (`vr_debris.cpp`, vr_debris.qc) | Server C++ plan, QC entities | Grabbed, thrown (they hurt) | Server | About 18 B a frame for each piece in sight (measured below) |
| Gibs and heads (`ThrowGib`, `ThrowHead`, vr_decap.qc) | Server QC | Grabbed, thrown, burst, squished, stuck | Server | Entities |
| Small gibs and brain chunks (vr_smallgibs.qc; `vr_smallgibs_max` 64) | Server QC | Grabbed, thrown, burst | Server | Entities |
| Mission-pack rubble (hip_rubbl.qc, rogue_newmisc.qc), lava balls (`misc_fireball`), zombie gibs | Server QC (id's) | They deal damage | Server | Entities |
| Burning flames (`vr_burn_flame`, vr_burning.qc) | Server QC: a model on the burning thing | None of their own (the fire entity burns and spreads) | Server (follow-up) | Up to `vr_burn_flames_max` (12) entities per fire. They are VFX-only by the rule, but they follow the server's fire. Moving them needs a "burning" bit on the entity, with the client fitting flames to the body. They are not debris, so they were left as they are. |
| Bubbles (id's `air_bubbles`, death bubbles) | Server QC (id's) | None | Server (as id made them) | A few entities. VFX-only by the rule, but tiny, so left as id's code. |
| Wooden crates placed about the map (`vr_crates.cpp`) | Server | Props | Server | Still single player only (`svs.maxclients == 1`). A crate placed by a mapper (`vr_crate`) works in multiplayer. |

**Rocks and bricks in multiplayer:** `vr_debris_mp_max` (Settings > Rocks and Bricks > Most in Multiplayer, the host's
value) caps the pieces in a multiplayer map. Its default is -1, "Single Player's": the same as in single player (Most
in a Map, `vr_debris_max` 160, and the free-entity limit), the author's call (2026-10-06; config 94 moves the old 0).
0: none. A remote client standing near a cluster of pieces spends about 480 B of its 1400 B datagram on them every
frame (measured below: about 17 B a piece in sight, while the baselines fix below is not done). e1m1 at the default:
34 pieces in single player, 29 on a two-player listen server (`-listen 2`, coop 1, `vr_debug_debris 1`; the same 29
as the measured `vr_debris_mp_max` 64 row below, so that row is the default's cost: 256 -> 739 B a frame by them);
`vr_debris_mp_max 16` gives 16, 0 none.

**Explosion chunks in multiplayer** (the author's decision of 2026-10-08, an exception to the rule above: they became
server entities, Box3D props, so that they ride lifts and doors and later blasts move them, as rubble's pieces do;
ROUND21.md, "Explosion debris as Box3D bodies"): `vr_explosion_debris_mp_max` (the Explosion Debris page's Most in
Multiplayer, the host's value) caps the chunks at once in a multiplayer game, with `vr_debris_mp_max`'s
semantics: -1 "Single Player's" (Maximum Active Chunks, `vr_explosion_debris_max` 96), 0 none, else the most; its
default is 24 (two explosions' worth: about 630 B of a remote client's 1400 B datagram while they live, against 96's
2.5 KB, which would overflow it and drop the farthest entities). An explosion launches `vr_explosion_debris_count` (12)
but no more than the cap; the oldest go first. A remote client sees the same chunks as the host (one simulation). The
measured rows below (2026-10-06) are the old client-side chunks'. A future option without any network cost (a
client-only Box3D world for visual-only physics) is in ROUND21.md's section and PERF_DECISIONS.md item 3.

**Measured** (e1m1, release build, `vr_net_stats`; a listen server started with `-listen 2 -ip 127.0.0.1`, and a second
game connected over UDP; ROUND notes for the scripts):

| Case | Server entities | Entities sent to the client, bytes a frame |
|---|---|---|
| Single player: explosion (an explosive box) | 215 → 214 (the box is gone); 16 chunks on the client | 4 → 7 sent, 113 → 168 B (the local client) |
| Single player: crate break (12 pieces) | 215 → 227 | 188 → 456 B (the local client) |
| Listen + remote: crate break | 200 → 211 | 7 → 18 sent, 176 → 446 B, while flying and at rest |
| Listen + remote: explosion | No debris entities | 0 B for the chunks. The host and the remote client each made 16 chunks of their own, at different speeds (separate simulations). |
| Listen + remote: standing by rocks (`vr_debris_mp_max` 64, which gives 29 pieces in e1m1) | +29 | 17 → 43 sent, 256 → 739 B |

**Why resting pieces cost so much:** NetQuake sends every entity in sight every frame, as its difference from its
baseline. A piece made after signon has no baseline (its baseline is zeros), so it sends its model, origin and angles
every frame, even at rest. The rocks, made before the baselines, still cost about 18 B each: either their baselines miss
Quake VR's fields, or settling moves them. Next step: send a piece a new baseline once it comes to rest
(`svc_spawnbaseline` is parsed at any time), and let the `U_QVR_*` fields default to the baseline's. The datagram sends
the nearest entities first, so an overflow drops the farthest ones for that frame (`vr_net_stats` counts those frames).

## Server rules (decided 2026-10-06)

The author's decision: "Melee speed should be server-side." The server's QC always judged melee by its own cvars. Now
the clients know those values too: `vr_serverrules.cpp` sends each rule to a client with its spawn state, and to all
clients whenever it changes (`QVR_SVC_RULES`, reliable). The rules are the melee timing settings, which decide whether
a blow lands: `vr_melee_speed`, `vr_melee_wrist_speed`, `vr_melee_pommel_wait`, `vr_melee_butt_run`, `vr_bash_speed`,
`vr_bash_deflect_window`, `vr_headbutt_speed`, `vr_deflect_speed`, `vr_deflect_window`.

- **Single player, and a listen server's host:** unchanged. The server is this process, so its cvar is the rule, and
  the menu edits it as before.
- **A client of a remote server:** the menu shows the server's value, dimmed, and does not change it. Left and right
  play the "no" sound, and Reset This Page skips these rows. The help says the value is the server's and gives the
  player's own value, which stays theirs for when they host. `vr_serverrules` (Debug > Other > Server Rules) prints
  both.
- [run] Listen server at 3, the remote client's own value 2.5: the client shows "server 3 (yours 2.5)". The host sets
  5: the client shows 5 at the next server frame, and its own value stays 2.5.
- Each further rule (damage multipliers, stamina, grapple...) is one line in `rules[]`. The menu dims it the same way.

## Listen-server shortcuts (complete list found)

The server reads the local client's state:

| Where | What |
|---|---|
| vr_grip.cpp:399 | `h.local = cls.state != ca_dedicated && ... NUM_FOR_EDICT(player) == 1`: the measured palm frame, player 1 only |
| vr_held.cpp:424, 608 | `fists[2]`, the host's drawn fist, used by `grabTouch` for every player |
| vr_physics.cpp:131 | `byFist = ... && NUM_FOR_EDICT(player) == 1` |
| vr_box3d.cpp:1726 | `i == 1 && vr_box3d_hand_push_fist ? held::fist(h)` |
| vr_box3d.cpp:1929 | `i == 1 && cls.state == ca_connected`: `view::drawnWeapon` |
| vr_box3d.cpp:1218-1219 | `cl.protocolflags` and the client's weapon cvars in the hull scale |
| vr_builtins.cpp:751 | `weaponhotspot*`: `cls.state == ca_connected && NUM_FOR_EDICT(player) == 1` → `view::weaponHotspot` |
| vr_climb.cpp:945-962 | `bodycal::`, `units::bodyScale` (host body) |
| vr_hitmodel.cpp:373, 377 | the host's `r_lerpmodels`, `r_lerpmove` |
| vr_motion.cpp:1501-1557 | recorder: `svs.clients[0]` plus `hands::current()` (dev tool) |
| QC (see Settings) | host personal cvars |

The client reads the local server:

| Where | What | Remote client gets |
|---|---|---|
| vr_trace.cpp:13-25 `worldtrace::move` | `SV_Move` as `svs.clients[0].edict` | nothing. Used by teleport (broken), crosshair, flashlight and handpose (world only) |
| vr_held.cpp:1078-1119, 1262 | `sv.qcvm` edicts, `grip::serverPlace`, `carry2h::serverHold` | fallback from `msg_origins` |
| vr_modelcollide.cpp:378, 389, 865 | `box3d::restsOnHand`, `EDICT_NUM` flags | a guess by model |
| vr_weight.cpp:151-164 | `EDICT_NUM(ent)`, `box3d::propMass` | an estimated mass |
| vr_hull.cpp:3064 | narrow hull if `world == sv.worldmodel` | hull 1 |
| vr_weapons.cpp:1051 | `sv.active && sv.protocolflags` | the client flag path |
| vr_fatigue.cpp:56, vr_held.cpp:1910, vr_weight.cpp:765 | debug commands on `svs.clients[0]` / `EDICT_NUM(1)` | n/a |

## Where computation should live

- **Stay on the server (authority):**
  - carry and grip placement, melee judgement, hit models, climbing, grapple and rope corners, Box3D, force grab,
    hulls, teleport validation, weapon state.
  - Reason: shared world state, cheating, and determinism (one Box3D world).
- **Stay on the client (feel, cosmetic):**
  - hand calibration, weapon offsets, two-hand aim, throw estimate, the weight spring, flick detection, the chainsaw
    cord, the flashlight, the rope slack, the IK solve, the grasp solve, fatigue shake.
  - Reason: each needs 72+ Hz local tracking or is drawing only.
- **Client → server (data the server now takes from the host):**
  - A per-client rig: palm frame, fist spheres, weapon hotspots, drawn weapon hull key and scale.
  - Body calibration.
  - Personal preferences.
  - Send them once on connect and on change, not per move.
- **Server → client (logic the client now takes from the local server):**
  - Traces for teleport, crosshair, flashlight and handpose: use client-side world plus entity traces.
  - Prop mass, the "rests on hand" flag, the hull width: send as data.
- **Move to the client:**
  - VFX-only debris (it is already: explosion chunks, casings, splinters; see "Debris and effects").
  - The hit-model lerp choice becomes a server setting instead of reading the client's lerp cvars.
- **Nothing should move from client to the server** except the per-client data above.

## Determinism and prediction

- Box3D is deterministic and server-only. Running it on clients as well would diverge (different inputs and
  timing) and needs full-state correction. Not worth it: draw held props in the local hand (done), interpolate the
  rest.
- NetQuake has no movement prediction. In VR the head is tracked locally, but body translation (stick locomotion,
  climbing pull, grapple swing, standing on a moving prop) waits a round trip. Over the internet this is the biggest
  comfort problem.
  - Fix: client-side player prediction with server reconciliation (QuakeWorld/FTE style). The server-side movement
    extras (climb pulls, Box3D standing, grapple) must be reproducible on the client, or the client predicts plain
    walking only and snaps otherwise.
- Melee and hitscan need lag compensation: rewind monster hit models to the client's view time
  (render time ≈ now − latency − lerp).
- Tick rate: VR servers should run at 72 Hz or process every received move (sub-ticking). Otherwise hand-based
  systems see 20 Hz samples. (Done 2026-10-06: `host_fixedtick 1`, a steady 72 Hz tick for listen and dedicated
  servers; ROUND21.md, "Server tick rate".)

## Remote avatars: proposed message

`QVR_SVC_AVATAR`, unreliable, per visible player per server frame, quantized:

| Content | Size |
|---|---|
| Entity number | 2 B |
| Head position and angles relative to origin | 6 + 6 B |
| Each hand: position (int16, 1/8 unit) + angles (shorts) | 2 × 12 B |
| Finger curls (5 per hand) | 10 B |
| Per hand: weapon id, model index, carried entity | 2 × 5 B |
| Flags: flashlight, climb holds, 2H grip, chainsaw handle | 2 B |

- Total about 60 B, i.e. about 4.3 KB/s per visible player at 72 Hz (1.2 KB/s at 20 Hz).
- Calibration goes in a reliable `QVR_SVC_AVATARCAL` per player on change: height, arm, shoulder, handedness, about
  80 B.
- The client poses each remote player with the existing solver.

## Prioritised work list

| # | Work | Size |
|---|---|---|
| P0-1 | Per-client preferences channel: a `clc_quakevr` prefs message (on connect and on change); the server stores per client; builtin `clientpref(ent, name)`. Convert the ~25 per-player QC and C++ reads. Tag cvars as rule or pref; grey out rule pages on a remote client. | M (2-3 d) |
| P0-2 | Per-client rig and body data up (palm frame, fist spheres, weapon hotspots, weapon hull key and scale, body calibration). Replace `vr_grip` `h.local`, `held::fists`, `view::drawnWeapon`, `weaponhotspot`, `bodycal::` in climb, and every `== 1` / `cls.state` test. | M (2-3 d) |
| P0-3 | Client-side traces instead of `worldtrace::move` (teleport, crosshair, flashlight, handpose); prop mass and hull width as data. Remove the client's `sv.qcvm` reads outside debug tools. | M (2 d) |
| P0-4 | Reset per-client VR state on connect and drop (`clientMoves`, `clientBits`, climbers, hand bodies). | S |
| P0-5 | OR edge bits (press, flick, grab) across all moves in a tick instead of the last move winning. | S |
| P0-6 | Dedicated server: fix the crash on quit (`vr_imgprefetch.cpp:47` `workers` destroyed unjoined → `std::terminate` [run]); default a 72 Hz tick for VR progs (done: `host_fixedtick`); reject non-VR clients cleanly. | S |
| P1-1 | Remote avatars: `QVR_SVC_AVATAR` plus `AVATARCAL`; make `avatar`, `hands`, `held` and `grasp` state per instance; draw other players' body, hands, weapons, held props, holsters and flashlight. | L (5-7 d) |
| P1-2 | Others' ropes start at their replicated muzzle; rope beam PVS-culled and sent on change; client draws corners without checking its own cvar. | S |
| P2-1 | Melee over the network: every move's samples with client timestamps (or client-side swing detection with server validation); monster hit-model rewind. | L (4-6 d) |
| P2-2 | Client movement prediction (walk, then climb and grapple) with reconciliation. | XL (1-2 wk) |
| P2-3 | Prop smoothing: snapshot or quaternion interpolation with render delay; delta-compress `U_QVR_*`; resend props only when changed. | M |
| P2-4 | Quantize the `clc_move` VR block (291 → about 150 B). | S |
| P2-5 | Haptics: continuous buzzes unreliable. Climb hold stats: unreliable or event-based. | S |
| P2-6 | Anti-cheat caps: throw speed, hotspot distance, hand distance from head. | S |
| P3 | Debris in multiplayer: decided and classified (2026-10-06, "Debris and effects"). Left: resting-piece baselines (`vr_debris_mp_max` defaults to single player's, 2026-10-06); force-grabbing world weapons in multiplayer. | M |

Sizes: S < 1 day, M 1-3 days, L 4-7 days, XL > 1 week. The estimates are rough.

## How it was tested

Both processes ran on one machine using the kit. The host used `run.sh` and the second client used
`run.ps1 -Instance 1`. The script is at `C:/OHWorkspace/qvr-kit/scratch/mp2.sh`.

- **Loopback needs `-ip 127.0.0.1` on both processes and `-port 26001` on the client.** Without `-ip`, the socket
  binds the LAN address and Windows refuses to send to loopback.
- **Connect with `connect 127.0.0.1:26000`.** Without the port, the client uses its own `-port`.
- **The host script needs `listen 1` before `map`.** `maxplayers` queues its `listen 1` behind the whole autoexec.

Results:

- **Listen server with 2 clients** (`maxplayers 2; listen 1; coop 1; map e1m1`):
  - The client got "Connection accepted" and protocol 999.
  - `vr_dumpplayer 2` showed the remote player's handpos, headpos and muzzles in world space around its own origin
    (432, −296, 72), separate from player 1's (528, …). `vrbits0` was 16384 = bit 14, `HANDSTRACKED`, from the mock.
  - The client's `entities` listed players 1 and 2 as `progs/player.mdl` only.
- **Bandwidth:** `cl_shownet 1` for 144 client frames.
  - Listen server: 144 packets, mean 251 B.
  - Dedicated server: 45 packets (22.5 Hz), mean 254 B.
- **Dedicated server** (`-dedicated 2`):
  - VR progs ran and accepted the VR client; `vr_dumpplayer 1` showed its hands.
  - `quit` crashed: exception 0xc0000409, `std::terminate` from the `workers` atexit destructor.
- **Everything else in this document comes from reading the code** (marked [code] or [inf]). Not run: grabbing,
  melee, climbing, the grapple or Box3D with two players. The mock has no second-player input script.
