# Quake VR installer: design and research

Research for an installer that makes Quake VR trivial to play: it finds the player's Quake, installs the mod
without touching Quake's own files, fetches the optional tools and assets, makes the shortcuts, and leaves a fresh
install that looks and plays like the author's own. This is the list of what it must do, with the facts from this
repository it depends on, and a plan. **Phase 1 of the installer exists**: a C# WPF app in `Installer/` (the author's
choice of technology), described in [section 13](#13-the-installer-app-phase-1).

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
- [13. The installer app (phase 1)](#13-the-installer-app-phase-1)
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
| Relight scripts | `quakevr\tools\`: `relight_maps.py`, `vis_maps.py`, `quakepak.py`, `quakeimage.py`, `relight_probe.py`, and `ericw-tools\` (`light.exe` and its DLLs, for the in-game relighting); `relight_textures.cfg` is in `quakevr\` | | Python 3.7+, standard library only |
| Symbols | `ironwail.pdb` (34 MB, full: clang-cl `/Z7` objects linked by lld-link `/DEBUG`) | | shipped beside the exe: `qvr_crash.txt` names the functions on the stack (DbgHelp looks in the exe's folder, then the working directory), and `qvr_crash.dmp` opens in a debugger with it |
| Build version | baked into the exe (`quakevr.props`, `QvrBuildVersion`: the last commit's date and short hash, `-dirty` with uncommitted changes) | | printed at start and by `version`, on the last line of VR Settings, and in `qvr_crash.txt`'s second line |

About 195 MB unpacked; the zip is "too large for a GitHub release" (`docs/INSTALL.md`).

### Run-time requirements

| Requirement | Detail (from `docs/INSTALL.md`, `README.md`) |
|---|---|
| OS | Windows 10/11 x64. VR only in the Windows x64 build. |
| GPU | OpenGL 4.3 (Ironwail). Shipped settings tuned on an RTX 4090; presets scale down. |
| VC++ runtime | Microsoft Visual C++ 2015-2022 Redistributable x64 **14.44 or later** (built with VS 2022 17.14, toolset 14.44.35207). Older than 14.40 crashes at start in `MSVCP140.dll` (the 17.10 `std::mutex` change). Permanent link: `https://aka.ms/vs/17/release/vc_redist.x64.exe`. `ironwail.exe` imports (`dumpbin /dependents`, 2026-10-07, with mimalloc): `MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll` and the Universal CRT's `api-ms-win-crt-*` (part of Windows 10 and later). mimalloc's override (`vr_crtheap.c`) only removed the heap functions from the imports: the engine stays on the DLL runtime (/MD), so the requirement is unchanged. |
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
| `hipnotic`, `rogue` | every file of `vr_pack_hipnotic.inc` / `vr_pack_rogue.inc` (100 / 131 entries) found intact in the packs of any base dir | yes (merged VR progs) | `inspectPack`: 1 available, 2 incomplete/corrupt (a pak or one of the pack's files is there, but not all of them), 0 missing (also a folder holding only an extracted texture pack's `textures\`); `vr_pack_status` |
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
| ericw-tools 2.0.0-alpha11 | **bundle** `light.exe` (and what it needs) unmodified; the game can also download the same release zip itself (section 4) | GPL-3.0 | run as a separate process: mere aggregation, so GPL-3 does not reach our code (decision 7); ship its licence text and offer its source (mirror the tag's source zip next to ours: the release checklist in section 9) |
| VisPatch data (`id1_vis.tgz`...) | download | **unknown** (1997 data, SourceForge project) | verify before mirroring; download only |
| QRP HQ textures | download (author's PNG release) | no licence file; free, credited, removal on request (`TEXTURES.md`) | as today |
| Python (embeddable) for the relight | **not needed**: the relight becomes an in-game tool (decision 5) | PSF-2.0 | dropped |

## 4. Optional downloads

| Item | Source (from this repo's docs) | Size | Goes to | Needs |
|---|---|---:|---|---|
| HQ textures (QRP, PNG), **ticked by default** | `https://github.com/vittorioromeo/quakevr/releases/tag/textures-2026-10-03`, `quakevr-hq-textures-png-2026-10-03.zip` | ~0.6 GB unpacked (id1 360 MB, hipnotic 88 MB, rogue 121 MB, Quetoo copy 47 MB) | `id1\textures`, and `hipnotic\textures` / `rogue\textures` **only for packs the player owns** | nothing; install before relighting |
| QRP archive (fallback) | `https://www.moddb.com/addons/quake-revitalization-project-archive`, `QuakeRevitalizationProject.7z` | 1.26 GB | the same, after unpacking the `.pk3`s | 7-Zip; not for the installer (use the PNG release) |
| ericw-tools | `https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11`, `ericw-tools-2.0.0-alpha11-win64.zip` (27,503,991 bytes, SHA-256 `4e5ea11be2194a1c4acac6d6da9d5b5b9f65324fda2d67efa0731d1fd8e0745f`) | 27.5 MB (what is kept: about 39 MB) | `<QVR>\quakevr\tools\ericw-tools\` (as `Windows/package-quakevr.ps1` lays it out: `light.exe`, `embree4.dll`, `tbb12.dll`, `tbbmalloc.dll`, `gpl_v3.txt`, `LICENSE-embree.txt`, `README.md`, `NOTICE.txt`) | exact version (the light differs between versions). The package ships it. If it is missing, the game offers it: Graphics > Relighting > **Download ericw-tools (27.5 MB)** (`vr_relight_get_tool`, `Quake/vr/vr_relight_tool.cpp`) fetches this exact zip, checks the pinned size and SHA-256, and unpacks only those files into the user's `quakevr\tools\ericw-tools\` (light.exe last). The author's own copy (`C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64`) is looked in only at Menu Detail: Developer (`vr_menu_level 2`) |
| VisPatch data | `https://sourceforge.net/projects/vispatch/files/vispatch%20data/1.0/`: `id1_vis.tgz` (949 KB), `hipnotic_vis.tgz` (807 KB), `rogue_vis.tgz` (787 KB); mirror: `https://www.quake-info-pool.net/vispatch/files.htm` | 2.5 MB | `<QVR>\quakevr\tools\vispatch\` (`id1.vis`, `hipnotic.vis`, `rogue.vis`), beside `relight_maps.py`, which takes that folder by itself (else `--vis-dir`, `QUAKEVR_VISPATCH`); the in-game relight reads it there too (Relighting > *See-Through Liquids*, `vr_relight_seethrough` 1, vr_relight_vis.cpp: vis_maps.py's patch in C++, the same bytes), so the installer only unpacks the files: **no separate vis step** | `tar` (in Windows 10+) |
| Relit maps | made **in the game** (the relight as an in-game tool, decision 5: Graphics > Relighting > Many Maps), with the bundled (or downloaded) `light.exe` and the VisPatch data | ~220 MB. A batch lights two maps at once by default (`vr_relight_parallel` 0: two from 8 cores) and skips the maps already relit with the same settings. Measured 2026-10-06 on 32 cores at the defaults (Smooth shadows, no Bounced Light): every map of id1, hipnotic, rogue and quakevr (79) in 1:52; e1's 8 in about 6 s (about 33 s with Bounced Light 1) | in-game results: `<QVR>\quakevr\relit_custom\<game>\maps\` (`relight_maps.py` still writes `<QVR>\quakevr\relit\`; the game prefers `relit_custom`, `vr_relight_use`) | `light.exe`, VisPatch; textures first. **Ticked by default** |
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
| Migrations | `defaultChanges` in `vr_cvars.cpp`, `configVersion` 89 (94 on 2026-10-06, 95 on 2026-10-07): a setting is moved to its new default only if it still has the old one; `vr_props_version` (58, `vr_props.cpp`) and `vr_wofs_version` (35, `vr_weapons.cpp`) do the same for held objects and weapon offsets | first start of a new build |
| Graphics presets | `vr_graphics_preset` 0 off .. 4 ultra (applies a group of settings) | on demand |
| First start | no saved config: `vr_migrate_config new` sets `vr_setup_pending 1`: **VR Calibration** starts once the headset is on (`vrcalibration.bsp`: height, body poses; calibration only: no buttons; the menu button pauses it on Body Calibration's page, Position: seated or standing; a doorway leads to the hub) | once |

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
| Quake VR Unleashed (Start menu + desktop; the installer names everything "Quake VR: Unleashed", as `Quake VR Unleashed` in file names) | `<QVR>\ironwail.exe -basedir "<Quake>" -basedir "<QVR>" -game quakevr`, Start in `<QVR>` | the working directory also receives `qvr_crash.txt`/`.dmp` and `qconsole.log` |
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

**Chosen (Vittorio, 2026-10-06): a C# WPF app** (`Installer/`, section 13) for what must happen before the game can
run (prerequisites, detection, payload, shortcuts, optional downloads with pinned hashes), and the game itself for
what is better done in the headset or after the first start (the relight, an in-game tool started by the first
launch; graphics preset by GPU, runtime choice, update notice). No embedded Python. The research's first
recommendation was Inno Setup; the app costs more code but gives the detection page, progress, updates and the
uninstaller in one place, in the language the author prefers. The app does not itself need the VC++ runtime it checks
for (.NET carries its own native parts), so it can install it; a self-contained single-file publish needs no .NET either.

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

### Publishing a release (what Vittorio does)

The installer already reads the final addresses (`InstallerSettings` in `Installer/src/QuakeVR.Installer.Core/Packaging/ReleaseFeed.cs`, the
defaults when no `installer-settings.json` sits beside the exe; one there overrides them, for tests or a move):

| Feed, in order | Status |
|---|---|
| `https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json` | Works as soon as a release has a `latest.json` asset and is the newest non-draft, non-prerelease one (GitHub's "latest" skips both) |
| `https://vittorioromeo.com/quakevr/latest.json` | **Needs you to create it**: the `/quakevr/` folder on the site and the file, uploaded with each release |

Until a release exists both answer 404: the installer then offers "Use a local package" (and installs from a
`QuakeVR.zip` beside it with no network at all), so nothing has to change in the code.

Steps for each release:

1. Build and package: `Windows\package-quakevr.ps1 -Build` (dist\QuakeVR with `manifest.json`, and dist\QuakeVR.zip). The
   installer: `dotnet publish Installer/src/QuakeVR.Installer -c Release -r win-x64 --self-contained -p:PublishSingleFile=true`.
2. `python Misc/quakevr/make_release.py --package dist/QuakeVR --setup <publish>\QuakeVR-Setup.exe
   [--textures <HD texture pack>.zip] --asset ericw-tools-2.0.0-alpha11-src.zip` writes `dist/release/<tag>/`: `QuakeVR.zip`
   (zipped from the folder after checking every file against its manifest), the other assets, `latest.json` (schema 1:
   version, `package`, `components.hdtextures`; each with file, size, SHA-256 and URLs
   `https://github.com/vittorioromeo/quakevr/releases/download/<tag>/<file>`, the release's own assets, so an old `latest.json`
   never points at newer files) and `PUBLISH.txt`. The tag defaults to `v` + the package's version
   (`2026-10-06 c131f4bf` gives `v2026-10-06-c131f4bf`); `--tag` sets another.
3. Create the GitHub release with every file of that folder (latest.json included), from the folder (the command is in
   `PUBLISH.txt`; the script runs nothing):
   `gh release create <tag> --repo vittorioromeo/quakevr --target <commit> --title "Quake VR: Unleashed <version>" --notes-file <notes.md> "QuakeVR.zip" "QuakeVR-Setup.exe" ... "latest.json"`
4. Upload the same `latest.json` to `https://vittorioromeo.com/quakevr/latest.json`. For a download mirror there too, also
   upload the other files (e.g. to `/quakevr/releases/<tag>/`) and run the script again with
   `--url-base "https://github.com/vittorioromeo/quakevr/releases/download/{tag}/{file}" --url-base "https://vittorioromeo.com/quakevr/releases/{tag}/{file}"`
   before uploading (the URLs are in `latest.json`; its hashes do not change).
5. Check: `qvr-setup feed --url <each feed>` prints the version and the package's size; then the installer with no local
   package.

Verified (2026-10-07) without publishing: a release made from a scratch package, served by a local HTTP server
(`--url-base http://127.0.0.1:8765/{file}`), read by `qvr-setup feed`, its package downloaded and checked (size, SHA-256),
installed, verified and uninstalled; the default run's `latest.json` and `gh` command checked by hand.

### Release checklist: third-party files

- **ericw-tools' source next to every release** (GPL-3, section 3; the package ships `light.exe` and the game can
  download it): upload `ericw-tools-2.0.0-alpha11-src.zip` (the 2.0.0-alpha11 tag's source with its submodules:
  `git clone --recursive --branch 2.0.0-alpha11 https://github.com/ericwa/ericw-tools`, zipped; GitHub's own tag
  archive leaves the submodules out) to the same release page on both hosts, beside the Quake VR package. Both `NOTICE.txt`s (the package's, `Misc/quakevr/ericw-tools-NOTICE.txt`,
  and the one the in-game download writes) say it is there.
- Check that the package has `quakevr\tools\ericw-tools\light.exe` (`package-quakevr.ps1` warns when it is missing;
  players would then need the in-game download).
- The in-game download's version, URL, size and SHA-256 are pinned in `Quake/vr/vr_relight_tool.cpp` (`releaseUrl`,
  `releaseBytes`, `releaseSha`): change them only together with the version the package ships, and keep it exact.

## 10. Implementation plan

| Phase | Deliverable | Notes |
|---|---|---|
| 0. Prerequisite fixes | **done:** packaging from `git ls-files` (allowlist); ship `ironwail.pdb`; embed a version string; textures-only pack folders read as "missing"; Map Library zips checked against their sha256; Epic in `ownedRoots`; **the package's hash manifest** (`manifest.json`, `Windows/write-package-manifest.ps1`, 2026-10-06). **Left:** the relight's `_luma.png` lookup (the in-game relight tool); drop unused codec DLLs; confirm monster-model licences | [section 12](#12-problems-found-while-researching) |
| 1. Minimal installer | **done (2026-10-06, `Installer/`, section 13):** C# WPF wizard, per-user, layout B: Steam/GOG/Epic/manual detection with a status page (expansions by the engine's rules, OpenXR runtime, Virtual Desktop, SteamVR, VC++ runtime, music, an older zip install), payload copy checked against `manifest.json`, `install.json`, shortcuts (VR, flat, log, folder), update in place, uninstall keeping player data, downloads from `latest.json` with mirrors, resume and pinned hashes (tested on a local server only). **Left:** installing the VC++ runtime (only a link now), an Apps & Features entry and a Setup copy in the install folder (maintenance mode is "run the installer again"), a published single-file exe | replaces "unzip into the Quake folder" |
| 2. Optional components | HQ textures (owned packs only, ticked: **done** from a local zip or the feed's `hdtextures`), bundled `light.exe` (**done**: in the package) + VisPatch (download), the in-game relight (**done**: the game's first start, however started, runs `vr_relight_batch everything` from the installer's marker), offline file pickers (package: done; textures: command line only), pinned hashes | |
| 3. Defaults | promote the author's approved values (Appendix A) with a `defaultChanges` entry; first-run graphics preset and runtime suggestion in the game | needs his answers |
| 4. Updates and repair | version check against `latest.json` at start, update in place keeping player files (**done**), adopt zip installs, verify (**done**: `qvr-setup verify`)/repair (re-run with the same package) | |
| 5. Integrations | Steam non-Steam shortcut (explained on the last page, with the launch options to copy: **done**), SteamVR manifest, bug-report collector, Xbox app detection | each optional |

## 11. Decisions (Vittorio, 2026-10-06)

| # | Question | Decision |
|---|---|---|
| 1 | "The same settings" as what? | As **his own setup**, out of the box ([section 6](#6-out-of-the-box-settings)). |
| 2 | Layout | The installer defaults to **its own folder** (B); Quake's folder is left untouched. The zip stays for A. |
| 3 | Which of his ~20 tuning values become defaults | **He picks later** from Appendix A's "promote?" tables; each then gets a `vr_default` line and a `defaultChanges` entry. |
| 4 | HQ textures (0.6 GB) and the relight (~1 min) by default? | **Both ticked by default.** |
| 5 | Embedded Python for the relight? | No: the relight became an **in-game tool** (Advanced VR > Graphics > Relighting), with
the VisPatch step in it (*See-Through Liquids*: the data files in `<QVR>\quakevr\tools\vispatch`). |
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
| `package-quakevr.ps1` did not parse | `"no ironwail.pdb in $bin: ..."` (PowerShell reads `$bin:` as a scoped variable) | **fixed** (`${bin}`): the packager failed at once, `-DryRun` included |
| `-DryRun` left out the bundled ericw-tools | the dry run listed `quakevr/tools/*.py` only | **fixed**: it lists `quakevr/tools/ericw-tools/*` when the tool is found, and `manifest.json` |

## 13. The installer app (phase 1)

`Installer/` (its README has the build, the command lines and the tests). C#, WPF, .NET 9, no NuGet package.

| Part | Where | What it does |
|---|---|---|
| Detection | `Core/Detection` | Steam: `HKCU\Software\Valve\Steam` `SteamPath` (then HKLM, then Program Files), every library of `steamapps\libraryfolders.vdf` (both VDF formats; a tolerant KeyValues reader), `appmanifest_2310.acf`'s `installdir`. GOG: the two registry keys of section 2. Epic: the launcher's `*.item` manifests (DisplayName has "quake", InstallLocation with `rerelease\id1\pak0.pak` or `id1\pak0.pak`). Manual: a folder, its `id1` or its `rerelease`. Each Quake gets its `id1` kind (original: pak0 + pak1, 1.06 sizes recognised; rerelease: large pak0 or `QuakeEX.kpf`; shareware; corrupt), its base dir (the original when present), its rerelease root, its music folder (read in place) and an older zip install (`<Quake>\quakevr`) |
| Expansions | `ExpansionDetector`, `PackInspector`, `ResourceValidator` | The engine's rules, ported: hipnotic/rogue over the base dirs; dopa/mg1/mg3 over `ownedRoots` (Steam's rerelease, GOG enhanced, Epic, then each base's `..\rerelease`, `rerelease`, itself), highest first, a data-less folder skipped; pak directories and every listed resource checked as `validResource` does. The resource lists are the engine's own `Quake/vr/vr_pack_*.inc`, compiled in as resources. mg1/mg3: "detected, not yet supported" (decision 9). On the author's PC the report matches the game's `vr_campaign_status` line for line |
| VR and system | `SystemChecks.cs` | `HKLM\SOFTWARE\Khronos\OpenXR\1` `ActiveRuntime` and `AvailableRuntimes` (classified by file name: VDXR, SteamVR, Quest Link, WMR); Virtual Desktop (the Streamer's folder or its runtime); SteamVR (app 250820 or its runtime); VDXR suggested when Virtual Desktop is installed and not active (decision 10); the VC++ runtime (`...\VC\Runtimes\x64` >= 14.44, and `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` in System32 present and >= 14.44: a key left by a broken uninstall does not count) |
| Package | `PackageSource`, `PackageManifest` | A folder or zip (one top folder allowed) from `package-quakevr.ps1`, with its `manifest.json` (schema 1: version, path/size/SHA-256 per file). Paths are refused unless plainly relative inside the install folder |
| Install | `InstallEngine` | Layout B (decision 2): refuses a folder inside (or around) the Quake folder, in Program Files, or not empty without an `install.json`; checks the free space; stages every file as `*.qvrnew` beside its destination while hashing it (a mismatch stops the install, naming the file), then moves them all into place: a failed or cancelled install changes nothing. HD textures (decision 4): `id1\textures` always, `hipnotic`/`rogue` only for owned packs, never over a file of the player's. Writes `install.json` (version, Quake dir, choices, every installed file with its hash and component, folders made, shortcuts, relight pending) |
| Update | the same | Over an existing install: the new payload replaces the old; files the old version shipped and the new one does not are removed when unchanged (kept and reported when the player changed them); stale shortcuts removed; textures kept unless reinstalled; everything not in the record is the player's and is never touched |
| Uninstall | `Uninstaller` | Removes the recorded files that are unchanged, the recorded shortcuts whose target is inside the install, then the folders it made once empty; HD textures only when asked; reports the player's files left. `Verify` lists missing or changed files |
| Shortcuts | `Shortcuts/` | `IShellLinkW` through COM. Named after the product, **Quake VR: Unleashed** (as a file name `Quake VR Unleashed`: Windows refuses `:`). Desktop: Quake VR Unleashed; Start menu `Quake VR Unleashed\`: Quake VR Unleashed, (flat screen) `+vr_enabled 0`, (log for bug reports) `-condebug`, Quake VR Unleashed files (the `quakevr` folder). An update removes the old names' shortcuts (recorded in `install.json`) and the emptied older `Quake VR` Start menu folder. All: `ironwail.exe -basedir "<Quake>" -basedir "<QVR>" -game quakevr`, started in `<QVR>`, paths without a trailing backslash |
| Downloads | `Downloader`, `ReleaseFeed` | `latest.json` (schema 1: version, `package` and `components.hdtextures`, each with file, size, SHA-256 and mirror URLs) read from the first host that answers (GitHub's `releases/latest/download/latest.json`, then `vittorioromeo.com/quakevr/latest.json`: the final addresses; the first release and the site's file make them answer: "Publishing a release"); each file from its mirrors in order, resumed with HTTP Range, checked for size and SHA-256 before it gets its name, a mirror serving another file skipped. `GitHubReleases` reads the releases API (assets' `digest: sha256:...`) for a later fallback. Tested only against local servers |
| Window | `QuakeVR.Installer` | Welcome (the official logo; update/remove when installed) > Your PC (the checks with status icons and fix hints; another folder) > Options (where the package comes from, folder, components with sizes, shortcuts) > Install (progress, log, Cancel) > Play (Play in VR, on the monitor, open the folder; VR Calibration, relight and VDXR notes; the Steam "Add a Non-Steam Game" steps with launch options to copy, decision 11) > Thanks (a word from the author and Ko-fi). Title "Quake VR: Unleashed Setup", the square logo as its icon. Writes no game settings (section 6); its only file is the mute choice (`%LOCALAPPDATA%\QuakeVR-Installer\ui.json`) |
| Package source | `LocalPackages`, `MainViewModel` | A package (zip or folder with `manifest.json`) beside the installer (`QuakeVR.zip`, a `QuakeVR` folder, or any other `QuakeVR*.zip` that has a manifest; texture packs skipped) or picked by the player installs with no network at all. Otherwise the release feed is asked in the background at start: online is the default only when a feed answers. When none answers (no release published yet, offline), the Options page says so in plain words and offers **Use a local package…** and **Try again**; Install stays off until one of them works. If the download fails at Install anyway, the error offers the same button and installs from the picked package. The HD textures are skipped with a note (or taken from a picked zip) when their download is unavailable |
| Skin | `Skin/`, `Themes/Theme.xaml` | "Quake VR: Unleashed" in a grimy Quake look: soot-black stone and riveted iron, ember and brass, bone-white text; the official logos (wide on the Welcome page, square in the sidebar, on Play and Thanks). Textures come from the **player's own Quake, read at run time** (`Core/Assets`: a PAK reader, `gfx/palette.lmp`, WAD2 pictures, the textures of `maps/start.bsp`, `e1m1`, `e1m2`, WAV decoding): bricks `wbrick1_5` behind the page, `wizmet1_2` in the sidebar, `metal1_4` on buttons and the footer, `wizmet1_3` on plaques, `*lava1` (warped like the engine's liquids) in the progress bar, the status bar's digits for the percentage, the palette for the flames. Nothing of id Software's is in the repository or the installer: before Quake is found (a quiet detection starts with the window) or without it, the same slots get generated textures (seeded noise: bricks, brushed metal with seams, rivets and rust, lava) and a smooth fire ramp. Fonts (SIL OFL 1.1, embedded with their licences, shown under "credits"): Grenze Gotisch for titles, Barlow for text, Barlow Semi Condensed for labels and buttons |
| Animation | `FrameClock`, `FireView`, `LavaView`, `Fx` | Flames behind the logos (the classic "Doom fire": heat rising a row a step, drifting, cooling at random, blurred and upscaled), embers rising from the bottom of the page (each its own small visual: a frame redraws only where they are), a flickering glow, lava flowing in the progress bar, buttons that heat up and glow under the mouse, check marks that pop in, a pulsing Ko-fi line, pages that rise in. One clock paced by a high-resolution timer (not WPF's per-frame event, which on a 144-360 Hz display redraws hundreds of times a second): 60 Hz while the window is active, 15 Hz in the background, stopped when minimized; a governor drops it to 30 Hz for the run when the process uses more than 12% of a core (software composition, remote sessions). Windows' "Animation effects" off (or `--reduce-motion`): still flames, no embers, no pulse, no transitions. Measured on the author's PC (2026-10-07, 359 Hz display behind a Citrix adapter): flames 0.05-0.08 ms and lava 0.12 ms of simulation a frame; the whole window 20-30% of a core at 60 Hz, so the governor settles at 30 Hz, about 9%; reduced motion 1-2% |
| Sounds | `Audio/` | Quake's own sounds from the player's paks once found: `misc/menu1` (clicks, check boxes, typing at a low volume and higher pitch), `misc/menu2` (the main button), `misc/menu3` (Back, Cancel), `weapons/pkup` (install starts), `misc/secret` (install done), `misc/talk` (an error), `items/health1` (Ko-fi), and `ambience/fire1` looping very quietly. Before or without Quake: synthesized stand-ins (metal ticks and clanks, a bell chime, a buzz, a crackle). A small mixer on winmm's wave-out (no dependency), quiet levels, each sound at most once per 45 ms and three at once; nothing is sent to the device while silent. The speaker button in the sidebar mutes everything, remembered |
| Ko-fi | sidebar, Thanks page | "Support me on Ko-fi" with Ko-fi's official cup logo (from its brand assets) is in the sidebar on every page, its line gently pulsing; after Play, the Thanks page asks warmly for a token of appreciation (https://ko-fi.com/vittorioromeovee). Finish is always one click away |
| VisPatch (see-through water) | `VisPatch`, `GetVisPatchAsync` | Part of the relight component, ticked with it: the game's relight applies VisPatch's water-vis itself whenever its data is in `<QVR>\quakevr\tools\vispatch` (`vr_relight_seethrough`, Quake/vr/vr_relight_vis.cpp), so the first-start `vr_relight_batch everything` includes it. The installer gets the "vispatch data" 1.0 archives for id1 and the owned packs from their original SourceForge location (`https://sourceforge.net/projects/vispatch/files/vispatch%20data/1.0/<file>/download`, then `downloads.sourceforge.net`; `visPatchUrls` in `installer-settings.json` adds mirrors, which must serve the same files): `id1_vis.tgz` 949,270 bytes, SHA-256 `b16e400e...55f0`; `hipnotic_vis.tgz` 806,759, `254febdc...fe49`; `rogue_vis.tgz` 786,802, `6dd44ae5...796a` (fetched once 2026-10-07; full hashes in `VisPatch.cs`). Licence unknown, so never bundled or mirrored by us; a copy beside the installer is used when it matches. **sezero/vispatch** (https://github.com/sezero/vispatch, read 2026-10-07) is not a mirror of the data: it hosts only the tool, O. Sezer's revision of Andy Bay's VisPatch 1.2a source (GPLv2, `COPYING`), and in `old1997/` the 1997 tool releases (`vispatch12.zip`, `unixvis.zip`, `vispsrc.zip`, `vispatch12.txt`); no `.vis`, `vispatch.dat` or `*_vis.tgz`, and no releases. Its README says only that "a lot of vispatch data files" are around, with no address or licence for them; `vispatch12.txt` credits "our Data donators" (community-made 1997 data, first hosted on allgames.com, razor.stomped.com/water, sod.net/vis and ftp.cdrom.com, all gone) and states no copyright or terms. So the data's licence is still unknown, nothing was added to `visPatchUrls`, and SourceForge stays the only source. The GPLv2 covers the tool's code, which the game does not use (its own patcher, `vr_relight_vis.cpp`, reads the same data format). Unpacked in C# (`System.Formats.Tar` over `GZipStream`): only `<game>.vis` at the top or `<game>/vispatch.dat`, nothing leaving the folder (`rogue.txt` skipped); installed and recorded in `install.json` (component `vispatch`, kept by updates that do not reinstall it, removed by uninstall). A failed download only skips the see-through water (a note in the log); the relight still runs. Tested against a local server with made-up archives; the three real archives unpacked byte-identical to `tar` |
| VC++ runtime install | `Prerequisites/VcRedist`, `AuthenticodeVerifier`, `EnsureVcRuntimeAsync` | After the files are in place, when the runtime is missing or old: a `vc_redist.x64.exe` beside the installer, else a fresh download of `https://aka.ms/vs/17/release/vc_redist.x64.exe` (not pinned by hash: the link serves Microsoft's current release). Run only when `WinVerifyTrust` accepts its embedded signature (no UI, no revocation fetch) and the signer is `CN=Microsoft Corporation, O=Microsoft Corporation`, and its file version is >= 14.44; a download that fails the check is deleted. Then `vc_redist.x64.exe /install /quiet /norestart` through ShellExecute `runas` (the one UAC prompt). Exit codes: 0 installed; 3010/1641 installed, restart if the game does not start; 1638 a same or newer one already there; 1602/1223 prompt declined; 1618 another install running; anything else failed. Never fatal: the Play page says what is left (with Microsoft's link). `--no-prerequisites` skips it, `--vcredist-dry-run` (and the screenshot harness) only logs what it would do; `qvr-setup vcredist [--check <exe>] [--dry-run [--assume-missing] [--file <exe>]]` from a console. Verified here without installing anything: detection 14.51.36247 with the three DLLs; `--check` on the two redistributables in `C:\ProgramData\Package Cache`: both Microsoft-signed, 14.51.36247 accepted, 14.36.32532 refused as too old; an unsigned exe refused (0x800B0100); the dry run names the download and the command. Self-tests: the detection's DLL cases on a made-up machine, the real `WinVerifyTrust` on .NET's own DLL and a made-up exe, the whole flow with a local server and a fake verifier and runner (every exit code, a bad signature, an old version, a local copy, the dry run downloading and running nothing) |
| Apps & Features | `UninstallEntry`, `SetupCopy`, `IRegistryWriter`; `App.StartUninstall`, `SetupRelaunch` | Per user, no administrator rights: `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\QuakeVRUnleashed` with DisplayName "Quake VR: Unleashed", Publisher Vittorio Romeo, DisplayVersion (the package's), DisplayIcon `<QVR>\ironwail.exe,0`, InstallLocation, InstallDate, EstimatedSize (the recorded files, KiB, DWORD), URLInfoAbout (the GitHub repository), NoModify/NoRepair 1, and UninstallString / QuietUninstallString `"<QVR>\setup\QuakeVR-Setup.exe" --uninstall [--quiet] --target "<QVR>"`. Every install and update copies Setup into `<QVR>\setup\` (the published single exe, or a development build's exe, DLLs and JSON files; `installer-settings.json` too), staged, hashed and recorded in `install.json` (component `setup`) like the package: an update run from that copy keeps it as it is, the uninstall removes it. `--uninstall` from the copy restarts from a copy in `%TEMP%` (a running exe cannot be deleted; that copy is left to Windows' temp clean-up), then removes exactly what `install.json` lists, the entry (only when its InstallLocation is this install) and the folder once empty: `--quiet` with no window (`%TEMP%\QuakeVR-Setup-uninstall.log`), otherwise the window opens on the Remove dialogs and closes when it is done. The Welcome page's Remove removes the entry too. The registry is written only through `IRegistryWriter`: `WindowsRegistryWriter` (HKCU only, HKLM refused) for a real install; `--registry-file <json>` (`JsonFileRegistry`, a made-up root in a file) for tests, and none for `--shortcuts-dir` test installs and the screenshot harness. Verified end to end into scratch (never the real registry: `reg query` finds no key): the window's install (`--screenshots --package --target --shortcuts-dir --registry-file`) wrote the entry to the JSON root and the copy into `setup\`; that copy's `--uninstall --quiet` restarted from `%TEMP%` (pointed at scratch), removed 10 files and 5 shortcuts, the entry and the folder |
| First-start relight | `FirstStartRelight`; Quake/vr/vr_relight.cpp `firstStart` | The relight ticked (decision 4/5): the install writes `<QVR>\quakevr\relight_on_first_start.txt` (unticked: none, and one left by an earlier install is removed; uninstall removes it; not in `install.json`'s files). quake.rc's `vr_startgame` (the end of every start, whichever way the game was started: Play, a shortcut, Steam, flat or VR) finds it in `com_gamedir`, removes it and queues `vr_relight_batch everything` ahead of the hub or calibration map, once (a marker that cannot be removed is left and nothing started, so it never runs at every start). The installer's Play passes no relight argument any more. Verified headless: the marker consumed and the batch queued ("Relight: a first start after the installer"), no marker no batch; the old one-off `+vr_relight_batch everything` was verified relighting 83 maps with the packaged `light.exe` |

Verified on the author's PC (2026-10-06): detection finds Steam Quake (original 1.06 + rerelease, music in
`rerelease\id1\music`, the older zip install), hipnotic/rogue/dopa ready, mg1/mg3 detected not supported, VDXR active,
SteamVR, VC++ 14.51. A real `package-quakevr.ps1` package (1,684 files, 260 MB, with `manifest.json`) installed into a
scratch folder in 3.4 s with every hash checked; the game started from its desktop shortcut (hidden, mock headset)
found the same campaigns; the Steam Quake folder's 3,626 files were unchanged (sizes and times); uninstall left only the
game's own `qconsole.log`/`history.txt`. The window's download path ran against a local server (a dead feed, then a
dead mirror, then the file).

Next phases:

1. **Ship it:** `dotnet publish` single-file self-contained exe (its copy in `<QVR>\setup` and the Apps & Features entry: done, above); a "Quake VR
   Setup" Start menu shortcut to that copy (maintenance mode = the Welcome page's Update / Remove); publish `latest.json` with each release on both hosts.
2. **Prerequisites:** done (VC++ runtime install, above).
3. **Optional components:** sizes from the feed before downloading.
4. **Updates:** check `latest.json` at start and on the Welcome page; adopt an older zip install (move its config, saves,
   notes and `relit\` with the player's consent, never deleting the old ones).
5. **Later:** the in-game first-run graphics preset; a "collect a bug report" button; SteamVR `.vrmanifest`; Xbox app
   detection; a code-signing certificate if SmartScreen becomes a problem.

The look was verified with off-screen renders of every page (`QuakeVR-Setup --screenshots <dir> --offline --extras`), with
the author's Steam Quake's textures and with the generated ones (`--no-quake-look`); `--extras` also writes a strip of
flame frames, a sheet of the textures the skin can pick from, and `report.txt` (what was loaded, per-frame costs, the
sound engine's check, the live window's frame rate and CPU). The local-package path ran end to end: a package beside a
copied `QuakeVR-Setup.exe`, `--offline`, into scratch folders (files, `install.json`, five shortcuts).

Open questions for the author: whether to show the installer's own version check before the Welcome page.


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
| `vr_menu_positions` | menu cursors | empty |
| `tips_seen.txt` (a file in the game folder, not a setting; was `vr_tips_seen`) | tips seen | absent |
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

### Gameplay and look: promoted (his own values)

Promoted on 2026-10-07: his values are the shipped defaults (`vr_cfg_version` 95, `vr_wofs_version` 35,
`vr_props_version` 58); a config still holding the old default takes the new one, a changed value is kept.
`vr_dummy_gib` was not part of it (still a question).

| Setting | His = now shipped | Was |
|---|---|---|
| `vr_fire_particles_alpha` / `_count` / `_origin` / `_size` | 1 / 8 / 0.2 / 2.5 | 0.55 / 6 / 0.25 / 2 |
| `vr_decap_pop_always_range` / `_never_range` | 2 / 12 | 3 / 15 |
| `vr_decap_pop_thrown_light_chance` | 0.25 | 0 |
| `vr_ragdoll_grab_reach` / `vr_ragdoll_hand_stick` | 2 / 2 | 6 / 12 |
| `vr_dummy_gib` | 1 | 0 (not promoted) |
| `vr_messages_hologram_height` | 10 | 5 |
| `vr_retro_all_average` / `_block` / `_dither` / `_fade` / `_palette` (the All Categories panel: now the shipped look every kind has had since config 89) | 0 / 0.5 / 0.5 / -1 / 1 | 1 / 1 / 0 / 1 / 0 |
| `vr_wofs_torch_out_18` / `vr_wofs_torch_up_18` (the grappling hook's flashlight) | -0.035 / 0.075 | 0 / 0 |
| Masses (kg), `vr_prop_mass_NN` | h_grem 9, gib2 15, gib3 10, h_guard 8, h_dog 9, h_mega 9, h_knight 8, h_hellkn 11, h_ogre 15, h_shal 10, h_shams 65, h_demon 18 (gib1 8, h_player 5, h_wizard 10, h_zombie 8, h_scourg 50 unchanged) | 18, 20, 12, 10, 12, 16, 12, 17, 30, 12, 70, 28 |
