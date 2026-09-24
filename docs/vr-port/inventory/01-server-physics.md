# Server, physics & world collision — Quake VR functional changes

## Summary
VR needs this subsystem for five things. (1) The server reads a much larger `clc_move` carrying per-frame hand, head and teleport state into hard-coded entvars. (2) Hand and weapon "touch" dispatch (`.handtouch`, `.vr_wpntouch`) happens in `SV_TouchLinks`, `SV_Handtouch` and `SV_VRWpntouch`. (3) Player movement changes: a teleport bypass, a second room-scale move pass, movement direction from the head (`v_viewangle`), and a cvar for step height. (4) The entity and clientdata encoding has new U_/SU_ bits (vec3 model scale/offset, second weapon, holsters, clips). (5) New QC entry points around `SV_SpawnServer`, plus `svc_particle2` and a world-text server state.

Many physics routines were rewritten: `SV_PushMove`, `SV_PushEntity`, `SV_Physics_Toss`, `SV_TouchLinks`, and the missile clip-box size. These are gameplay behaviour changes, not VR hooks.

Several QSS features were removed or disabled:
- rotating pushers and rotating BSP collision
- `MOVETYPE_EXT_FOLLOW`, `SOLID_EXT_CORPSE` (and its value was renumbered)
- ladders, `EndFrame`, `sv_gameplayfix_spawnbeforethinks`
- skin-contents pushers and point-contents checks against all BSPs
- the `.movement` and button3..8 ext fields
- the `clcdp_ackframe`, download-ack and qcrequest client messages
- every protocol except 8682

Biggest port risks:
- **IW is QuakeSpasm-based, not QSS.** It has no `MOVE_HITALLCONTENTS`/`trace.contents`, no FTE deltas or `PEXT2`, no botclients, and a different `SV_StartSound` signature. Its `U_SCALE` (bit 20, 1 byte) clashes with QVR's (bit 20, vec3).
- IW's `SV_PushMove` has `sv_gameplayfix_elevators`, which conflicts with QVR's pusher rewrite.
- `SV_VRWpntouch` reads **local** HMD state (`controllers[]`) from inside server physics. This is a listen-server-only design.
- QVR dispatches `touch` to *solid* entities, not only triggers. This changes vanilla semantics, and the QC relies on it.

## Changes

### New clc_move layout (VR input to server)
- **Where (QVR):** `sv_user.cpp:SV_ReadClientMove` (587-712)
- **Upstream (BASE):** QSS: optional PREDINFO sequence short, float time, 3 angles, 3 shorts move, button byte (8 buttons into ext fields button3..8), impulse byte, `.movement` ext field. Handled `drop`/`lastmovemessage`.
- **Change:** New order after the optional PREDINFO short, which is still read. `drop` is computed but ignored, and `lastmovemessage` is never updated.
  - float timestamp (ping)
  - 3× `MSG_ReadAngle16` → `v_angle` (aim)
  - 3× `MSG_ReadAngle16` → `v_viewangle` (head)
  - float → `vryaw`
  - main hand: vec3 `handpos`, vec3 `handrot`, vec3 `handvel`, vec3 `handthrowvel`, float `handvelmag`, vec3 `handavel`
  - off hand: the same six fields with the `offhand*` prefix
  - vec3 `headvel`, vec3 `muzzlepos`, vec3 `offmuzzlepos`
  - `MSG_ReadUnsignedShort` → `vrbits0`
  - 3× short: forward/side/up move
  - vec3 `teleport_target`
  - byte `offhand_hotspot`, byte `mainhand_hotspot`
  - vec3 `roomscalemove`
  - byte buttons: bit0 → button0, bit1 → button2, bit2 → button3 (now a hard entvar field)
  - byte impulse

  Every value is written into the player's hard-coded entvars and also into `usercmd_t`. Vec3s use `MSG_ReadVec3(sv.protocolflags)`. 16-bit angles are always used.
- **Purpose:** carries VR tracking state to QC every frame.
- **Ironwail:** `sv_user.c:SV_ReadClientMove` (438) is the vanilla/Fitz layout (8/16-bit angles, no PREDINFO).
- **Isolation idea:** `if (sv.protocol == PROTOCOL_QUAKEVR) VR_SV_ReadClientMoveExtras(move)` placed between the angles and the move shorts, or replace the whole function under the VR protocol. Store fields via a VR field-offset table (see Dependencies).
- **Tag:** PROTOCOL

### clc dispatch trimmed
- **Where (QVR):** `sv_user.cpp:SV_ReadClientMessage` (824-918); `getClcString` helper (47-72)
- **Upstream (BASE):** handled `clcdp_ackframe` (→ `SVFTE_Ack`), `clcdp_ackdownloaddata`, `clcfte_qcrequest`, `clcfte_voicechat`.
- **Change:** Only `clc_nop`, `clc_stringcmd`, `clc_disconnect`, `clc_move` and `clcfte_voicechat` remain. The badread message now prints the previous clc name. The unknown-command message prints its number.
- **Ironwail:** IW never had the FTE cases, so nothing is needed.
- **Isolation idea:** none. Just note that FTE delta acks are gone even though `SVFTE_*` code is still present.
- **Tag:** REMOVED

