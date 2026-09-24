# Everything else + build — Quake VR functional changes

## Summary
- This subsystem is mostly C→C++ churn. The functional content is concentrated in a few places:
  1. **Filesystem (common.cpp):** QVR rewrote pak loading. It loads `pak0..pak98` even with gaps in the numbering, adds a per-pak hook (`VR_OnLoadedPak`), and adds a "which `maps/start.bsp` wins" selector (`vr_activestartpaknameidx`). It dropped pk3/pak.lst/wildcard paks and QSS multi-gamedir support.
  2. **Startup / host:** VR cvars are registered early. quake.rc runs synchronously and is followed by `VR_ModAllModels`. The engine boots into `map vrstart`. VR mode disables the frame cap. There is also an autosave system and an `OnLoadGame` QC hook.
  3. **Cvar handles:** QC builtins #86/87/88/90.
  4. **Console:** 8 notify lines and `CANVAS_NOTIFY`.
  5. **Math helpers:** rotation-matrix helpers used by the VR hands.
  6. **Build:** C++17/20 with glm, GLEW, OpenVR 1.23.7, SDL 2.0.12, a fteqcc pre-build step, and a release layout of `pak10/11/12` + loose maps and textures + SteamVR binding JSONs.
- The sound subsystem has **no VR work**. The listener is still `r_origin`/`vpn`/`vright`/`vup`.
- Ironwail (IW) lacks VOIP, `set`/`seta`, extended spawn parms and the QSS extended-save block. IW is C11 with no GLEW (uses `SDL_GL_GetProcAddress`), and has `MAX_LIGHTSTYLES` 64 and `MAX_CVARS` 4096. QVR registers about 2,300 VR cvars (32 weapon slots × ~70 `vr_wofs_*`), which fits but is not small.
- **Biggest port risks:**
  - IW's `COM_AddGameDirectory` **stops at the first missing `pakN.pak`**, so QVR's `pak10/11/12` (with no `pak2..9`) would never load.
  - IW has no start.bsp selector.
  - IW's `Host_Startdemos_f`, frame limiter and save format all differ.
  - The shipped `config.cfg` holds the tuned weapon offsets. The engine defaults for them are stale.

### Coverage check (orphans)
I mapped every QVR `.cpp/.hpp` (excluding `.bak`) to an area:
- server/physics: `sv_*`, `server.*`, `serverdefines`, `world.*`, `areanode`
- QC VM: `pr_*`, `progs*`, `progdefs*`, `qcvm.*`, `edict`
- client/net/protocol: `cl_*`, `client.*`, `net_*`, `protocol`, `msg.*`, `sizebuf.*`, `wsaerror`
- rendering: `gl_*`, `r_*`, `glquake`, `render`, `refdef`, `entity`, `efrag`, `lerpdata`, `modeleffects`, `worldtext`, `shader.*`, `anorm*`, `bspfile`, `modelgen`, `spritegn`, `draw`, `qpic`, `qs_bmp`, `lodepng.*`, `stb_*`, `srcformat`
- view/HUD/menu/input: `view.*`, `chase`, `sbar.*`, `screen`, `menu*`, `mstate`, `keys.*`, `in_sdl`, `input`, `vid`
- VR core: `vr*.cpp/hpp`, `quakeglm*`, `util.*`, `openvr.hpp`

**Orphans handled here:**
- `host.cpp`, `host.hpp`, `host_cmd.cpp`, `main_sdl.cpp`, `quakeparms.hpp`
- `cmd.*`, `cmd_types.hpp`, `cvar.*`, `console.*`, `cd_*`, `cdaudio.hpp`
- `saveutil.*` (autosave)
- `quakedef.hpp`, `quakedef_macros.hpp`, `macros.hpp`, `variantutil.hpp`, `pch.hpp`
- `q_stdinc`, `q_ctype`, `arch_def`, `filenames.hpp`, `sys.hpp`, `platform.hpp`, `link.*`
- `history.txt` (a stray console history: `map q`, `impulse 100`), `quakespasm.pak`
- `json.hpp`: nlohmann 3.7.3, **not included anywhere**, dead.
- `msg.*` and `sizebuf.*` are also summarised here because they are pure-move files with small additions.

Pure moves with no behaviour change:
- `fshandle.cpp` (FS_* from common.cpp)
- `byteorder.cpp` (swap functions + `ByteOrder_Init` from `COM_Init`)
- `link.cpp` (ClearLink etc.)
- `developer.cpp`, `debugprint.hpp`, `stringcat.hpp`, `variantutil.hpp`, `macros.hpp`: helpers only (`debugPrint` is `OutputDebugStringA`)
- `crc.*`, `strlcat.cpp`, `strlcpy.cpp`, `strl_fn.hpp`, `cfgfile.*`, `image.*` (apart from one overload), `fs_zip.cpp` (dead code, see below), `pl_win.cpp`, `pl_linux.cpp`, `cd_null`, `cd_sdl` (SDL1 paths removed only)

## Changes

### Pak loading: gaps allowed in pak0..pak98, no pk3/pak.lst/wildcards
- **Where (QVR):** `common.cpp:COM_AddGameDirectory` (2169-2245).
- **Upstream (BASE):**
  - `COM_AddGameDirectory(dir)` read `pak.lst` if present.
  - It then loaded `pakN.pak` **and** `pakN.pk3` for N = 0.. **until the first N where neither exists**.
  - It then enumerated every other `*.pak`/`*.pk3` in the directory unless `-nowildpaks` was given (`COM_ListSystemFiles` + `COM_AddEnumeratedPackage`).
  - It added `quakespasm.pak/.pk3` after pak0 (skipped with `-fitz`).
  - It appended the dir to `com_gamenames`, set the rogue/hipnotic flags from the dir name, and linked the loose directory **last**, i.e. at highest priority.
