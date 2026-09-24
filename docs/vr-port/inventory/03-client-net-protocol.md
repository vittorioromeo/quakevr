# Client, networking & protocol — Quake VR functional changes

## Summary
- QVR replaces every supported protocol (15/666/999/BJP3/DP7, FTE PEXT1/2 on the client side) with **one** protocol, `PROTOCOL_QUAKEVR = 8682`. On the wire it is FitzQuake 666 with `protocolflags = 0` (16-bit 13.3 coords, 8-bit angles in updates, 16-bit angles in `clc_move`), plus VR extensions. Client and server both refuse any other number.
- VR needs the following from this subsystem:
  - a much larger `clc_move` (131 bytes) carrying head/aim angles, both hands' pos/rot/vel/throw-vel/ang-vel, head velocity, both muzzle positions, a 16-bit `vrbits0`, teleport target, hand hotspots and room-scale movement;
  - an extended `svc_clientdata` with off-hand weapon, holsters, clips and ammo counters (new `SU_VR_*` bits);
  - per-entity non-uniform scale, scale origin and model offset (`U_SCALE` redefined, new `U_MODELOFFSET`);
  - new svcs `svc_particle2` and `svc_worldtext_*`;
  - beam TEs with an extra "disambiguator" byte.
- Size: about 15 real protocol changes. Most of the 2.8k-line `cl_parse`/`host_cmd` diffs are style churn or removal of foreign-protocol code. The `net_*` files are pure churn.
- Biggest risks for the Ironwail port:
  1. **svc number clashes:** 45–49 collide with Ironwail's 2021-rerelease svcs.
  2. **`U_SCALE` clash:** the same bit has an incompatible payload in IW (1-byte uniform scale plus `B_SCALE`).
  3. **`EF_` clash:** bits 4–6 collide with IW's `EF_QEX_*`.
  4. **`STAT_` renumbering:** `STAT_ITEMS` moved from 15 to 43, and 15 is now `STAT_WEAPON2`.
  5. **Buffer size:** IW's `CL_SendMove` buffer is 128 bytes, but the VR move is 131.
  6. **Latent QVR bugs to avoid copying:**
     - baselines never use the fitz large-model/alpha bits under 8682;
     - baselines don't carry VR scale;
     - `cl.items` is clobbered at the end of every message.

## Changes

### PROTOCOL_QUAKEVR = 8682, single-protocol client/server
- **Where (QVR):**
  - `protocol.hpp:54`
  - `cl_parse.cpp:CL_ParseServerInfo` (1482–1575) and the `svc_version` case (~2898)
  - `sv_main.cpp:51` (`sv_protocol` default), `SV_Protocol_f` (~1566), `SV_SendServerinfo` (~2060–2130)
- **Upstream (BASE):** the client accepted 15/666/999/10002(BJP3)/3504(DP7). RMQ read `protocolflags` as a long. PREDINFO read a gamedir string. The client printed a verbose protocol name.
- **Change:**
  - `svc_serverinfo` is `[long 8682][byte maxclients][byte gametype][string levelname][models…0][sounds…0]`. There is no protocolflags and no gamedir (the server still writes the gamedir if `PEXT2_PREDINFO` were negotiated; see below).
  - The client sets `cl.protocolflags = 0` unconditionally and calls `VR_OnClientClearState()` right after `CL_ClearState()` (cl_parse 1508).
  - The server message says `"QUAKE VR %s SERVER"`.
  - `sv_protocol` rejects anything other than 8682.
  - All `PROTOCOL_FITZQUAKE`/`RMQ` branches in the client were rewritten to `== PROTOCOL_QUAKEVR` (even the comments were renamed).
  - Nehahra `U_TRANS`, BJP3 16-bit model/sound hacks, DP7 clientdata/entities and ProQuake stufftext stripping were all removed.
- **Purpose:** simplify; guarantee that VR data is always present.
- **Ironwail:**
  - IW supports 15/666/999 (`protocol.h:28-30`, `cl_parse.c:308,1147`).
  - 8682 does not clash with IW.
  - IW has many `cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_RMQ` checks (e.g. `cl_parse.c:477,589`, `sv_main.c`).
- **Isolation idea:**
  - Keep IW's protocols and add 8682 as a *fitz-like* protocol: a helper `PROTOCOL_IS_FITZLIKE(p)` that includes 8682, so every existing fitz branch works unchanged.
  - Add VR extras only under `if (cl.protocol == PROTOCOL_QUAKEVR)`.
  - Put a `vr_protocol.c` next to it with the read/write helpers.
- **Tag:** PROTOCOL