### Movement uses head direction; step height cvar
- **Where (QVR):** `sv_user.cpp:SV_WaterMove` (335-336), `SV_AirMove` (465-466); `sv_phys.cpp:SV_WalkMove` (1015)
- **Upstream (BASE):** WaterMove used `AngleVectors(v_angle)` and AirMove used `AngleVectors(v.angles)`. `STEPSIZE` was 18.
- **Change:** WaterMove and AirMove derive forward/right from `v_viewangle` (the head angles sent by the client). Noclip still uses `v_angle`. The step-up height in `SV_WalkMove` is `vr_player_stepsize.value` (cvar, default 18, archived). `sv_move.cpp` `STEPSIZE` for monsters is unchanged.
- **Purpose:** locomotion follows the HMD, not the weapon aim.
- **Ironwail:** `sv_user.c` 225/325, `sv_phys.c:SV_WalkMove` 845 (`STEPSIZE` 844).
- **Isolation idea:** a helper `VR_SV_MoveAngles(sv_player)` that returns `v_viewangle` when VR fields exist. Replace the `STEPSIZE` uses in WalkMove with `VR_StepSize()`.
- **Tag:** VR-CORE

### SV_Physics_Client: teleport, VR hand touches, room-scale pass
- **Where (QVR):** `sv_phys.cpp:SV_Physics_Client` (1218-1390)
- **Upstream (BASE):** returned early when `!knowntoqc && sv_gameplayfix_spawnbeforethinks`. Then PreThink → movetype switch → PostThink. Unknown movetype → `Host_EndGame`.
- **Change:**
  - The `knowntoqc` gate is removed. The cvar is still defined at line 67 but unused.
  - After `SV_CheckVelocity`, it calls `SV_Handtouch(ent)` and `SV_VRWpntouch(ent)`.
  - If `vrbits0 & QVR_VRBITS0_TELEPORTING` (bit 0): run `SV_RunThink`, set `teleport_time = time + 0.3`, and set `origin = oldorigin = teleport_target`. The movetype switch is skipped.
  - Otherwise the normal switch runs; MOVETYPE_WALK calls `SV_WalkMove(ent, true)`. Then a **second pass**: save velocity, set `velocity = {roomscalemove.x, roomscalemove.y, 0}`, and switch on movetype:
    - WALK: `SV_CheckStuck` + `SV_WalkMove(ent, false /*don't clear FL_ONGROUND*/)`
    - FLY: `SV_FlyMove`
    - NOCLIP: `origin += dt*vel`
    - NONE/TOSS/BOUNCE: nothing

    Velocity is restored afterwards.
  - A bad movetype now calls `Sys_Error`.
- **Purpose:** teleport locomotion; room-scale physical walking with collision.
- **Ironwail:** `sv_phys.c:SV_Physics_Client` (946). It has an extra `MOVETYPE_GIB` case.
- **Isolation idea:** wrap the movetype switch as `if (!VR_SV_ClientTeleport(ent)) { switch...; VR_SV_RoomscaleMove(ent); }`, plus `VR_SV_HandTouches(ent)` before the switch. `SV_WalkMove` needs a `resetOnGround` parameter, or a small wrapper that saves and restores `FL_ONGROUND`.
- **Tag:** VR-CORE

### SV_Handtouch — traced hand-touch dispatch
- **Where (QVR):** `sv_phys.cpp:SV_Handtouch` (1076-1178)
- **Upstream (BASE):** none.
- **Change:** Hand boxes are ±2.5. Six `SV_Move(..., MOVE_NORMAL, ent)` traces run toward `hand + fwd(handrot)*1`:
  - from the player box, for each hand
  - from a union box of the player and both hands, for each hand
  - from a ±2.5 box at each hand
  
  For each `trace.ent`, it tests AABB overlap of `trace.ent` (origin+mins/maxs ± bonus) with each hand box. The bonus is `VR_GetEasyHandTouchBonus()` = 4.5 if the target has `FL_EASYHANDTOUCH`. On overlap it calls `VR_SetHandtouchParams(hand, ent, target)`: this sets player `.touchinghand`, target `.handtouch_hand` and `.handtouch_ent`. Then it calls `SV_Impact(ent, target, &handtouch)`. The hand checks run offhand (0) first, then mainhand (1).
- **Purpose:** pick up items and press buttons with the hands.
- **Ironwail:** n/a.
- **Isolation idea:** entirely inside a `vr_sv.c` module. It needs `SV_Impact` generalised to take a func field (see below).
- **Tag:** VR-CORE

### SV_VRWpntouch — weapon muzzle collision touch (uses local HMD data!)
- **Where (QVR):** `sv_phys.cpp:SV_VRWpntouch` (1180-1210); helpers in `vr.cpp` 1637, 1775-1870
- **Upstream (BASE):** none.
- **Change:** For the main hand, then the off hand:
  - `VR_GetWorldHandPos(hand, origin)` builds the hand position from **client-local** `controllers[hand].position`, `headOrigin` and turn yaw.
  - `VR_GetResolvedHandPos` traces a ±1 box from `origin.z = handZOrigin+40`.
  - `VR_UpdateGunWallCollisions` traces a ±1 box from the resolved hand to hand + local muzzle offset.
  
  If the hit entity has `.vr_wpntouch`, it calls `VR_SetHandtouchParams` and then `SV_Impact(ent, hitEnt, &vr_wpntouch)`.