- **Change:**
  - Signature is now `COM_AddGameDirectory(base, dir)`.
  - The loose directory searchpath is pushed first. Then, for `i = 0..98`, `pak%i.pak` is loaded with `COM_LoadPackFile`; a missing file is skipped, not a stop.
  - Each loaded pak is pushed in front, so **paks override loose files in the same gamedir** and higher N overrides lower N. After each pak, `VR_OnLoadedPak(*pak)` is called.
  - At `i == 0 && path_id == 1`, `quakespasm.pak` from basedir (or userdir on the second pass) is pushed right after pak0.
  - No pk3, no `pak.lst`, no wildcard paks, no `-fitz`/`fitzmode` (removed from `COM_Init`), no `com_gamenames` update.
  - The mission-pack flags are set only in `COM_Game_f` and `COM_InitFilesystem`.
  - As a result, `COM_AddPackage`, `COM_AddEnumeratedPackage`, `COM_ListSystemFiles` and `FSZIP_LoadArchive` (fs_zip.cpp) are dead code.
- **Purpose:** The README install flow depends on it:
  - The user copies id `PAK0/PAK1`.
  - Mission packs are renamed to `PAK2.PAK` (hipnotic) and `PAK3.PAK` (rogue) **inside id1**.
  - QVR ships `PAK10/11/12`.
  - Custom map paks get "any number between 3 and 9".
- **Ironwail:**
  - `common.c:COM_AddGameDirectory` (2496-2560) uses the same "dir first, then paks prepended" order, so paks override loose files there too.
  - It adds `ironwail.pak` after pak0 via `COM_AddEnginePak` (2449), which searches exedir, then basedir, then all basedirs.
  - It **breaks at the first missing pak** (`if (!pak) break;`), so `pak10.pak` is unreachable without `pak2..pak9`.
  - IW supports multiple basedirs (Steam/GOG rerelease auto-detect, `COM_AddBaseDir`), which could make copying id paks unnecessary.
- **Isolation idea:** Replace the IW `break` with `continue` up to 99 (gated by a `-vrpaks` flag or always on). Add a `VR_OnLoadedPak(pak)` call after each pak push. Better long term: ship QVR data as a gamedir (`-game quakevr`) with contiguous pak numbering, and mission packs as their own gamedirs.
- **Tag:** GAMEPLAY

### `maps/start.bsp` selector across paks
- **Where (QVR):**
  - `common.cpp:COM_FindFile` (1436-1443): the check.
  - `vr.cpp` 4675-4722: `VR_GetActiveStartPakName`, `VR_GetLoadedPakNames`, `VR_GetLoadedPakNamesWithStartMaps`, `VR_ExtractPakName`, `VR_OnLoadedPak`.
  - Cvar `vr_activestartpaknameidx` (vr_cvars.cpp:171, archived, default 0).
  - Menu entry "Start map from:" (menu.cpp:470).
- **Upstream (BASE):** The first pak in search order containing the file wins.
- **Change:**
  - `VR_OnLoadedPak` records the pak basename, e.g. `"pak2"` (the path minus directory and the last 4 characters). It prints `Added pakfile to search paths: '%s'`. If the pak has `maps/start.bsp`, it also appends the name to the "with start maps" list.
  - In `COM_FindFile`, a pak match on exactly `"maps/start.bsp"` is **skipped** unless the pak name equals `list[vr_activestartpaknameidx % list.size()]` or is `"pak0"`.
  - This picks the id, hipnotic or rogue start map when all live in id1.
  - The lists are never cleared on `game` change, and `idx % size` divides by zero if no pak has start.bsp.
- **Purpose:** Mission packs merged into id1 overwrite id's start.bsp. This lets the user pick.
- **Ironwail:** Nothing equivalent. Missions are separate gamedirs (`game hipnotic`), which removes the need if the port keeps mission packs in their own dirs.
- **Isolation idea:** If the id1-merge layout is kept, add a `VR_FS_FilterPakFile(pak, filename)` hook inside IW's `COM_FindFile` pack loop, with lists kept in the VR module. Otherwise drop the feature and use IW's `game` switching.
- **Tag:** GAMEPLAY

### `game` command reverted to the older single-gamedir form, with VR hooks
- **Where (QVR):** `common.cpp:COM_Game_f` (2253-2428) and `COM_InitFilesystem` (2436-2530).
- **Upstream (BASE):**
  - QSS multi-gamedir: `game a b -c` builds `"id1;..."`, uses `COM_ResetGameDirectories`, and registers a `gamedir` alias.
  - Refuses to switch when shareware.
  - Supports multiple `-game` args (`COM_CheckParmNext`).
- **Change:**
  - Old QS syntax: `game <dir> [-hipnotic|-rogue|-quoth]`.
  - The `gamedir` alias is removed, as are `COM_ResetGameDirectories` and multiple `-game`.
  - The shareware check in the command is gone.
  - On switch the order is: `CL_Disconnect` → `Host_ShutdownServer` → **`VR_InitGame()`** (2326; re-inits the weapon cvars for the new gamedir, e.g. Arcane Dimensions table when dir is `ad`) → write config → free searchpaths → add dirs → flush and reset models/textures → `exec quake.rc` → **`Cbuf_Execute()`** (synchronous, 2412-2414) → `vid_unlock`.
  - Then, if `vr_enabled`, `map vrstart` + **`VR_ModAllModels()`** (2418-2422).
  - `com_gamenames` is never filled, so `COM_GetGameNames` / `COM_GameDirMatches` (used for server/client gamedir matching) always see only id1.
- **Purpose:** Per-mod weapon offset tables. Re-apply model mods after the config loads.
- **Ironwail:** `common.c:COM_Game_f` (~2680-2760) is multi-gamedir, uses `COM_ResetGameDirectories` (2565), and also has q64/mg3 flags.
- **Isolation idea:** Put `VR_OnGameChanged()` after the config exec in IW's `COM_Game_f`. It would do: `VR_InitGame`, `Cbuf_Execute`, `VR_ModAllModels`, `map vrstart`.
- **Tag:** VR-CORE

