# Quake VR: Unleashed installer

A Windows installer for Quake VR: Unleashed, written in C# with WPF (.NET 9). It finds the player's Quake and its expansions,
checks their VR setup, installs Quake VR into its own folder without touching Quake's files, makes the shortcuts, and
can update or remove what it installed. The design, the research behind it and the author's decisions are in
[docs/vr-port/INSTALLER.md](../docs/vr-port/INSTALLER.md); section 13 there describes this code.

## Layout

| Folder | What |
|---|---|
| `src/QuakeVR.Installer.Core` | Everything that is not UI, testable without a window: detection (`Detection/`), packages, manifests, local packages, install/update/uninstall and downloads (`Packaging/`), shortcuts (`Shortcuts/`), the registry and special folders behind an interface (`Platform/`), Quake's file formats for the skin (`Assets/`: PAK, palette, WAD2, BSP textures, WAV), the sounds' mixer (`Audio/`, no device) |
| `src/QuakeVR.Installer` | The wizard window (`QuakeVR-Setup.exe`): Welcome, Statement, Your PC, Options, Install, Play, Thanks. `ViewModels/MainViewModel.cs` holds the state; `Views/` the pages; `Themes/Theme.xaml` the look; `Skin/` the textures (from the player's Quake, or generated), flames, lava and the frame clock; `Audio/` the sounds and the wave-out device; `Fonts/` and `Assets/` the embedded fonts (SIL OFL), logos, icon, Ko-fi logo (the sidebar's Ko-fi and Discord buttons) and the author's photo (`author.jpg`, the Statement page); the window is 1200 x 800 (`MainWindow.DefaultWidth`/`DefaultHeight`), never larger than the screen's work area (the pages and the sidebar then scroll); `ScreenshotHarness.cs` renders the pages to PNG |
| `src/QuakeVR.Installer.Cli` | `qvr-setup.exe`: the core from a console (detection report, install/uninstall/verify from a local package, manifests, downloads) |
| `tests/QuakeVR.Installer.SelfTest` | The core's tests, as a console program (see "Tests") |