- **Purpose:** weapons physically poke or trigger things (e.g. shootable buttons by melee).
- **Ironwail:** n/a.
- **Isolation idea:** VR module. Better: rewrite it to use the networked `handpos`, `handrot` and `muzzlepos` so it works for any client.
- **Tag:** VR-CORE

### SV_TouchLinks / SV_AreaTriggerEdicts — touch any touchable entity + hand touch
- **Where (QVR):** `world.cpp:SV_AreaTriggerEdicts` (321-393), `SV_TouchLinks` (402-525)
- **Upstream (BASE):** collected only SOLID_TRIGGER entities with `.touch` from `trigger_edicts` whose absbox overlaps, then called `.touch`.
- **Change:**
  - **Collection.** It loops over **both** `trigger_edicts` and `solid_edicts`. An entity qualifies if `canBeTouched(target)` = `(touch || handtouch) && solid != SOLID_NOT`. There is no box test at collection time. The recursion stops early only if both loops overflow.
  - **Dispatch**, for each target != ent:
    - `doTouch`: if `canBeTouched && entBoxIntersection(ent,target)`, run `.touch` if set. If the target has `.handtouch` **and** ent is `FL_CLIENT` **and** (`!ent.ishuman || vr_body_interactions || vr_fakevr`), also call `VR_SetFakeHandtouchParams` (hand = 2, `cVR_FakeHand`) and run `.handtouch`.
    - `doHandtouch`, only if ent is `FL_CLIENT`: hand boxes are `handpos`/`offhandpos` ±2.5, against the target absbox ± 4.5 when it has `FL_EASYHANDTOUCH`. When `vr_enabled` is set it uses hand overlap; otherwise it uses body absbox overlap. If `handtouch && solid != SOLID_NOT` and the test passes, call `VR_SetHandtouchParams(offHandIntersects ? 0 : 1, …)` and run `.handtouch`.
  - So `.touch` of SOLID_BBOX/SLIDEBOX/BSP entities now fires on absbox overlap during linking. That is new versus vanilla.
- **Purpose:** hand pickup of ammo, slipgates and thrown weapons; body-touch fallback for bots and pancake mode.
- **Ironwail:** `world.c` 287/336. It is vanilla (triggers only) and uses `sv_areanodes` and `Hunk_AllocNoFill`.
- **Isolation idea:** replace the IW `SV_AreaTriggerEdicts`/`SV_TouchLinks` bodies when the VR progs are loaded. Put the hand part in `VR_SV_TouchLinksHands(ent, list, n)`. Keep an `if (vr_progs)` guard so vanilla behaviour stays for normal progs.
- **Tag:** VR-CORE / GAMEPLAY

### New solid/flag constants; SOLID_EXT_CORPSE renumbered
- **Where (QVR):** `serverdefines.hpp` (all), moved out of `server.h`
- **Upstream (BASE):** `SOLID_EXT_CORPSE 5`; flags up to `FL_JUMPRELEASED 4096`.
- **Change:**
  - `SOLID_NOT_BUT_TOUCHABLE 5` (not solid, but hand/touchable). `SOLID_EXT_CORPSE` moves to 6, and its clip logic is `#if 0`'d (`world.cpp` 1186).
  - New flags: `FL_EASYHANDTOUCH 8192`, `FL_SPECIFICDAMAGE 16384`, `FL_FORCEGRABBABLE 32768`.
  - `SOLID_NOT_BUT_TOUCHABLE` is skipped in `SV_ClipToLinks` (1159). It is treated like NOT/TRIGGER in `SV_PushEntity` (clips only against bmodels) and in `SV_PushMove` block handling.
  - It is linked into `solid_edicts`: only `SOLID_TRIGGER` goes to `trigger_edicts`, and `SOLID_NOT` returns early.
- **Purpose:** hand-touchable non-solid objects (e.g. thrown weapons).
- **Ironwail:** `server.h` enums (189-229). They lack all of these.
- **Isolation idea:** add the enum values and change `SV_ClipToLinks` and `SV_PushEntity` with one-line `|| solid == SOLID_NOT_BUT_TOUCHABLE` checks.
- **Tag:** GAMEPLAY

### SV_LinkEdict: easy-handtouch abs expansion; rotated bbox disabled
- **Where (QVR):** `world.cpp:SV_LinkEdict` (584-697). The `#if 0` block is at 601-624; the `FL_EASYHANDTOUCH` branch at 645.
- **Upstream (BASE):** QSS rotated-BSP absbox (q2 method) when `pr_checkextension`; `FL_ITEM` gets ±15 xy; everything else ±1.
- **Change:** The rotated-absbox code is disabled. New middle branch: `else if (FL_EASYHANDTOUCH)` expands the absbox xy by ±4.5, **instead of** the ±1 epsilon (the z epsilon is also skipped).
- **Ironwail:** `world.c:SV_LinkEdict` (467) has no rotation code (QS), so the disabled block is irrelevant.
- **Isolation idea:** one extra `else if` in IW's `SV_LinkEdict`.
- **Tag:** VR-CORE

