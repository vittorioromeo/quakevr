# Linux (native build)

Status, 2026-10-17 (ROUND22.md, "Native Linux build"): the engine builds natively on x86-64 Linux with clang, runs
flatscreen and on the mock headset, and runs VR through OpenXR with the GLX (Xlib) or the EGL (`XR_MNDX_egl_enable`)
graphics binding (2026-10-19: EGL added for a player on WiVRn whose SDL made an EGL context). It was tested in WSL2 Arch
Linux only, on software rendering (llvmpipe) and Monado's simulated headset with the null compositor: GLX on X11, EGL on
X11 (`SDL_VIDEO_X11_FORCE_EGL=1`) and EGL on Wayland (WSLg). **No real headset has run it yet.**

Why native: under Proton the game binds OpenXR with `XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR`, which wineopenxr
rejects ("Unhandled graphics binding type 1000023000"). VR on Linux needs the native build.

## Build

Packages (Arch): `clang lld cmake ninja sdl2 mesa libx11 libglvnd openxr` (`openxr`: the loader and headers, found with
pkg-config). Debian/Ubuntu: `clang cmake ninja-build libsdl2-dev libgl-dev libx11-dev libopenxr-dev libopenxr-loader1`.

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
    cmake --build build

- Configure prints `Quake VR: OpenXR backend on (loader ..., GLX binding)` (the EGL binding needs no package);
  without pkg-config `openxr` (or with
  `-DQVR_OPENXR=OFF`) the build has the mock backend alone.
- **clang**, not gcc: gcc 15/16 rejects Zancle's `__decay` builtin in function signatures (`vr_jobs.hpp`, "use of
  built-in trait ... in function signature") and instantiates a defaulted `za::Pair::operator==` eagerly
  (`vr_motion.cpp`). Both are in vendored Zancle (no local changes), so gcc stays unsupported.
- The QuakeC (`quakevr/progs.dat`) and the game data are the same as on Windows: build QC on Windows (or with fteqcc on
  Linux), run with `-basedir <folder with id1 and quakevr>`.
- The build number shows `build 0 (unknown)` when git can't read the checkout (a Windows worktree seen from WSL).

## Run