One NuGet package, `Microsoft.ML.OnnxRuntime.DirectML` 1.22.0 (MIT; Core's reliefs step, "Reliefs for Dwell and
Quoth" below: its native `onnxruntime.dll` and `DirectML.dll` go inside the single-file exe, about 10 MB of it; its
import library is dropped from the publish, `Directory.Build.props`). Everything else is in the .NET 9 SDK (WPF,
`System.Text.Json`, `System.IO.Compression`, `HttpClient`, COM interop for `IShellLink`, winmm's wave-out for the sounds).

## The look, and what is shipped

"Quake VR: Unleashed" in a grimy Quake skin (INSTALLER.md, section 13, "Skin", "Animation", "Sounds"). **None of id
Software's data is in this folder or in the installer**: the Quake textures and sounds are read while the installer runs
from the player's own Quake (found by a quiet detection when the window opens); before that, or without Quake, the skin
uses textures and sounds made in code. What is embedded:

| File | What | Licence |
|---|---|---|
| `Fonts/GrenzeGotisch-Bold.ttf` | Grenze Gotisch Bold (titles), from Google Fonts (a static instance of the variable font) | SIL OFL 1.1, `Fonts/OFL-GrenzeGotisch.txt` |
| `Fonts/Barlow-*.ttf`, `Fonts/BarlowSemiCondensed-*.ttf` | Barlow Regular/Medium/SemiBold/Bold (text), Barlow Semi Condensed SemiBold/Bold (labels, buttons), from the google/fonts repository | SIL OFL 1.1, `Fonts/OFL-Barlow.txt` |
| `Assets/logo_wide.png`, `logo_square.png`, `app.ico` | The official Quake VR: Unleashed logos (Vittorio's; the originals are `docs/images/*.webp`). `app.ico` (the exe's and the window's icon) is the icon logo, `docs/images/quakevr-unleashed-icon.webp` ("QVR:U" under the emblem), made by `Misc/quakevr/make_exe_icon.py` like the game's `Windows/QuakeVR.ico` | the project's |
| `Assets/kofi_symbol.png` | Ko-fi's cup logo, unaltered, from Ko-fi's brand assets (`kofi_brandasset.zip`) | Ko-fi's brand asset, used to link to the author's page |
| `DiscordMark` in `Themes/Theme.xaml` | Discord's mark ("Clyde"), vector path data typed in from Simple Icons' `discord` path (not downloaded; swap in Discord's official asset if wanted), in Discord's blurple #5865F2 | Simple Icons' paths are CC0; the mark is Discord's, used to link to the Quake VR Discord server (`discord.me/quakevr`) |

The fonts' licences are also shown in the window ("credits" in the sidebar).

## Build and run

```
cd Installer
dotnet build QuakeVR.Installer.sln            # warnings are errors (Directory.Build.props)
src\QuakeVR.Installer\bin\Debug\net9.0-windows\QuakeVR-Setup.exe
```

`global.json` pins the SDK to 9.0.305 (or a newer 9.0 feature band). A release build for players:
`dotnet publish src/QuakeVR.Installer -c Release -r win-x64 --self-contained -p:PublishSingleFile=true
-p:IncludeNativeLibrariesForSelfExtract=true -p:EnableCompressionInSingleFile=true` (one unsigned `QuakeVR-Setup.exe`,
about 60 MB, with WPF's native DLLs inside; see INSTALLER.md, "Hosting and SmartScreen").
`Misc\release\make_release.ps1` does this for a release ([RELEASING.md](../docs/vr-port/RELEASING.md)).

### Command line (the window)

| Option | What |
|---|---|
| `--package <zip or folder>` | Install this package (otherwise `QuakeVR.zip`, a `QuakeVR\` folder or another `QuakeVR*.zip` with a `manifest.json` beside the exe, otherwise a download; without one and without a release online, the Options page offers "Use a local package…") |
| `--textures <zip>` | The HD texture pack, already downloaded |
| `--target <dir>` | The install folder (default `%LOCALAPPDATA%\Programs\QuakeVR`) |
| `--shortcuts-dir <dir>` | Shortcuts go to `<dir>\Desktop` and `<dir>\Programs` instead of the real desktop and Start menu (tests) |
| `--feed <url>` | Where `latest.json` is read (repeatable; default: the GitHub Latest release's; a prerelease's Setup reads its own tag's first). Also `installer-settings.json` beside the exe |
| `--downloads <dir>` | Where downloads go (default `%LOCALAPPDATA%\QuakeVR-Installer\downloads`) |
| `--screenshots <dir>` | Render every page to PNG and exit, no window (the Statement page unanswered, mixed and all YES; exit 1 unless its Continue is enabled exactly with YES to both, or unless both Play buttons mute the installer: a 0.3 s fade, then silence, with the game's start recorded instead of run); `fit.txt` (also printed) says which page would scroll, and by how much, at the default size, on 1366x768 at 100%, on 1080p at 150% and in the window at its minimum size; `7-*-150pct*.png` are renders at 150%, `7c-statement-minimum.png` the Statement page at the minimum size (with `--package --target --shortcuts-dir` it runs a real install into those folders first) |
| `--offline` | Never ask the network: the online release counts as unavailable (the "Use a local package" path) |
| `--no-quake-look` | The generated textures and sounds even when Quake is found (screenshots of the fallback) |
| `--reduce-motion` | Animations off, as with Windows' "Animation effects" off |
| `--silent` | No sound |
| `--no-prerequisites` | Never install the VC++ runtime (it is still detected) |
| `--no-prepare` | No preparation run after the install (the game hidden once: its first maps' disk caches and the pending relight, `GamePreparation`; docs/vr-port/INSTALLER.md section 13): the game's first start does that work instead |
| `--uninstall [--quiet]` | Remove the install in `--target` (default: the install this copy of Setup is in, `<QVR>\setup`): the Remove dialogs, or none with `--quiet`. Apps & Features runs this. From the install's own copy it restarts from a copy in `%TEMP%` first |
| `--sandbox <dir>` | A test install kept in `<dir>`: the game in `<dir>\QuakeVR`, shortcuts in `<dir>\_shortcuts`, downloads in `<dir>\_downloads`, no Apps & Features entry, the VC++ runtime only checked (`Misc\release\test_local_release.ps1`). A yellow bar says SANDBOX |
| `QVR_SETUP_FEED` (environment) | Like `--feed` (several separated by `;`); `--feed` wins. Any feed other than the release hosts' shows a yellow TEST FEED bar on every page and `[TEST]` in the title |
| `--registry-file <json>` | The Apps & Features entry goes into this made-up registry root instead of HKCU (tests; installs with `--shortcuts-dir` and the screenshot harness write none) |
| `--apk <file>` | The Quest app for the Quest target (otherwise `QuakeVR.apk` beside the exe, otherwise latest.json's `quest` component) |
| `--adb <adb.exe>` | adb for the Quest target (otherwise found or fetched: see "Quest") |
| `--vcredist-dry-run` | Only log what the VC++ runtime's install would do (download, signature check, `/install /quiet /norestart`); download and run nothing |
| `--extras` | With `--screenshots`: also a strip of flame frames, a sheet of Quake's textures, and `report.txt` (skin, sounds, per-frame costs, the live window's frame rate and CPU) |

### qvr-setup (console)

```
qvr-setup detect                                   # what is on this PC (Quake, expansions, runtimes, VC++)
qvr-setup manifest <package folder> --version <v>  # write manifest.json (package-quakevr.ps1 has its own writer)
qvr-setup install --package dist\QuakeVR.zip --target <dir> --accept-statement [--shortcuts-dir <dir>] [--textures <zip>] [--relight] [--vispatch id1_vis.tgz ...]
                  [--setup-from QuakeVR-Setup.exe] [--registry-file <json> | --register]   # Setup's copy in <dir>\setup; the Apps & Features entry
qvr-setup verify --target <dir>
qvr-setup install --feed http://127.0.0.1:8517/latest.json --sandbox <dir> --accept-statement [--hd] [--relight]   # the window's download path
qvr-setup install --package dist\QuakeVR.zip --hd [--no-feed] --dry-run   # what would be downloaded: --hd is the feed's hdtextures, else the built-in pinned pack
qvr-setup install ... --ad [--ad-zip <zip>] [--no-stores]   # Arcane Dimensions (experimental, opt-in): see "Arcane Dimensions" below
qvr-setup serve --dir out\release\<v>-local\assets --port 8517 [--drop-after <bytes>]   # a local release over HTTP (Range), 127.0.0.1 only
qvr-setup settings --target <dir> [--set <cvar>=<value>]...   # the settings page without its window (docs/vr-port/INSTALLER.md, section 14)
qvr-setup statement                                # the author's statement on AI usage (install exits 3 without --accept-statement)
qvr-setup vcredist [--check <vc_redist.x64.exe>] [--dry-run [--assume-missing] [--file <exe>]]   # the VC++ runtime (without --dry-run: installs it, one UAC prompt)
qvr-setup uninstall --target <dir> [--remove-textures] [--registry-file <json> | --register]
qvr-setup quest --target <install dir> --apk QuakeVR.apk [--quake <dir>] [--serial <s>] [--adb <adb.exe> | --no-fetch] [--dry-run]   # the Quest target (see "Quest")
qvr-setup quest-devices                            # what adb sees (exit 0: one Quest ready)
qvr-setup download --url <u> [--url <mirror>] --out <file> --size <n> --sha256 <hex>
qvr-setup assets --game <Quake>\id1 [--map maps/start.bsp] [--prefix sound/misc]   # what the skin can read (nothing written)
```

The console never writes the real desktop or Start menu: shortcuts only with `--shortcuts-dir`; and the real registry only with `--register`.

## Tests

```
dotnet run --project tests/QuakeVR.Installer.SelfTest -- <scratch folder> [name filter]
```

Tests: the Statement page's answers (all 9 mixes of unanswered/YES/NO: Continue only with YES to both, no way back to unanswered), VDF parsing, a fake Steam (libraries, app manifests), GOG and Epic, id1 kinds, the engine's resource checks
and pack states (ported from `Quake/vr/vr_gamedir.cpp`), expansion roots and priorities, the expansions' readiness labels (checked against the engine's `campaigns[]` and `soloOnly()` in `Quake/vr/vr_gamedir.cpp`), OpenXR/Virtual Desktop/VC++
detection (the registry key and the three DLLs the game imports), the VC++ redistributable's install (the real
Authenticode check on files already here; the download, signature and version checks, exit codes and dry run with a local
server and a fake runner: nothing is ever run elevated), the first-start relight's marker, the Apps & Features entry (in a made-up registry root: values, update from Setup's
own copy, removed by the uninstall only when it is this install's) and Setup's copy in the install, launch arguments and shortcut plans, `.lnk` round trips, manifest safety (paths outside the folder
refused), install target rules, install/verify/update/uninstall end to end (zip and folder packages, player files kept,
the Quake folder unchanged), damaged and cancelled installs, HD textures for owned packs only, downloads (mirror
fall-back, a wrong file skipped, resume with HTTP Range, pinned SHA-256) and the release feed, all against a local
HTTP server; the skin's readers (pak search order, palette, WAD2 pictures and CONCHARS, a BSP's textures, 8/16-bit WAV,
junk refused) on made-up files, the sounds' mixer (no step in the output: voice fades, stolen voices, the loop's seam, the mute and Play's 0.3 s fade, a smooth limiter), local packages (found beside the installer, checked for a manifest, texture packs
skipped), and VisPatch's data (made-up `.tgz` archives from a local server: mirror order, pinned hash, safe unpacking,
installed where the game looks, kept by updates, removed by uninstall). The machine is a `MemorySystemProbe`: no test reads the real registry or writes outside the scratch folder.
Arcane Dimensions: the pin (built in = `support_assets.json`'s `thirdParty.ad` = the engine's pak2 pin in
`Quake/vr/vr_ad_data.inc`; the real 1.80p1 on this PC checked read-only when present), a copy found in the owned roots
(1.80p1 used, an older or incomplete one offers the download, the game's root order), a sandboxed install from a local
server with a small fake zip (size and SHA-256 pinned, a wrong one refused, unpacked unmodified into `ad\`, recorded,
kept by updates, restored by a repair, the player's own file never overwritten, removed by the uninstall), and a failed
or cancelled download that leaves the install as it was.
xUnit/MSTest were not used because their NuGet packages are not available offline here; moving the tests to xUnit later
is mechanical.

## Quest (install to a Meta Quest)

The Welcome page's **Install on** card: *This PC (PC VR and flatscreen)* (as before) and *Meta Quest (standalone)*;
both at once works, and at least one is needed. Quest is another target of the same install (`Core/Quest/`):

- **The PC steps first, unchanged.** With the Quest alone, they run into a staging folder,
  `%LOCALAPPDATA%\QuakeVR-Installer\quest-files` (`<sandbox>\quest-files`; `QuestTarget.StagingDir`), without shortcuts,
  the Apps & Features entry, Setup's copy, the VC++ runtime or the preparation run; it is kept, so the next run updates it
  like any install and copies only what changed. Package, HD textures, VisPatch, AD, Dwell, Quoth and the reliefs are the
  same code paths as the PC's.
- **The files** (`QuestFiles.Plan`): the install record's files less the PC's programs (root files, `setup/`,
  `quakevr/tools/`, `quakevr/xr_layers/`, `.exe/.dll/.pdb/.bat`), plus what the PC game reads in place from the
  player's Quake: `id1/pak*.pak` and the music (lower case: Android's names are case-sensitive; from where the
  detection finds it, the rerelease's first, into `id1/music/`, where bgmusic.c's `music/trackNN.<ext>` finds it; only
  `QuestFiles.MusicFormats`: WAV and Ogg, the Quest build's codecs (stb_vorbis), so the stores' Ogg soundtracks go
  too; no MP3/FLAC there), the official expansions `ExpansionDetector` finds ready (their paks and the engine's resource list),
  AD/Dwell/Quoth copies found by their `FindExisting` when Setup did not install its own, and the maps the PC game
  relit (`quakevr/relit_custom/<game>/maps/*.bsp/.lit/.lux/.relight`, the copies with their note, only for the games
  the headset gets, `<game>` as the headset names it: the Quest cannot relight, and VR_ModelFile loads these while the
  note names the same map). Device folder: `/sdcard/Android/data/com.vittorioromeo.quakevr/files` (the game's base
  dir; Android/README.md).
- **The Done page** (Quest ticked): a META QUEST card, `QuestTarget.Outcome`: installed on the device (the app
  installed or up to date, the files copied and already there, "Library > Unknown Sources"), or not (the last problem
  or skipped, and "run Setup again"). Quest alone: no PC Play buttons, Steam card or PC notes (relight, Virtual
  Desktop); the title says whether it is on the Quest.
- **adb** (`AdbLocator`): `--adb`, `<downloads>\platform-tools\adb.exe`, PATH, `ANDROID_HOME`/`ANDROID_SDK_ROOT`,
  `%LOCALAPPDATA%\Android\Sdk`; else Google's `platform-tools-latest-windows.zip` from `dl.google.com` (official URL only;
  no pinned hash, Google publishes the latest under one name: HTTPS is the trust, adb.exe's Authenticode signature is
  logged), unpacked (adb.exe and its two DLLs) into the downloads folder. Offline: no fetch, a message.
- **The headset** (`QuestDeviceStatus`, `adb devices -l`): none, unauthorized ("Allow USB debugging" not accepted),
  offline, several (unplug the others; the CLI's `--serial` picks one), ready. Not ready: the Install page's Quest card
  shows the message and the steps (`QuestTarget.SetupSteps`: developer mode, a data cable, the prompt) with **Check
  again** and **Skip the Quest**.
- **The install** (`QuestInstaller`): `install -r` (keeps the app's files folder; skipped when `pm path`'s APK has the
  same MD5; another signature, `INSTALL_FAILED_UPDATE_INCOMPATIBLE`, is explained, never uninstalled: that would delete the
  saves), then sizes (`find -exec stat`) and MD5s (`md5sum`, only files of the same size) against the headset's, the
  free space (`df`) against what is copied, one tar stream of the missing or changed files into `exec-in tar -x` (one
  stream, as Android/README.md does it), `chmod` of those files and their folders (what adb's shell writes is the
  shell's), and every copied file's size checked afterwards (a disk that fills up mid-copy is reported as such).
  **Nothing it sends removes anything**; only shipped paths are written, so saves and configs on the headset stay.
  Then: "start it from Library > Unknown Sources".
- **The APK** is a release asset, not bundled: `make_release.py --apk build-android/apk/QuakeVR.apk` adds it and
  latest.json's `quest` component; the window downloads it only when the Quest is ticked (every PC-only player would
  otherwise download 22 MB more in Setup), and it stays the same version as the package from the same feed. A
  `QuakeVR.apk` beside the exe or `--apk` is used first (local releases, tests).

Self-tests `quest: ...` (`tests/.../QuestTests.cs`, `FakeAdb.cs`: a fake adb that records every command and keeps the
headset's files folder in a scratch folder; it reads the tar stream and answers getprop, pm path, md5sum, mkdir,
find/stat, chmod, df): the device states, adb's discovery and fetch from a local server, the file plan, the whole
procedure after a PC install (the PC install and Quake unchanged, the player's save and config on the headset kept),
installing again (nothing copied, the APK not reinstalled; a changed file and APK copied), no device / unauthorized /
several (only `adb devices` sent), a full headset (refused before copying) and one filling mid-copy, a failed APK
install, and the Quest-only staging folder. Every test also checks that no command removes anything.

The real headset: `qvr-setup install --package <QuakeVR.zip> --sandbox <dir> --quake <Quake> --accept-statement`, then
`qvr-setup quest --sandbox <dir> --apk <QuakeVR.apk>` (or the window with `--package`, `--apk`, Quest ticked).

## Arcane Dimensions (experimental, opt-in)

The Options page's "Arcane Dimensions (experimental) — free, by sock and team" box is **off by default**; its size is
shown first (306 MB download, about 792 MB on disk), with a link to AD's readme and home page. Ticked, AD's release
`ad_v1_80p1final.zip` (pinned: 320,914,417 bytes, SHA-256 `7ad993da...a7d`, `BuiltInComponents.ArcaneDimensions()`;
latest.json's `components.ad` may override it) is downloaded **from its authors' hosts only** (Quaddicted's
content-addressed copy, Quaddicted's filebase, simonoc.com, born-robotic.net; Quake VR never hosts it), checked (size,
then SHA-256: anything else is refused) and unpacked as it is into `<install>\ad` (its readme stays with it), recorded
in install.json as component `ad`. It runs last, after the game is installed and prepared: a failed or cancelled AD
download never fails the install (the Done page says so; Setup's Update screen then offers "Add Arcane Dimensions").
Updates keep it, a repair restores a damaged file, the uninstall removes it. A copy of 1.80 patch 1 already in an owned
Quake root (Steam/GOG/Epic, the Quake folder; validated by pak2.pak's header and directory hash, as the engine does) is
used where it is: the row says "Found in <path>: will be used" and the box is disabled; an older or incomplete copy is
reported and the download offered (the copy in Quake VR's folder then wins: it is the game's highest-priority root).
Core: `Installer/src/QuakeVR.Installer.Core/Packaging/ArcaneDimensions.cs`.

## Dwell (recommended, on by default)

The same third-party path as Arcane Dimensions (`Packaging/ModComponent.cs`: one spec per mod, `ArcaneDimensions.Spec`
and `Dwell.Spec`; checks, staging, install.json records, plan and the whole optional step are shared, so a fix in one is
a fix in both). The Options page's "Dwell (recommended) — free, by Fairweather and the Dwell team: two episodes, 24
maps, soundtrack included" box shows its size first (367 MB download, about 762 MB on disk, in the installer's usual
units: 384,629,995 bytes and 799,533,222 bytes) and is **ticked** (`Dwell.OnByDefault`, true since 1.1.1: Dwell's maps
play by Copper's rules, docs/vr-port/DWELL.md sections 7 and 9.2). `dwellv2p2.zip` (pinned: 384,629,995
bytes, SHA-256 `15bbc5dc...d11d`, `BuiltInComponents.Dwell()`; latest.json's `components.dwell` may override it) comes
from Quaddicted only (the content-addressed copy, then the filebase), is checked (size, then SHA-256) and unpacked
unmodified into `<install>\dwell` (the zip's `dwellv2p2/` folder is `dwell/`; its two READMEs go inside it), recorded as
component `dwell`. One change on the way in: `music/Track107.ogg` is **renamed** to `track107.ogg` (the engine looks
tracks up in lower case; a case-sensitive file system, Linux or the Quest, would not find the capital T; a rename, not a
copy, so no 2 MB duplicate; recorded under the new name, so verify and uninstall agree). "I already have it": a `dwell`
or `dwellv2p2` folder in an owned Quake root whose `progs.dat` has v2.2's SHA-256 (`Dwell.ProgsSha256`) and a pak0.pak
is used where it is; another progs.dat is reported as an older version, not used; no progs.dat is incomplete. CLI:
`qvr-setup install --dwell [--dwell-zip <zip>] [--no-stores]` (by default: `OnByDefault`; `--no-dwell`
skips it). Self-tests: `dwell: ...` (the pin and its three copies, existing copies, a sandboxed install of a
Dwell-shaped zip from a local server, and the real zip from `qvr-kit/downloads/dwell/`, skipped when absent: 447 files,
progs.dat's hash, Track107). The game plays it as campaign 7, `dwell` (Select Campaign > Dwell; `vr_dwell_status`).

## Quoth (experimental, needs Arcane Dimensions)

The same shared path (`Quoth.Spec`). The Options page's "Quoth (experimental) — free, by Kell, Necros and Preach" row
sits under AD's: **ticked** while Arcane Dimensions is chosen (ticked, already in the install, or a usable copy found:
`AdChosen`), otherwise **off and greyed** with the reason (`Quoth.NeedsAdText`): Quake VR plays Quoth's maps only by AD's
rules (docs/vr-port/QUOTH.md). Its licence lets the zip be passed on only unmodified, so it is never hosted by us:
`quoth2pt2full_2.zip` (54,792,367 bytes, SHA-256 `9fce1af3...1139`, `BuiltInComponents.Quoth()`; latest.json's
`components.quoth` may override it) from Quaddicted (content-addressed, then the filebase), its six files unpacked as
they are into `<install>\quoth` (136 MB), component `quoth`. Its step runs after AD's and only where AD now is (AD's
failed download skips it, said on the Done page). "I already have it": a `quoth` folder in an owned root whose pak2.pak
is 2.2's (`Quoth.Pak2Sha256`, the engine's `VR_QUOTH_PIN_SHA`) with pak0 and pak1. CLI: `--quoth [--quoth-zip <zip>]`,
on by default (`--no-quoth`), skipped with the reason without AD. Self-tests: `quoth: ...` (the pin, existing copies,
the real zip from `qvr-kit/downloads/modsurvey/quoth/`: install, repair, update, uninstall).

## Reliefs for Dwell and Quoth

AD's surface relief comes from a bundle made on the developer's machine (`Misc/quakevr/pbr_generate.py`,
docs/vr-port/PBR_PIPELINE.md) and shipped in `quakevr/textures/pbr`. Dwell's and Quoth's cannot be shipped (made from
their textures: DWELL.md section 1, QUOTH.md), so Setup makes them on the player's PC from his own copies
(`Core/Reliefs/ReliefMaker.cs`; DWELL.md 9.9, option A). The Options page's "Make reliefs for Dwell and Quoth" row sits
under Dwell's: **ticked** wherever Dwell or Quoth will be there (ticked, found, or in the install: `ReliefsPossible`),
greyed otherwise. Its step runs last, after Quoth's: the Dwell and Quoth folders the game plays (the install's own, else
the found copies) are read like the generator reads AD (their paks and loose maps, the miptex through Quake's palette,
`textures/` replacements PNG and TGA as the engine picks them; JPG replacements are not decoded and get none), each
drawn texture hashed as the engine hashes it, and those without a companion (not stock, `quakevr/textures/pbr/stock.txt`;
not in quakevr's bundle; not made before) go through PBRify's two CC0 models: `Reliefs/Models/pbrify_normal.onnx` and
`pbrify_height.onnx` (1.6 MB each, embedded; exported once by `Misc/quakevr/pbr_export_onnx.py`), run by onnxruntime
with DirectML, the CPU when DirectML is not there or fails. The packing is the generator's (256-pixel tiles with 48
pixels of wrapped context, cutout and glow colours filled, calibrated height within 0.5, green orientation, flat holes,
lighting-only animation frames sharing their first frame's relief). Output: `<install>\reliefs\textures\pbr\<hash>_norm.png`
and its `provenance.txt` (the engine searches `<last basedir>\reliefs` below every game folder: vr_pbr.cpp,
`VR_AddReliefsPath`). Progress in the status line; Cancel skips the rest (the next Setup run makes only the missing
ones); a failure is a note on the Done page, never a failed install. The uninstaller removes the folder. Measured: Dwell
and Quoth, 2323 textures, 1768 made in 115 s on an RTX 4090 (DirectML), 82 MB. Parity: 60 AD textures against
`pbr_generate.py` (PyTorch): mean texel difference 0.0001, worst 1. CLI: `qvr-setup reliefs --target <folder>
[--quake <Quake>] [--mods "<dir>;<dir>"] [--palette-dirs ...] [--texture-roots ...] [--quakevr <dir>] [--cpu] [--limit N]`
(by itself it finds the folder's `dwell` or `dwellv2p2` and `quoth`, Quake's palette in them, the folder's `id1` or the
install record's Quake; it prints how many reliefs were there and ends with `reliefs: N made, M already existed`).

**Make reliefs only** (`Core/Reliefs/ReliefTarget.cs`, shared by the window and the CLI): the step alone on a folder
that already plays Quake VR, nothing else installed or changed (only `<folder>\reliefs` is written). Offered on the
Welcome page as a card with a "Make reliefs only" button when a folder has Dwell (`dwell`, else `dwellv2p2`) and/or
Quoth (`quoth`) and is either Setup's own install (install.json: the folder given, Setup's copy, Apps & Features, the
default folder) or a Quake folder with a `quakevr` folder in it (a hand or developer install, found by the quiet Quake
detection or given with `--target`); the engine reads `<last basedir>\reliefs`, so that folder is where they go. The run
skips the statement, the package and the options: the Install page's log, then the Done page ("Reliefs for Dwell and
Quoth are ready") with how many were made and how many already existed. A folder that already has them makes 0.
Self-tests: `reliefs: ...` (images and packing; a made-up texture set end to end on the CPU: made, skipped, frames
shared, holes flat, kept, made again when missing, removed by uninstall) and `reliefs-only: ...` (a made-up developer
folder: offered, 4 made, nothing outside `reliefs` changed, a second run 0 made and 4 existing; Setup's install offered,
`dwell` before `dwellv2p2`; folders without `quakevr` or the mods not). The screenshot harness drives the window's
flow on a made-up folder (`1g-welcome-reliefs-only.png`, `5c-done-reliefs-only.png`; exit 1 unless offered, ended on
Done with its count, and nothing else written) and prints whether this PC's folders are offered.

## Packages

`Windows/package-quakevr.ps1` writes `manifest.json` into the package (`Windows/write-package-manifest.ps1`): the
version and each file's path, size and SHA-256. The installer installs exactly those files and refuses a package whose
files do not match (`--unverified` on the console accepts one without a manifest, for development).

## Releases

`python Misc/quakevr/make_release.py --package dist/QuakeVR --setup <QuakeVR-Setup.exe> [--textures <zip>] [--asset <file>]`
makes a release from a package: the zip, the assets, `latest.json` in the format `ReleaseFeed` reads, and `PUBLISH.txt`
with the `gh release create` command (it publishes nothing). The steps are in
docs/vr-port/INSTALLER.md, "Publishing a release". `Misc\release\make_release.ps1` runs it as part of a whole release
(docs/vr-port/RELEASING.md), and checks its `latest.json` with `qvr-setup feed --file latest.json --assets <folder>`.