### Rotating BSP collision and pushers removed
- **Where (QVR):** `world.cpp` `SV_HullForEntity` (163), `SV_ClipMoveToEntity` (1088), `SV_LinkEdict` (601) — all `#if 0 // TODO VR: (P0) QSS Merge`. The `SV_PushMoveAngles` function is deleted.
- **Upstream (BASE):** QSS rotating SOLID_BSP collision and avelocity pushers (`SV_PushMoveAngles`), both gated by `pr_checkextension`.
- **Change:** Removed or disabled. Pushers with `avelocity` no longer rotate their riders.
- **Ironwail:** IW (QS) never had these, so the removal matches IW.
- **Isolation idea:** nothing to port.
- **Tag:** REMOVED

### Missile clip box shrunk 15 → 2
- **Where (QVR):** `world.cpp:SV_Move` (1348-1357)
- **Upstream (BASE):** `MOVE_MISSILE` used `mins2/maxs2 = ±15` against `FL_MONSTER`.
- **Change:** `±2`.
- **Purpose:** aiming matters in VR (projectiles no longer get a huge hitbox against monsters).
- **Ironwail:** `world.c` 950-956 (±15).
- **Isolation idea:** replace the constant with a cvar or macro `VR_MISSILE_EXTENT`.
- **Tag:** GAMEPLAY

### SV_Move helpers
- **Where (QVR):** `world.cpp:SV_TestEntityPositionCustomOrigin` (805), `SV_MoveTrace` (1380)
- **Change:** New helpers. `SV_TestEntityPositionCustomOrigin(ent, org)` tests a box at an arbitrary origin. `SV_MoveTrace(start,end,type,pass)` is a point trace with zero mins/maxs. They are used by `SV_PushMove` and `quake::util::checkGroundCollision`.
- **Ironwail:** add both as trivial wrappers.
- **Tag:** MISC

### SV_PushEntity traces from origin−push
- **Where (QVR):** `sv_phys.cpp:SV_PushEntity` (498-530)
- **Upstream (BASE):** trace `origin → origin+push`.
- **Change:** The trace goes `start = origin − push` → `end = origin + push`, and the entity is placed at `trace.endpos`. The unobstructed displacement is the same, but the trace starts behind the entity. `SOLID_NOT_BUT_TOUCHABLE` uses `MOVE_NOMONSTERS`. The impact is factored into `SV_PushEntityImpact` (481).
- **Purpose:** unclear. Probably to stop fast or thin objects (thrown weapons, gibs) tunnelling out of what they already overlap.
- **Ironwail:** `sv_phys.c:SV_PushEntity` (403).
- **Isolation idea:** port verbatim behind a VR-progs flag. This is a vanilla-behaviour change, so it needs testing.
- **Tag:** GAMEPLAY

### SV_PushMove rewritten (lifts / doors)
- **Where (QVR):** `sv_phys.cpp:SV_PushMove` (534-700)
- **Upstream (BASE):** QSS: skin<0 contents-pusher branch, QIP end.bsp solid backup, corpse squashing (SOLID_NOT/TRIGGER get `mins/maxs` zeroed), `SV_TestEntityPosition` to decide candidates, and the avelocity path.
- **Change:**
  - **Candidate check.** If an entity is not riding the pusher (`FL_ONGROUND` + `groundentity == pusher`), it must overlap the pusher's new absbox. It must then also be either stuck (`SV_TestEntityPositionCustomOrigin`) or "on top of the pusher". "On top" means `checkGroundCollision(MOVE_NOMONSTERS, check, …, move={0,0,-1})` hits the pusher at one of the four xy corners.
  - **Push.** The pusher is set to `SOLID_NOT` and `SV_PushEntity(check, move)` runs. The pusher's solid is then **hard-set to `SOLID_BSP`** (the original value is not restored).
  - If `move.z > 0`, set `FL_ONGROUND` and `groundentity = pusher` on the pushed entity.
  - **Block test.** A `SV_Move` point-box test at `origin + |move.z|`, with startsolid meaning blocked. Zero-width entities and SOLID_NOT/TRIGGER/NOT_BUT_TOUCHABLE entities are skipped, with no corpse squashing.
  - Blocked → restore everything and call `.blocked`, as vanilla does.
  - The source comment lists the test maps: E1M1 lift and button platform, E1M3 crusher and big doors, E2M6 slow elevator, HIP3M4 crusher.
- **Purpose:** keeps the player (whose origin is moved by room-scale motion) and physics items riding lifts reliably.
- **Ironwail:** `sv_phys.c:SV_PushMove` (434-616) has `sv_gameplayfix_elevators` (0/1/2) nudging. That is a different fix for the same problem.
- **Isolation idea:** first try IW's elevator fix with VR. Port QVR's version only if riders still fall through, then keep it as `SV_PushMove_VR` selected by a cvar.
- **Tag:** GAMEPLAY / BUGFIX

