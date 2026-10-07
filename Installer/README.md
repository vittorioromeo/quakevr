# Quake VR: Unleashed installer

A Windows installer for Quake VR: Unleashed, written in C# with WPF (.NET 9). It finds the player's Quake and its expansions,
checks their VR setup, installs Quake VR into its own folder without touching Quake's files, makes the shortcuts, and
can update or remove what it installed. The design, the research behind it and the author's decisions are in
[docs/vr-port/INSTALLER.md](../docs/vr-port/INSTALLER.md); section 13 there describes this code.

## Layout

| Folder | What |
|---|---|
| `src/QuakeVR.Installer.Core` | Everything that is not UI, testable without a window: detection (`Detection/`), packages, manifests, local packages, install/update/uninstall and downloads (`Packaging/`), shortcuts (`Shortcuts/`), the registry and special folders behind an interface (`Platform/`), Quake's file formats for the skin (`Assets/`: PAK, palette, WAD2, BSP textures, WAV) |
| `src/QuakeVR.Installer` | The wizard window (`QuakeVR-Setup.exe`): Welcome, Your PC, Options, Install, Play, Thanks. `ViewModels/MainViewModel.cs` holds the state; `Views/` the pages; `Themes/Theme.xaml` the look; `Skin/` the textures (from the player's Quake, or generated), flames, lava and the frame clock; `Audio/` the sounds and their mixer; `Fonts/` and `Assets/` the embedded fonts (SIL OFL), logos, icon and Ko-fi logo; `ScreenshotHarness.cs` renders the pages to PNG |
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

The fonts' licences are also shown in the window ("credits" in the sidebar).

## Build and run

```
cd Installer
dotnet build QuakeVR.Installer.sln            # warnings are errors (Directory.Build.props)
src\QuakeVR.Installer\bin\Debug\net9.0-windows\QuakeVR-Setup.exe
```

`global.json` pins the SDK to 9.0.305 (or a newer 9.0 feature band). A release build for players:
`dotnet publish src/QuakeVR.Installer -c Release -r win-x64 --self-contained -p:PublishSingleFile=true` (phase 2:
one unsigned `QuakeVR-Setup.exe`; see INSTALLER.md, "Hosting and SmartScreen").

### Command line (the window)

| Option | What |
|---|---|
| `--package <zip or folder>` | Install this package (otherwise `QuakeVR.zip`, a `QuakeVR\` folder or another `QuakeVR*.zip` with a `manifest.json` beside the exe, otherwise a download; without one and without a release online, the Options page offers "Use a local package…") |
| `--textures <zip>` | The HD texture pack, already downloaded |
| `--target <dir>` | The install folder (default `%LOCALAPPDATA%\Programs\QuakeVR`) |
| `--shortcuts-dir <dir>` | Shortcuts go to `<dir>\Desktop` and `<dir>\Programs` instead of the real desktop and Start menu (tests) |
| `--feed <url>` | Where `latest.json` is read (repeatable; default: GitHub, then vittorioromeo.com). Also `installer-settings.json` beside the exe |
| `--downloads <dir>` | Where downloads go (default `%LOCALAPPDATA%\QuakeVR-Installer\downloads`) |
| `--screenshots <dir>` | Render every page to PNG and exit, no window (with `--package --target --shortcuts-dir` it runs a real install into those folders first) |
| `--offline` | Never ask the network: the online release counts as unavailable (the "Use a local package" path) |
| `--no-quake-look` | The generated textures and sounds even when Quake is found (screenshots of the fallback) |
| `--reduce-motion` | Animations off, as with Windows' "Animation effects" off |
| `--silent` | No sound |
| `--extras` | With `--screenshots`: also a strip of flame frames, a sheet of Quake's textures, and `report.txt` (skin, sounds, per-frame costs, the live window's frame rate and CPU) |

### qvr-setup (console)

```
qvr-setup detect                                   # what is on this PC (Quake, expansions, runtimes, VC++)
qvr-setup manifest <package folder> --version <v>  # write manifest.json (package-quakevr.ps1 has its own writer)
qvr-setup install --package dist\QuakeVR.zip --target <dir> [--shortcuts-dir <dir>] [--textures <zip>] [--relight] [--vispatch id1_vis.tgz ...]
qvr-setup verify --target <dir>
qvr-setup uninstall --target <dir> [--remove-textures]
qvr-setup download --url <u> [--url <mirror>] --out <file> --size <n> --sha256 <hex>
qvr-setup assets --game <Quake>\id1 [--map maps/start.bsp] [--prefix sound/misc]   # what the skin can read (nothing written)
```

The console never writes the real desktop or Start menu: shortcuts only with `--shortcuts-dir`.

## Tests

```
dotnet run --project tests/QuakeVR.Installer.SelfTest -- <scratch folder> [name filter]
```

21 tests: VDF parsing, a fake Steam (libraries, app manifests), GOG and Epic, id1 kinds, the engine's resource checks
and pack states (ported from `Quake/vr/vr_gamedir.cpp`), expansion roots and priorities, OpenXR/Virtual Desktop/VC++
detection, launch arguments and shortcut plans, `.lnk` round trips, manifest safety (paths outside the folder
refused), install target rules, install/verify/update/uninstall end to end (zip and folder packages, player files kept,
the Quake folder unchanged), damaged and cancelled installs, HD textures for owned packs only, downloads (mirror
fall-back, a wrong file skipped, resume with HTTP Range, pinned SHA-256) and the release feed, all against a local
HTTP server; the skin's readers (pak search order, palette, WAD2 pictures and CONCHARS, a BSP's textures, 8/16-bit WAV,
junk refused) on made-up files, local packages (found beside the installer, checked for a manifest, texture packs
skipped), and VisPatch's data (made-up `.tgz` archives from a local server: mirror order, pinned hash, safe unpacking,
installed where the game looks, kept by updates, removed by uninstall). The machine is a `MemorySystemProbe`: no test reads the real registry or writes outside the scratch folder.
xUnit/MSTest were not used because their NuGet packages are not available offline here; moving the tests to xUnit later
is mechanical.

## Packages

`Windows/package-quakevr.ps1` writes `manifest.json` into the package (`Windows/write-package-manifest.ps1`): the
version and each file's path, size and SHA-256. The installer installs exactly those files and refuses a package whose
files do not match (`--unverified` on the console accepts one without a manifest, for development).
