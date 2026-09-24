# QuakeC VM, progs & builtins — Quake VR functional changes

## Summary
The QC VM itself (pr_exec, pr_comp opcodes, qcvm_t layout, stack sizes) is functionally **unchanged** from QSS; the VR
work lives in (1) **32 new SSQC builtins #79–#110** appended to the fixed `pr_ssqcbuiltins[]` table, (2) a **custom
system-defs ABI**: `progdefs_generated.hpp` (FTEQCC-generated, `PROGHEADER_CRC 52440`) with VR fields/globals added to
`entvars_t`/`globalvars_t` *and two fields inserted in the middle of the vanilla layout*, so every vanilla offset after
`currentammo`/`watertype`/`killed_monsters` shifts, (3) three new QC entry points called by the engine
(`OnSpawnServerBeforeLoad`, `OnSpawnServerAfterLoad`, `OnLoadGame`) plus global `spawnServerFromSaveFile`, (4) 40 spawn
parms (QSS extended-parm mechanism, ungated), and (5) a handful of behaviour tweaks to vanilla builtins
(droptofloor, random, setmodel, precache).
**Biggest port risks:** (a) 8 builtin-number collisions with Ironwail's table (#79, #80, #81, #94, #95, #96, #97, #99),
including `WriteVec3=#95` which the QC calls 68 times; (b) IW's DP trig builtins use **radians**, QVR's use **degrees**;
(c) IW hardcodes vanilla `progdefs.q1` (CRC 5927) and only 16 spawn parms, so `vrprogs.dat` is rejected outright;
(d) handle-returning builtins return raw ints in float slots.
QVR QC uses **no** QSS extension builtins (#0 / checkextension / autocvars / CSQC / SV_ParseClientCommand); it only
relies on QSS for extended spawn parms and the `extfields` lookups (`gravity`, `alpha`).

## Builtin table (every builtin the QVR QC declares)

Sources: `QC/builtins.qc` (included from `defs.qc:835`) and `QC/frikbot/bot.qc:293-309`, `frikbot/bot_ed.qc:1452`.
QVR table: `Quake/pr_cmds.cpp:2332-2433` (`pr_ssqcbuiltins[]`, `pr_ssqcnumbuiltins` = 111). CSQC table
(`pr_cmds.cpp:2594-2663`) unchanged apart from style. IW table: `quakevr-iw/Quake/pr_cmds.c:3277-3440`
(`pr_builtindefs[]`, number-indexed; unnumbered entries allocated downward from `MAX_BUILTINS-2`=1278).
"Calls" = call sites in QC (excluding the declaration).

### Vanilla range #1–#78

All vanilla numbers are present in both QVR and IW with the same meaning. Only the ones with a QVR behaviour
difference are listed; everything else (#1-2, #4, #6, #8-15, #17-18, #21-33, #35-38, #40-41, #43-49, #52-59, #67-70,
#72-77) is style-only.

| # | QC decl | QVR impl (pr_cmds.cpp) | QVR behaviour difference vs BASE | IW status |
|---|---|---|---|---|
| 3 | `setmodel(e, m)` | `PF_sv_setmodel` 351-442 | Non-precached model **always** auto-precached with warning (`Con_DWarning` while loading, `Con_Warning` otherwise) then `SV_Precache_Model`; BASE only did that when `pr_checkextension`, else `PR_RunError("no precache")`. | IW `PF_setmodel` pr_cmds.c:303-323 **always errors** "no precache". Needs QVR behaviour (weapon/holster models set late). |
| 7 | `random()` | `PF_random` 652 | Returns `(rand()&0x7fff)/0x7fff` → **[0,1] inclusive** (vanilla bug restored; BASE had /0x8000). | IW has `sv_gameplayfix_random` (default 1 = new formula); `0` gives exactly QVR's formula. |
| 16 | `traceline` | `PF_traceline` 858 | Uses `SV_MoveTrace` (= `SV_Move` with zero mins/maxs, world.cpp:1380) — same result. NaN warning gated on `quake::vr::developerMode()` (debug build) instead of `developer`. | same |
| 19/20/75/76 | precache_* | `SV_Precache_Sound` 1609, `SV_Precache_Model` 1655, `PF_sv_precache_model` 1682 | Removed the "Precache should only be done in spawn functions" warning (BASE warned when `!pr_checkextension` and not loading). Late precache is still allowed. | IW has no such warning. OK. |
| 34 | `droptofloor()` | `PF_droptofloor` 1792-1842 | **Rewritten.** Instead of one box `SV_Move` down 256: point traces (`SV_MoveTrace`, `MOVE_NOMONSTERS`) from `origin + (x,y,mins.z)` down 256 for the centre `(0,0)`, then the 4 XY corners (`anyXYCorner`, util.hpp:305: (minx,miny),(minx,maxy),(maxx,miny),(maxx,maxy)); evaluation **short-circuits at the first trace that hits** (`fraction<1 && !allsolid`). `origin.z = hit.z - mins.z`, sets `FL_ONGROUND`, `groundentity = hit.ent`. Returns 0 if none hit. Effect: items no longer fail to drop when their box starts in solid; monsters are ignored. | IW is vanilla (pr_cmds.c:1240). Needs replacement (or a VR hook when QVR progs are loaded). |
| 51 | `vectoangles` | `PF_vectoangles` 602 | QSS's `PR_EnableExtensions` swapped #51 for the 2-arg `PF_ext_vectoangles`; QVR **removed that swap** (pr_ext.cpp:8386, "crashes on safeAtan2 assertion"). So #51 is always the vanilla 1-arg version. | IW #51 is vanilla 1-arg. OK. |
| 73 | `centerprint` / `frik_big_centerprint(client, s1..s7)` | `PF_centerprint` 501 | unchanged (uses `PF_VarString(1)`). | IW same (VarString). OK. |
| 78 | `setspawnparms(e)` | `PF_sv_setspawnparms` 2251-2284 | Copies parms 1-16 into `parm1..` then **always** (no `pr_checkextension` gate) copies 17..64 via `ED_FindGlobal("parmN")`. QC uses parm1..parm40. | IW copies only `NUM_SPAWN_PARMS`=16 (pr_cmds.c:1688). Must be extended. |

Frikbot (`bot.qc:293-309`) re-declares #8, #21, #24, #44, #52-59, #73, #78 under `frik_*` names — all vanilla.
`frik_checkextension = #99` (`bot_ed.qc:1452`) is **declared but never called**; in QVR #99 is `substr`.

### VR range #79–#110 (all new, SSQC only; CSQC table does not have them)

| # | QC signature | QVR impl | Semantics (enough to re-implement) | Calls in QC | IW at this number |
|---|---|---|---|---|---|
| 79 | `void(vector o, vector d, float preset, float count) particle2` | `PF_particle2` pr_cmds.cpp:696 → `SV_StartParticle2` sv_main.cpp:1748 | Writes `svc_particle2`(45) to `sv.datagram` (skip if cursize > MAX_DATAGRAM-16): `vec3 org` (WriteCoord×3), `dir*16` as 3 clamped chars, `byte preset`, `short count`. | 41 | **COLLISION** `finaleFinished` (2021 rerelease) |
| 80 | `float(float base, float exp) pow` | `PF_pow` 664 | `std::pow`. | 3 | **COLLISION** `localsound` (IW `pow` is #97) |
| 81 | `void(float hand, float delay, float duration, float frequency, float amplitude) haptic` | `PF_haptic` 712 → `VR_DoHaptic` vr.cpp:3910 | hand: 0=off hand, 1=main hand. Calls the **local** VR haptics directly from server QC (no network message) — only meaningful on a listen server; any player's event buzzes the host's controllers. | 10 | **COLLISION** `stof` (FRIK_FILE) |
| 82 | `float(float a, float b) min` | `PF_min` 734 | 2-arg `std::min`. | 0 | free (IW `min` is #94, variadic) |
| 83 | `float(float a, float b) max` | `PF_max` 746 | 2-arg `std::max`. | 3 | free (IW `max` is #95) |
| 84 | `void(vector ang) makeforward` | `PF_makeforward` 187 | Sets only `v_forward = AngleVectorsOnlyFwd(ang)` (util.hpp:168). | 12 | free |
| 85 | `float(float in, float inMin, float inMax, float outMin, float outMax) maprange` | `PF_maprange` 203 → `mapRange` util.hpp:29 | `outMin + (outMax-outMin)/(inMax-inMin)*(in-inMin)`, no clamping. | 0 | free |
| 86 | `float(string s) cvar_hmake` | `PF_cvar_hmake` 1146 → `Cvar_MakeHandle` cvar.cpp:820 | Pushes `cvar_t*` into static `std::vector cvar_handles` (cvar.cpp:37); returns index **as `G_INT`** (int bit pattern in a float global), -1 + console msg if cvar missing. | 1 (`vr_cvars.qc:84`, ~70 cvars) | free |
| 87 | `float(float h) cvar_hget` | `PF_cvar_hget` 1158 → cvar.cpp:832 | Reads handle via `G_INT(PARM0)`, returns `cvar->value`; bad handle → 0 + msg. | **115** | free |
| 88 | `void() cvar_hclear` | `PF_cvar_hclear` 1445 → cvar.cpp:854 | Clears handle vector. QC calls it at start of `VR_CVars_InitAllHandles` (called from `OnSpawnServerBeforeLoad`, `OnSpawnServerAfterLoad`, `OnLoadGame`). | 1 | free |
| 89 | `vector(vector input, vector exemplar) redirectvector` | `PF_redirectvector` 1450 → util.hpp:216 | `[f,r,u]=AngleVectors(exemplar)`; returns `f*in.x + r*in.y + u*in.z`. | 0 | free |
| 90 | `void(float h, float v) cvar_hset` | `PF_cvar_hset` 1170 → cvar.cpp:843 | `Cvar_SetValueQuick(handles[G_INT(PARM0)], v)`. | 0 | free (IW has no #90; rerelease `centerprint`=#90 patched only for fn named `centerprint`, see below) |
| 91 | `float() worldtext_hmake` | `PF_worldtext_hmake` 1189 | `Host_Error` if no free handle; `sv.makeWorldTextHandle()` (uint16); sends `svc_worldtext_hmake` to every active/spawned client; returns handle **as `G_INT`**. | 2 | free |
| 92 | `void(float h, string s) worldtext_hsettext` | 1205 | Validates handle (`Host_Error` if invalid), stores `_text`, broadcasts `svc_worldtext_hsettext`. | 3 | free |
| 93 | `void(float h, vector v) worldtext_hsetpos` | 1222 | same pattern, `_pos`, `svc_worldtext_hsetpos`. | 2 | free |
| 94 | `void(float h, vector v) worldtext_hsetangles` | 1239 | `_angles`, `svc_worldtext_hsetangles`. | 2 | **COLLISION** `min` (DP_QC_MINMAXBOUND) |
| 95 | `void(float to, vector v) WriteVec3` | `PF_sv_WriteVec3` 2203 → `MSG_WriteVec3` msg.cpp:191 | `WriteDest()` + 3×`MSG_WriteCoord(v[i], sv.protocolflags)`. Pure QC convenience; wire-identical to 3 WriteCoords. | **68** (temp entities everywhere) | **COLLISION** `max` |
| 96 | `void(float h, float v) worldtext_hsethalign` | 1256 | `_hAlign = (WorldText::HAlign)v` (0 left, 1 centre, 2 right), `svc_worldtext_hsethalign`. | 2 | **COLLISION** `bound` |
| 97 | `float(string s) strlen` | `PF_strlen` 1292 | `std::strlen`. | 1 | **COLLISION** `pow` (IW `strlen` is #114) |
| 98 | `float(string s, float n) nthchar` | `PF_nthchar` 1297 | Returns `(float)s[(int)n]` (signed char, no bounds check). | 1 | free |
| 99 | `string(string s, float b, float e) substr` | `PF_substr` 1303 | Copies `s[b..e)` into `PR_GetTempString()` (no bounds check), returns `PR_SetEngineString(buf)`. **End index**, not length (≠ FRIK `substring`). | 1 | **COLLISION** `checkextension` |
| 100 | `float(float entGravity, float throwSpeed, vector from, vector to) calcthrowangle` | `PF_calcthrowangle` 1322 | g = -(entGravity?:1)·sv_gravity·host_frametime; x = horiz dist, z = from.z-to.z; if `v⁴ - g(gx²+2zv²) < 0` → 0; else `degrees(atan2(v²-sqrt(..), g·x))`. | 0 | free |
| 101 | `vector(vector vec, vector up, float angle) rotatevec` | `PF_rotatevec` 1359 | **Buggy/WIP**: returns `normalize(vec + (0,0,|tan(rad(angle))|))`; rest is dead code; `up` ignored. | 0 | free |
| 102 | `float(float angle) sin` | `PF_sin` 1395 | `sin(radians(a))` — **degrees in** | 1 | free (IW `sin` #60 takes **radians**) |
| 103 | `float(float angle) cos` | 1400 | `cos(radians(a))` — degrees | 1 | free (IW `cos` #61 radians) |
| 104 | `float(float) asin` | 1410 | `degrees(asin(x))` — **degrees out** | 0 | free (IW #471 radians) |
| 105 | `float(float) acos` | 1415 | `degrees(acos(x))` | 0 | free (IW #472) |
| 106 | `float(float angle) tan` | 1405 | `tan(radians(a))` | 0 | free (IW #475) |
| 107 | `float(float) atan` | 1420 | `degrees(atan(x))` | 0 | free (IW #473) |
| 108 | `float(float) sqrt` | 1425 | `std::sqrt` | 1 | free (IW `sqrt` #62) |
| 109 | `float(float x, float y) atan2` | 1431 | `degrees(atan2(PARM0, PARM1))` | 1 | free (IW #474, radians) |
| 110 | `void(float h, float v) worldtext_hsetscale` | 1275 | `_scale`, `svc_worldtext_hsetscale`. | 2 | free |

Used-but-unused summary: `min`, `maprange`, `redirectvector`, `cvar_hset`, `calcthrowangle`, `rotatevec`, `asin`,
`acos`, `tan`, `atan` are declared but never called by the current QC (still keep numbers stable).
Trig users are only in `vr_wpnforcegrab.qc`.

## Changes

### New SSQC builtins #79–#110
- **Where (QVR):** `pr_cmds.cpp` 187-229, 652-757, 1146-1462, 2203; table 2385-2431. Helpers: `cvar.cpp:820-857`, `util.hpp:29,168,216`, `msg.cpp:191`, `sv_main.cpp:1702-1760`, `server.cpp` (worldtext), `vr.cpp:3910`.
- **Upstream (BASE):** table ended at #78 (`PF_sv_setspawnparms`); #79+ reachable only through QSS extension lazy-binding in `PF_Fixme` (pr_ext.cpp:8208).
- **Change:** see table above. Because QVR's fixed table now covers 0..110, every QSS extension whose documented number is ≤110 (sin=60, tracebox=90, min=94, pow=97, checkextension=99, …) is **unreachable by number** in SSQC; extensions >110 still lazy-bind.
- **Purpose:** haptics, particle presets, world-text banners, cvar access without string lookups, force-grab/throw math.
- **Ironwail:** `pr_builtindefs[]` pr_cmds.c:3277; `PR_InitBuiltins` pr_edict.c:1857 fills `builtins[def->number]` in table order (later entry wins). Collisions at #79/80/81/94/95/96/97/99 (see table). IW numbers 60-62/65 (sin/cos/sqrt/etos) are unused by QVR QC, fine to keep.
- **Isolation idea:** new `pr_vr.c` with the 32 PF_ functions and a `vr_builtindefs[]` array; in `PR_InitBuiltins` (or right after it, only for `sv.qcvm` when a VR progs is detected, e.g. by CRC 52440 or presence of global `OnLoadGame`) overwrite `builtins[79..110]` and set `builtin_ext[n]=STD_QC` so `PR_CheckBuiltinExtension` doesn't warn. Name the IW-colliding entries with explicit numbers so name-based `#0` remap is unaffected. Do **not** reuse IW's `PF_Sin/PF_pow/PF_min` for the VR numbers (radians/variadic semantics differ only harmlessly for min/max/pow/sqrt/strlen; must differ for trig).
- **Tag:** VR-CORE

### Handle-typed builtins return int bit patterns
- **Where (QVR):** `PF_cvar_hmake` 1148, `PF_cvar_hget` 1160, `PF_cvar_hset` 1172, `PF_worldtext_*` 1189-1290 (`G_INT`).
- **Change:** handles are written/read with `G_INT` though QC declares them `float`. Handle 0 is denormal 0.0; -1 is NaN-ish. QC only stores/passes them, so it works; comparisons in QC would be wrong.
- **Ironwail:** n/a.
- **Isolation idea:** keep `G_INT` symmetry exactly (or switch all to `G_FLOAT` together; QC is agnostic). Cvar handles are rebuilt by QC on every spawn/load, so nothing is persisted.
- **Tag:** VR-CORE

### Custom system defs: entvars_t / globalvars_t (progdefs_generated.hpp, CRC 52440)
- **Where (QVR):** `progdefs.hpp` (`#define vec3_t qvec3` + include), `progdefs_generated.hpp` 1-254; QC side `QC/defs.qc` 1-289 (system globals/fields) + `QC/vr_sys_fields.qc` (included at defs.qc:289, before `end_sys_fields`). CRC check `pr_edict.cpp:1321` against `PROGHEADER_CRC`.
- **Upstream (BASE):** `progdefs.q1`, CRC 5927.
- **Change — globalvars_t:** `pad[28]` spelled as `ofs_return/ofs_parm0..7` (same size). **Inserted** `float spawnServerFromSaveFile` between `killed_monsters` and `parm1` (shifts every later global). `parm17..parm40` added after `parm16` (so v_forward etc. shift again). Appended after `SetChangeParms`: `func_t OnSpawnServerBeforeLoad, OnSpawnServerAfterLoad, OnLoadGame`.
- **Change — entvars_t:** **Inserted** `float ammocounter` after `currentammo` and `float lastwatertime` after `watertype` (both shift all later vanilla fields). Appended after `noise3`: `v_viewangle`(vec), `model_scale`, `model_scale_origin`, `model_offset` (vec), `vr_itemId`, `handtouch`(func), `vr_wpntouch`(func), `think2`(func), `nextthink2`, `weaponflags`, `weapon2`, `weaponmodel2`(string), `weaponframe2`, `weaponflags2`, `weaponclip`, `weaponclip2`, `holsterweaponclip0..5`, `weaponclipsize`, `weaponclipsize2`, `holsterweapon0..5`, `holsterweaponmodel0..5`(string), `holsterweaponflags0..5`, `offhand_hotspot`, `mainhand_hotspot`, `currentammo2`, `ammocounter2`, `button3`, `vryaw`, `handpos/handrot/handvel/handthrowvel`(vec), `handvelmag`, `handavel`(vec), `offhandpos/offhandrot/offhandvel/offhandthrowvel`(vec), `offhandvelmag`, `offhandavel`(vec), `headvel`, `muzzlepos`, `offmuzzlepos`(vec), `vrbits0`, `teleport_target`, `roomscalemove`(vec), `touchinghand`, `handtouch_hand`, `handtouch_ent`(entity), `ishuman`.
- **How the engine accesses them:** all via **hardcoded struct offsets** (`ent->v.handpos`, `pr_global_struct->OnLoadGame`, `&entvars_t::think2` member pointers in sv_phys.cpp:204-205, 1143, 1201). No `ED_FindField` for VR fields. Engine users: `sv_user.cpp` (hand/offhand/head/muzzle/vryaw/hotspots/v_viewangle/button3/vrbits0/teleport_target/roomscalemove), `sv_phys.cpp` (lastwatertime, vr_wpntouch, think2/nextthink2, handpos/rot, vrbits0, teleport_target, roomscalemove), `sv_main.cpp` (weapon2/flags/clips/holster* → client stats; model_scale*/model_offset → entity state; ammocounter/currentammo2/ammocounter2), `world.cpp` (handtouch, handpos, offhandpos, ishuman), `vr.cpp`/`view.cpp`/`vr_showfn.cpp` (hand fields, touchinghand, handtouch_hand/ent, headvel), `host_cmd.cpp` (ammocounter*, currentammo2), `pr_edict.cpp:148`, `cl_parse.cpp` (model_scale). Unused by engine: `vr_itemId` (QC-only). The ammo fields of mission packs in `host_cmd.cpp:2655-2866` (`give`) use `ED_FindFieldOffset` (QSS style).
- **Purpose:** hands/weapons/holsters networked and simulated engine-side.
- **Ironwail:** `progdefs.h` includes vanilla `progdefs.q1`; `PR_LoadProgs` pr_edict.c:2127 rejects any other CRC. IW code everywhere uses vanilla offsets.
- **Isolation idea:** two options. (A) Drop QVR's generated header into IW as-is and bump `PROGHEADER_CRC` → simplest but breaks loading any non-VR progs and touches every IW `ent->v.*`/`pr_global_struct->*` offset silently (vanilla fields shift!). (B, preferred) **Move VR things out of the system block in QC**: keep vanilla `defs.qc` prefix exactly (remove the inserted `ammocounter`, `lastwatertime`, `spawnServerFromSaveFile`, `parm17..40`, `v_viewangle`, VR fields, and the 3 func globals from before `end_sys_fields`), recompile → CRC 5927. Engine side: a `vr_fields.c` that at `PR_LoadProgs` caches offsets with `ED_FindFieldOffset` for each VR field (like IW `QCEXTFIELD`) and globals/functions via `ED_FindGlobal`/`ED_FindFunction`; access through `GetEdictFieldValue`/macros `VRF_VEC(ent, handpos)`. `think2` member pointers become offset-based. This also makes VR features auto-disable with vanilla progs.
- **Tag:** VR-CORE

### New engine→QC entry points and `spawnServerFromSaveFile`
- **Where (QVR):** `sv_main.cpp:4196-4204` (before `ED_LoadFromFile`), `4253-4260` (after serverinfo sent, followed by C++ `VR_OnSpawnServer()`), `host_cmd.cpp:1771-1772` (end of `Host_Loadgame`). `SpawnServerSrc` enum server.hpp:339; `SV_SpawnServer(const char*, SpawnServerSrc)` server.hpp:347.
- **Upstream (BASE):** none.
- **Change:** before each of the two spawn calls, sets `pr_global_struct->spawnServerFromSaveFile = (src == FromSaveFile)`; calls `PR_ExecuteProgram(pr_global_struct->OnSpawnServerBeforeLoad/AfterLoad)`; after loading a save calls `OnLoadGame`. Called unconditionally (a progs without them would hit `PR_ExecuteProgram(0)` error). QC uses them to rebuild cvar handles (`client.qc:137-165`) and to spawn ammo-box weapons when not from save.
- **Ironwail:** `SV_SpawnServer(const char *server)` has no source parameter; IW `Host_Loadgame_f` has no QC callback.
- **Isolation idea:** `VR_OnSpawnServerBeforeLoad(bool fromSave)` / `...AfterLoad` / `VR_OnLoadGame()` hooks that look the functions/global up by name (`ED_FindFunction`, `ED_FindGlobal`) and no-op if absent. Need a flag set by the loadgame path before calling `SV_SpawnServer`.
- **Tag:** VR-CORE

### 40 spawn parms (QSS extended parms, ungated)
- **Where (QVR):** `server.hpp:159-160` (`NUM_BASIC_SPAWN_PARMS 16`, `NUM_TOTAL_SPAWN_PARMS 64`), `pr_cmds.cpp:2263-2276`, `sv_main.cpp:2363-2371` (`SV_ConnectClient`/SetNewParms), `sv_main.cpp:4011-4020` (`SV_SaveSpawnparms`), `host_cmd.cpp:2275-2285`, save/load `host_cmd.cpp:1392-1395, 1463-1470, 1566-1580, 1700-1706, 1760-1763`.
- **Upstream (BASE):** same QSS mechanism, but `setspawnparms` and one host_cmd path were gated on `pr_checkextension`.
- **Change:** gate removed; QC `defs.qc:60-90` uses parm10..parm40 to carry weapon2, holster weapons/flags/clips, clip sizes, hipnotic/rogue items, extra ammo across levels. Parms 17+ found by `ED_FindGlobal(va("parm%i"))`. Saved in the QSS `/* ... */` extended savegame block as `spawnparm N "value"`.
- **Ironwail:** `NUM_SPAWN_PARMS 16` (quakedef.h:224); `client_t.spawn_parms[16]` (server.h:156); loops in pr_cmds.c:1702, sv_main.c:506/1699, host_cmd.c:2497/2606/2695/3089, pr_edict.c:2465/2550. No extended parms at all.
- **Isolation idea:** add `vr_spawn_parms[64-16]` to `client_t` (or grow array) and a helper `VR_CopyExtParms(to/from globals)` called next to each of those loops; add `spawnparm` lines to IW's savegame extension block.
- **Tag:** VR-CORE

### ED_ClearEdict initialises `nextthink2`
- **Where (QVR):** `pr_edict.cpp:148` (`ed->v.nextthink2 = -1;`). `pr_exec.cpp:611-612` OP_STATE deliberately does **not** touch think2 (commented TODO).
- **Change:** second think timer disabled by default for every new/cleared edict (physics side is in sv_phys, other inventory).
- **Ironwail:** `ED_ClearEdict` in pr_edict.c.
- **Isolation idea:** set via cached field offset in a `VR_OnEdictClear(ed)` hook (or in QC spawn()).
- **Tag:** VR-CORE

### `qcvm->gravityfieldoffset` cache
- **Where (QVR):** `qcvm.hpp` (`int gravityfieldoffset`), set `pr_edict.cpp:1465`, used `sv_phys.cpp:460`.
- **Change:** perf cache of `ED_FindFieldOffset("gravity")`; identical to `extfields.gravity`.
- **Ironwail:** has `qcvm->extfields.gravity` (QCEXTFIELDS_GAME). Use that.
- **Tag:** PERF

### PR_GetString is lenient on bad offsets
- **Where (QVR):** `pr_edict.cpp:1555-1578`.
- **Upstream (BASE):** `Host_Error("PR_GetString: invalid string offset")` for out-of-range positive/negative ids.
- **Change:** out-of-range → returns `qcvm->strings` (empty string) silently; only a freed known-string still errors.
- **Ironwail:** IW PR_GetString errors like BASE. Only matters if QVR QC relies on it (e.g. handle floats passed as strings); low risk, keep IW behaviour unless crashes appear.
- **Tag:** BUGFIX (defensive)

### QSS extension tweaks in pr_ext.cpp
- **Where (QVR):** `pr_ext.cpp:8386-8388` (vectoangles override removed, see #51); `pr_ext.cpp:4716` `serverkey("protocol")` returns `"quakevr"` for `PROTOCOL_QUAKEVR`; `pr_ext.cpp:5749-5772` `PF_cl_stringwidth` fixes inverted `usecolours` test (BASE counted markup when usecolours==0).
- **Ironwail:** IW has its own subset of extensions in pr_cmds.c (no `serverkey`, no 2-arg vectoangles swap; IW stringwidth separate). Nothing to port except `serverkey` if ever needed (QVR QC doesn't call it).
- **Tag:** MISC / BUGFIX

### Savegame / autosave (edict-facing parts)
- **Where (QVR):** `saveutil.cpp` (whole file), `host_cmd.cpp:1288-1831` (`Host_MakeSavegame`, `Host_Loadgame(filename, hasTimestamp)`, `Host_LoadAutosave_f`), triggers `host.cpp:1108` (`doAutomaticAutosave`), `host_cmd.cpp:2311` (`doChangelevelAutosave`), command `autosave` (menu.cpp:5087).
- **Change:** `SAVEGAME_VERSION 5` format unchanged except: autosave files `auto%d.sav` (12 slots, `MAX_AUTOSAVES`) start with an extra **first line** `strftime("%F %T", gmtime(now))`, parsed by `COM_ParseTimestampNewline` when `hasTimestamp`. Slot choice: first unused, else oldest timestamp. Cvars `vr_autosave_seconds`, `vr_autosave_on_changelevel`, `vr_autosave_show_message`. `MAX_SAVEGAMES` = 20. Loading calls `SV_SpawnServer(map, FromSaveFile)` then QC `OnLoadGame`. Edict read/write (`ED_Write`, `ED_ParseEdict`, `ED_WriteGlobals`) are unchanged — all VR fields are saved because they are ordinary QC fields.
- **Ironwail:** IW has its own save code (host_cmd.c) with `PR_FindSavegameFields` (pr_edict.c:1959) that **skips fields whose name ends in `_<char>`** — QVR QC field `hit_z` (`weapons.qc:2099`) would not be saved. IW's saves have no timestamp line.
- **Isolation idea:** autosave as a separate `vr_autosave.c` that calls IW's save function with a different filename + prepends timestamp (or store timestamp in a side file/mtime to keep IW's loader untouched). Hook the three QC callbacks as above.
- **Tag:** UI / VR-CORE

### World text server state (builtins #91–#96, #110)
- **Where (QVR):** `server.hpp:124-155`, `server.cpp` (`makeWorldTextHandle`, `SendMsg_WorldText*`), `sv_main.cpp:4155` (`sv.initializeWorldTexts()` on spawn), `host_cmd.cpp:2419` (resend all to a client on spawn), `worldtext.hpp` (`WorldTextHandle = uint16`, `HAlign` uint8), svc numbers protocol.hpp:397-402 (46-49, 35, 36).
- **Change:** server keeps a vector of `WorldText{_text,_pos,_angles,_hAlign,_scale}`; not saved in savegames — on load, spawn functions run during `SV_SpawnServer` and recreate texts in the same order so QC-stored handles (`self.weapon` in `func_worldtext_banner`) stay valid.
- **Ironwail:** nothing equivalent.
- **Isolation idea:** `vr_worldtext.c` (server list + svc + client renderer); builtins call into it. See protocol/render inventories.
- **Tag:** PROTOCOL / RENDER

## QSS extension features: what QVR relies on / removed, and IW status

| Feature | QVR | IW v0.8.2 |
|---|---|---|
| Extended spawn parms (17..64 via `ED_FindGlobal`) | **Relied on** (40 used), ungated | Absent (16) |
| `extfields` lookup (`gravity`, `alpha`, `items2`, `scale`, `colormod`, `button3`…) | Relied on for `gravity` (hip/rogue `defs`), `alpha` (honey_defs); `button3` is also a hard entvar | Present (`QCEXTFIELDS_*`, `PR_MergeEngineFieldDefs` adds alpha/scale/colormod/...) |
| Lazy extension builtins via `PF_Fixme` + `extensionbuiltins[]` | Kept but shadowed for #≤110; QC uses none | Different design (`pr_builtindefs` + `checkextension` tracking) |
| #51 → 2-arg `PF_ext_vectoangles` swap | **Removed** | Not present |
| `pr_checkextension` gating of setmodel auto-precache, precache warnings, extended parms | **Removed** (always on) | setmodel errors; no ext parms |
| Autocvars, `SV_ParseClientCommand`, `EndFrame`, CSQC | Present in engine, unused by QVR QC | Present (subset) |
| `STRINGTEMP_BUFFERS/LENGTH` 1024/1024 (used by `substr`) | same as QSS | same values |
| `MAX_STACK_DEPTH 1024`, `LOCALSTACK_SIZE 16384` | same as QSS | same |
| QSS VM opcodes / `ev_ext_integer` | unchanged | IW has its own executor; QVR QC is plain v6 FTEQCC output |
| `PR_PatchRereleaseBuiltins` (IW remaps functions named centerprint/bprint/sprint at #90/91/92) | n/a | Harmless: QVR's `centerprint/sprint` are QC wrappers, `bprint` is #23 |

## Dependencies on other subsystems
- **Protocol:** `svc_particle2` (45), `svc_worldtext_*` (46-49, 35, 36), `PROTOCOL_QUAKEVR`; `WriteVec3` relies on `sv.protocolflags` coord encoding.
- **Physics (sv_phys):** `think2/nextthink2`, `handtouch`, `vr_wpntouch`, `lastwatertime`, `gravity` field, `droptofloor` style corner traces (`quake::util::anyXYCorner`).
- **Server/client stats (sv_main, cl_parse):** weapon2/holster*/clip fields → stats; `model_scale*` → entity state.
- **Input (sv_user):** writes hand/head/muzzle fields and `button3` from VR clc.
- **VR module:** `VR_DoHaptic`, `VR_OnSpawnServer`, reads `handtouch_*`, `touchinghand`.
- **Cvars:** handle system in cvar.cpp; ~70 cvars named by QC `vr_cvars.qc` must exist (including `vr_*` gameplay cvars).
- **Host/save:** loadgame path, autosave, extended parms in save file.

## Open questions
- Keep QVR's custom CRC-52440 ABI (option A) or restructure `defs.qc` so VR fields sit after `end_sys_fields` (option B)? B requires QC changes to remove the two mid-struct insertions (`ammocounter`, `lastwatertime`) and `spawnServerFromSaveFile`; check that QC does not depend on their position.
- Should `haptic` become a networked message so it works for remote clients, or stay listen-server-only?
- `droptofloor` rewrite: should it apply to all progs or only VR progs (it changes vanilla item placement, e.g. items partially in walls now drop)?
- `random()` inclusive-1: set IW `sv_gameplayfix_random 0` when VR progs load, or leave IW default?
- `hit_z` is not saved by IW's `PR_FindSavegameFields` naming heuristic — rename in QC or relax the heuristic?
- `rotatevec` is broken and unused — drop or fix?