### Shareware and loose-file restrictions removed; downloads silently disabled
- **Where (QVR):**
  - `common.cpp:COM_FindFile`, loose-file branch (~1500).
  - `common.cpp:85`, where `allow_download` is defined but **never registered** (the `Cvar_RegisterVariable(&allow_download)` was dropped from `COM_InitFilesystem`).
- **Upstream (BASE):**
  - Unregistered games could not read loose files in subdirectories.
  - `allow_download` was registered with default `1`.
- **Change:**
  - The registered-version gate for loose files is gone.
  - `allow_download.value` stays `0` (the string is `"1"` but the cvar is never parsed), so `COM_DownloadNameOkay` always returns false. Client and server downloads are off and cannot be enabled.
- **Ironwail:** keeps the subdir gate (`common.c:2047`). It has its own download handling.
- **Isolation idea:** Nothing to port (registered pak1 is required anyway). Note it only.
- **Tag:** REMOVED / MISC

### New `COM_ParseTimestampNewline`
- **Where (QVR):** `common.cpp:1792`.
- **Change:** It consumes a `"%d-%d-%d %d:%d:%d\n"` line and returns the pointer after it, or null. It is used only by autosave loading (`host_cmd.cpp:1547`).
- **Ironwail:** n/a.
- **Isolation idea:** Only needed if the autosave timestamp line is kept.
- **Tag:** MISC

### Startup: early VR cvars, synchronous quake.rc, model patching
- **Where (QVR):** `host.cpp:Host_Init`:
  - 1232: `VR_InitCvars()`, right after `Cvar_Init`.
  - 1288-1307: `exec quake.rc`, then `cl_warncmd 1`, then `Cbuf_Execute()`, then `if(vr_enabled.value == 1) VR_ModAllModels();`, then `vid_unlock`.
- **Upstream (BASE):** No VR. quake.rc was only inserted into the buffer and ran on the first frame.
- **Change:**
  - `VR_InitCvars` (vr.cpp:1100-1111) sets callbacks for `vr_enabled`/`vr_deadzone`, **forces `vr_enabled 1`**, registers all VR cvars and runs `InitAllWeaponCVars`.
  - config.cfg and autoexec run inside `Host_Init`, including quake.rc's `startdemos`.
- **Ironwail:** `host.c:1399` `Cvar_Init`. quake.rc is inserted at about host.c:1460.
- **Isolation idea:** Call `VR_InitCvars()` after `Cvar_Init`. After the quake.rc insert, call `Cbuf_Execute(); VR_PostConfigInit();`. An alternative is to apply model mods on model load or cvar change, which removes the ordering dependency.
- **Tag:** VR-CORE

### Boot into `vrstart` instead of the menu or demos
- **Where (QVR):** `host_cmd.cpp:Host_Startdemos_f` (3102-3127).
- **Upstream (BASE):** If not `fitzmode`, `demonum=-1` and `menu_main`; otherwise `CL_NextDemo`.
- **Change:**
  - With `vr_enabled`, it queues `maxplayers 1; deathmatch 0; coop 0; map vrstart; centerview`.
  - The `menu_main` branch is commented out, so `CL_NextDemo()` always runs. With VR on, the demo is replaced by the map. With VR off, the demo loop plays.
  - Needs loose `id1/maps/vrstart.bsp`.
- **Ironwail:** `host_cmd.c:3657-3690` always shows `menu_main`, and plays demos only with `cl_startdemos`.
- **Isolation idea:** At the top of the demo branch: `if (vr_enabled.value) { queue the 5 cmds; cls.demonum = -1; return; }`.
- **Tag:** VR-CORE / UI

### VR shutdown
- **Where (QVR):** `host.cpp:Host_Shutdown:1370`, `VID_VR_Shutdown()` between `IN_Shutdown` and `VID_Shutdown`.
- **Ironwail:** `host.c:1524`.
- **Isolation idea:** One-line hook.
- **Tag:** VR-CORE

### Frame cap bypass in VR
- **Where (QVR):** `host.cpp:Host_FilterTime` (794-797). `&& !vr_enabled.value` is added to the `host_maxfps` early return.
- **Change:** No fps throttle in VR; the headset runtime paces frames. The 0.0001-0.1 frametime clamp remains.
- **Ironwail:**
  - Throttling lives in `main_sdl.c` via `Host_GetFrameInterval()` (host.c:780-801).
  - Both engines still sleep 16/32 ms when unfocused or minimized (QVR main_sdl.cpp:190-202; IW main_sdl.c:188-200). That would kill headset framerate.
- **Isolation idea:** In `Host_GetFrameInterval`, `if (vr_enabled.value) return 0;`. Also skip the unfocused sleep in VR.
- **Tag:** VR-CORE / PERF

### Autosave system (timer, changelevel, slots, timestamp line, `load_autosave`)
- **Where (QVR):**
  - `saveutil.cpp` (whole file; `MAX_AUTOSAVES` 12, `MAX_SAVEGAMES` 20)
  - `host.cpp:_Host_Frame` 1100-1110: `if(!deathmatch.value) quake::saveutil::doAutomaticAutosave();` after `CL_ReadFromServer`
  - `host_cmd.cpp:Host_Spawn_f` 2303-2313: `doChangelevelAutosave()` when `!deathmatch && !coop`, non-loadgame branch
  - `host_cmd.cpp:1322-1489`: `Host_MakeSavegame(filename, const time_t* ts, bool printMessage)`
  - `host_cmd.cpp:1808-1831`: `load_autosave` (registered at 3384)
  - Cvars (vr_cvars.cpp:175-185): `vr_autosave_seconds` 240, `vr_autosave_on_changelevel` 1, `vr_autosave_show_message` 0 (all archived)
- **Change:**
  - The timed autosave writes `auto%d` into the slot with the oldest timestamp.
  - An autosave file starts with a `strftime("%F %T", gmtime)` line, then the normal QSS save. Manual saves are unchanged.
  - The failure messages ("Not playing a local game." etc.) are not gated by `printMessage`, so they print on each timer expiry in menus and demos.
  - The engine writes **no VR state**; VR state persists only via QC globals and fields.