### SV_Physics_Toss: ground pre-check (items fall when support vanishes)
- **Where (QVR):** `sv_phys.cpp:SV_Physics_Toss` (1494-1595)
- **Upstream (BASE):** `if (FL_ONGROUND) return;`, then gravity, then move. Landing: `normal.z > 0.7` → set onground, zero velocity and avelocity.
- **Change:**
  - **Pre-check (step 1).** Compute `vel` with gravity applied (except FLY/FLYMISSILE) and `move = vel*dt`. Run `checkGroundCollision(MOVE_NOMONSTERS, ent, tr, off, move)`: point traces along `move` from the four bottom xy corners, or from the bottom origin if the entity has zero size.
    - No ground hit → clear `FL_ONGROUND` and fall through to the normal move.
    - Hit → `ClipVelocity(vel, n, backoff 1.5 for BOUNCE, else 1)`. If `vel.z < 60` or the movetype is not BOUNCE: when not already onground, set onground, set `groundentity`, zero vel/avel, snap the origin to the trace, relink with touch, and call impact. In both cases return.
  - **Normal move.** Then `CheckVelocity`, gravity, angles and `SV_PushEntity`. If it hit something and the entity is not free, `ClipVelocity`. The onground flag is **not** set here. Finally `SV_CheckWaterTransition`.
  - Note the glm quirk: `origin + mins[2]` and `endpos - mins[2]` add or subtract the scalar on **all three** axes. The two cancel, but a C port must be written consistently.
- **Purpose:** thrown or dropped weapons and items stop floating when a platform moves away, and they rest on moving platforms.
- **Ironwail:** `sv_phys.c:SV_Physics_Toss` (1113) is vanilla plus `MOVETYPE_GIB`.
- **Isolation idea:** port as `SV_Physics_Toss_VR` behind a flag. `checkGroundCollision` becomes a small static helper.
- **Tag:** GAMEPLAY

### Secondary think (`think2` / `nextthink2`)
- **Where (QVR):** `sv_phys.cpp:SV_RunThinkImpl` template (150-199), `SV_RunThink` (201-206)
- **Upstream (BASE):** a single think.
- **Change:** `SV_RunThink` = `RunThinkImpl(nextthink, think, doLerp=true) && RunThinkImpl(nextthink2, think2, doLerp=false)`. The first call now returns `!ent->free` immediately when `.think` is null, instead of checking `nextthink`. The `sendinterval` (U_LERPFINISH) calculation happens only for the primary think. `SV_Physics_Pusher` still uses only `.think`.
- **Purpose:** QC can run two independent timers per entity (`vr_sys_fields.qc`).
- **Ironwail:** `sv_phys.c:SV_RunThink` (123).
- **Isolation idea:** after the IW `SV_RunThink` body, call `VR_SV_RunThink2(ent)` using the `nextthink2`/`think2` field offsets.
- **Tag:** VR-CORE

### SV_Impact generalised to any func field
- **Where (QVR):** `sv_phys.cpp:SV_Impact(e1,e2, func_t entvars_t::*)` (215-240)
- **Change:** Same logic, parameterised on `touch`, `handtouch` or `vr_wpntouch`. It calls both e1's and e2's function when set and `solid != SOLID_NOT`.
- **Ironwail:** `sv_phys.c:SV_Impact` (155).
- **Isolation idea:** add `SV_ImpactField(e1,e2,fieldofs)` in C that uses a field offset.
- **Tag:** VR-CORE

### Water: lastwatertime + splash debounce; ladder and all-BSP contents removed
- **Where (QVR):** `sv_phys.cpp:SV_CheckWater` (808-850), `SV_CheckWaterTransition` (1438-1490)
- **Upstream (BASE):** QSS FTE_ENT_SKIN_CONTENTS ladder detection (`onladder`), `SV_PointContentsAllBsps`. The splash played only if `*sv_sound_watersplash.string` was non-empty.
- **Change:**
  - The ladder code and the submodel contents checks are removed; plain `SV_PointContents` is used. `edict_t::onladder` still exists but is never set, so the ladder branches in `sv_user` are dead.
  - Both functions set `.lastwatertime = time` whenever `waterlevel` changes.
  - The splash sound plays only if `time - lastwatertime > 0.2`, and the empty-string guard is gone. `SV_Physics_Step`'s land sound also lost its empty-string guard.
- **Purpose:** avoid splash spam when hands or a room-scale body bob at the surface.
- **Ironwail:** `sv_phys.c` 717/1073.
- **Isolation idea:** two lines per site plus a `lastwatertime` field offset.
- **Tag:** GAMEPLAY

### SV_Physics loop: removed follow / walk / EndFrame
- **Where (QVR):** `sv_phys.cpp:SV_Physics` (1645-1729)
- **Upstream (BASE):** `MOVETYPE_EXT_FOLLOW` → `SV_Physics_Follow`; non-client `MOVETYPE_WALK` (bots or NPCs) handled; `qcvm->extfuncs.EndFrame` called; bad movetype → `Host_EndGame`.
- **Change:** All removed. A bad movetype calls `Sys_Error`.
- **Ironwail:** QS has none of these (it has `MOVETYPE_GIB`).
- **Tag:** REMOVED

### Gravity helper
- **Where (QVR):** `sv_phys.cpp:SV_AddGravityImpl` (453-466)
- **Change:** Split out to return `ent_gravity*sv_gravity*frametime` without applying it; used by the Toss pre-check. Behaviour is otherwise unchanged (it uses `qcvm->gravityfieldoffset`).
- **Tag:** MISC

