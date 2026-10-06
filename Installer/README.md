# Quake VR installer

A Windows installer for Quake VR, written in C# with WPF (.NET 9). It finds the player's Quake and its expansions,
checks their VR setup, installs Quake VR into its own folder without touching Quake's files, makes the shortcuts, and
can update or remove what it installed. The design, the research behind it and the author's decisions are in
[docs/vr-port/INSTALLER.md](../docs/vr-port/INSTALLER.md); section 13 there describes this code.

## Layout

| Folder | What |
|---|---|
| `src/QuakeVR.Installer.Core` | Everything that is not UI, testable without a window: detection (`Detection/`), packages, manifests, install/update/uninstall and downloads (`Packaging/`), shortcuts (`Shortcuts/`), the registry and special folders behind an interface (`Platform/`) |
| `src/QuakeVR.Installer` | The wizard window (`QuakeVR-Setup.exe`): Welcome, Your PC, Options, Install, Play. `ViewModels/MainViewModel.cs` holds the state; `Views/` the pages; `Themes/Theme.xaml` the look; `ScreenshotHarness.cs` renders the pages to PNG |
| `src/QuakeVR.Installer.Cli` | `qvr-setup.exe`: the core from a console (detection report, install/uninstall/verify from a local package, manifests, downloads) |
| `tests/QuakeVR.Installer.SelfTest` | The core's tests, as a console program (see "Tests") |

No NuGet packages: everything is in the .NET 9 SDK (WPF, `System.Text.Json`, `System.IO.Compression`, `HttpClient`,
COM interop for `IShellLink`).

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
| `--package <zip or folder>` | Install this package (otherwise `QuakeVR.zip` or a `QuakeVR\` folder beside the exe, otherwise a download) |
| `--textures <zip>` | The HD texture pack, already downloaded |
| `--target <dir>` | The install folder (default `%LOCALAPPDATA%\Programs\QuakeVR`) |
| `--shortcuts-dir <dir>` | Shortcuts go to `<dir>\Desktop` and `<dir>\Programs` instead of the real desktop and Start menu (tests) |
| `--feed <url>` | Where `latest.json` is read (repeatable; default: GitHub, then vittorioromeo.com). Also `installer-settings.json` beside the exe |
| `--downloads <dir>` | Where downloads go (default `%LOCALAPPDATA%\QuakeVR-Installer\downloads`) |
| `--screenshots <dir>` | Render every page to PNG and exit, no window (with `--package --target --shortcuts-dir` it runs a real install into those folders first) |

### qvr-setup (console)

```
qvr-setup detect                                   # what is on this PC (Quake, expansions, runtimes, VC++)
qvr-setup manifest <package folder> --version <v>  # write manifest.json (package-quakevr.ps1 has its own writer)
qvr-setup install --package dist\QuakeVR.zip --target <dir> [--shortcuts-dir <dir>] [--textures <zip>] [--relight]
qvr-setup verify --target <dir>
qvr-setup uninstall --target <dir> [--remove-textures]
qvr-setup download --url <u> [--url <mirror>] --out <file> --size <n> --sha256 <hex>
```

The console never writes the real desktop or Start menu: shortcuts only with `--shortcuts-dir`.

## Tests

```
dotnet run --project tests/QuakeVR.Installer.SelfTest -- <scratch folder> [name filter]
```

18 tests: VDF parsing, a fake Steam (libraries, app manifests), GOG and Epic, id1 kinds, the engine's resource checks
and pack states (ported from `Quake/vr/vr_gamedir.cpp`), expansion roots and priorities, OpenXR/Virtual Desktop/VC++
detection, launch arguments and shortcut plans, `.lnk` round trips, manifest safety (paths outside the folder
refused), install target rules, install/verify/update/uninstall end to end (zip and folder packages, player files kept,
the Quake folder unchanged), damaged and cancelled installs, HD textures for owned packs only, downloads (mirror
fall-back, a wrong file skipped, resume with HTTP Range, pinned SHA-256) and the release feed, all against a local
HTTP server. The machine is a `MemorySystemProbe`: no test reads the real registry or writes outside the scratch folder.
xUnit/MSTest were not used because their NuGet packages are not available offline here; moving the tests to xUnit later
is mechanical.

## Packages

`Windows/package-quakevr.ps1` writes `manifest.json` into the package (`Windows/write-package-manifest.ps1`): the
version and each file's path, size and SHA-256. The installer installs exactly those files and refuses a package whose
files do not match (`--unverified` on the console accepts one without a manifest, for development).