- **Ironwail:** has its own heuristic autosave: `sv_autosave`/`sv_autosave_interval`, `Host_CheckAutosave` at host.c:852-936, writing `autosave/<map>`. Saving runs on a background thread.
- **Isolation idea:** Use IW autosave and drop the QVR timestamp format. If the menu needs slots, keep a small `vr_autosave.c` that uses IW's `save name 0` and file mtimes.
- **Tag:** GAMEPLAY / UI

### `OnLoadGame` QC hook and `SV_SpawnServer` source argument
- **Where (QVR):**
  - `host_cmd.cpp:Host_Loadgame` 1508-1776. `PR_ExecuteProgram(pr_global_struct->OnLoadGame)` runs at 1771 after `CL_EstablishConnection` and the reconnect, with `sv.qcvm` still active and `self` unset.
  - `SV_SpawnServer(name, SpawnServerSrc::{FromMapCmd, FromChangelevelCmd, FromRestart, FromSaveFile})` is called at 1041/1147/1195/1600.
  - `OnLoadGame` is a required `globalvars_t` field (progdefs_generated.hpp:98), so it changes the progs CRC.
- **Ironwail:** No hook. `SV_SpawnServer(name)` takes one argument.
- **Isolation idea:** Look up `OnLoadGame` by name (`ED_FindFunction`) instead of a progdefs slot. Set a `vr_spawnsrc` global before `SV_SpawnServer`.
- **Tag:** GAMEPLAY

### QVR regression: QSS extended-savegame block parser broken
- **Where (QVR):** `host_cmd.cpp:1645` (`end = "\0";`) and `1707` (`end = "\n"; ext = end+1;`).
- **Change:** Const-churn broke the in-place line termination. Only the first line of the `/* // QuakeSpasm extended savegame */` block is applied (late precaches, lightstyles >63, parm17+).
- **Ironwail:** Has no such block. Moot.
- **Tag:** BUGFIX (note only)

### World texts resent on spawn
- **Where (QVR):** `host_cmd.cpp:Host_Spawn_f` 2415-2429. For each `sv.worldTexts[i]`, it sends `SendMsg_WorldTextHMake/HSetText/HSetPos/HSetAngles/HSetHAlign/HSetScale`.
- **Ironwail:** n/a.
- **Isolation idea:** `VR_SendWorldTexts(client)` hook. The protocol is in the client/net inventory.
- **Tag:** PROTOCOL

### Extended spawn parms always copied
- **Where (QVR):** `host_cmd.cpp:Host_Spawn_f` 2280-2288.
- **Upstream (BASE):** `parm17+` was copied only if `pr_checkextension`.
- **Ironwail:** Has only 16 spawn parms.
- **Isolation idea:** Check whether the VR QC uses `parm17+`.
- **Tag:** GAMEPLAY

### `give` writes ammo counters by ammo ID
- **Where (QVR):** `host_cmd.cpp:Host_Give_f` 2812-2879.
- **Change:**
  - `currentammo`/`currentammo2` now hold `AID_*` IDs (quakedef_macros.hpp:276-283: NONE 0, SHELLS 1, NAILS 2, ROCKETS 3, CELLS 4, LAVA_NAILS 5, MULTI_ROCKETS 6, PLASMA 7).
  - The ID maps to a count, which is written to the fields `ammocounter`/`ammocounter2`.
  - The rogue ammo types come from `ammo_lava_nails`, `ammo_multi_rockets` and `ammo_plasma` via `ED_FindFieldOffset`, with no null check.
- **Ironwail:** `host_cmd.c:3476-3510` still has the old weapon switch.
- **Isolation idea:** `VR_Give_UpdateAmmoCounters(ent)`.
- **Tag:** GAMEPLAY

### Misc host changes
- **`maxclientslimit` minimum 4 → 16:** `host.cpp:371-375`. Tag MISC.
- **New `Host_Warn`:** `host.cpp:226-254`, a non-fatal error with the loading plaque cleared. Used by the gl_model missing-model fallback.
- **`Host_Error` before the first setjmp:** calls `std::terminate` (host.cpp:89, 218, 309, 935). BUGFIX.
- **Removed version details:** `Host_Version_f` no longer prints the git revision; `status` prints `VERSION`. REMOVED (trivial).
- **`Host_ClearMemory`:** value-initialises `sv`/`cl` (host.cpp:768) because `server_t` holds C++ containers. IW uses memset, so world-text storage must be cleared explicitly.
- **`main_sdl.cpp:98-114`:** sets `vr_working_directory` from `argv[0]` (Win32). Linux uses a **hard-coded developer path**. It is used for `actions.json` (vr.cpp:1458). It also prints `Quake VR 0.0.8` (line 161). IW should use `SDL_GetBasePath()`. VR-CORE.

### Cvar handles (QC builtins #86/87/88/90)
- **Where (QVR):** `cvar.cpp:37` (`std::vector<cvar_t*> cvar_handles`), 301 (reserve), 818-857 (`Cvar_MakeHandle`, `Cvar_GetValueFromHandle`, `Cvar_SetValueFromHandle`, `Cvar_ClearAllHandles`). The builtins are in pr_cmds.cpp:1145-1173 and 1440-1448, with the table at 2397-2403.
- **Change:**
  - `float cvar_hmake(string)=#86`: appends and returns the index, or -1 with a warning.
  - `float cvar_hget(float)=#87`: returns 0 for an invalid index.
  - `void cvar_hclear()=#88`
  - `void cvar_hset(float,float)=#90`
  - The handle is a raw **int stored in a float slot** (`G_INT`). QC `VR_CVars_InitAllHandles` (vr_cvars.qc:79) calls clear then make.
- **Purpose:** Fast QC access to the many `vr_*` cvars.
- **Ironwail:** None. Check IW builtin slots 86-90 for conflicts.
- **Isolation idea:** `vr_cvarhandle.c` with a fixed `cvar_t*` array and 4 PF_ functions. Keep the int-in-float encoding. Handles are not persistent across loads, so QC must re-init them.
- **Tag:** GAMEPLAY