### Protocol forced to PROTOCOL_QUAKEVR (8682)
- **Where (QVR):** `sv_main.cpp` 51, `SV_Protocol_f` (1509-1596), `SV_Init` (1665-1680), `SV_SpawnServer` 4085
- **Upstream (BASE):** default `PROTOCOL_RMQ` with float coords when pext2 is on; accepted 15/666/999/BJP3.
- **Change:** Only 8682 is accepted, by the cvar and by `-protocol`. `sv.protocolflags = 0` always, so coords and angles use vanilla or short encoding. The serverinfo banner is `"QUAKE VR %s SERVER (%i CRC)"` (2063) and the buffer is 4096. The BJP3 and NETQUAKE branches are dropped. `sv_protocol_pext2 = PEXT2_SUPPORTED_SERVER` (voicechat | replacementdeltas | predinfo) is still offered.
- **Ironwail:** `protocol.h` 15/666/999; `SV_Protocol_f` (105).
- **Isolation idea:** add `PROTOCOL_QUAKEVR` as a fourth protocol and branch on it inside the entity and clientdata writers.
- **Tag:** PROTOCOL

### Entity update encoding (non-FTE path)
- **Where (QVR):** `sv_main.cpp:SV_WriteEntitiesToClient` (2564-2925)
- **Upstream (BASE):** Fitz/RMQ: U_ALPHA, U_FRAME2, U_MODEL2 and U_LERPFINISH only if protocol != 15; BJP3 special cases; skips ents with `modelindex >= limit_models`.
- **Change:**
  - Bits are always "Fitz-style", and several changed:
    - `U_SCALE (1<<20)` is set if `v.model_scale` or `v.model_scale_origin` differs from the baseline.
    - New `U_MODELOFFSET (1<<21)`, set if `v.model_offset` differs.
    - `U_ALPHA` is always allowed.
  - Write order after the entity number: model(byte), frame, colormap, skin, effects, then:

    ```
    ORIGIN1, ANGLE1, [SCALE] coord model_scale.x,
    ORIGIN2, ANGLE2, [SCALE] coord model_scale.y,
    ORIGIN3, ANGLE3, [SCALE] coord model_scale.z,
    [SCALE] vec3 model_scale_origin, [MODELOFFSET] vec3 model_offset,
    [ALPHA] byte, [FRAME2] byte, [MODEL2] byte, [LERPFINISH] byte
    ```
  - The `limit_models` skip is commented out.
  - The "invisible (alpha 0, no effects) → skip" test now runs **before** `ent->alpha` is refreshed from `.alpha`, so it uses last frame's value.
- **Purpose:** per-entity non-uniform model scaling and offset (weapon and hand models, world items).
- **Ironwail:** `sv_main.c:SV_WriteEntitiesToClient` (692). IW's `U_SCALE` is **bit 20 = one-byte RMQ/QEX scale** (lines 885/946), which is a direct bit clash.
- **Isolation idea:** under `PROTOCOL_QUAKEVR`, call `VR_SV_WriteEntityExtras`, or fork the writer. Keep the bit numbers identical to QVR so the protocol matches.
- **Tag:** PROTOCOL

### Baseline gets model_scale / scale_origin / offset
- **Where (QVR):** `sv_main.cpp:SV_CreateBaseline` (3917-3919)
- **Change:** The baseline copies these three vec3s from entvars. `MSG_WriteStaticOrBaseLine` is **unchanged**, so they are not transmitted in baselines; see Open questions.
- **Ironwail:** `sv_main.c:SV_CreateBaseline` (1535).
- **Tag:** PROTOCOL

### svc_clientdata: 32-bit bits + VR weapon/holster/clip payload
- **Where (QVR):** `sv_main.cpp:SV_WriteClientdataToMessage` (2996-3375). The VR bit logic is at 3134-3150 and the writes at 3190-3358.
- **Upstream (BASE):** `MSG_WriteShort(bits)` plus the SU_EXTEND bytes; `*2` high bytes only if protocol != 15.
- **Change:**
  - **Bits.** They are always Fitz-style. `SU_VR_WEAPON2 (1<<26)` and `SU_VR_WEAPONFRAME2 (1<<27)` are **always** set. `SU_VR_HOLSTERS (1<<28)` is set if any `holsterweapon0..5`, `holsterweaponmodel0..5` or `holsterweaponflags0..5` is non-zero. `SU_AMMO2` is also set if `currentammo2 & 0xFF00`.
  - **Header.** `svc_clientdata`, then **`MSG_WriteLong(bits)`**, and then *still* the `SU_EXTEND1` byte (`bits>>16`) and `SU_EXTEND2` byte (`bits>>24`). The bits are duplicated, and the client must read them the same way.
  - **Body:**
    - the usual view height, idealpitch, punch/velocity, items long, weaponframe, armor, weapon byte, health short
    - `currentammo` byte, **`currentammo2` byte, `ammocounter` short, `ammocounter2` short**, then shells/nails/rockets/cells, then the weapon byte or bit index
    - `*2` high bytes (`SU_AMMO2` writes both `currentammo>>8` and `currentammo2>>8`), weaponframe2-hi, weaponalpha
    - `[VR_WEAPON2]` byte `weapon2`, byte `modelindex(weaponmodel2)`
    - `[VR_WEAPONFRAME2]` byte `weaponframe2`
    - `[VR_HOLSTERS]` 24 bytes: `holsterweapon0..5`, `modelindex(holsterweaponmodel0..5)`, `holsterweaponflags0..5`, `holsterweaponclip0..5`
    - always: bytes `weapon`, `weapon2`, `weaponflags`, `weaponflags2`, `weaponclip`, `weaponclip2`, `weaponclipsize`, `weaponclipsize2`
  - There is an unused `#if 0` hand-collision experiment (3362).
