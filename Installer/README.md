# Quake VR: Unleashed installer

A Windows installer for Quake VR: Unleashed, written in C# with WPF (.NET 9). It finds the player's Quake and its expansions,
checks their VR setup, installs Quake VR into its own folder without touching Quake's files, makes the shortcuts, and
can update or remove what it installed. The design, the research behind it and the author's decisions are in
[docs/vr-port/INSTALLER.md](../docs/vr-port/INSTALLER.md); section 13 there describes this code.

## Layout

| Folder | What |
|---|---|
| `src/QuakeVR.Installer.Core` | Everything that is not UI, testable without a window: detection (`Detection/`), packages, manifests, local packages, install/update/uninstall and downloads (`Packaging/`), shortcuts (`Shortcuts/`), the registry and special folders behind an interface (`Platform/`), Quake's file formats for the skin (`Assets/`: PAK, palette, WAD2, BSP textures, WAV), the sounds' mixer (`Audio/`, no device) |
| `src/QuakeVR.Installer` | The wizard window (`QuakeVR-Setup.exe`): Welcome, Statement, Your PC, Options, Install, Play, Thanks. `ViewModels/MainViewModel.cs` holds the state; `Views/` the pages; `Themes/Theme.xaml` the look; `Skin/` the textures (from the player's Quake, or generated), flames, lava and the frame clock; `Audio/` the sounds and the wave-out device; `Fonts/` and `Assets/` the embedded fonts (SIL OFL), logos, icon and Ko-fi logo (the sidebar's Ko-fi and Discord buttons); the window is 1200 x 800 (`MainWindow.DefaultWidth`/`DefaultHeight`), never larger than the screen's work area (the pages and the sidebar then scroll); `ScreenshotHarness.cs` renders the pages to PNG |
| `src/QuakeVR.Installer.Cli` | `qvr-setup.exe`: the core from a console (detection report, install/uninstall/verify from a local package, manifests, downloads) |
| `tests/QuakeVR.Installer.SelfTest` | The core's tests, as a console program (see "Tests") |

No NuGet packages: everything is in the .NET 9 SDK (WPF, `System.Text.Json`, `System.IO.Compression`, `HttpClient`,
COM interop for `IShellLink`, winmm's wave-out for the sounds).

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
| `--feed <url>` | Where `latest.json` is read (repeatable; default: the GitHub release's, the only feed). Also `installer-settings.json` beside the exe |
| `--downloads <dir>` | Where downloads go (default `%LOCALAPPDATA%\QuakeVR-Installer\downloads`) |
| `--screenshots <dir>` | Render every page to PNG and exit, no window (the Statement page unanswered, mixed and all YES; exit 1 unless its Continue is enabled exactly with YES to all four, or unless both Play buttons mute the installer: a 0.3 s fade, then silence, with the game's start recorded instead of run); `fit.txt` (also printed) says which page would scroll, and by how much, at the default size, on 1366x768 at 100% and on 1080p at 150%; `7-*-150pct*.png` are renders at 150% (with `--package --target --shortcuts-dir` it runs a real install into those folders first) |
| `--offline` | Never ask the network: the online release counts as unavailable (the "Use a local package" path) |
| `--no-quake-look` | The generated textures and sounds even when Quake is found (screenshots of the fallback) |
| `--reduce-motion` | Animations off, as with Windows' "Animation effects" off |
| `--silent` | No sound |
| `--no-prerequisites` | Never install the VC++ runtime (it is still detected) |
| `--uninstall [--quiet]` | Remove the install in `--target` (default: the install this copy of Setup is in, `<QVR>\setup`): the Remove dialogs, or none with `--quiet`. Apps & Features runs this. From the install's own copy it restarts from a copy in `%TEMP%` first |
| `--sandbox <dir>` | A test install kept in `<dir>`: the game in `<dir>\QuakeVR`, shortcuts in `<dir>\_shortcuts`, downloads in `<dir>\_downloads`, no Apps & Features entry, the VC++ runtime only checked (`Misc\release\test_local_release.ps1`). A yellow bar says SANDBOX |
| `QVR_SETUP_FEED` (environment) | Like `--feed` (several separated by `;`); `--feed` wins. Any feed other than the release hosts' shows a yellow TEST FEED bar on every page and `[TEST]` in the title |
| `--registry-file <json>` | The Apps & Features entry goes into this made-up registry root instead of HKCU (tests; installs with `--shortcuts-dir` and the screenshot harness write none) |
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
qvr-setup serve --dir out\release\<v>-local\assets --port 8517 [--drop-after <bytes>]   # a local release over HTTP (Range), 127.0.0.1 only
qvr-setup statement                                # the author's statement on AI usage (install exits 3 without --accept-statement)
qvr-setup vcredist [--check <vc_redist.x64.exe>] [--dry-run [--assume-missing] [--file <exe>]]   # the VC++ runtime (without --dry-run: installs it, one UAC prompt)
qvr-setup uninstall --target <dir> [--remove-textures] [--registry-file <json> | --register]
qvr-setup download --url <u> [--url <mirror>] --out <file> --size <n> --sha256 <hex>
qvr-setup assets --game <Quake>\id1 [--map maps/start.bsp] [--prefix sound/misc]   # what the skin can read (nothing written)
```

The console never writes the real desktop or Start menu: shortcuts only with `--shortcuts-dir`; and the real registry only with `--register`.

## Tests

```
dotnet run --project tests/QuakeVR.Installer.SelfTest -- <scratch folder> [name filter]
```

Tests: the Statement page's answers (all 81 mixes of unanswered/YES/NO: Continue only with YES to all four, no way back to unanswered), VDF parsing, a fake Steam (libraries, app manifests), GOG and Epic, id1 kinds, the engine's resource checks
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
xUnit/MSTest were not used because their NuGet packages are not available offline here; moving the tests to xUnit later
is mechanical.

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
