# Building Quake VR from source

How to build the engine, the QuakeC and the release package, and what the tool scripts in `Misc/quakevr/` do. To
understand the code, start with [vr-port/PLAN.md](vr-port/PLAN.md) (design and status) and
[vr-port/PORTING.md](vr-port/PORTING.md).

## What you need

- **Windows x64** and **Visual Studio 2022** with the C++ desktop workload (toolset v143). The VR code is C++20,
  and the engine is C.
- **[FTEQCC](https://www.fteqcc.org/)** (`fteqcc64.exe`) to compile the QuakeC.
- **Python 3**, only for the tool scripts.
- Your Quake folder (with `id1`) to run the game.

Everything else is in the repository: the OpenXR loader and headers (`Windows/OpenXR`), SDL2 and the codec
libraries (`Windows/`), and GLM (`Quake/vr/external/glm`).

## Where things are

| Path | What |
|---|---|
| `Quake/` | The Ironwail engine (0.8.2). Changes to it are small hooks marked `// QVR`, so `git grep QVR` shows the engine's whole footprint. |
| `Quake/vr/` | The VR module (C++20): OpenXR and mock backends, rendering, input, body, physics, menus, effects. The engine sees only `vr_api.h`. |
| `Quake/vr/vr_cvars.inc` | Every `vr_*` variable, with its default and a comment |
| `QC/` | Quake VR's QuakeC (Quake, Scourge of Armagon and Dissolution of Eternity in one progs, plus FrikBot) |
| `quakevr/` | The game folder: models, sounds, textures, Quake VR's maps, configs. `progs.dat` is built here. |
| `Misc/quakevr/` | Tool scripts (see below) and the source models they work on |
| `Windows/package-quakevr.ps1` | Builds the release package |
| `docs/vr-port/` | Developer notes: the plan, the playtest guide, research, and the notes of each feedback round |

## Building the engine

Open `Windows\VisualStudio\ironwail.sln` and build **Release | x64**, or from a developer prompt:

```
msbuild Windows\VisualStudio\ironwail.sln -p:Configuration=Release -p:Platform=x64 -m
```

The output is `Windows\VisualStudio\Build-ironwail\bin\x64\Release\ironwail.exe`, with `openxr_loader.dll` and the
other DLLs next to it.

**Release flags** (`ironwail.vcxproj`, the same for the engine's C and the VR module's C++):

| Flags | Why |
|---|---|
| `/O2 /Ot /Oi /GF` | Full speed optimisation, intrinsics, pooled strings |
| `/Ob3` | More inlining than `/O2`'s `/Ob2` |
| `/GL` + `/LTCG:incremental` | Link-time (whole-program) optimisation across all files. Incremental: after editing one file the link re-optimises only what changed (about 3 s) |
| `/Gy /Gw` + `/OPT:REF /OPT:ICF` | Every function and global in its own section, so the linker drops the unused ones and merges identical ones |
| `/GS-`, `/sdl-`, no `/guard:cf` | No stack cookies or control-flow checks: a game, not a security boundary |
| `/fp:precise` | Kept everywhere. The engine's physics, NaN checks and demos rely on it, and `/fp:fast` in the VR module alone measured no gain |
| no `/arch` (SSE2, the x64 baseline) | Runs on any x64 CPU. `/arch:AVX2` measured no gain |
| `/Zi` + `/DEBUG` | A `.pdb` for crash dumps. It does not change the code |

The frame is not CPU-bound (about 0.35 ms of CPU a frame on the development PC), so these flags make little
difference to the frame rate. ROUND19.md ("Build flags") has the measurements. SDL2, the codecs, curl and the OpenXR
loader are prebuilt DLLs, used as they are.

**CMake** also works, for development: the top-level `CMakeLists.txt` includes `Quake/vr/vr.cmake`, which adds the
VR module and, on Windows x64, OpenXR.

**Other platforms:** the Makefile builds (Linux, macOS, MinGW) compile the VR module with the mock backend only. The
OpenXR backend binds to OpenGL through WGL, so there is no VR outside Windows x64 yet.

## Building the QuakeC

```
set FTEQCC=C:\path\to\fteqcc64.exe
QC\build.bat
```

This writes `quakevr\progs.dat` (`QC/build.sh` does the same elsewhere). A new engine with old progs, or the other
way round, can break level changes and saves, so rebuild both together.

## Building the release package

```
powershell -ExecutionPolicy Bypass -File Windows\package-quakevr.ps1 [-Build] [-Fteqcc C:\path\to\fteqcc64.exe]
```

- `-Build` builds the solution (Release | x64) first. Without it, the script uses the last build.
- `-Fteqcc` is the compiler. The script falls back to the `FTEQCC` environment variable, then to `fteqcc64` on
  `PATH`. The QuakeC is always compiled fresh.
- The result is `dist\QuakeVR\` and `dist\QuakeVR.zip`. They contain the engine's `.exe`, `.dll` and `.pak` files,
  the `quakevr` folder, `QuakeVR.bat` (`ironwail.exe -game quakevr %*`), and a short `README-QuakeVR.txt`.
- The script copies `quakevr` as it is on disk, leaving out player files: `ironwail.cfg`, `config.cfg`,
  `autoexec.cfg`, `history.txt`, `qconsole.log`, saves and demos, and the `screenshots`, `notes`, `profile`,
  `autosave` and `eyeshots` folders. It also leaves out **`relit`**: the relit maps are id Software's maps and must
  never be redistributed. Anything else in the folder is copied, so package from a clean game folder.
- It adds the relighting scripts (`relight_maps.py`, `vis_maps.py`, `quakepak.py`, `relight_textures.cfg`) in
  `quakevr\tools\`, so players can relight their own maps without the repository
  ([RELIGHTING.md](RELIGHTING.md)). From there, the script's default output is the installed `quakevr\relit`.
- The release is too large for a GitHub release, so it is published on [vittorioromeo.com](https://vittorioromeo.com).

## Running from the repository

Link the repository's game folder into your Quake folder, so that edits and rebuilt progs are picked up directly:

```
mklink /J "C:\...\Quake\quakevr" "C:\path\to\repo\quakevr"
ironwail.exe -basedir "C:\...\Quake" -game quakevr
```

`-condebug` writes the console to `qconsole.log`. Keep the command line short: Ironwail loses `+command` arguments
beyond 256 characters. Use an `autoexec.cfg` for long test setups.

## Testing without a headset

`vr_backend mock; vr_enabled 1` runs everything with a pretend headset, driven from the console:

- `vr_mock_hand <main|off|head> <x> <y> <z> [<pitch> <yaw> <roll>]` and `vr_mock_look <pitch> <yaw>` pose it;
- `vr_mock_button <main|off> <trigger|grip|primary|secondary|stickclick|menu> <0|1>` and
  `vr_mock_stick <main|off> <x> <y>` press the controls;
- `vr_mock_swing <period>` swings the main hand, for throwing tests;
- `vr_mock_play <file>` plays a scripted motion (see the comment in `Quake/vr/vr_backend_mock.cpp`).

[vr-port/TESTING.md](vr-port/TESTING.md) has more, including profiling.

## Tool scripts (`Misc/quakevr/`)

All of them are Python 3. Run them from the repository root. Each script's header comment documents it fully.

| Script | What it does |
|---|---|
| `relight_maps.py` | Relights the player's own id1, hipnotic and rogue maps with ericw-tools `light` (2.0.0-alpha11) into `quakevr/relit/`, with lights for glowing textures and light fixtures (`relight_textures.cfg`), deluxemaps and a light grid; optionally water-vises them. Shipped in the package's `quakevr\tools\`. See [RELIGHTING.md](RELIGHTING.md). |
| `vis_maps.py` | Adds water-vis data (from the VisPatch `.vis` files) to relit or original maps, or checks which maps have it |
| `relight_quakevr_maps.py` | Relights Quake VR's own maps (tutorial, firing range) in place; they are committed |
| `make_spawn_buttons.py` | Builds the brush models (panel, button) of the firing range's second row of monster buttons, which its entity file places (ericw-tools `qbsp` and `light`); they are committed |
| `transcribe_notes.py` | Transcribes voice notes with faster-whisper into `quakevr/notes/NOTES.md` |
| `make_vrbody.py`, `make_pauldron.py`, `make_gadget.py`, `make_holster.py`, `make_flashlight.py`, `make_shell.py`, `make_swords.py` | Generate the body, pauldrons, wrist gadget, holsters, flashlight, shell casing and knights' swords models (with `mdlgen.py`) |
| `improve_weapons*.py`, `recolor_shotgun_sight.py`, `taper_hand.py`, `make_bloody_hands.py` | Rework the weapon and hand models (grips, trigger guards, details, sights, damage skins) from the sources in `src_models/` |
| `make_detail.py`, `make_grades.py`, `make_sounds.py` | Generate the detail textures, the colour grades and the synthesised sounds |
| `quakepak.py` | Reads Quake's `.pak` files (used by the others) |

## Contributing

Changes to Ironwail's own files should stay small hooks marked `// QVR`, with the logic in `Quake/vr/`, so that
upstream Ironwail updates keep merging cleanly. Gameplay rule changes are opt-in variables, so the engine still
plays vanilla Quake with `vr_enabled 0`. When something new is used or learned from, add it to
[vr-port/CREDITS.md](vr-port/CREDITS.md). The principles are in [vr-port/PLAN.md](vr-port/PLAN.md).