### Console: 8 notify lines, `CANVAS_NOTIFY`, `^` markup always on
- **Where (QVR):**
  - `console.cpp:84` `NUM_CON_TIMES 8` (was 4)
  - `console.cpp:1432` `GL_SetCanvas(CANVAS_NOTIFY)`; the new enum is in screen.hpp:68
  - gl_draw.cpp:946-960: `CANVAS_CONSOLE` is now a fixed 800x600 × `scr_menuscale`; `CANVAS_NOTIFY` takes the old conwidth ortho
  - gl_draw.cpp:932-935: `GL_SetCanvas` returns early in VR unless `con_forcedup`
  - `console.cpp:505`: `^` colour codes are parsed even without `pr_checkextension`
- **Purpose:** The console and menu are drawn on a fixed-size VR quad.
- **Ironwail:** 4 notify lines with a fade, a batched canvas system, and no `^` markup.
- **Isolation idea:** Mostly the rendering/HUD inventory. Change the define or add a cvar, and add a VR branch to IW `GL_SetCanvas`.
- **Tag:** UI / RENDER

### Developer-level changes
- **Where (QVR):** `developer.cpp:6-13` (`quake::vr::developerMode()` returns 1 in `!NDEBUG` builds). It is used in cmd.cpp:1017 and console.cpp:772-842.
- **Change:**
  - `Con_DWarning` threshold drops from ≥2 to ≥1.
  - New `Con_DPrintf3` (≥3; console.cpp:851-868), used for "FindFile: can't find".
- **Ironwail:** `Con_DWarning` is already ≥1.
- **Isolation idea:** Optional.
- **Tag:** MISC

### Removed `cmd pext` / `cmd protocols` replies
- **Where (QVR):** `cmd.cpp:Cmd_ForwardToServer` (1034-1062). `cl_nopext` is still registered (cmd.cpp:39/680) but unused.
- **Upstream (BASE):** Replied to server-stuffed `cmd protocols` / `cmd pext` with protocol and PEXT2 capabilities.
- **Change:** The QVR client never negotiates FTE/QSS extensions this way and relies on its fixed QUAKEVR protocol.
- **Ironwail:** Also has none.
- **Tag:** REMOVED / PROTOCOL

### Minor cmd/cvar behaviour
- **`exec` newline:** `Cmd_Exec_f` inserts an extra `"\n"` (cmd.cpp:339). No effect.
- **`set`/`seta` (unchanged QSS):** `cvar.cpp:127-150` plus `CVAR_SETA` (cvar.hpp:86). IW has neither. The shipped config and QC don't use them; user configs might. Port it if needed (about 30 lines).
- **CSQC console commands:** `Cmd_ExecuteString` (cmd.cpp:990-1012) routes to `CSQC_ConsoleCommand`. IW does not.
- **Tag:** MISC

### MSG/SZ helpers
- **Where (QVR):** `msg.cpp`: `MSG_WriteUnsignedChar` :28, `MSG_WriteUnsignedShort` :69, `MSG_WriteVec3` :191, `MSG_ReadUnsignedChar` :225, `MSG_ReadUnsignedShort` :269, `MSG_ReadVec3` :410.
- **Change:**
  - `MSG_WriteVec3`/`MSG_ReadVec3` are 3× coord.
  - The C port must read x, y, z in separate statements (argument evaluation order).
  - `sizebuf.cpp:29-32`: `SZ_Clear` no longer resets `overflowed`. This reverts a QSS change; IW behaves the same.
- **Ironwail:** Only the classic set exists; the wire formats are identical.
- **Isolation idea:** Static-inline helpers in a `vr_msg.h`.
- **Tag:** PROTOCOL (helpers)

### Engine limits and constants (`quakedef_macros.hpp`)
- **Where (QVR):** `quakedef_macros.hpp`:
  - 69: `MINIMUM_MEMORY` 1'048'576 (BASE 0x550000; the comment says 16 MB, which is wrong)
  - 83: `MAX_QPATH` 128 (BASE 64)
  - 333: `MAX_BEAMS` 128 (BASE 32)
  - 337: `MAX_TEMP_ENTITIES` 512 (BASE 256)
  - 134-196: `STAT_*` renumbered and extended (`STAT_WEAPON2` 15, `STAT_ITEMS` 43, `STAT_VIEWHEIGHT` 44, `STAT_IDEALPITCH` 52, `STAT_PUNCH*` 53-58, holster / clip / flags / WID stats up to 68)
  - `AID_*`, `WID_*` (0-12), `QVR_HS_*` (0-9) and `QVR_VRBITS0_*` bits
  - `CACHE_SIZE` removed
- **Ironwail:** `MAX_QPATH` 64, `MAX_BEAMS` 32, `MAX_TEMP_ENTITIES` 256, `MAX_LIGHTSTYLES` 64, vanilla STAT numbering.
- **Isolation idea:**
  - `MAX_QPATH` 128 affects model/sound name buffers and savegames. Check whether the VR QC uses names over 63 characters; if not, keep 64.
  - Raise `MAX_BEAMS`/`MAX_TEMP_ENTITIES` if QVR's tent code (grapple beams, lightning) needs them.
  - The STAT layout belongs to the protocol inventory.
- **Tag:** PROTOCOL / MISC

### Zone / wad / sys / image / mathlib details
- **zone.cpp:248:** the `Z_CheckHeap()` call in `Z_Malloc` is now `#ifdef PARANOID`. It was unconditional in BASE. Debug x64 defines `PARANOID`. `zone.hpp:118` adds `Hunk_AllocNameAndConstruct<T>`; its placement-new offset `(char*)ptr + count` is buggy, but it is unused. **PERF**.
- **wad.cpp:112/123/136:** the QSS tolerant wad loader (clamp bad lumps, warn) is reverted to `Sys_Error` on a bad id, header or lump range. IW is tolerant. **REMOVED**; don't port.
- **sys_sdl_win.cpp / sys_sdl_unix.cpp:**
  - `Sys_Error` prints a stack trace first. The working copy (uncommitted) switched from `boost::stacktrace` to `std::stacktrace`.
  - The Win32 dedicated `AllocConsole` is disabled (`if(false && ...)`, line 373). x64 builds use the Console subsystem.
  - `GetLastErrorAsString` was added.
  - **MISC**.