- Flat or mock: `vr_backend mock` (or `-novr`, as on Windows). SDL picks X11 or Wayland.
- VR: `vr_backend openxr`, with an OpenXR runtime: Monado, WiVRn, SteamVR for Linux (the loader finds the active one
  in `~/.config/openxr/1/active_runtime.json`, `/etc/xdg/openxr/1/`, `/etc/openxr/1/`, or `XR_RUNTIME_JSON`).
  `active_runtime.x86_64.json` wins over `active_runtime.json` in each folder (the loader's order), and manifests for
  another architecture (`openxr_wivrn.i686.json`, or a 32-bit library) are left out ("for i686 ...: left out" in
  `vr_xr_runtime_explain` and the start's log).
- **The session binds SDL's context, whichever it is** (`vr_xr_linuxgl.cpp`; the log's `xrCreateSession (OpenGL, ...
  binding ...: SDL video driver ...)` line names it):
  - GLX (SDL's X11 driver, also under XWayland): `XrGraphicsBindingOpenGLXlibKHR` (`XR_KHR_opengl_enable`). Every
    runtime has it.
  - EGL (SDL's Wayland driver; X11 with `SDL_VIDEO_X11_FORCE_EGL=1`; SDL3/sdl2-compat on Wayland desktops):
    `XrGraphicsBindingEGLMNDX` (`XR_MNDX_egl_enable`): Monado and WiVRn have it. libEGL is looked up in the process
    (SDL loaded it), not linked.
  - SteamVR has no EGL binding. When the start may try SteamVR (the plan has it), an X server is there (`DISPLAY`) and
    no SDL video variable is set, the game asks SDL for X11 with GLX before video init (`SDL_VIDEODRIVER=x11`,
    `SDL_VIDEO_X11_FORCE_EGL=0`, and SDL3's `SDL_VIDEO_DRIVER`, `SDL_VIDEO_FORCE_EGL`); the console and the log say
    "OpenXR: before video init: ...". An EGL context with a runtime lacking `XR_MNDX_egl_enable` fails the start with
    a warning naming those variables.
- The runtime needs GL to import its images: Monado's GL client uses `GL_EXT_memory_object_fd` (the log's "GL interop"
  line). Mesa and NVIDIA have it.

## Flatpak

The portable Linux build: the Flatpak `io.github.vittorioromeo.QuakeVR` (the Release engine built with clang, the
compiled QuakeC, the `quakevr` game folder, the OpenXR loader 1.1.65) on the freedesktop runtime 25.08. Nothing of id's,
Arcane Dimensions' or the expansions' is in it: it reads the player's own Quake in place.

- **CI** (`.github/workflows/flatpak.yml`): every push (any branch) and manual runs build it in the
  `ghcr.io/flathub-infra/flatpak-github-actions:freedesktop-25.08` container (the maintained successor of the
  `bilelmoussaoui` images, which stop at 24.08) with `flatpak/flatpak-github-actions/flatpak-builder@v6.8`, and keep the
  bundle as the run's artifact `QuakeVR-<branch>-<hash>` (30 days: the Actions tab, the run, Artifacts). No release is
  published.
- **Files** (`Misc/linux/flatpak/`): the manifest `io.github.vittorioromeo.QuakeVR.yml` (modules: fteqcc at the commit of
  the author's `fteqcc64.exe`, for the build only; the OpenXR loader; Quake VR), `build.sh` (in the sandbox: the QuakeC,
  CMake, the install; the game folder is what git would track under `quakevr/`, less `.gitignore` and `motions/`, as
  `Windows/package-quakevr.ps1` has it), `quakevr.sh` (the launcher), the desktop entry, the AppStream metadata and the
  icon (the installer's logo). The build number: CI writes `.qvr-buildinfo` ("<commit count> <hash>"), which
  `qvr_buildinfo.cmake` reads when the copy has no `.git`; a local build says `build 0 (unknown)`.
- **Building it locally** (flatpak-builder 1.4; it installs the runtime, the SDK and
  `org.freedesktop.Sdk.Extension.llvm22` from Flathub):

      flatpak-builder --user --install-deps-from=flathub --force-clean --repo=repo build-dir \
          Misc/linux/flatpak/io.github.vittorioromeo.QuakeVR.yml
      flatpak build-bundle repo QuakeVR.flatpak io.github.vittorioromeo.QuakeVR

### Installing and running

    flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
    flatpak install --user QuakeVR-next-abc1234.flatpak     # the runtime comes from Flathub
    flatpak run io.github.vittorioromeo.QuakeVR             # or "Quake VR" in the desktop's menu

- **Quake's data**: the launcher runs `-basedir <Quake> -basedir <Quake VR> -game quakevr`, as the Windows installer's
  shortcuts do. It looks for Quake in this order: `--quake-dir <folder>` (then remembered in
  `~/.var/app/io.github.vittorioromeo.QuakeVR/config/quake-dir`), `QUAKEVR_QUAKE_DIR`, the remembered folder,
  `steamapps/common/Quake` in every Steam library (native and Flatpak Steam, from `libraryfolders.vdf`), then
  `~/GOG Games/Quake`, `~/Games/Heroic/Quake`, `~/Games/Quake`, `~/quake`. A folder qualifies with `id1/pak0.pak` (lower
  case, as Ironwail looks for it) or with `rerelease/id1/pak0.pak`: Steam's and GOG's current Quake (the original
  release's `Id1/PAK0.PAK` beside it is upper case, so not used).

      flatpak run io.github.vittorioromeo.QuakeVR --quake-dir ~/.local/share/Steam/steamapps/common/Quake

  Home is readable (read-only). Quake on another disk needs sharing too:
  `flatpak override --user --filesystem=/mnt/games/Quake:ro io.github.vittorioromeo.QuakeVR`. Without a Quake, the
  engine's error dialog says how to give one. Mods and Arcane Dimensions are found in the Quake folder, as on Windows.
- **What it writes**: `~/.var/app/io.github.vittorioromeo.QuakeVR/data/QuakeVR/quakevr/` (config, saves, logs, crash
  and bug reports, screenshots), beside links to the shipped files (`/app/share/quakevr/quakevr`, refreshed at every
  start). The Quake folder is never written.
- **VR runtimes**: the host's `~/.config/openxr` (shared read-only) shows in the app's own config folder too, where the
  loader and the game's detection look first (`vr_xr_runtime_explain` lists it as
  `~/.var/app/io.github.vittorioromeo.QuakeVR/config/openxr/1/active_runtime.json`); the launcher also adds `~/.config`
  to `XDG_CONFIG_DIRS`. The runtime's library is loaded into the sandboxed game, so it must be
  visible there and work with the freedesktop runtime's libraries:
  - **WiVRn's Flatpak** (`io.github.wivrn.wivrn`): expected to work (its library is built on the freedesktop runtime;
    its folder and `xdg-run/wivrn` are shared). Not tried yet.
  - **Monado or WiVRn from distribution packages**: their manifests (`/usr/share/openxr/1`, `/etc/xdg/openxr/1`) name a
    library under the host's `/usr`, which the sandbox does not see (its `/usr` is the runtime's): no runtime found. Use
    the native build.
  - **SteamVR**: its manifest and `vrclient` are in the Steam folder (shared read-only), and the game asks SDL for
    X11/GLX. Not tried; the likely limits: SteamVR's library expecting the Steam Runtime's libraries, and the sandbox
    hides the host's processes, so the game's "SteamVR is running" detection (`vrserver` in `/proc`) sees nothing.
  - Any manifest by hand: `flatpak run --env=XR_RUNTIME_JSON=/path/to/manifest.json io.github.vittorioromeo.QuakeVR`
    (the manifest and its library readable in the sandbox).
- **Not in it**: the relighting scripts (`quakevr/tools`, Python: in the repository), ericw-tools, the HD texture pack,
  debug symbols (the binary is stripped: a crash report's offsets need that CI build's unstripped binary, not kept). No
  real headset has run the Flatpak yet.

| Permission | Why |
|---|---|
| `--socket=wayland`, `--socket=x11`, `--share=ipc` | the window; X11 always (not `fallback-x11`): SteamVR has only the GLX binding, so with it the game asks SDL for X11 (XWayland); ipc: X11 shared memory |
| `--device=all` | the GPU (dri) and the headset's and controllers' USB/HID devices |
| `--socket=pulseaudio` | sound (PipeWire's Pulse server too) |
| `--share=network` | the update check, OBS's websocket, multiplayer |
| `--filesystem=home:ro` | the player's Quake, Steam libraries and GOG folders under home, read in place |
| `--filesystem=~/.var/app/com.valvesoftware.Steam:ro` | Flatpak Steam's library |
| `--filesystem=xdg-config/openxr:ro` | the active OpenXR runtime's manifest |
| `--filesystem=xdg-run/monado_comp_ipc`, `xdg-run/wivrn` | the runtimes' IPC sockets (Monado and Envision; WiVRn) |
| `--filesystem=xdg-data/flatpak/app/io.github.wivrn.wivrn:ro` | WiVRn's Flatpak: its OpenXR library |
| `--filesystem=xdg-data/Steam:ro`, `~/.steam:ro`, `xdg-config/openvr:ro` | SteamVR: its OpenXR manifest, `vrclient`, OpenVR's paths |

## Platform pieces (what the Windows-only code became)

| Piece | Linux |
|---|---|
| Crash reports (`vr_crash.cpp`) | signal handlers (SEGV, BUS, FPE, ILL, ABRT) on an alternate stack, glibc `backtrace` + `dladdr`: `<game>/crash/<date>_<time>.txt` with module offsets (resolve with `addr2line -f -C -e ironwail 0x<offset>`), stderr, `qvr_crash.txt` in test runs. No minidump, no symbols from debug info. Sys_Error writes a report too (`sys_sdl_unix.c`), and its dialog names it (`pl_linux.c`). |
| Run logs, bug report (`vr_runlogs.cpp`) | as on Windows (logs/, rotation, `bugreport` zip); OS line from `/etc/os-release` and `uname`; runtime logs: SteamVR's under `~/.local/share/Steam/logs`. |
| Diagnostics mode (`vr_diagnostics.cpp`) | the logs copied; no debug-output capture (OutputDebugString is Windows') and no core validation layer (the shipped layer is a Windows DLL). |
| Opening a folder (bug report, diagnostics) | `xdg-open` on the folder. |
| RenderDoc (`vr_renderdoc.cpp`) | `librenderdoc.so` when RenderDoc loaded it (`dlopen` with `RTLD_NOLOAD`). |
| OpenXR runtime detection (`vr_xr_runtime.cpp`) | active runtime from `active_runtime.json` (XDG config dirs, `/etc`), available manifests in `/usr/share/openxr/1` and `/usr/local/share/openxr/1`, running processes from `/proc/*/comm` (SteamVR's `vrserver`); `XR_RUNTIME_JSON` set with `setenv`. No implicit-layer switch (that reads the Windows registry). |
| Runtime crash guard (`vr_xr_guard`) | none: a crash inside the runtime is a crash report (no structured exceptions). |
| mimalloc | the process heap as on Windows; its own `operator new/delete` renamed away (`vr.cmake`) so `vr_alloccount.cpp`'s count. |
| QC profiler ticks (`pr_exec.c`) | `__rdtsc` from `x86intrin.h`; the monotonic clock on ARM. |

Still Windows only (stubbed off on Linux, from before this port): the Doom module (`qvr_doom.dll`), GPU and VRAM stats
(NVML, PDH: `vr_gpustats.cpp`), the high-resolution frame sleep (`vr_sleep.cpp`: a plain sleep), OBS's process
detection (its websocket works), the start-up timing before `main` (`vr_startup.cpp`), the installer and updater, and
reading Virtual Desktop's settings. Steam Audio looks for `libphonon.so` (not shipped).

## Testing from Windows (WSL)

`qvr-kit/linux/README.md`: `arch_setup.sh` (packages), `monado_build.sh` (Monado from source: Arch has it in the AUR
only), `linux_wsl.sh <worker>` (build + a Monado run), `QVR_WSL_MODE=mock linux_wsl.sh <worker>` (build + the e1m1 mock
smoke), `wsl_run.sh` (one run with any commands).