- **Purpose:** dual-wield HUD, holster display, clip and ammo counters.
- **Ironwail:** `sv_main.c:SV_WriteClientdataToMessage` (990; `WriteShort` at 1090).
- **Isolation idea:** under `PROTOCOL_QUAKEVR`: write a long for the bits, then call `VR_SV_WriteClientdataExtras(ent,msg)` at three insertion points (after currentammo, after the *2 bytes, and at the end). A cleaner option is to fork the whole function into a `vr_protocol.c` file.
- **Tag:** PROTOCOL

### svc_particle2 / SV_StartParticle2
- **Where (QVR):** `sv_main.cpp:writeCommonParticleData` (1702), `SV_StartParticle` (1727), `SV_StartParticle2` (1748-1770); `server.hpp` declaration
- **Change:** `svc_particle2` (45): `[vec3 org via MSG_WriteVec3][3× char dir*16 clamped][byte preset][short count]`. `svc_particle`'s origin now also uses `MSG_WriteVec3` (the same bytes as 3 coords). This is called from a QC builtin (the builtin itself belongs to the builtins agent).
- **Ironwail:** `sv_main.c:SV_StartParticle` (223).
- **Isolation idea:** new function in `vr_protocol.c`.
- **Tag:** PROTOCOL

### SV_StartSound: pext gating removed
- **Where (QVR):** `sv_main.cpp:SV_StartSound` (1777-1945)
- **Upstream (BASE):** clients without pext or on protocol 15 skipped large entity/sound sounds. The `ent > 0x7fff` long form needed `PEXT2_REPLACEMENTDELTAS`. BJP3 always used a short sound number.
- **Change:** The skip is commented out. The long entity form is always allowed for `ent > 0x7fff`. BJP3 is ignored. The origin is written with `MSG_WriteVec3`. It keeps the QSS `origin` parameter.
- **Ironwail:** `sv_main.c:SV_StartSound(entity, channel, sample, vol, atten)` (261) has **no origin parameter**. QVR callers that pass an origin need an IW equivalent (IW has an origin-aware variant? verify).
- **Tag:** PROTOCOL / MISC

### World text server state + svc_worldtext_* messages
- **Where (QVR):** `server.cpp` (1-90), `server.hpp` (`server_t` members), `worldtext.hpp`; init at `sv_main.cpp:SV_SpawnServer` 4155; replay on spawn at `host_cmd.cpp` 2415-2428; builtins at `pr_cmds.cpp` 1189+
- **Change:**
  - `server_t` holds `std::vector<WorldText>` (text, pos, angles, halign, scale) and a free-handle stack of 65535 `uint16` handles.
  - `makeWorldTextHandle` pops from the back (0 first) and `resize(wth+1)`s. Handles are never freed.
  - Messages, each with a `short handle` after the svc byte:
    - `svc_worldtext_hmake` (46): no payload
    - `hsettext` (47): string
    - `hsetpos` (48): vec3
    - `hsetangles` (49): vec3
    - `hsethalign` (35): byte 0/1/2
    - `hsetscale` (36): float
  - Every existing world text is resent to each client at spawn.
- **Purpose:** 3D text labels in the world (VR menus, hints).
- **Ironwail:** n/a. IW `server_t` is plain C and memset.
- **Isolation idea:** `vr_worldtext.c` with a static array plus count, and a reset hook in `SV_SpawnServer`. Note that svc 35/36 overlap Fitz/IW numbering (the protocol agent must check this).
- **Tag:** PROTOCOL / RENDER

### SV_SpawnServer: vrprogs.dat + spawn hooks + source enum
- **Where (QVR):** `sv_main.cpp:SV_SpawnServer(server, SpawnServerSrc)` (4043-4268); enum in `server.hpp`
- **Upstream (BASE):** `SV_SpawnServer(server)`, loads `progs.dat`.
- **Change:**
  - New `enum class SpawnServerSrc {FromSaveFile, FromMapCmd, FromChangelevelCmd, FromRestart}`.
  - It loads **`vrprogs.dat`**.
  - Before `ED_LoadFromFile`, it sets global `spawnServerFromSaveFile = (src==FromSaveFile)` and runs QC `OnSpawnServerBeforeLoad`.
  - After serverinfo has been sent to clients, it sets the global again and runs QC `OnSpawnServerAfterLoad`, then calls `VR_OnSpawnServer()` (= `VR_ResetGlobals()`).
  - `sv.initializeWorldTexts()` runs after `SV_ClearWorld`.
  - The RMQ protocolflags setup is removed.