- **image.cpp:863:** new overload `Image_LoadImage(name, &w, &h)`, used by the r_part particle textures.
- **mathlib.cpp:**
  - New `VectorAngles(fwd)` (284): pitch/yaw, roll 0.
  - `VectorAngles(fwd, up)` (300): `up` is now mandatory (BASE `up == NULL` meant yaw/roll 0 at the poles).
  - New `RotMatFromAngleVector` (361): rows are fwd, -right, up.
  - New `AngleVectorFromRotMat` (381): pitch/yaw from row 0, roll from row 1 vs the unrolled matrix.
  - New `CreateRotMat(axis, angle)` (402).
  - `safeNormalize` and `AngleVectorsOnlyFwd` added (mathlib.hpp:88).
  - `VectorNormalizeFast`, `FloorDivMod`, `Invert24To16`, `GreatestCommonDivisor` and `Q_log2` removed.
  - Used by vr.cpp:3154-3163 (hand rotation offsets).
  - IW: `VectorAngles(fwd, angles)` only, and has 4x4 matrix helpers. Port the three rotation helpers into the VR module in C.
  - **VR-CORE**.

### Sound: no VR changes; a few platform and VOIP edits
- **Linux audio disabled:** `snd_sdl.cpp:SNDDMA_Init` 94-101. The `SDL_InitSubSystem(SDL_INIT_AUDIO)` check is `#ifdef WIN32`; elsewhere the failure path always runs ("TODO VR: hangs on linux"). Don't port. BUGFIX (a regression).
- **VOIP negotiation gates removed:** `snd_voip.cpp` 2295-2307, 2884-2896, 3044-3052; `cl_parse.cpp:3378`. `svcfte_voicechat` / `clcfte_voicechat` (83) are always sent and accepted, regardless of `PEXT2_VOICECHAT`. `USE_SDL_CAPTURE` is forced (192). IW has **no VOIP at all**. Recommend dropping it, and the protocol inventory decides whether opcode 83 stays reserved. PROTOCOL.
- **`snd_modplug.cpp`:** re-added (`mLoopCount=0`, master-volume call disabled) but **never registered and never enabled**. Dead.
- **Other:**
  - The `SND_Spatialize` VOIP branch (`snd_dma.cpp:480`) is unchanged, with a TODO.
  - The precache stubs were removed.
  - The listener comes from `r_origin`/`vpn`/`vright`/`vup` (host.cpp:1131), so the view code must fill these from the HMD.
  - IW uses fixed `MAX_CHANNELS` 1024 and adds underwater FX.

## Build

### CMake (`C:\OHWorkspace\quakevr\CMakeLists.txt`)
- **Project:** `quakevr` 0.0.8, `LANGUAGES CXX`, C++17 (`cxx_std_17`), PCH `Quake/pch.hpp` (glm headers + `<GL/glew.h>` + windows.h). The target is `quakevr-debug` in Debug, else `quakevr`.
- **Sources (lines 7-104):** includes `snd_modplug`, `snd_xmp`, `snd_voip`, `fs_zip`, `vr*`, `quakeglm*`, `util`, `saveutil`, `shader`, `gl_util`, `menu_keyboard`, `menu_util`. Win32 adds `net_win/wins/wipx`, `pl_win`, `sys_sdl_win`; otherwise `net_bsd/udp`, `pl_linux`, `sys_sdl_unix`.
- **Defines (150-170):**
  - `USE_SDL2`, `_AMD64_`, `PARANOID` (always, even Release), `_USE_WINSOCK2`, the `_CRT*`/winsock warning defines
  - `USE_CODEC_{MP3,VORBIS,WAVE,FLAC,OPUS,MIKMOD,UMX}` (no XMP, no MODPLUG)
  - `GLM_COMPILER=0`
- **Includes:** `Windows/SDL2/include`, `Windows/glew/include`, `Windows/codecs/include`, `glm/` (glm **0.9.9.8**).
- **`find_package(Boost 1.36 REQUIRED)`:** only used for `boost::stacktrace` in the committed `sys_sdl_*`. The working copy removed it.
- **Win32 libraries:**
  - Hard-coded absolute paths `C:/OHWorkspace/quakevr/Windows/SDL2/lib64/SDL2(main).lib` and `.../OpenVR/lib/win64/openvr_api.lib`; the link dir also points at `C:/OHWorkspace/openvr/lib/win64`.
  - Libraries: `openvr_api`, `libvorbisfile`, `libvorbis`, `libopusfile`, `libopus`, `libFLAC`, `libogg`, `libmad`, `libmikmod`, `ws2_32`, `opengl32`, `winmm`, `SDL2`, `SDL2main`, `glew32`.
- **Linux:**
  - `find_package(SDL2)` and `find_package(GLEW)`.
  - OpenVR comes from a hard-coded `/home/vittorioromeo/Repos/openvr/bin/linux64/libopenvr_api.so`.
  - Links `asan` unconditionally, plus `opus`, `FLAC`, `ogg`, `mad`, `mikmod`, `vorbis`, `vorbisfile`, `opusfile`, `mpg123`.

### Visual Studio (`Windows\VisualStudio\quakespasm-sdl2.vcxproj`)
- **Configurations:** Debug/Asan/Release × Win32 (v142) / x64 (**ClangCL**). Only x64 is maintained. The Win32 configs lack glew and glm and are stale.
- **x64 compile settings:**
  - `LanguageStandard` `stdcpplatest`; Asan uses stdcpp17. The working copy changed stdcpp20 → stdcpplatest and removed the Boost include paths `C:\boost_1_80_0` and `C:\OHWorkspace\boost_1_81_0`.
  - `CompileAsCpp`.
  - Includes `..\..\glm; ..\SDL2\include; ..\codecs\include; ..\misc\include; ..\..\Quake; ..\glew\include`.