### FTE/DP protocol extensions effectively disabled
- **Where (QVR):**
  - `cmd.cpp` (the client no longer answers the server's `cmd pext` probe; `cl_nopext` is still registered at 39/680 but unused)
  - `cl_parse.cpp` (the DP7 `CL_EntitiesDeltaed` call is commented out at ~2842)
  - `svcfte_voicechat` no longer checks `PEXT2_VOICECHAT` (3378)
- **Upstream (BASE):** `cmd.cpp:925-929` replied `pext <PROTOCOL_FTE_PEXT2> <PEXT2_SUPPORTED_CLIENT>`.
- **Change:**
  - `cl.protocol_pext2` is always 0, so replacement deltas, PREDINFO and DP downloads are unused.
  - The server side still contains the whole pext machinery. If a foreign client did negotiate PREDINFO, the QVR server would write an input-ack short and a gamedir string that the QVR client does not read.
  - Dead-code bugs:
    - `CLFTE_ReadDelta` `enc==32` now reads a short instead of a long (cl_parse 496).
    - `SND_FTE_MOREFLAGS` parsing is commented out (1298).
- **Purpose:** simplification.
- **Ironwail:** IW has no FTE/DP extensions at all (no PEXT, no CSQC entities), so nothing needs porting.
- **Isolation idea:** nothing to port. Don't bring back pext.
- **Tag:** REMOVED

### clc_move: VR input layout (client → server)
- **Where (QVR):**
  - `cl_input.cpp:CL_SendMove` (571–735)
  - `vr.cpp:VR_Move` (4431ff) fills the `usercmd_t` VR fields
  - `sv_user.cpp:SV_ReadClientMove` (587–715)
  - struct at `protocol.hpp:555-590`
- **Upstream (BASE):** `[byte 3][float time][3× angle(16-bit for fitz)][short fwd][short side][short up][byte buttons][byte impulse]`. PREDINFO prefixed an input-sequence short; DP7 had a long-form variant.
- **Change:** the exact layout below. Each `coord` is `MSG_WriteCoord` with flags 0, meaning a **16-bit 13.3 fixed-point value (±4096, 1/8 precision)**. Each `ang16` is `MSG_WriteAngle16`, a 16-bit short where 65536 = 360°. The total is **131 bytes**.

  | # | field | encoding | bytes | server writes to |
  |---|---|---|---|---|
  | 1 | `clc_move` (3) | byte | 1 | |
  | 2 | `cl.mtime[0]` | float | 4 | ping |
  | 3 | `cl.aimangles` | 3×ang16 | 6 | `v.v_angle` |
  | 4 | `cl.viewangles` (head) | 3×ang16 | 6 | `v.v_viewangle` (new field) |
  | 5 | `vryaw` | float | 4 | `v.vryaw` |
  | 6 | `handpos` | 3×coord | 6 | `v.handpos` |
  | 7 | `handrot` | 3×coord (angles sent as coords) | 6 | `v.handrot` |
  | 8 | `handvel` | 3×coord | 6 | `v.handvel` |
  | 9 | `handthrowvel` | 3×coord | 6 | `v.handthrowvel` |
  | 10 | `handvelmag` | float | 4 | `v.handvelmag` |
  | 11 | `handavel` | 3×coord | 6 | `v.handavel` |
  | 12–17 | `offhandpos`, `offhandrot`, `offhandvel`, `offhandthrowvel`, `offhandvelmag`(float), `offhandavel` | same as 6–11 | 34 | `v.offhand*` |
  | 18 | `headvel` | 3×coord | 6 | `v.headvel` |
  | 19 | `muzzlepos` | 3×coord | 6 | `v.muzzlepos` |
  | 20 | `offmuzzlepos` | 3×coord | 6 | `v.offmuzzlepos` |
  | 21 | `vrbits0` | unsigned short | 2 | `v.vrbits0` |
  | 22 | forward/side/up move | 3×short | 6 | `usercmd` |
  | 23 | `teleport_target` | 3×coord | 6 | `v.teleport_target` |
  | 24 | `offhand_hotspot` | byte | 1 | `v.offhand_hotspot` |
  | 25 | `mainhand_hotspot` | byte | 1 | `v.mainhand_hotspot` |
  | 26 | `roomscalemove` | 3×coord | 6 | `v.roomscalemove` |
  | 27 | buttons | byte | 1 | see below |
  | 28 | impulse | byte | 1 | `v.impulse` if ≠0 |

  - **Button bits:**
    - bit0 = `+attack` → `button0`
    - bit1 = `+jump` → `button2`
    - bit2 = `+offhandattack` → `button3` (`button3` is now a regular progdefs field, not an extfield)
    - Bits 3–7 are never sent. `+button3..8` still exist as commands but are not transmitted. `+use` now drives `in_use`, which nothing reads. In BASE, `+use` was button3 = bit2 and bits 2–7 went to the extfields `button3..8`.
  - **`vrbits0`** (`quakedef_macros.hpp:299-312`, only 14 bits used), in bit order 0–13:
    `TELEPORTING`, `OFFHAND_GRABBING`, `OFFHAND_PREVGRABBING`, `MAINHAND_GRABBING`, `MAINHAND_PREVGRABBING`, `2H_AIMING`, `OFFHAND_RELOADING`, `OFFHAND_PREVRELOADING`, `MAINHAND_RELOADING`, `MAINHAND_PREVRELOADING`, `OFFHAND_RELOADFLICKING`, `OFFHAND_PREVRELOADFLICKING`, `MAINHAND_RELOADFLICKING`, `MAINHAND_PREVRELOADFLICKING`.
  - **Hotspot values** (`quakedef_macros.hpp:286-295`):
    0 = NONE, 1 = OFFHAND_2H_GRAB, 2 = MAINHAND_2H_GRAB, 3 = L_SHOULDER, 4 = R_SHOULDER, 5 = L_HIP, 6 = R_HIP, 7 = HAND_SWITCH, 8 = L_UPPER, 9 = R_UPPER.
  - The server writes all VR values **directly into edict fields** while parsing (before `SV_RunClients`).
  - The server also dropped the following BASE behaviour:
    - PREDINFO sequence/drop handling;
    - writing the `movement` extfield;
    - the ProQuake 8-bit angle path.
  - The client returns early during demo playback before the first-two-moves dump.
- **Purpose:** the server-side QC implements hand-based aiming, weapons, throwing, melee, grabbing, holsters, reload and teleport.
- **Ironwail:**
  - `cl_input.c:385 CL_SendMove` uses **`byte data[128]`** and `buf.maxsize = 128`. This must grow (QVR uses 1024), because 131 bytes overflows it.
  - `sv_user.c:438 SV_ReadClientMove` reads only the base layout.
  - IW's `usercmd_t` (`protocol.h`) has only `viewangles` and the moves.
  - IW has no `MSG_WriteVec3`/`MSG_ReadVec3`/`MSG_ReadUnsignedShort`.
  - The 16-bit coords limit hand velocity/throw-velocity to ±4096 and 0.125 precision; consider float coords for VR data in the port.
- **Isolation idea:**
  - Client: in `CL_SendMove`, after the angles, `if (cl.protocol==PROTOCOL_QUAKEVR) VR_WriteMoveExtras(&buf, cmd)`. The field order is fixed, so the move shorts must sit between `vrbits0` and `teleport_target`. It is simpler to write the whole QVR layout in a `VR_WriteMove()` that replaces the body when the protocol is 8682.
  - Server: a mirrored `VR_ReadClientMove(host_client)`.
  - Put the `usercmd_t` VR fields in a separate `vr_usercmd_t` stored in `client_state_t`/`client_t`.
- **Tag:** PROTOCOL / VR-CORE

### Aim vs. view angles split (`cl.aimangles`)
- **Where (QVR):**
  - `cl_input.cpp:CL_AdjustAngles` (460–518) modifies `cl.aimangles` instead of `cl.viewangles`
  - `client.hpp` adds `aimangles`, `vmeshoffset`, `handpos[2]`, `handrot[2]`, `prevhandrot[2]`, `handvel[2]`, `handthrowvel[2]`, `handvelmag[2]`, `handavel[2]`, `headvel`, `visual_handrot[2]`, `hotspot[2]`
  - `vr.cpp:2850` sets `cl.aimangles = cl.handrot[main]`
- **Upstream (BASE):** there was only `cl.viewangles`, which was also sent as `v_angle`.
- **Change:** keyboard turning and pitch now affect `aimangles`. Both angle sets are sent (see the table above). The QC sees `v_angle` = aim and `v_viewangle` = head.
- **Purpose:** VR aims with the hand, not the head.
- **Ironwail:** IW has only `cl.viewangles`.
- **Isolation idea:** add `aimangles` to IW's `client_state_t` (or to a VR struct). In non-VR mode keep `aimangles = viewangles` so IW's mouse and keyboard code is untouched: copy it in `VR_Move`/`CL_SendMove`.
- **Tag:** VR-CORE / INPUT

### Input buttons and commands
- **Where (QVR):** `cl_input.cpp` 66–71 (new kbuttons), 324–333, 743–806
- **Upstream (BASE):** `+use` was bound to `in_button3`.
- **Change:**
  - New commands:
    - `+/-offhandattack` (`in_offhandattack`, sent as bit2);
    - `+/-grableft`, `+/-grabright`, `+/-reloadleft`, `+/-reloadright`, `+/-flickreloadleft`, `+/-flickreloadright`. These kbuttons are read by `vr.cpp` (4034ff, 4250ff) to build `vrbits0`.
  - `+use` now uses the dead `in_use` kbutton.
  - The `in_*` kbuttons are exported in `client.hpp`.
- **Purpose:** VR controller bindings, so `vr.cpp` can map controller buttons to these commands.
- **Ironwail:** base-like, with `+use` → button3.
- **Isolation idea:** register these commands from the VR module (`VR_InitInput`), keeping the kbuttons VR-local. Only the bit2 mapping in `CL_SendMove` touches the engine.
- **Tag:** INPUT

### cl_movespeedkey / always-run semantics inverted
- **Where (QVR):** `cl_input.cpp:443` (cvar), 465 (`CL_AdjustAngles`), 557 (`CL_BaseMove`)
- **Upstream (BASE):** `cl_movespeedkey "2.0"` (not archived). The multiplier applies when `(in_speed.state&1) ^ (cl_alwaysrun != 0)`.
- **Change:**
  - `cl_movespeedkey "0.5"` CVAR_ARCHIVE.
  - The condition is now `(in_speed.state&1) ^ (cl_alwaysrun == 0)`.
  - With the default `cl_alwaysrun 0`, keyboard movement and turning are multiplied by 0.5 unless `+speed` is held. The speed key has effectively become a walk key.
- **Purpose:** probably comfort in VR (slower default movement).
- **Ironwail:** `cl_input.c:242,279,371`. IW's `cl_alwaysrun` defaults to `"1"`, so porting literally would make `+speed` a walk key.
- **Isolation idea:** don't port it. VR locomotion speed belongs in `VR_Move`/`VR_DoInput`.
- **Tag:** INPUT / MISC

### CL_SendCmd: pendingcmd replaced by IN_Move + VR_Move
- **Where (QVR):** `cl_main.cpp:CL_SendCmd` (1435–1485), `CL_AccumulateCmd` (1418)
- **Upstream (BASE):** `cmd += cl.pendingcmd` (accumulated by `IN_Move` in `CL_AccumulateCmd`).
- **Change:**
  - `CL_BaseMove(&cmd); IN_Move(&cmd); VR_Move(&cmd); CL_SendMove(&cmd);`
  - `pendingcmd` is still accumulated, then discarded. `IN_Move` therefore runs twice per network frame; the second call mostly sees zero mouse delta.
  - `VR_Move` returns early when `!vr_enabled`.
- **Purpose:** VR controller input and thumbstick locomotion.
- **Ironwail:** `cl_main.c:812 CL_SendCmd` has its own accumulate logic.
- **Isolation idea:** a single hook `VR_Move(&cmd)` just before `CL_SendMove` in IW's `CL_SendCmd`. Leave IW's `IN_Move` handling alone.
- **Tag:** VR-CORE

### svc_clientdata: VR extension (SU_VR_* bits, always-long bits)
- **Where (QVR):**
  - `protocol.hpp:262-264`
  - client `cl_parse.cpp:CL_ParseClientdata` (2011–2330)
  - server `sv_main.cpp:SV_WriteClientdataToMessage` (2996–3357)
- **Upstream (BASE):** `[short bits][ext bytes]`, fitz layout. Items were read into `cl.stats[STAT_ITEMS]`.
- **Change:**
  - New bits:
    - `SU_VR_WEAPON2 = 1<<26`
    - `SU_VR_WEAPONFRAME2 = 1<<27`
    - `SU_VR_HOLSTERS = 1<<28`
    - IW has these three as `SU_UNUSED26-28` (no clash).
  - Exact layout:
    ```
    [byte svc_clientdata][long bits (all 32 bits)]
    [byte bits>>16 if SU_EXTEND1][byte bits>>24 if SU_EXTEND2]   // redundant, kept from fitz
    [char viewheight?][char idealpitch?] then for i=0..2: [char punch[i]?][char vel[i]/16?]
    [long items]  (always; client writes cl.items directly + item_gettime flash)
    [byte weaponframe?][byte armor?][byte weaponmodelindex?(SU_WEAPON always set)]
    [short health][byte currentammo][byte currentammo2 NEW][short ammocounter NEW][short ammocounter2 NEW]
    [byte shells][byte nails][byte rockets][byte cells][byte activeweapon]
    [fitz high bytes: WEAPON2, ARMOR2, AMMO2 (now 2 bytes: currentammo>>8, currentammo2>>8), SHELLS2..CELLS2, WEAPONFRAME2, WEAPONALPHA]
    SU_VR_WEAPON2 (always set):     [byte weapon2 id][byte modelindex(weaponmodel2)]
    SU_VR_WEAPONFRAME2 (always set): [byte weaponframe2]
    SU_VR_HOLSTERS (if any holster field non-zero): 6×[byte holsterweapon0-5] 6×[byte modelindex(holsterweaponmodel0-5)]
                                                    6×[byte holsterweaponflags0-5] 6×[byte holsterweaponclip0-5]
    always: [byte weapon][byte weapon2][byte weaponflags][byte weaponflags2]
            [byte weaponclip][byte weaponclip2][byte weaponclipsize][byte weaponclipsize2]
    ```
  - The client stores these into the new stats (next section). If the off-hand weapon model index changed, it sets `LERP_RESETANIM` on `cl.offhand_viewent` (2322).
  - Off-hand and holster model indices are single bytes: models above 255 are truncated.
  - If `SU_VR_WEAPON2`/`SU_VR_WEAPONFRAME2` are clear, the client zeroes the corresponding stats.
- **Purpose:** the dual-wield HUD and view models, holster rendering, clip counters.
- **Ironwail:**
  - `cl_parse.c:710 CL_ParseClientdata` reads a short.
  - `sv_main.c:990 SV_WriteClientdataToMessage`.
  - IW also has a generic stat channel: `SV_WriteStats` (`sv_main.c:1224`) sends every stat ≥ `STAT_NONCLIENT` on change, via `svc_updatestat` (<32) or `//st` stufftext (≥32), and QC `clientstat` (#232, `pr_cmds.c:3232`) registers fields.
- **Isolation idea:** either option works.
  - (a) Keep the QVR binary layout inside `if(protocol==8682)`: extra reads after the fitz block, via `VR_ParseClientdataExtras()` and `VR_WriteClientdataExtras()`. That still requires the bits long instead of the short, plus `currentammo2`/`ammocounter*` mid-stream, so it is invasive.
  - (b) **Preferred:** keep the IW clientdata untouched and carry every VR stat via IW's stat system, from the engine's `SV_CalcStats` VR hook or from QC `clientstat()`. This removes the `SU_VR_*` bits and the mid-stream `currentammo2`/`ammocounter` fields entirely.
- **Tag:** PROTOCOL

### STAT_* renumbering
- **Where (QVR):** `quakedef_macros.hpp:117-167`
- **Upstream (BASE):** `STAT_ITEMS 15`, `VIEWHEIGHT 16`, `VIEWZOOM 21`, `IDEALPITCH 25`, `PUNCHANGLE_X..Z 26-28`, `PUNCHVECTOR 29-31`.
- **Change:**
  - New stats:

    | stat | index |
    |---|---|
    | `STAT_WEAPON2` | 15 |
    | `WEAPONMODEL2` | 16 |
    | `WEAPONFRAME2` | 17 |
    | `HOLSTERWEAPON0-3` | 18–21 |
    | `HOLSTERWEAPONMODEL0-3` | 22–25 |
    | `AMMO2` | 26 |
    | `AMMOCOUNTER` | 27 |
    | `AMMOCOUNTER2` | 28 |
    | `HOLSTERWEAPON4-5` | 29–30 |
    | `HOLSTERWEAPONMODEL4-5` | 31–32 |
    | `MAINHAND_WID` | 33 |
    | `OFFHAND_WID` | 34 |
    | `WEAPONFLAGS` | 35 |
    | `WEAPONFLAGS2` | 36 |
    | `HOLSTERWEAPONFLAGS0-5` | 37–42 |
    | `WEAPONCLIP` | 59 |
    | `WEAPONCLIP2` | 60 |
    | `HOLSTERWEAPONCLIP0-5` | 61–66 |
    | `WEAPONCLIPSIZE` | 67 |
    | `WEAPONCLIPSIZE2` | 68 |

  - The QSS stats moved by +28: `STAT_ITEMS 43`, `VIEWHEIGHT 44`, `VIEWZOOM 48`, `IDEALPITCH 52`, `PUNCHANGLE 53-55`, `PUNCHVECTOR 56-58`.
  - `STAT_FRAGS 1` is uncommented.
  - `pr_ext.cpp:8721` still advertises `STAT_ITEMS = 15` to CSQC (stale).
- **Purpose:** VR weapon, holster and clip HUD.
- **Ironwail:**
  - `quakedef.h:122-141`: `STAT_ITEMS = 15` (set by `cl_parse.c:771`), `STAT_NONCLIENT = 11`, `MAX_CL_BASE_STATS = 32`.
  - **Clash:** QVR `STAT_WEAPON2 = 15` equals IW `STAT_ITEMS`.
- **Isolation idea:** keep IW's numbering and put the VR stats in a free range (e.g. 64+, above any mod stats). Send them via IW's `SV_WriteStats`/`clientstat`. Define them in a `vr_stats.h`.
- **Tag:** PROTOCOL

### Apparent QVR bug: cl.items clobbered at end of message
- **Where (QVR):** `cl_parse.cpp:2829-2840` (the `cmd == -1` block) vs `CL_ParseClientdata` (2080–2094)
- **Upstream (BASE):** clientdata wrote `cl.stats[STAT_ITEMS]`; the end-of-message block copied it into `cl.items`.
- **Change:** QVR clientdata sets `cl.items` directly, but the end-of-message block still does `cl.items = cl.stats[STAT_ITEMS(43)]`. Stat 43 is only filled in the (unused) PREDINFO path, so `cl.items` ends up 0. Verify at runtime; it affects sbar items and invisibility viewmodel alpha (`gl_rmain.cpp:999`).
- **Ironwail:** IW handles this correctly.
- **Isolation idea:** do not port.
- **Tag:** BUGFIX (do not replicate)

### Entity update: U_SCALE redefined, U_MODELOFFSET added, per-axis scale lerp
- **Where (QVR):**
  - `protocol.hpp:143-146`
  - client `cl_parse.cpp:CL_ParseUpdate` (1687–1965, VR at 1826–1866)
  - server `sv_main.cpp` (~2660–2900)
  - lerp in `cl_main.cpp:CL_RelinkEntities` (752–809)
  - fields in `entity.hpp:95-100` and `protocol.hpp:518-520`
- **Upstream (BASE):**
  - `U_SCALE (1<<20)` = 1 byte (RMQ `PRFL_EDICTSCALE`), read after `U_ALPHA` into `netstate.scale`.
  - `U_UNUSED21`.
  - Nehahra `U_TRANS` for protocol 15.
- **Change:**
  - `U_MODELOFFSET = 1<<21`.
  - Update layout after the header bytes and entnum, with the model/frame/colormap/skin/effects bytes unchanged:
    ```
    [coord origin.x?U_ORIGIN1][angle8 ang.x?U_ANGLE1][coord model_scale.x?U_SCALE]
    [coord origin.y?U_ORIGIN2][angle8 ang.y?U_ANGLE2][coord model_scale.y?U_SCALE]
    [coord origin.z?U_ORIGIN3][angle8 ang.z?U_ANGLE3][coord model_scale.z?U_SCALE]
    [3×coord model_scale_origin ?U_SCALE][3×coord model_offset ?U_MODELOFFSET]
    [byte alpha?U_ALPHA][byte frame>>8?U_FRAME2][byte model>>8?U_MODEL2][byte lerpfinish?U_LERPFINISH]
    ```
  - The byte `U_SCALE` after alpha was removed.
  - The server sets `U_SCALE` if `v.model_scale` or `v.model_scale_origin` differs from the baseline, and `U_MODELOFFSET` if `v.model_offset` differs.
  - `model_scale` is an *offset from 1* (the renderer uses `scale+1`, `r_alias.cpp:1215-1226`, `r_brush.cpp:602-609`), so 0 means unscaled.
  - The client keeps `msg_scales[2]` and lerps `model_scale` like origin (teleport and forcelink included).
  - Effects stay 1 byte (`EF_` bits above 7 are never networked).
- **Purpose:** per-instance non-uniform scaling of weapons, hands and holster models, and pivot offset.
- **Ironwail:**
  - IW `protocol.h:66` `U_SCALE` = 1-byte uniform `ENTSCALE` (1/16), read in `cl_parse.c:595`, written in `sv_main.c:885,946`.
  - IW also has `B_SCALE (1<<3)` for baselines and `entity_state_t.scale`.
  - **Direct clash:** same bit, different payload.
  - IW has `U_UNUSED21`/`U_UNUSED22` free, and bits 24–31 (after `U_EXTEND2`) are all free.
- **Isolation idea:**
  - Leave `U_SCALE` with IW semantics.
  - Allocate new bits under 8682 only:
    - `U_VRSCALE (1<<24)`: 3 coords;
    - `U_VRSCALEORIGIN (1<<25)`: vec3;
    - `U_MODELOFFSET (1<<21)`: vec3.
  - Read and write them in a `VR_ParseUpdateExtras`/`VR_WriteUpdateExtras` block **after** IW's fitz block, so the fitz byte order is untouched.
  - Lerp `model_scale` in a small hook in `CL_RelinkEntities`.
- **Tag:** PROTOCOL / RENDER

### Baselines don't carry VR scale; fitz large-index bits never used under 8682 (QVR bug)
- **Where (QVR):**
  - `sv_main.cpp:SV_CreateBaseline` (3918-3919) copies `model_scale`/`model_scale_origin`/`model_offset` into the server baseline
  - `MSG_WriteStaticOrBaseLine` (1390–1500) never sends them, and its `bits` are only set for `protocol == PROTOCOL_FITZQUAKE || PROTOCOL_RMQ` (1433)
  - client `CL_ParseBaseline` (1969–2001)
- **Upstream (BASE):** same writer; fitz protocols set the `B_LARGEMODEL`/`B_LARGEFRAME`/`B_ALPHA` bits.
- **Change and effects:**
  - Under 8682, `svc_spawnbaseline2`/`svc_spawnstatic2` are never emitted: model index and frame go out as bytes (truncated above 255), and baseline alpha is lost.
  - The client baseline `model_scale` stays 0 while the server baseline holds the QC value. An entity spawned with a scale never gets `U_SCALE`, which is a client/server desync.
  - The client still parses `spawnbaseline2`/`static2`/`staticsound2`.
- **Ironwail:** IW's writer (`sv_main.c:1535` `SV_CreateBaseline`/signon) handles fitz and `B_SCALE`.
- **Isolation idea:**
  - Treat 8682 as fitz-like in IW's baseline code.
  - Either add `B_VRSCALE`/`B_MODELOFFSET` (`1<<4`, `1<<5`) baseline bits with vec3 payloads, or zero the VR fields in the server baseline so the first update always sends them.
- **Tag:** BUGFIX / PROTOCOL

### svc_particle2 (45)
- **Where (QVR):**
  - `protocol.hpp:395`
  - `cl_parse.cpp:3009` → `r_part.cpp:R_ParseParticle2Effect` (940–954)
  - `sv_main.cpp:SV_StartParticle2` (1748–1760), sharing `writeCommonParticleData` (1700) with `SV_StartParticle`
- **Upstream (BASE):** did not exist.
- **Change:**
  - Layout: `[byte 45][3×coord org][3×char dir*16 (clamped −128..127)][byte preset][short count]`.
  - Unreliable (`sv.datagram`), skipped if `cursize > MAX_DATAGRAM-16`.
  - The client calls `R_RunParticle2Effect(org, dir, preset, count)` (the preset list belongs to the particles subsystem).
  - The QC builtin lives in the progs subsystem.
- **Purpose:** preset-based particle effects (blood, sparks, …) for VR melee and impacts.
- **Ironwail:** **45 = `svc_setviews`** (2021 rerelease, `protocol.h:213`). IW defines it but does not parse it (only `svc_achievement` 52 and `svc_localsound` 56 are parsed; `cl_parse.c:1375-1382`).
- **Isolation idea:** renumber (e.g. to 39 or ≥ 87) or parse only when `cl.protocol==PROTOCOL_QUAKEVR`, in a `VR_ParseServerMessage(cmd)` fallback called from IW's `default:` branch before `Host_Error`.
- **Tag:** PROTOCOL

### svc_worldtext_* (46–49, 35, 36)
- **Where (QVR):**
  - `protocol.hpp:397-402`
  - client `client.cpp` (1–83) and `cl_parse.cpp:3340-3375`
  - server `server.cpp` (1–92)
  - resend on spawn in `host_cmd.cpp:Host_Spawn_f` (2415–2428)
  - `worldtext.hpp`
- **Upstream (BASE):** did not exist (BASE `svc_strings` had "35"/"36" placeholders).
- **Change:** all are reliable, per client (`client.message`), and broadcast to every client by the QC builtins (`pr_cmds.cpp:1200-1260`).

  | svc | number | payload |
  |---|---|---|
  | `hmake` | 46 | `[short handle]` (client does `worldTexts.resize(h+1)`) |
  | `hsettext` | 47 | `[short h][string]` |
  | `hsetpos` | 48 | `[short h][3×coord]` |
  | `hsetangles` | 49 | `[short h][3×coord]` (angles sent as coords) |
  | `hsethalign` | 35 | `[short h][byte 0=Left,1=Center,2=Right]` |
  | `hsetscale` | 36 | `[short h][float]` |

  - Handles are `uint16`, with `maxWorldTextInstances = 65535`.
  - The server allocates handles from a free list in ascending order and has no free message.
  - On spawn, after `svc_signonnum 3`, the server resends make/text/pos/angles/halign/scale for every existing world text.
  - `cl.worldTexts` is cleared in `CL_ClearState`.
  - Risk: `hmake` with a lower handle would *shrink* the client vector.
- **Purpose:** in-world 3D text (VR menus, labels, tutorial signs).
- **Ironwail:**
  - 46–49 are IW `svc_updateping`/`updatesocial`/`updateplinfo`/`rawprint` (unparsed).
  - 35 and 36 are free in IW and BASE.
  - IW is C, so there is no `std::vector`/`std::string`.
- **Isolation idea:** a `vr_worldtext.c` holding a fixed or `Vec_`-based array, with client parse, server send and resend-on-spawn hooks. Renumber into a free range, or gate on 8682 via `VR_ParseServerMessage`.
- **Tag:** PROTOCOL / UI

### Beam temp entities: disambiguator byte, VR-friendly beams
- **Where (QVR):** `cl_tent.cpp:CL_ParseBeam` (70–121), `CL_ParseTEnt` (144–353), `CL_UpdateTEnts` (429–513); `beam_t` in `client.hpp` adds `disambiguator`, `spin`, `scaleRatioX`
- **Upstream (BASE):** `[short ent][3×coord start][3×coord end]`; one beam per entity; start snapped to the player origin when `ent == viewentity`; PScript trail and impact effects.
- **Change:**
  - TE_LIGHTNING1/2/3 and TE_BEAM layout: `[byte te][short ent][byte disambiguator][3×coord start][3×coord end]`. The QC writes it, e.g. `hip_items.qc:359-363` (`WriteByte(...,0); /* disambiguator */ WriteVec3`).
  - A beam is replaced only if both entity and disambiguator match, so one entity can have two beams (dual-wielded lightning guns).
  - The start is **no longer snapped** to the player origin (it comes from the hand/muzzle).
  - Beam models are shrunk by mutating the alias header once:
    - `bolt2.mdl` scale ×0.5;
    - `beam.mdl` ×0.25, with X ×0.12;
    - segment step = `30*scaleRatioX`.
  - `spin` controls random roll (false for TE_BEAM).
  - `srand` is now always called (not only when paused).
- **Purpose:** dual-wield lightning guns and a hand-origin grapple.
- **Ironwail:** `cl_tent.c:59 CL_ParseBeam`, 294 `CL_UpdateTEnts` (snaps start to `viewentity`).
- **Isolation idea:** under 8682, read the extra byte in IW's `CL_ParseBeam`, add `disambiguator`/`spin`/`scaleRatioX` to `beam_t`, and skip the start snap for VR.
- **Tag:** PROTOCOL / RENDER

### Other temp-entity changes
- **Where (QVR):** `cl_tent.cpp:CL_ParseTEnt` (144–353)
- **Upstream (BASE):**
  - Supported TEDP_*, TENEH_*, TEFTE_* and the quad variants.
  - `TE_GUNSHOT` used 20 particles.
  - `TE_TAREXPLOSION` used `R_BlobExplosion`.
  - Particle effects went through PScript.
  - Unknown types raised `Host_Error`.
- **Change:**
  - Only TE 0–13 are supported; an unknown type is a `Sys_Error`.
  - Spike and gunshot impacts use `R_RunParticleEffect_BulletPuff`; gunshot count is 10.
  - Tarbaby uses `R_ParticleExplosion`.
- **Ironwail:** `cl_tent.c:118` (vanilla set).
- **Isolation idea:** only the beam change matters for the protocol. Particle choices belong to the particles subsystem.
- **Tag:** RENDER / REMOVED

### EF_* bit reassignment (entity effects)
- **Where (QVR):** `modeleffects.hpp:6-15`, used in `cl_main.cpp:CL_RelinkEntities` (812–930)
- **Upstream (BASE):**
  - `protocol.hpp`: `EF_FULLBRIGHT 1<<9`, `EF_NOSHADOW 1<<12`, `EF_NOMODELFLAGS 1<<23`.
  - Model flags could be overridden via `effects>>24`.
- **Change:**

  | effect | bit | behaviour |
  |---|---|---|
  | `EF_VERYDIMLIGHT` | 4 (16) | 50 + rand&31 radius dlight |
  | `EF_MINIROCKET` | 5 (32) | with `EF_ROCKET` model flag: trail type 7, 70-radius light |
  | `EF_LAVATRAIL` | 6 (64) | `R_RunParticleEffect_LavaSpike(org, 0, 4)` |
  | `EF_FULLBRIGHT` | 7 | defined, not referenced in the engine |
  | `EF_NOSHADOW` | 8 | defined, not referenced in the engine |
  | `EF_NOMODELFLAGS` | 9 | defined, not referenced in the engine |

  - The `effects>>24` model-flag override and `EF_NOMODELFLAGS` handling were removed. Only `ent->model->flags` is used.
  - The QC mirrors these values (`defs.qc:590-592`).
  - `U_EFFECTS` is 1 byte, so bits 8–9 never reach the client.
- **Ironwail:** `server.h:235-241`: `EF_QEX_QUADLIGHT = 16`, `EF_QEX_PENTALIGHT = 32`, `EF_QEX_CANDLELIGHT = 64` (2021 rerelease), handled in `cl_main.c:653-663`. **Direct clash** on bits 4–6.
- **Isolation idea:** under protocol 8682 (or when a VR progs is loaded) interpret bits 4–6 as the VR effects, in a `VR_EntityEffects(ent)` hook in `CL_RelinkEntities`. Alternatively, remap the VR effects to higher bits and widen `U_EFFECTS` for 8682 (this needs QC changes).
- **Tag:** PROTOCOL / RENDER

### CL_RelinkEntities: other client-side changes
- **Where (QVR):** `cl_main.cpp:681-990`
- **Change:**
  - The lerp is inlined (`CL_LerpEntity` removed) and adds scale lerp.
  - A teleport of the view entity (delta above 100 on any axis) calls `VR_PushYaw()` (778).
  - A muzzle flash on the player resets lerp on `cl.offhand_viewent` too (846).
  - `CL_AttachEntity` (tag_entity) is no longer called (dead code at 578), and it now uses `VectorAngles(fwd)` without `up`, so roll is lost.
  - Per-entity PScript trail effects were removed; `PSET_SCRIPT` is effectively off.
  - `CL_ClearState` resets `netstate` for all VR view-model entities (`forAllViewmodels`, 149) and clears `worldTexts`.
- **Ironwail:** `cl_main.c:506` (IW has its own lerp; no tags or PScript).
- **Isolation idea:** hooks `VR_OnEntityTeleport(ent)` and a scale lerp; nothing else.
- **Tag:** VR-CORE / REMOVED

### svc_setangle / svc_setview hooks
- **Where (QVR):** `cl_parse.cpp:2942` (`VR_SetAngles(cl.viewangles)` after `svc_setangle`), 2954 (`VR_PushYaw()` after `svc_setview`)
- **Upstream (BASE):** plain assignment.
- **Change:** the VR module re-bases its yaw when the server forces angles or changes the view entity.
- **Ironwail:** `cl_parse.c:1181,1187`
- **Isolation idea:** two one-line hooks.
- **Tag:** VR-CORE

### Demo recording reverted to ProQuake-style header capture
- **Where (QVR):** `cl_demo.cpp:51-52` (`demo_head[3][MAX_MSGLEN]` static, 384 KB), 200–206 (capture while `signon < 2`), `CL_Record_f` (254–380)
- **Upstream (BASE):** `CL_Record_Serverdata/Prespawn/Spawn` synthesised the signon from the current client state (all protocols and stats), and set `cl_recordingdemo`.
- **Change:**
  - QVR replays the first two captured signon messages, then writes names, frags, colors, all lightstyles, 4 kill/secret stats, setview and `signonnum 3`.
  - Other stats and fog are not written.
  - World texts are not re-recorded when recording starts mid-map.
  - `cl_recordingdemo` is no longer updated.
  - The per-protocol refusal check was removed.
  - VR hand data is client-local and not in demos.
- **Ironwail:** IW has its own `demo_head` plus demo rewind (`cl_demo.c:509`, 320ff). The IW rewind snapshot would need the VR entity fields (`model_scale` etc.).
- **Isolation idea:** keep IW's demo code. Make sure mid-map recording under 8682 writes 8682 in serverinfo and re-emits VR state (world texts, VR stats).
- **Tag:** MISC / REMOVED

### Packet and buffer size limits
- **Where (QVR):** `quakedef_macros.hpp:83-108,329-341`, `net.hpp:40`, `cl_input.cpp:573`, `host.cpp:372`
- **Change:**

  | const | BASE | QVR | IW |
  |---|---|---|---|
  | `MAX_MSGLEN` | 64000 | **128000** | 64000 |
  | `MAX_DATAGRAM` | 32000 | **128000** | 64000 |
  | `DATAGRAM_MTU` | 1400 | **2048** | 1400 |
  | `NET_MAXMESSAGE` | 64000 | 65535 | 65535 |
  | `MAX_QPATH` | 64 | **128** | 64 |
  | `MAX_BEAMS` | 32 | **128** | 32 |
  | `MAX_TEMP_ENTITIES` | 256 | **512** | 256 |
  | `MAX_LIGHTSTYLES` | 1024 | 1024 | 64 |
  | `CL_SendMove` buf | 1024 | 1024 | **128** |
  | `svs.maxclientslimit` min | 4 | **16** | 4 |

  - `NETFLAG_LENGTH_MASK` is 0xffff, so datagrams above 65535 cannot be framed.
  - `limit_reliable`/`limit_unreliable` for local clients is `MAX_DATAGRAM` (128000, `sv_main.cpp:2006-2024`), which exceeds the loopback message buffer (65535).
  - Remote unreliable limit 2048 is larger than a typical 1500 MTU, so packets will fragment.
- **Purpose:** big VR `clientdata` and move, and many VR models (long paths).
- **Isolation idea:**
  - Raise IW's `CL_SendMove` buffer (mandatory).
  - Don't raise `DATAGRAM_MTU`.
  - Raise `MAX_BEAMS`/`MAX_TEMP_ENTITIES` only if needed (dual lightning × segments).
  - Check whether any QVR asset path is longer than 63 characters before bumping `MAX_QPATH`, because it affects `MAX_QPATH`-sized arrays and savegames.
- **Tag:** PROTOCOL / PERF

### msg / sizebuf helpers
- **Where (QVR):** `msg.cpp` / `msg.hpp` (split from `common.cpp`), `sizebuf.cpp`
- **Upstream (BASE):** `common.cpp:617-1015`
- **Change:**
  - New `MSG_WriteUnsignedChar`, `MSG_WriteUnsignedShort`, `MSG_ReadUnsignedChar`, `MSG_ReadUnsignedShort`, `MSG_WriteVec3`/`MSG_ReadVec3` (3× coord).
  - PARANOID range checks are disabled.
  - `SZ_Clear` no longer resets `overflowed` (BASE did), and the overflow message lost its `\n`.
  - Otherwise identical.
- **Ironwail:** `common.h:220-243` has none of the new helpers.
- **Isolation idea:** add the Vec3 and unsigned-short helpers to `vr_protocol.c` or `common.c`. Keep IW's `SZ_Clear`.
- **Tag:** MISC

### Host frame / init changes
- **Where (QVR):** `host.cpp`
- **Change:**
  - `Host_FilterTime` (795): the `host_maxfps` cap is skipped when `vr_enabled`, because the headset paces the frame loop.
  - `_Host_Frame` (1105–1109): `quake::saveutil::doAutomaticAutosave()` runs after `CL_ReadFromServer` when `!deathmatch`.
  - `Host_Init`:
    - `VR_InitCvars()` right after `Cvar_Init` (1232);
    - after `exec quake.rc` it runs `Cbuf_Execute()` then, if `vr_enabled==1`, `VR_ModAllModels()` (1294-1301).
  - `Host_Shutdown`: `VID_VR_Shutdown()` (1370).
  - `Host_Error`/`Host_EndGame` call `std::terminate()` if the first frame's `setjmp` hasn't run yet (`host_abortserver_setjmp_done`, 220/311/937).
  - New `Host_Warn()` (233).
  - `svs.maxclientslimit` minimum 16 (372).
  - `Host_ShutdownServer` value-initialises `sv`/`cl`.
- **Ironwail:** `host.c` (`maxclientslimit` at 265). IW has its own frame pacing and vsync logic.
- **Isolation idea:** hooks in `Host_Init`/`Host_Shutdown`, a `VR_IsActive()` check in the frame-rate limiter, and an autosave hook (savegame subsystem).
- **Tag:** VR-CORE / MISC

### host_cmd changes (save/load, spawn, startdemos, give)
- **Where (QVR):** `host_cmd.cpp`
- **Change:**
  - **Save and load:**
    - `Host_MakeSavegame(filename, const time_t* timestamp, printMessage)` (1322). When a timestamp is given, the file's **first line** is `strftime("%F %T")` (autosaves).
    - `Host_Loadgame(filename, hasTimestamp)` (1508) skips that line.
    - New command `load_autosave <name>` (1808, 3384).
    - After loading, the engine runs the QC function `pr_global_struct->OnLoadGame` (1771–1772). This is a new progdefs global, so it is a progs-format dependency.
    - The format is otherwise QSS extended save.
  - **`Host_Spawn_f`:**
    - In single player, after `PutClientInServer`, the engine calls `quake::saveutil::doChangelevelAutosave()` (2305–2313).
    - After `svc_signonnum 3`, it resends all world texts (2415–2428).
  - **`Host_Startdemos_f` (3108–3116):** if `vr_enabled`, it queues `maxplayers 1; deathmatch 0; coop 0; map vrstart; centerview` instead of the demo loop. QuakeSpasm's "go to menu" (`!fitzmode`) branch is commented out, so the demo loop plays when not in VR.
  - **`Host_Give_f` (2828–2879):** after giving, it recomputes `v.ammocounter`/`v.ammocounter2` from `v.currentammo`/`v.currentammo2`, interpreted as `AID_*` ammo ids (NONE, SHELLS, NAILS, ROCKETS, CELLS, LAVA_NAILS, MULTI_ROCKETS, PLASMA; the last three via fields `ammo_lava_nails`/`ammo_multi_rockets`/`ammo_plasma`). This replaces the BASE `currentammo` switch on the weapon `IT_` bit.
  - **`SV_SpawnServer`** takes `SpawnServerSrc::{FromMapCmd, FromChangelevelCmd, FromRestart, FromSaveFile}`.
  - `status` prints `VERSION` as a float.
- **Ironwail:** `host_cmd.c` (`Host_Savegame_f` 2376, `Host_Loadgame_f` 2484, `Host_Spawn_f` 3054, `Host_Startdemos_f` 3657). IW's save format differs (IW has its own extended save and async save).
- **Isolation idea:**
  - A world-text resend hook in `Host_Spawn_f`.
  - An autosave hook.
  - `vrstart` auto-launch in the VR module (on first `startdemos` or at init).
  - Give/ammo-counter logic, which is QC-mod specific, only when a VR progs is loaded.
  - Details belong to the savegame and server inventories.
- **Tag:** GAMEPLAY / MISC

### Misc client-state additions (VR view-model entities)
- **Where (QVR):** `client.hpp`
  - `cl.offhand_viewent`
  - 4 holster entities plus 4 holster "slot" entities
  - `vrtorso`
  - `mainhand_wpn_button`/`offhand_wpn_button`
  - `textentity_t mainhand_wpn_text`/`offhand_wpn_text`
  - `hand_entities` (base + 5 fingers) for left/right and left/right ghost
  - `anyViewmodel`/`forAllViewmodels` helpers
  - `entity_t` extras in `entity.hpp:88-107`: `horizFlip`, `msg_scales`, `model_scale*`, `model_offset`, `hidden`, `zeroBlend`, `lightmod`/`lightmodvalue`
- **Change:** client-side only; nothing is networked except via the stats. Consumed by `vr.cpp`/render.
- **Ironwail:** `client.h` `cl.viewent` only.
- **Isolation idea:** keep them in a VR-owned struct (`vr_client_state`). Only the added `entity_t` render fields must go into IW's `entity_t`.
- **Tag:** VR-CORE / RENDER

### net_* files
- **Where (QVR):** `net_dgrm/main/udp/wins/loop/wipx/bsd`
- **Change:**
  - Style churn only.
  - The `sv_public` cvar default is `"1"` (BASE: NULL/registered later).
  - `qsocket_t` gains an unused `char address[NET_NAMELEN]`.
  - `net_wins`: `in6addr_any` is now properly initialised (BUGFIX).
- **Ironwail:** IW's net code is QS-based. Nothing to port.
- **Tag:** MISC

## Dependencies on other subsystems
- **QC/progs:** new entvars written by `SV_ReadClientMove` and read by the clientdata writer:
  - `v_viewangle`, `vryaw`
  - `handpos/rot/vel/throwvel/velmag/avel` and their `offhand*` counterparts
  - `headvel`, `muzzlepos`, `offmuzzlepos`, `vrbits0`, `teleport_target`
  - `offhand_hotspot`, `mainhand_hotspot`, `roomscalemove`
  - `button3` (now a core field)
  - `model_scale`, `model_scale_origin`, `model_offset`
  - `weapon2`, `weaponmodel2`, `weaponframe2`
  - `currentammo2`, `ammocounter(2)`
  - `holsterweapon0-5`, `holsterweaponmodel0-5`, `holsterweaponflags0-5`, `holsterweaponclip0-5`
  - `weaponflags(2)`, `weaponclip(2)`, `weaponclipsize(2)`
  - global `OnLoadGame`
  - builtins for world text, `particle2` and `WriteVec3` (progs inventory)
- **VR core (`vr.cpp`):**
  - `VR_Move`, `VR_SetAngles`, `VR_PushYaw`, `VR_OnClientClearState`, `VR_InitCvars`, `VR_ModAllModels`, `VID_VR_Shutdown`
  - `vr_enabled`
  - hotspot and holster geometry
- **Particles:** `R_ParseParticle2Effect`/`R_RunParticle2Effect` presets, `R_RunParticleEffect_BulletPuff`, `R_RunParticleEffect_LavaSpike`, `R_RocketTrail` type 7.
- **Render:** `model_scale`/`scale_origin`/`offset` application in `r_alias`/`r_brush`; world-text rendering; beam model header scaling.
- **Savegame:** `saveutil` autosaves and the timestamped save format.
- **Server:** `SV_WriteEntitiesToClient`, `SV_CreateBaseline`, `SV_StartParticle2`, `server_t` world-text storage, `SpawnServerSrc`.

## Open questions
- Does QVR really clobber `cl.items` every message (sbar keys and powerups invisible)? Verify in-game before deciding which behaviour the port should keep.
- Are 16-bit coords (±4096, 1/8 unit) acceptable for hand velocities, throw velocities and angular velocities, or should the port send VR move fields as floats (protocolflags or dedicated float writes)? QVR's `handvelmag` is already a float.
- Should VR stats go through IW's generic stat path (QC `clientstat`) instead of `SU_VR_*`? That requires adding `clientstat` calls to the QVR QC, or registering the stats in the engine's VR hook.
- Should the port pick new svc numbers (35/36 are fine; 45–49 clash) or keep QVR's numbers and gate them on protocol 8682? Keeping them allows mixing QVR-built demos and servers, but only if exact QVR wire compatibility is a goal. It likely is not, given the baseline bugs.
- Do the `EF_` bits 4–6 need to stay binary-compatible with the QVR QC (`defs.qc:590-592`), or can the QC be changed to higher bits and `U_EFFECTS` widened?
- Does any QVR asset path exceed 63 characters (the reason `MAX_QPATH` went to 128)?
- Is `svs.maxclientslimit ≥ 16` needed for VR, or is it incidental?