- **Purpose:** QC init that must know whether this is a save load; the VR module resets per map.
- **Ironwail:** `sv_main.c:SV_SpawnServer(server)` (1908; `PR_LoadProgs("progs.dat")` at 1968).
- **Isolation idea:** add `VR_SV_PreLoadEntities(src)` and `VR_SV_PostSpawn(src)` hooks. Use a global `sv_spawnsrc` instead of changing the signature, set by the callers (the host agent owns those). Pick the progs name via a cvar or hook. QC globals are looked up by name (`ED_FindGlobal`/`ED_FindFunction`) so a normal `progs.dat` still works.
- **Tag:** VR-CORE

### Misc server.hpp / client_t
- **Where (QVR):** `server.hpp`
- **Change:** `ambientsound_s` is hoisted out of the struct; `SV_StartSound` takes `const qvec3*`. `client_t` has no functional additions. `link_t`/`areanode_t` are split into `link.hpp`/`link.cpp`/`areanode.hpp` (moved, unchanged).
- **Tag:** MISC

### sv_move.cpp
- **Where (QVR):** `sv_move.cpp` (whole file)
- **Change:** No functional change (style only; `SV_MoveTrace` substitution is equivalent).
- **Tag:** MISC

## Dependencies on other subsystems
- **QC / progs:** many hard-coded entvars from `QC/vr_sys_fields.qc`:
  - hands and head: `handpos/rot/vel/throwvel/velmag/avel` and `offhand*`, `headvel`, `muzzlepos`, `offmuzzlepos`, `vrbits0`, `vryaw`, `v_viewangle`
  - locomotion: `teleport_target`, `roomscalemove`, `*_hotspot`, `button3`
  - touch: `handtouch`, `vr_wpntouch`, `handtouch_hand`, `handtouch_ent`, `touchinghand`, `ishuman`
  - timers and water: `think2`, `nextthink2`, `lastwatertime`
  - model transforms: `model_scale`, `model_scale_origin`, `model_offset`
  - weapons and holsters: `weapon2`, `weaponmodel2`, `weaponframe2`, `currentammo2`, `ammocounter(2)`, `weaponflags(2)`, `weaponclip(2)`, `weaponclipsize(2)`, `holsterweapon*/model*/flags*/clip*`
  - globals: `spawnServerFromSaveFile`, `OnSpawnServerBeforeLoad`, `OnSpawnServerAfterLoad`

  IW must resolve these through field offsets (ED_FindField-style `extfields`), not `entvars_t`, to stay C and progs-agnostic.
- **VR module:** `VR_SetHandtouchParams`, `VR_SetFakeHandtouchParams`, `VR_GetEasyHandTouchBonus` (4.5), `VR_GetWorldHandPos`, `VR_GetResolvedHandPos`, `VR_UpdateGunWallCollisions`, `VR_GetAdjustedPlayerOrigin`, `VR_OnSpawnServer`, and `cVR_OffHand=0 / MainHand=1 / FakeHand=2`.
- **Cvars (vr_cvars):** `vr_player_stepsize` (18), `vr_body_interactions` (0), `vr_fakevr` (0), `vr_enabled` (0).
- **Protocol / client:** matching client parse for clc_move, U_SCALE/U_MODELOFFSET, svc_clientdata (long bits + VR tail), svc_particle2, svc_worldtext_*.
- **Host:** the `SpawnServerSrc` callers (map, changelevel, restart, load) and the world-text replay in `Host_Spawn_f`.
- **Builtins:** worldtext builtins, particle2 builtin.

## Open questions
- `SVFTE_WriteStats` (PREDINFO path, used instead of `SV_WriteClientdataToMessage` when the client negotiates PREDINFO) does **not** carry the VR clientdata payload. Does the QVR client ever advertise PREDINFO or REPLACEMENTDELTAS? If it does, weapon2 and holster data are silently lost. `SV_CalcStats` STAT indices may also collide with QVR's `STAT_WEAPON2=15`, `STAT_HOLSTER*`, etc. (protocol agent).
- The baseline's `model_scale`/`model_offset` are set on the server but never sent in baselines. The client baseline defaults must equal the spawn values, or entities whose scale was set at spawn will show the wrong scale until it changes.
- What is the intent of `SV_PushEntity` starting at `origin − push`? It could cause false `startsolid` or back-hits on entities just behind a mover. It needs testing before porting.
- `SV_VRWpntouch` uses local HMD state, so it is wrong for remote clients on multiplayer or dedicated servers. Should the IW port use the networked `handpos`/`muzzlepos` instead?
- Dispatching `touch` to solid (non-trigger) entities in `SV_TouchLinks` changes vanilla semantics (e.g. monster or door `.touch` fires on absbox overlap during linking). Confirm the QC depends on this before replicating it. It could be limited to entities with `.handtouch`.
- `SV_PushMove` hard-restores pusher solid to `SOLID_BSP`, which loses QIP's end.bsp fix. Should IW's `sv_gameplayfix_elevators` be kept instead?
- The `sv_gameplayfix_spawnbeforethinks` cvar is still registered but has no effect.
- Does IW have an origin-taking `SV_StartSound` variant? QVR/QSS callers (the builtins agent) pass explicit origins.