- **x64 defines:** `WIN32;_DEBUG|NDEBUG;_WINDOWS;_USE_WINSOCK2;_CRT_NONSTDC_NO_DEPRECATE;_CRT_SECURE_NO_WARNINGS;_WINSOCK_DEPRECATED_NO_WARNINGS;USE_SDL2;USE_CODEC_MP3;USE_CODEC_VORBIS;USE_CODEC_WAVE;USE_CODEC_FLAC;USE_CODEC_OPUS;USE_CODEC_MIKMOD;USE_CODEC_UMX;PARANOID(Debug/Asan);__clang__`.
- **Release x64:** `-flto=thin /Zc:threadSafeInit-`, fast floating point, no RTTI, no buffer checks, no CFG.
- **Link:** `openvr_api.lib; libvorbisfile; libvorbis; libopusfile; libopus; libFLAC; libogg; libmad; libmikmod; ws2_32; opengl32; winmm; SDL2; SDL2main; glew32`. Library dirs are `..\OpenVR\lib\win64; ..\codecs\x64; ..\SDL2\lib64; ..\glew\lib`. **SubSystem Console**, 4 MB stack reserve. Release x64 OutDir is `...\x64\Debug\` (shared with Debug).
- **PreBuildEvent (x64):** `cd $(SolutionDir)..\..\ && make_pak.bat`, so QC is compiled on every engine build.
- **PostBuildEvent:** copies `codecs\x64\*.dll`, `SDL2\lib64\*.dll`, `OpenVR\lib\win64\*.dll` and `glew\bin\Release\x64\*.dll` next to the exe.
- **Dependency versions:**
  - SDL **2.0.12**
  - OpenVR header `Quake/openvr.hpp` = SteamVR **1.23.7** (`IVRSystem_022`); a local copy of `openvr.h`
  - GLEW 2.x
  - glm 0.9.9.8
  - codecs: FLAC-8, mad-0, mikmod-3, mpg123-0, ogg-0, opus-0, opusfile-0, vorbis-0, vorbisfile-3, vorbisidec-1, xmp
- **Stale output directory:** `Windows\VisualStudio\Build-quakespasm-sdl2\x64\Debug` contains a ninja/CPM tree from an unrelated project (imgui, sfml, luajit, libsodium, boostpfr). It is not part of QVR. It also has helper scripts: `run.bat` (`quakevr-debug -novr +map vrfiringrange`), `compile.bat` (ericw-tools qbsp/vis/light of `map_src\vrfiringrange.map` → ReleaseFiles), `quakevr-dedicated.bat`, and `Id1\maps\ent patches.bat` (frikbot `-onlyents`).
- **Ironwail comparison:**
  - C11 (`stdc11`), no GLEW (GL entry points loaded with `SDL_GL_GetProcAddress`, gl_vidsdl.c:992), SDL **2.30.9**, curl, zlib.
  - The VS project defines `USE_CODEC_XMP` (not MIKMOD).
  - CMake detects codecs with pkg-config (MP3 via mpg123/mad, OPUS, FLAC, VORBIS/tremor, MIKMOD, MODPLUG, XMP, UMX).
  - To match QVR music support, enable at least MP3, VORBIS, OPUS, FLAC and UMX, plus MIKMOD or XMP.
  - The VR port must add OpenVR or OpenXR as a C-callable library; the OpenVR C API is `openvr_capi.h`.

### QC build step (`C:\OHWorkspace\quakevr\make_pak.bat`)
```
cd QC
.\fteqcc64.exe -O3 -Fautoproto -progdefs -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208
.\pak.exe -c -v pak11.pak vrprogs.dat
xcopy /y pak11.pak ..\ReleaseFiles\Id1
:: echo F | xcopy /y progdefs.h ..\Quake\progdefs_generated.hpp
```
- `QC/progs.src` outputs `vrprogs.dat`. It is a merged codebase: id + `hip_*` + `rogue_*` + `honey_*` + `frikbot/` + `vr_*.qc`.
- `-progdefs` emits `progdefs.h`, which was **manually** copied to `Quake/progdefs_generated.hpp` (the copy is commented out). The engine's `globalvars_t`/`entvars_t` therefore must match this QC exactly (CRC check).
- The engine hard-codes `PR_LoadProgs("vrprogs.dat", ...)` (sv_main.cpp:4090) instead of `progs.dat`. The QC agent should confirm whether any `sv_progs` cvar exists.
- `QC/pak2.pak` and `QC/pak3.pak` each contain only a `progs.dat` (older builds); leftovers. `fteqcc64-old.exe`, `frikqcc` and `QC_other/` are also leftovers.
- **Ironwail:** loads `progs.dat` (or `sv_progs`). The port needs the progs name `vrprogs.dat` (or a rename in the pak). It also needs QVR's field/global layout supported without a fixed CRC: IW accepts non-standard progs through `ED_FindField` lookups, but the required globals such as `OnLoadGame` and `OnSpawnServer*` must be looked up by name.

### Release layout (`C:\OHWorkspace\quakevr\ReleaseFiles`)
| File | Content | Engine-required? |
|---|---|---|
| `SDL2.dll`, `glew32.dll`, `libopus-0.dll` | runtime DLLs | build deps (other codec DLLs and `openvr_api.dll` come from the post-build copy; they are **missing** from ReleaseFiles) |
| `actions.json`, `bindings_{generic,vive,knuckles,touch,holographic,cosmos}.json` | SteamVR Input action manifest + default bindings, loaded from `vr_working_directory + "/actions.json"` (vr.cpp:1458) | **engine (VR input)** |
| `quakevr.exe -novr` / `quakevr-debug.exe -novr` (.bat) | desktop mode. `-novr` sets `vr_novrinit 1`; `-fakevr` also exists (vr.cpp:1425-1433) | launcher |
| `Id1/pak10.pak` | 15 `maps/b_*.bsp` ammo/health boxes; 72 `progs/*.mdl` (replacement monster, item and weapon models, incl. hipnotic/rogue `v_prox`, `v_lava*`, `v_multi*`, `v_plasma`, `g_shot0`); a stale `progs.dat` (380 KB, unused because the engine loads `vrprogs.dat`) | **mod data**. It overrides id models; the engine only needs them if QC precaches them |
| `Id1/pak11.pak` | `vrprogs.dat` only (built by make_pak.bat) | **engine-required** (hard-coded progs name) |
| `Id1/pak12.pak` | hand models (`hand.mdl`, `hand_base.mdl`, `finger_{index,middle,ring,pinky,thumb}.mdl`, `openhand.mdl`), `vrtorso.mdl`, `legholster.mdl`, `wpnbutton.mdl`, VR weapon view models (`v_*` incl. `v_hammer`, `v_laserg`, `v_grpple`, `hook.mdl`, `beam.mdl`), `proxbomb.mdl`, `mervup.mdl`, `lasrspik.mdl`, `s_bullet.spr`, sounds (`fisthit`, `gunclick`, `forcegrab`, `reload1`, `weapons/holster0/1`, `weapons/chain1`, `misc/*`, `hipweap/*`, `pendulum/*`) | **Mixed.** The engine loads these directly: `hand*`, `finger_*`, `vrtorso`, `legholster`, `wpnbutton`, and `proxbomb`/`mervup` (vr.cpp:1117-1118, `Mod_ForName(...,true)`, so they are fatal if missing). The weapon `v_*` models come from the `vr_wofs_id_*` cvars. The rest is QC/mod data |
| `Id1/maps/vrstart.bsp`, `vrtutorial.bsp`, `vrfiringrange.bsp` (+ `.pts`, `.log`) | loose VR hub, tutorial and firing-range maps | `vrstart` is **engine-required** (auto-loaded); the others are mod data |
| `Id1/textures/particle_{explosion,smoke,blood,blood_mist,lightning,spark,rock,gun_smoke}.tga` | particle atlas sources (r_part.cpp:730-737) | **engine-required** (rendering) |
| `Id1/config.cfg` (65 KB, 2442 lines) | the **author's personal config** (`_cl_name "vee"`, `host_maxfps 72`, `r_particle_mult`, VOIP cvars, 62 binds incl. `+grableft/right`, `+reloadleft/right`, `+flickreload*`, `+offhandattack`, `+voip`), plus **2,285 `vr_*` lines**, mostly `vr_wofs_*` for 32 weapon slots | **Effectively required.** These tuned weapon offsets differ from the engine defaults in `InitAllWeaponCVars` (e.g. slot 1 axe: config `x -0.6 y 1.1 z -0.8 scale 0.34` vs code `-4, 24, 37, 0.33`) |
- **`quakespasm.pak`:** byte-identical to BASE (conback, 6 `.ent` fixes, `default.cfg`), so there is no config-default change there. IW ships `ironwail.pak` (menu gfx + its own `default.cfg`) and also compiles in a `default_cfg` fallback.
- **Config name:** IW writes `ironwail.cfg`, but `exec config.cfg` falls back to `config.cfg` if `ironwail.cfg` is missing (cmd.c:300-312). The shipped `config.cfg` is therefore picked up on first run, and later saves go to `ironwail.cfg`.
- **User-supplied files:**
  - `id1/PAK0.PAK`, `PAK1.PAK`
  - optional `PAK2.PAK` (hipnotic) / `PAK3.PAK` (rogue), dropped into id1
  - optional `music/`
  - custom map paks numbered between 3 and 9, or extracted loose (loose files lose to paks in the same dir)

## Dependencies on other subsystems
- VR core: `VR_InitCvars`, `VR_InitGame`/`InitAllWeaponCVars` (the per-gamedir table, e.g. `ad`), `VR_ModAllModels`, `VID_VR_Shutdown`, `VR_OnLoadedPak`/start-pak lists, `vr_working_directory`, `-novr`/`-fakevr`, the rotation-matrix helpers.
- QC VM / progdefs: `vrprogs.dat` name; `OnLoadGame`, `OnSpawnServerBeforeLoad/AfterLoad`, `spawnServerFromSaveFile` globals; `ammocounter*`, `currentammo2` fields; cvar-handle builtins #86-#90.
- Server: `SV_SpawnServer(src)`, `sv.worldTexts`, extended spawn parms.
- Client/protocol: `STAT_*` layout, world-text messages, VOIP opcodes, `MSG_*Vec3` helpers, no pext handshake.
- Rendering/HUD: `CANVAS_NOTIFY`/`CANVAS_CONSOLE`, VR early return in `GL_SetCanvas`, particle textures, listener vectors from the HMD for sound.
- Menu: "Start map from" (`vr_activestartpaknameidx`), the autosave slot list and `load_autosave`, "Change Map" (`ExtraMaps_NewGame`).

## Open questions
1. Keep the "merge mission packs into id1 as pak2/pak3 + start.bsp selector" layout, or move to IW gamedirs (`-game quakevr`, `game hipnotic`)? This decides whether the pak-gap and start.bsp hooks are needed.
2. Are the engine weapon-offset defaults to be regenerated from the shipped `config.cfg`, so the port doesn't depend on a personal config?
3. Does the VR QC rely on `MAX_QPATH` > 64, extended spawn parms (`parm17+`), or late precaches (the QSS extended-save block, broken in QVR anyway)?
4. Is VOIP a requirement? IW has none.
5. Should IW's heuristic autosave replace QVR's timer and slots? The menu lists `auto%d` with timestamps.
6. IW's `MAX_CVARS` 4096 vs QVR's ~2,300 VR cvars plus the engine's: this fits, but should the per-weapon cvars move to a data file?
7. Is `pak10.pak/progs.dat` stale, and can it be removed? It is loaded as a CSQC fallback candidate (host.cpp:989-992) when `csprogs.dat` is absent and `pr_checkextension` is set; it would be rejected, but is wasted work.
