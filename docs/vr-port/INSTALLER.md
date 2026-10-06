# Quake VR installer: design and research

Research for an installer that makes Quake VR trivial to play: it finds the player's Quake, installs the mod
without touching Quake's own files, fetches the optional tools and assets, makes the shortcuts, and leaves a fresh
install that looks and plays like the author's own. **There is no installer code yet**; this is the list of what it
must do, with the facts from this repository it depends on, and a plan.

The request (Vittorio, 2026-10-06) was cut off after "Out-of-the box, the mod should have the same settings". He
confirmed it means **the same settings as his own setup**: a fresh install looks and plays like his tuned config,
except for what is personal to a body, a headset or a PC ([section 6](#6-out-of-the-box-settings)). His answers to the
other open questions are in [section 11](#11-decisions-vittorio-2026-10-06); the prerequisite fixes of
[section 12](#12-problems-found-while-researching) are done (branch `agent/releasefix`).

- [1. What the game needs today](#1-what-the-game-needs-today)
- [2. Detecting Quake and its expansions](#2-detecting-quake-and-its-expansions)
- [3. What may be shipped, borrowed or downloaded](#3-what-may-be-shipped-borrowed-or-downloaded)
- [4. Optional downloads](#4-optional-downloads)
- [5. Where it installs; updates, uninstall, integrity](#5-where-it-installs-updates-uninstall-integrity)
- [6. Out-of-the-box settings](#6-out-of-the-box-settings)
- [7. Shortcuts and launch variants](#7-shortcuts-and-launch-variants)
- [8. Other useful checks and features](#8-other-useful-checks-and-features)
- [9. Installer technology](#9-installer-technology)
- [10. Implementation plan](#10-implementation-plan)
- [11. Decisions (Vittorio, 2026-10-06)](#11-decisions-vittorio-2026-10-06)
- [12. Problems found while researching](#12-problems-found-while-researching)
- [Appendix A: the author's config against a fresh install](#appendix-a-the-authors-config-against-a-fresh-install)

Sources: `docs/INSTALL.md`, `docs/RELIGHTING.md`, `docs/vr-port/TEXTURES.md`, `docs/vr-port/CREDITS.md`,
`docs/vr-port/EXPANSIONS.md`, `Windows/package-quakevr.ps1`, `Quake/common.c` (`COM_InitBaseDir`), `Quake/steam.c`,
`Quake/sys_sdl_win.c`, `Quake/vr/vr_gamedir.cpp`, `Quake/vr/vr_cvars.cpp`, `Quake/vr/vr_setup.hpp`,
`Quake/vr/vr_mapinstall.cpp`, `Quake/vr/vr_backend_openxr.cpp`, `Misc/quakevr/relight_maps.py`. "Verify" marks a fact
this research could not confirm from the repository.

## 1. What the game needs today

### Files (the current `QuakeVR.zip`, made by `Windows/package-quakevr.ps1`)

The game folder is an **allowlist**: the files git tracks under `quakevr\` (less `.gitignore` and the motion
recorder's `motions\` test verdicts) plus the build's `progs.dat`. Untracked content never ships, whatever is in the
packager's folder (custom maps, mod folders, saves, configs, screenshots, notes, caches, test dumps).
`package-quakevr.ps1 -DryRun [-Root <checkout>]` prints the file list without building or copying.

| What | Files | Size | Notes |
|---|---|---:|---|
| Engine | `ironwail.exe`, `ironwail.pak` | 11 MB | `ironwail.pak` is looked up in the working directory first, then in each `-basedir` |
| Spatial audio | `phonon.dll` (Steam Audio 4.8.1) | 51 MB | loaded at run time by `vr_steamaudio.cpp` |
| OpenXR | `openxr_loader.dll` | 2.6 MB | Khronos loader |
| SDL, network, zip | `SDL2.dll`, `libcurl.dll`, `zlib1.dll` | 3 MB | miniz (zip) is compiled in |
| Audio codecs | `libFLAC-8`, `libogg-0`, `libvorbis-0`, `libvorbisfile-3`, `libopus-0`, `libopusfile-0`, `libmpg123-0`, `libxmp`, `libmad-0`, `libmikmod-3` `.dll` | ~2 MB | `ironwail.vcxproj` links vorbis, opus, FLAC, mpg123 and xmp; `libmad` and `libmikmod` look unused (verify) |
| Launcher | `QuakeVR.bat`: `start "" "%~dp0ironwail.exe" -game quakevr %*` | | the working directory is the caller's |
| Game folder | `quakevr\`: `progs.dat` (1.9 MB, built), `progs\` (66 MB of models and skins), `sound\` (4.5 MB), `maps\` (6 MB: hub, tutorial, firing range, calibration room, already relit), `textures\`, `textures_quetoo\` (47 MB, CC BY-SA 4.0), `gfx\`, `wads\`, `motions\`, `quake.rc`, `default.cfg`, `quakevr.cfg`, `vr_defaults.cfg`, `vr_bindings.cfg`, `retro_overrides_default.txt`, `bindlist.lst`, `checklist.txt` | 124 MB tracked | |
| Relight scripts | `quakevr\tools\`: `relight_maps.py`, `vis_maps.py`, `quakepak.py`, `relight_textures.cfg` | | Python 3.7+, standard library only |
| Symbols | `ironwail.pdb` (34 MB, full: clang-cl `/Z7` objects linked by lld-link `/DEBUG`) | | shipped beside the exe: `qvr_crash.txt` names the functions on the stack (DbgHelp looks in the exe's folder, then the working directory), and `qvr_crash.dmp` opens in a debugger with it |
| Build version | baked into the exe (`quakevr.props`, `QvrBuildVersion`: the last commit's date and short hash, `-dirty` with uncommitted changes) | | printed at start and by `version`, on the last line of VR Settings, and in `qvr_crash.txt`'s second line |

About 195 MB unpacked; the zip is "too large for a GitHub release" (`docs/INSTALL.md`).

### Run-time requirements

| Requirement | Detail (from `docs/INSTALL.md`, `README.md`) |
|---|---|
| OS | Windows 10/11 x64. VR only in the Windows x64 build. |
| GPU | OpenGL 4.3 (Ironwail). Shipped settings tuned on an RTX 4090; presets scale down. |
| VC++ runtime | Microsoft Visual C++ 2015-2022 Redistributable x64 **14.44 or later** (built with VS 2022 17.14, toolset 14.44.35207). Older than 14.40 crashes at start in `MSVCP140.dll` (the 17.10 `std::mutex` change). Permanent link: `https://aka.ms/vs/17/release/vc_redist.x64.exe`. |
| OpenXR runtime | Any: SteamVR, Virtual Desktop (VDXR), Meta Quest Link, WMR. `vr_xr_runtime`: 0 the system's active runtime (or `XR_RUNTIME_JSON`), 1 VDXR, 2 SteamVR, 3 `vr_xr_runtime_json`. VDXR and SteamVR are found under `HKLM\SOFTWARE\Khronos\OpenXR\1\AvailableRuntimes` by file name (`virtualdesktop-openxr.json`, `steamxr_win64.json`), else at `C:\Program Files\Virtual Desktop Streamer\OpenXR\` and `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\`. Tested mostly on Quest 3 through Virtual Desktop; Virtual Desktop's *Emulate Index controllers* must be off. |
| Controllers | Bindings for Touch/Touch Plus, Index, Vive wands, WMR, simple controller. |
| Quake data | `id1\PAK0.PAK` and `PAK1.PAK` (the full game; `COM_SetBaseDir` only checks `pak0`). |

### How the engine finds its folders (`Quake/common.c`, `COM_InitBaseDir`)

1. `-basedir <dir>` given: the first must hold `id1\pak0.pak`; further `-basedir`s are added (no check). **Every game
   folder is mounted from every base dir** (`VR_GameDirectoryRoot`), and the **last** base dir is the writable one
   (`com_gamedir` = last base + game: config, saves, screenshots, `relit\`, the Map Library's `cache\` and
   `qvr_addons\`).
2. Otherwise the **working directory** (not the exe's folder), then its parents.
3. Otherwise Steam (app 2310), GOG, then Epic: when both original and rerelease are found, a message box asks
   "Remastered / Original" (`ChooseQuakeFlavor`, or `-prefremaster` / `-preforiginal`), and the writable folder becomes
   `%USERPROFILE%\Saved Games\Ironwail\original` or `\rerelease`. `-steam`, `-gog`, `-egs` force one store;
   `-nosteam`, `-nogog`, `-noegs` skip one.

So a launch that does not rely on the working directory is
`ironwail.exe -basedir "<Quake>" -basedir "<Quake VR folder>" -game quakevr` (no trailing `\` on either path;
`docs/INSTALL.md` already documents this form for rerelease-only installs).

## 2. Detecting Quake and its expansions

### Stores

| Source | Where | Layout | Already in the engine |
|---|---|---|---|
| Steam | `HKCU\Software\Valve\Steam` `SteamPath` -> `config\libraryfolders.vdf` (all libraries) -> `steamapps\appmanifest_2310.acf` -> `steamapps\common\<installdir>` | Original data at the root (`id1`, `hipnotic`, `rogue`); the 2021 rerelease in `rerelease\` (`QuakeEX.kpf`, `id1\pak0.pak` ~220 MB, `hipnotic`, `rogue`, `dopa`, `mg1`, `mg3`, music in `rerelease\id1\music`) | `Steam_FindGame` / `Steam_ResolvePath` (`steam.c`); user files in `Saved Games\Nightdive Studios\Quake` |
| GOG, original | `HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\1435828198` `path` | `id1`, `hipnotic`, `rogue` | `Sys_GetGOGQuakeDir` |
| GOG, enhanced (rerelease) | `HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\1739637082` `path` | rerelease layout at the root | `Sys_GetGOGQuakeEnhancedDir` |
| Epic | `%ProgramData%\Epic\EpicGamesLauncher\Data\Manifests\*.item` (namespace `f57987ad...`, item `19e3c0be...`, app `18161d3e...`), `%ProgramData%\Epic\UnrealEngineLauncher\LauncherInstalled.dat` | rerelease only | `EGS_FindGame` (base dir); `ownedRoots` reads the same manifests (`addEpicRoots`: a Quake title whose `InstallLocation` holds `id1\pak0.pak` in `rerelease\` or at its root), so Epic's `dopa`/`mg1`/`mg3` are found like Steam's and GOG's (`-noepic` turns it off) |
| Microsoft Store / Xbox app (Game Pass) | probably `<drive>:\XboxGames\Quake\Content` (verify; older installs under `WindowsApps` are unreadable) | rerelease layout (verify) | no |
| Manual folder | folder picker | either layout | `-basedir` |
| Existing Quake VR zip install | `<Quake>\quakevr\` next to `id1` | | offer to adopt or migrate it ([section 5](#5-where-it-installs-updates-uninstall-integrity)) |

### What counts as present and ready

| Folder | Present | Ready for VR play | Engine check |
|---|---|---|---|
| `id1` (original) | `pak0.pak` + `pak1.pak` | yes | `COM_SetBaseDir`; installer should also check `pak1` and the known 1.06 sizes (PAK0 18,689,235 bytes, PAK1 34,257,856 bytes: verify) |
| `id1` (rerelease) | `pak0.pak` ~220 MB | yes (campaign, packs); relight works from it with `--quake <rerelease>` | |
| `hipnotic`, `rogue` | every file of `vr_pack_hipnotic.inc` / `vr_pack_rogue.inc` (101 / 132 entries) found intact in the packs of any base dir | yes (merged VR progs) | `inspectPack`: 1 available, 2 incomplete/corrupt (a pak or one of the pack's files is there, but not all of them), 0 missing (also a folder holding only an extracted texture pack's `textures\`); `vr_pack_status` |
| `dopa` | its resource list, from `ownedRoots()` (Steam `rerelease`, GOG enhanced, each base dir, its `rerelease` and `..\rerelease`) | **yes**, single player only (`coop 0`, `deathmatch 0`, `maxplayers 1`) | `discoverCampaigns`; also needs current language tables from the rerelease `id1` pak |
| `mg1`, `mg3` | as dopa | **no**: shown as "detected, not yet supported" (decision 9); selection refused ("native support in progress") | `nativeReady` false in `campaigns[]`; `vr_campaign_status` |

The installer only reports these (a checklist page) and passes the right `-basedir`s; the engine stays the authority
(`vr_pack_status`, `vr_campaign_status`, Official Campaigns page). It never copies a pack.

## 3. What may be shipped, borrowed or downloaded

Rule: **never redistribute id Software's (or Nightdive's, MachineGames') data or anything derived from it** (relit
maps, `.lit`/`.lux`, music, language tables, font glyphs cut from `gfx/*.lmp`). These stay on the player's PC, made
or read there.

| Item | How | Licence | Status |
|---|---|---|---|
| Engine, QuakeC, `progs.dat` | ship | GPL-2.0 (id, Ironwail, QuakeSpasm, FitzQuake) | ship source offer/link with binaries |
| Port's own models, sounds, maps (generated by `Misc/quakevr/make_*.py`) | ship | GPL-2.0 (project) | ok |
| Monster models in `quakevr/progs` (knight, hknight, ogre, soldier, enforcer...) | ship | appear to come from *Authentic Model Improvements*; licence and author "to confirm" (`CREDITS.md`) | **verify before a public installer** |
| Freesound/Kenney sounds | ship | CC0 | ok |
| `quakevr/textures_quetoo` | ship | CC BY-SA 4.0 (attribution, share-alike; `LICENSE.md` in the folder) | ok |
| Steam Audio `phonon.dll` | ship | Apache-2.0 + bundled third-party licences (`THIRDPARTY.md`) | include the notices |
| OpenXR loader, SDL2, libcurl, zlib, ogg/vorbis/opus/FLAC | ship | Apache-2.0, zlib, curl (MIT-style), zlib, BSD | include notices (verify the list) |
| mpg123, libxmp, libmad, libmikmod | ship (or drop if unused) | LGPL-2.1 / MIT? / GPL-2.0 / LGPL-2.0 (verify each) | verify; drop unused ones |
| VC++ Redistributable | bundle or download | Microsoft redistributable (VS licence allows `vc_redist.x64.exe`) | ok to bundle |
| `id1`, `hipnotic`, `rogue`, rerelease data, music, language tables | **borrow** (read in place) | id/Bethesda | never copy into a package |
| Relit maps (`quakevr\relit\`) | **make locally** | derived from id's maps | never distribute (`package-quakevr.ps1` already excludes `relit`) |
| Big menu font | **cut at run time** from the player's `pak0.pak` (`make_bigfont.py` ships only offsets and hashes) | | ok |
| ericw-tools 2.0.0-alpha11 | **bundle** `light.exe` (and what it needs) unmodified | GPL-3.0 | run as a separate process: mere aggregation, so GPL-3 does not reach our code (decision 7); ship its licence text and offer its source (mirror the tag's source zip next to ours) |
| VisPatch data (`id1_vis.tgz`...) | download | **unknown** (1997 data, SourceForge project) | verify before mirroring; download only |
| QRP HQ textures | download (author's PNG release) | no licence file; free, credited, removal on request (`TEXTURES.md`) | as today |
| Python (embeddable) for the relight | **not needed**: the relight becomes an in-game tool (decision 5) | PSF-2.0 | dropped |

## 4. Optional downloads

| Item | Source (from this repo's docs) | Size | Goes to | Needs |
|---|---|---:|---|---|
| HQ textures (QRP, PNG), **ticked by default** | `https://github.com/vittorioromeo/quakevr/releases/tag/textures-2026-10-03`, `quakevr-hq-textures-png-2026-10-03.zip` | ~0.6 GB unpacked (id1 360 MB, hipnotic 88 MB, rogue 121 MB, Quetoo copy 47 MB) | `id1\textures`, and `hipnotic\textures` / `rogue\textures` **only for packs the player owns** | nothing; install before relighting |
| QRP archive (fallback) | `https://www.moddb.com/addons/quake-revitalization-project-archive`, `QuakeRevitalizationProject.7z` | 1.26 GB | the same, after unpacking the `.pk3`s | 7-Zip; not for the installer (use the PNG release) |
| ericw-tools | `https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11`, `ericw-tools-2.0.0-alpha11-win64.zip` | 27.5 MB | `<QVR>\tools\ericw-tools\` | exact version (the light differs between versions) |
| VisPatch data | `https://sourceforge.net/projects/vispatch/files/vispatch%20data/1.0/`: `id1_vis.tgz` (949 KB), `hipnotic_vis.tgz` (807 KB), `rogue_vis.tgz` (787 KB); mirror: `https://www.quake-info-pool.net/vispatch/files.htm` | 2.5 MB | `<QVR>\tools\vispatch\` (`id1.vis`, `hipnotic.vis`, `rogue.vis`) | `tar` (in Windows 10+) |
| Relit maps | made **in the game** (the relight as an in-game tool, decision 5), with the bundled `light.exe` and the VisPatch data | ~210 MB, about a minute (73 maps) | `<QVR>\quakevr\relit\` | `light.exe`, VisPatch; textures first. **Ticked by default** |
| Music | read **in place**: the rerelease's `rerelease\id1\music` and the original Quake's music (the store copies' tracks) | | nothing copied (decision 8) | an owned copy |
| Normal/specular maps | already shipped (`textures_quetoo`, `vr_extmaps 1`, `vr_extmaps_dir textures_quetoo`) | | | nothing |
| Whisper transcription | `faster-whisper` via pip | large | | playtesters only: not in the installer |
| Community maps | in game: the Map Library (Quaddicted API) | per map | `<writable base>\cache\maps\`, `<writable base>\qvr_addons\` | nothing |

Every download: a pinned URL, a pinned SHA-256, a size shown before downloading, resume or retry, and an "I already
have this file" path picker (offline installs).

## 5. Where it installs; updates, uninstall, integrity

### Layout options

| | A. Side by side (today's zip) | B. Own folder, two `-basedir`s (recommended) |
|---|---|---|
| Where | `ironwail.exe`, DLLs, `QuakeVR.bat`, `quakevr\` added to the Quake folder | e.g. `%LOCALAPPDATA%\Programs\QuakeVR` or a folder the player picks; Quake untouched |
| Quake folder changed | adds files; the Map Library writes `cache\` and `qvr_addons\` at its root; textures go in `id1\textures` | **nothing written** |
| HQ textures | `<Quake>\id1\textures` ... | `<QVR>\id1\textures` (every base dir's game folders are mounted) |
| Rerelease-only installs | must not go into `rerelease\` (SDL2.dll clash) | same as any other |
| Store updates / "verify files" | may be disturbed by or disturb extra files (verify) | independent |
| Uninstall | pick files out of the Quake folder | delete one folder |
| Launch | working directory = Quake folder | `-basedir "<Quake>" -basedir "<QVR>" -game quakevr`, Start in = `<QVR>` |

**Decided: B by default** (decision 2); the zip stays for people who want A. It satisfies "never modify the user's
Quake files", survives store updates, and the engine already supports it. Two cautions:

- `<QVR>` must be **writable by the user** (config, saves, `relit`, Map Library): a per-user install under
  `%LOCALAPPDATA%\Programs`, or a chosen folder, never `Program Files` (unless the engine moves its writes to
  `Saved Games`, which Ironwail already does for store-found installs: a later option).
- Creating a pack folder in `<QVR>` (`hipnotic\textures`) for a pack the player does not own is safe now: a folder
  without any of the pack's data reads as "missing", not "incomplete" (fixed; still, install a pack's textures only
  when the player owns it).

### Install steps

1. Prerequisites: Windows x64; VC++ runtime >= 14.44 (registry `HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64`
   `Major/Minor/Bld`; install the bundled or downloaded redistributable if older: needs elevation once); OpenXR runtime
   present (`HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime`, `AvailableRuntimes`) - warn only; disk space.
2. Find Quake ([section 2](#2-detecting-quake-and-its-expansions)); the player confirms or picks. Original and
   rerelease both found: prefer original `id1` for the base (what the relight and the author use) and add the rerelease
   root for dopa/mg1/mg3 (verify that `ownedRoots` picks up `<Quake>\rerelease` from the first `-basedir`: it does by
   code, `%s/rerelease`).
3. Copy the payload to `<QVR>`; write `install.json` (version, Quake path(s), chosen options, file manifest).
4. Optional components (checkboxes, sizes shown; **HQ textures and the relight ticked by default**): HQ textures
   (owned packs only), relight (the bundled `light.exe` and the VisPatch data; the game's relight tool runs it with
   its progress shown, textures first). Music needs no step: the game reads it in place.
5. Shortcuts ([section 7](#7-shortcuts-and-launch-variants)).
6. Finish: "Put on your headset and start Quake VR": the first start runs VR Calibration ([section 6](#6-out-of-the-box-settings)).

### Updates

- The payload is replaced; **player files are kept**: `quakevr\ironwail.cfg`, `*.sav`, `autosave\`, `screenshots\`,
  `notes\`, `profile\`, `relit\`, `bodycal\`, `motions\` (except shipped ones), `eyeshots\`, `checklist_ticks.txt`,
  `retro_overrides.txt`, `cache\`, `qvr_addons\` (everything the package's allowlist does not name; `quakevr\.gitignore` lists them).
- Settings migrate by themselves on the first start (`configVersion`, [section 6](#6-out-of-the-box-settings)).
- After an update the relight is re-run: it only relights maps whose stamp changed (fast).
- Update check: the build now names itself (`VR_BuildVersion`: commit date and short hash); a small `latest.json`
  on both hosts (GitHub releases and vittorioromeo.com) names the newest. Offer, never auto-install.
- Adopting a zip install (layout A): offer to move `quakevr\ironwail.cfg`, saves, notes, `relit\` and Map Library
  data to `<QVR>`; never delete the old ones (say what can be removed).

### Uninstall, integrity, offline, disk

| Topic | Design |
|---|---|
| Uninstall | removes the manifest's files and the downloaded tools; asks before removing player data (saves, config, relit maps, Map Library downloads, HQ textures). Never touches the Quake folder in layout B. |
| Integrity | `manifest.json`: SHA-256 of every shipped file; a "Verify / repair" action. Downloads checked against pinned SHA-256 (the VC++ redistributable by its Microsoft signature instead, as `aka.ms` moves). |
| Offline | a full installer containing the payload; optional components accept local files picked by the player (same hashes). The game itself runs offline (`-nomapindex`, `-noaddons`, `vr_maps_fetch 0`). |
| Disk | payload ~0.2 GB; HQ textures ~0.6 GB; relit maps ~0.21 GB; tools ~0.05 GB; Map Library grows with use. ~1.1 GB for everything. |

## 6. Out-of-the-box settings

### How defaults work today

| Layer | File / code | When |
|---|---|---|
| Compiled defaults | `Quake/vr/vr_cvars.inc` (`QVR_CVAR(name, "default", flags)`), engine cvars in their `.c` files | always |
| Shipped tuning | `quakevr/vr_defaults.cfg`: `vr_default <cvar> <value>` sets the value **and makes it the default** (resets go to it); run from `default.cfg`, before the saved config; rewritten by `vr_savedefaults` | every start |
| Saved config | `quakevr/ironwail.cfg` (`exec config.cfg` in `quake.rc`) | every start |
| Forced | `quakevr/quakevr.cfg`: gameplay rules (`vr_gameplayfix_*`, `vr_pickup_scale`, `sv_gameplayfix_random 0`), `vr_checkbindings`, `vr_enabled 1` | after the saved config |
| Migrations | `defaultChanges` in `vr_cvars.cpp`, `configVersion` 89: a setting is moved to its new default only if it still has the old one; `vr_props_version` (57, `vr_props.cpp`) and `vr_wofs_version` (`vr_weapons.cpp`) do the same for held objects and weapon offsets | first start of a new build |
| Graphics presets | `vr_graphics_preset` 0 off .. 4 ultra (applies a group of settings) | on demand |
| First start | no saved config: `vr_migrate_config new` sets `vr_setup_pending 1`: **VR Calibration** starts once the headset is on (`vrcalibration.bsp`: height, body poses; wall buttons for turning, locomotion, sticks, gadget arm, torch side, world scale, body, HUD) | once |

`vr_savedefaults` already leaves out personal settings (`personal()` in `vr_cvars.cpp`: config/props/weapon versions,
height, `vr_bodycal_*`, `vr_body_tweak_*`, the arm settings `vr_body_arm_*`/`vr_body_elbow_*` (the author's decision,
2026-09-28), runtime choice, tips seen, menu positions, the motion recorder's). It does **not** cover engine cvars
other than `r_*`/`gl_*` (so `gamma`, `contrast`, `fov`, `scr_*` stay out), nor the per-prop (`vr_prop_*`) and
per-weapon (`vr_wofs_*`) tables.

### Making a fresh install match the author

1. A fresh-install dump was compared with his config (`his_cfg_20261006_1237.cfg`, 13,372 settings, 73 binds):
   **69 settings differ, binds identical** ([Appendix A](#appendix-a-the-authors-config-against-a-fresh-install)). His
   config is at version 88; version 89 already promoted his 2026-10-06 parallax and combat tweaks.
2. None of his differing gameplay values was ever a default (checked in `vr_cvars.inc` / `vr_defaults.cfg` history
   since July), so they are his own choices. What remains to promote is small: fire particles, decapitation pops,
   ragdoll grab/stick, the training dummy gib, the hologram height, the gib/head masses, the torch offset, and maybe
   the retro and desktop-mirror values. **He will pick which of these ~20 candidates become defaults** (decision 3;
   Appendix A, "promote?").
3. Process for later rounds: before a release, dump a fresh install (`resetall; exec quakevr.cfg; writeconfig
   fresh` with `-vrmock`, as done here) and diff it with his current config; promote with `vr_default` lines plus a
   `defaultChanges` entry (so existing players get it too).
4. The installer itself **writes no settings**. Machine-dependent choices belong to the first start in the game:
   the graphics preset from the GPU (`GL_RENDERER`; a new first-run step) and the runtime: **when Virtual Desktop is
   installed, suggest its OpenXR runtime** (VDXR, `vr_xr_runtime 1`, as the author uses; decision 10).

## 7. Shortcuts and launch variants

| Shortcut | Command | Notes |
|---|---|---|
| Quake VR (Start menu + desktop) | `<QVR>\ironwail.exe -basedir "<Quake>" -basedir "<QVR>" -game quakevr`, Start in `<QVR>` | the working directory also receives `qvr_crash.txt`/`.dmp` and `qconsole.log` |
| Quake VR (flat screen) | the same + `+vr_enabled 0` (verify that a command-line `+` runs after `quakevr.cfg`'s `vr_enabled 1`: `stuffcmds` follows it in `quake.rc`, so it should) | for trying without a headset |
| Quake VR (log for bug reports) | the same + `-condebug` | |
| Quake VR Setup / Repair | the installer in maintenance mode: change options, re-run relight, verify files, uninstall | |
| Folder shortcuts | "Quake VR files" (`<QVR>\quakevr`: screenshots, notes, profiles) | |
| Steam (non-Steam game) | **explained only** (decision 11): the installer's last page and the README say how (Steam > Games > Add a Non-Steam Game > pick `<QVR>\ironwail.exe`, then set its launch options to the shortcut's arguments); nothing writes `shortcuts.vdf` | makes it launchable from SteamVR's and Virtual Desktop's game lists |
| SteamVR library | a `.vrmanifest` (`app_key`, `launch_type: binary`, `binary_path_windows`, `arguments`, `image_path`) registered through OpenVR's `IVRApplications::AddApplicationManifest` (needs `openvr_api.dll`, MIT/BSD-3: verify) | optional; SteamVR users only |
| Virtual Desktop | lists Steam (and Oculus) games: the Steam shortcut covers it (verify) | |

Keep `QuakeVR.bat` for zip users and for passing arguments; make it pass both base dirs when `install.json` exists
(or leave it unchanged for layout A).

## 8. Other useful checks and features

| Item | What |
|---|---|
| GPU / driver | read the adapter (DXGI or WMI) and driver version; warn below a known-good tier (no hard block); the engine needs GL 4.3 |
| SteamVR / VD present | `AvailableRuntimes`; tell the player which runtime will be used; link to INSTALL.md's runtime section |
| Logs | `qconsole.log` (with `-condebug`, in the base dir); `qvr_crash.txt` + `qvr_crash.dmp` in the **working directory**; `qvr_error.txt` (engine errors, test runs). A "Collect a bug report" shortcut could zip these with `ironwail.cfg`, `vr_status` output and the newest `profile\memstats_*.csv`. `ironwail.pdb` (34 MB) now ships, so crash stacks have names; the report's second line names the build. |
| Map Library | `<writable base>\cache\maps\<sha256>.zip`, `cache\maps_installed.txt`, `cache\maps_index.txt`, `qvr_addons\<sha16>\` |
| Network use (no telemetry) | Quaddicted index at start (`vr_maps_fetch`, `-nomapindex`), Map Library downloads on request, Ironwail's add-on list from `kexquake.s3.amazonaws.com` (`-noaddons`). Nothing is sent about the player. Say so in the installer. |
| Multiple installs | layout B allows several `<QVR>` folders (stable / test) on one Quake; each has its own config (two copies sharing one config already merge their writes) |
| Portable mode | layout B on a removable drive with a relative `-basedir` is possible; the installer could write a `portable.txt` and a launcher that resolves paths at start (later) |
| Voice notes | `quakevr\notes\` (playtesters); transcription stays a separate script |
| Saved Games | when started without `-basedir` from a non-Quake folder, Ironwail writes to `Saved Games\Ironwail\original` (store mode): another reason for explicit `-basedir`s |

## 9. Installer technology

| Option | Pros | Cons |
|---|---|---|
| **Inno Setup 6** | free (incl. commercial); one signed `setup.exe`; Pascal script reads the registry and files (VDF parsing is ~50 lines); built-in download page with SHA-256 check (`CreateDownloadPage`, `DownloadTemporaryFile`, 6.1+); shortcuts, per-user installs without admin, uninstaller, `[Run]` for the VC++ redistributable; zip/tgz via Windows `tar` | Pascal script for logic; progress of a long relight shown by a console window or a polled log |
| NSIS | small; plugins for everything | script language harder to maintain; downloads/hashes through plugins |
| WiX (MSI) | enterprise-grade, repairs, upgrades | heavy for a mod; MSI rules (per-user, custom actions) fight downloads and relighting; licence terms changed in recent versions (verify) |
| Custom launcher app (C++ / SDL or a small WinUI/WPF app) | full control: detection UI, downloads, relight progress, update check, launch variants, later "news"; reuses our C++ | more code to maintain; needs code signing to avoid SmartScreen; still needs a way to install the VC++ runtime it depends on (static CRT for the launcher) |
| In-game first-run setup (reuse `Download()`/libcurl, miniz, threads, VR menus) | works in the headset; no second app; HQ textures and relight could be offered in VR; status pages already exist (`vr_campaign_status`) | the engine cannot start without `id1` (detection must precede it); a crashing VC++ runtime cannot be fixed from inside; relight needs Python or a C++ port of `relight_maps.py` (~900 lines) |

**Recommendation:** an **Inno Setup** installer for what must happen before the game can run (prerequisites,
detection, payload, shortcuts, optional downloads with pinned hashes), and an **in-game "Setup" page** for what is
better done in the headset or after the first start (the relight, now an in-game tool; graphics preset by GPU,
runtime suggestion, install HQ textures, update notice). No embedded Python. A separate launcher app is not needed
yet; revisit if the setup grows (news, multiple profiles).

### Hosting and SmartScreen (decision 6)

Downloads are hosted on **both GitHub releases and vittorioromeo.com** (the same files, the same SHA-256s; the
installer and the update check try one, then the other). There is **no code-signing certificate**, so:

- Windows marks downloaded files (Mark of the Web). An unsigned `setup.exe` (or an `ironwail.exe` from an extracted
  zip) shows SmartScreen's "Windows protected your PC" until the file has built reputation: the player clicks
  **More info > Run anyway**. Reputation is per file hash, so every release starts again; a signed file would carry
  the publisher's reputation instead.
- Say so on the download page and in the README, with a screenshot of the two clicks, and publish each file's SHA-256
  next to it (both hosts) so players can check what they run (`Get-FileHash`).
- Keep file names stable and ship few executables (one installer). Antivirus false positives on an unsigned
  installer can be reported to Microsoft's file submission portal.
- If it becomes a problem: Azure Trusted Signing (a monthly subscription, no hardware token) signs the installer and
  the exe without changing anything else.

## 10. Implementation plan

| Phase | Deliverable | Notes |
|---|---|---|
| 0. Prerequisite fixes | **done:** packaging from `git ls-files` (allowlist); ship `ironwail.pdb`; embed a version string; textures-only pack folders read as "missing"; Map Library zips checked against their sha256; Epic in `ownedRoots`. **Left:** a hash manifest; the relight's `_luma.png` lookup (the in-game relight tool); drop unused codec DLLs; confirm monster-model licences | [section 12](#12-problems-found-while-researching) |
| 1. Minimal installer | Inno Setup, per-user, layout B: VC++ check/install, Steam/GOG/Epic/manual detection with a status page, payload copy, `install.json`, shortcuts (VR, flat, log), uninstaller keeping player data | replaces "unzip into the Quake folder" |
| 2. Optional components | HQ textures (owned packs only, ticked), bundled `light.exe` + VisPatch, the in-game relight with progress (ticked), offline file pickers, pinned hashes | |
| 3. Defaults | promote the author's approved values (Appendix A) with a `defaultChanges` entry; first-run graphics preset and runtime suggestion in the game | needs his answers |
| 4. Updates and repair | version check, update in place keeping player files, adopt zip installs, verify/repair | |
| 5. Integrations | Steam non-Steam shortcut (explained, not written), SteamVR manifest, bug-report collector, Xbox app detection | each optional |

## 11. Decisions (Vittorio, 2026-10-06)

| # | Question | Decision |
|---|---|---|
| 1 | "The same settings" as what? | As **his own setup**, out of the box ([section 6](#6-out-of-the-box-settings)). |
| 2 | Layout | The installer defaults to **its own folder** (B); Quake's folder is left untouched. The zip stays for A. |
| 3 | Which of his ~20 tuning values become defaults | **He picks later** from Appendix A's "promote?" tables; each then gets a `vr_default` line and a `defaultChanges` entry. |
| 4 | HQ textures (0.6 GB) and the relight (~1 min) by default? | **Both ticked by default.** |
| 5 | Embedded Python for the relight? | No: the relight becomes an **in-game tool** (worker `relight` is building it). |
| 6 | Hosting; code signing | Host on **both GitHub and vittorioromeo.com**; **no code-signing certificate** (SmartScreen: [section 9](#hosting-and-smartscreen-decision-6)). |
| 7 | Bundle ericw-tools' `light.exe`? | **Bundle it**, since GPL-3 does not extend to our code: it is a separate program run as its own process (mere aggregation), shipped unmodified with its licence and a source offer. Were that ever in doubt, download it on demand (pinned hash) instead. |
| 8 | Music | Read the rerelease's music **in place** (worker `music` is doing it), and the original Quake's music too; never copied. |
| 9 | mg1/mg3 in the detection page | Shown as **"detected, not yet supported"**. |
| 10 | Offer VDXR? | **Suggest Virtual Desktop's OpenXR runtime** when Virtual Desktop is installed. |
| 11 | Steam integration | **Explain how only** (Add a Non-Steam Game); nothing writes `shortcuts.vdf`. |

## 12. Problems found while researching

Found by the research; the ones marked **fixed** were fixed on branch `agent/releasefix` (2026-10-06).

| Problem | Where | Effect |
|---|---|---|
| The relight reads only `textures/<name>_luma.tga` | `Misc/quakevr/relight_maps.py` (`Lumas.glow`, line ~443) | with the PNG HQ pack (the recommended one), glowing QRP lamps get no light: the result differs from the author's, contrary to `TEXTURES.md`/`RELIGHTING.md` |
| The relight looks for lumas only under `--quake` | `relight_maps.py` (`os.path.join(self.quake, folder, "textures", ...)`) | in layout B the HQ textures live in `<QVR>\id1\textures`: the relight needs a second texture root (or the installer passes `<QVR>`'s too) |
| A pack folder holding only `textures\` reads as "incomplete" | `inspectPack` in `vr_gamedir.cpp` (`directory ? 2 : 0`) | **fixed**: "incomplete" now needs a pak or one of the pack's files; a data-less rerelease campaign folder no longer hides a lower root's real copy |
| The package copies every untracked file of the game folder | `package-quakevr.ps1` (file walk with an exclude list) | **fixed**: the game folder is an allowlist (tracked files plus `progs.dat`); a dry run on the author's checkout lists none of its untracked files |
| No `.pdb` in the package | `package-quakevr.ps1` copies `.exe/.dll/.pak` only | **fixed**: `ironwail.pdb` ships (the packager fails without it); DbgHelp also looks beside the exe |
| No version number in the build | | **fixed**: the commit date and short hash (`-dirty` when changed) in the console, VR Settings and `qvr_crash.txt` |
| Map Library downloads are not checked against their sha256 | `vr_mapinstall.cpp` (`Download()` result used as is; the sha256 is an identifier) | **fixed**: a zip whose SHA-256 differs from the index's is neither kept nor unpacked (that mirror fails, the next is tried); `vr_sha256_test` |
| Epic rerelease not in `ownedRoots` | `vr_gamedir.cpp` | **fixed**: the launcher's manifests are read (`-noepic`, `-epicmanifests <dir>` for tests) |
| `build.sh` fails on HEAD `4504c684` | QC precedence check: `VR_Pack_Refresh`, `QC/vr_packutil.qc:50` | unrelated to this document |

## Appendix A: the author's config against a fresh install

Method: the worktree's build, `-vrmock`, `resetall; exec quakevr.cfg; writeconfig` (every setting at its shipped
default; `vid_*` cannot be reset while running, so their "fresh" column is the test machine's), compared with
`C:/OHWorkspace/qvr-kit/scratch/his_cfg_20261006_1237.cfg` numerically. 13,372 settings each; **69 differ; 73
binds, none differ.** "Former default?" = his value appears as a removed default in `vr_cvars.inc`/`vr_defaults.cfg`
history since 2026-07-01 (none do).

### Personal: never shipped (first-run calibration makes them)

| Setting | His | Fresh |
|---|---|---|
| `vr_height_calibration` | 1.5520 | 1.646099 |
| `vr_bodycal_upper_arm` / `_forearm` | 29.4 / 22.7 | 0 / 0 |
| `vr_bodycal_shoulder_rise` / `_shoulders_back` / `_out` / `_up` | 13.5 / -0.023 / -0.057 / -0.004 | 0 |
| `vr_bodycal_undo` | (his previous calibration) | empty |
| `vr_body_elbow_back` / `_hand` / `_lift` | 0.35 / 0.5 / 8 | 0.25 / 0.4 / 4 (arm settings are the player's: decision of 2026-09-28) |

### State and bookkeeping: not settings

| Setting | His | Fresh |
|---|---|---|
| `vr_cfg_version` | 88 | 89 on a real first start |
| `vr_props_version` / `vr_wofs_version` | 57 / 34 | set to current on a first start |
| `vr_menu_positions`, `vr_tips_seen` | menu cursors, tips seen | empty |
| `vr_motion_button` / `_category` / `_detail` | 2 / 7 / 1 | 0 (motion recorder UI) |
| `vr_prop_id_33`, `_49`, `_50`, `_51` | `v_crowbar`, `v_ksword`, `v_shot`, `v_light` | empty (slots assigned when first held) |

### Machine and runtime: decided per PC

| Setting | His | Fresh | Note |
|---|---|---|---|
| `vr_xr_runtime` | 1 (VDXR) | 0 (system) | offer VDXR when present (question 10) |
| `vid_width` x `vid_height`, `vid_borderless`, `vid_fsaa` | 1920x1080, 1, 0 | (machine) | desktop window only |

### Desktop mirror and flat screen: promote?

| Setting | His | Fresh |
|---|---|---|
| `vr_window_view` | 0 | 1 |
| `vr_spectator_fov` | 120 | 130 |
| `vr_mirror_hide_hud_text` | 1 | 0 |
| `fov` | 109.94 | 90 |
| `gamma` / `contrast` | 0.95 / 1.2 | 1 / 1 (check whether they reach the headset image) |
| `scr_conscale`, `scr_menuscale`, `scr_sbarscale`, `scr_crosshairscale` | 3 | 1 |
| `scr_centerprintbg` / `scr_menubgstyle` | 3 / 0 | 2 / -1 |

### Gameplay and look: promote? (his own values)

| Setting | His | Fresh |
|---|---|---|
| `vr_fire_particles_alpha` / `_count` / `_origin` / `_size` | 1 / 8 / 0.2 / 2.5 | 0.55 / 6 / 0.25 / 2 |
| `vr_decap_pop_always_range` / `_never_range` | 2 / 12 | 3 / 15 |
| `vr_decap_pop_thrown_light_chance` | 0.25 | 0 |
| `vr_ragdoll_grab_reach` / `vr_ragdoll_hand_stick` | 2 / 2 | 6 / 12 |
| `vr_dummy_gib` | 1 | 0 |
| `vr_messages_hologram_height` | 10 | 5 |
| `vr_retro_all_average` / `_block` / `_dither` / `_fade` / `_palette` | 0 / 0.5 / 0.5 / -1 / 1 | 1 / 1 / 0 / 1 / 0 (maybe an experiment: confirm) |
| `vr_wofs_torch_out_18` / `vr_wofs_torch_up_18` | -0.035 / 0.075 | 0 / 0 (a weapon-offset slot: needs a `vr_wofs_version` change) |
| Masses (kg), `vr_prop_mass_NN` (needs a `vr_props_version` change) | gib2 15, gib3 10, h_guard 8, h_dog 9, h_mega 9, h_knight 8, h_hellkn 11, h_ogre 15, h_shal 10, h_shams 65, h_demon 18 | 20, 12, 10, 12, 16, 12, 17, 30, 12, 70, 28 |
