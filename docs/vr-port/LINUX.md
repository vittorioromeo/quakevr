# Linux (native build)

Status, 2026-10-17 (ROUND22.md, "Native Linux build"): the engine builds natively on x86-64 Linux with clang, runs
flatscreen and on the mock headset, and runs VR through OpenXR with the GLX (Xlib) or the EGL (`XR_MNDX_egl_enable`)
graphics binding (2026-10-19: EGL added for a player on WiVRn whose SDL made an EGL context). It was tested in WSL2 Arch
Linux only, on software rendering (llvmpipe) and Monado's simulated headset with the null compositor: GLX on X11, EGL on
X11 (`SDL_VIDEO_X11_FORCE_EGL=1`) and EGL on Wayland (WSLg). **No real headset has run it yet.**

Why native: under Proton the game binds OpenXR with `XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR`, which wineopenxr
rejects ("Unhandled graphics binding type 1000023000"). VR on Linux needs the native build.

## Build

Packages (Arch): `clang lld cmake ninja sdl2 mesa libx11 libglvnd openxr patch make` (`openxr`: the loader and
headers, found with pkg-config; `sdl2-compat` serves for `sdl2`). Debian/Ubuntu: `clang cmake ninja-build libsdl2-dev
libgl-dev libx11-dev libopenxr-dev libopenxr-loader1 zlib1g-dev patch make`.

    cmake --preset linux-release
    cmake --build --preset linux-release

That builds everything: the engine (`build/ironwail`) and the QuakeC (`quakevr/progs.dat`). `linux-debug` is the same
in `build-debug/`, unoptimised with the asserts on. Without the presets (CMake before 3.21):
`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`.

- Configure prints `Quake VR: OpenXR backend on (loader ..., GLX binding)` (the EGL binding needs no package);
  without pkg-config `openxr` (or with
  `-DQVR_OPENXR=OFF`) the build has the mock backend alone.
- **clang**, not gcc: gcc 15/16 rejects Zancle's `__decay` builtin in function signatures (`vr_jobs.hpp`, "use of
  built-in trait ... in function signature") and instantiates a defaulted `za::Pair::operator==` eagerly
  (`vr_motion.cpp`). Both are in vendored Zancle (no local changes), so gcc stays unsupported: configure stops with a
  message when the compiler is gcc (`-DQVR_ALLOW_GCC=ON` tries it anyway).
- **The QuakeC** (`Misc/cmake/QuakeC.cmake`): the build compiles it with `QC/build.sh` (its flags and checks) whenever
  a QC source changes, its output in `build/qc-build.log`. The compiler is FTEQCC with the `fte32` flag
  (`Tools/fteqcc/README.md`; a stock FTEQCC fails the format check): the first build downloads the source pinned in
  `Misc/linux/flatpak/fteqcc.json` (the Flatpak's fteqcc module, the one place the URL, checksum and patch are written;
  checksum checked), applies its patch (`Tools/fteqcc/fte32.patch`) and builds it in `build/fteqcc` (network once,
  `make`, `patch` and zlib). `-DQVR_FTEQCC=<path>` uses your own instead; `-DQVR_QC=OFF` leaves `progs.dat` alone;
  `-DQVR_QC_RELEASE=ON` compiles the release flavour (`-DQVR_NO_TESTS`, `QC/build.sh`'s `QVR_QC_RELEASE=1`), as the
  Flatpak does.
- The build number shows `build 0 (unknown)` when git can't read the checkout (a Windows worktree seen from WSL).

## Run

From the repository, with your Quake folder first and the repository (its `quakevr` folder) last:

    ./build/ironwail -basedir ~/.local/share/Steam/steamapps/common/Quake -basedir . -game quakevr

- **Steam's and GOG's Quake** as they are: the original release's paks are named in upper case (`id1/PAK0.PAK`), which
  the engine matches on Linux too (`Sys_MatchFileCase`: a `pakN.pak` not found by its own name is looked for ignoring
  case). The `rerelease` folder (`.../Quake/rerelease`) works as a base dir as well.
- Flat or mock: `-flatscreenmode` (the flat-screen mode, as the Windows installer's flat shortcut; the launcher's
  `--flat`) or `vr_backend mock`. SDL picks X11 or Wayland.
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
  the author's `fteqcc64.exe`, for the build only, in its own file `fteqcc.json`, which the CMake build reads its pin
  from too; the OpenXR loader; Quake VR), `build.sh` (in the sandbox: the CMake build, the engine and the QuakeC's
  release flavour with the module's fteqcc, then the install; the game folder is what git would track under `quakevr/`, less `.gitignore` and `motions/`, as
  `Windows/package-quakevr.ps1` has it), `quakevr.sh` (the launcher, also the tarball's), the desktop entry, the AppStream metadata and the
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
    the tarball ("Linux tarball" below).
  - **A runtime built on the host** (Envision's Monado, one built by hand under home): visible through `home:ro`, but it
    fails to load once the host's glibc is newer than the Flatpak runtime's ("The Flatpak and glibc" below). Use the
    tarball.
  - **SteamVR**: its manifest and `vrclient` are in the Steam folder (shared read-only), the game asks SDL for
    X11/GLX, and the host's `/dev/shm` is shared (`--device=shm`, since 2026-11-17). Not working yet for the two players
    who tried (GitHub #82: `XR_ERROR_RUNTIME_UNAVAILABLE`): see "SteamVR" below.
  - Any manifest by hand: `flatpak run --env=XR_RUNTIME_JSON=/path/to/manifest.json io.github.vittorioromeo.QuakeVR`
    (the manifest and its library readable in the sandbox).
- **Not in it**: the relighting scripts (`quakevr/tools`, Python: in the repository), ericw-tools, the HD texture pack,
  debug symbols (the binary is stripped: a crash report's offsets need that CI build's unstripped binary, not kept). No
  real headset has run the Flatpak yet.

### The Flatpak and glibc (2026-11-27)

An Arch player on Monado: the Flatpak failed with "GLIBC_2.43 not found" on `libopenxr_monado.so`; the same files run
outside the sandbox worked. Why: OpenXR runtimes are libraries loaded **into the game's process**. In the sandbox that
process runs on the freedesktop runtime's `ld.so` and `libc.so.6` (25.08: glibc 2.42), and one process has one libc. A
runtime built on the host (Envision's Monado, a self-built one under home: readable through `home:ro`) was linked
against the host's glibc (Arch: 2.43, now 2.44) and asks for symbol versions the sandbox's libc doesn't have, so the
dynamic linker refuses it. Nothing in the app can change that. The options:

- **A newer Flatpak runtime** (26.08 when Flathub has it, with its `llvm22` extension): three lines (the manifest's
  `runtime-version`, the CI container's tag, the extension if it changes), but it only works until the host's glibc
  moves ahead again (a rolling distribution is always ahead), and a host runtime's other libraries (libusb, hidapi,
  Vulkan loaders) must exist in the sandbox too. **Not done** (2026-11-27): it can't be built here without downloading
  the new runtime, and a broken manifest would fail every push's CI. Try it with one CI run when convenient.
- **Runtimes built for the Flatpak runtime** (WiVRn's Flatpak) keep working: same libc.
- **The tarball** (below): no sandbox, the host's libc for both the game and the runtime. **Done**: the answer for host
  runtimes (and distribution-packaged Monado/WiVRn, which the sandbox can't see at all).

## Linux tarball (2026-11-27)

`QuakeVR-linux-x86_64.tar.gz`: the Flatpak's own build (the same engine binary, `ironwail.pak`, the OpenXR loader
1.1.65 and the `quakevr` game folder) as a plain folder, run unsandboxed. CI makes it in the Flatpak job from the build
directory the action leaves (`flatpak_app/files`, the Flatpak's `/app`) and keeps it as the run's artifact
`QuakeVR-<branch>-<hash>-linux-x86_64`; for a release attach it with `make_release.py --asset`.

- **Made by** `Misc/linux/tarball/make_tarball.sh <prefix> [out dir] [runtime lib dir]`: the engine, its pak, the
  loader (`libopenxr_loader.so.1`), the music codecs the engine links (mpg123, opusfile/opus, FLAC 14, vorbisfile/vorbis,
  ogg: copied from the freedesktop runtime, since sonames differ between distributions: Ubuntu 24.04 has FLAC 12), the
  game folder, `README.txt` (`Misc/linux/tarball/README.txt`, with the version and the glibc filled in) and the launcher.
  It fails when a linked library is neither bundled nor on its system list (glibc, libstdc++, libX11, libGLX/libOpenGL,
  SDL2, libcurl), or when the binaries need a glibc newer than `QVR_GLIBC_MAX` (2.38).
- **glibc floor: 2.38** (Ubuntu 24.04, Debian 13, Fedora 39 or newer; libstdc++ GLIBCXX_3.4.32, gcc 13). It comes from
  building on the 25.08 SDK: C23's `__isoc23_strtol` family (glibc 2.38 headers in C++) and libm's new `fmod`. The one
  2.42 symbol, `__inet_pton_chk` (glibc's fortified `inet_pton`, which clang doesn't fold), is avoided in
  `host_cmd.c` by calling `inet_pton` through a pointer. An older floor (Ubuntu 22.04's 2.35) needs a build on an older
  system: the AppImage plan's `ubuntu-22.04` job below.
- **The launcher** is the Flatpak's (`Misc/linux/flatpak/quakevr.sh`, one script): beside `ironwail` it runs as the
  tarball's, `LD_LIBRARY_PATH=<its folder> ./ironwail -basedir <Quake> -basedir <its folder> -game quakevr "$@"`, with
  the same Quake search (Steam libraries from `steamapps/` and `config/libraryfolders.vdf`, native, Debian's, Flatpak and
  Snap Steam; GOG, Heroic, Lutris folders; `/usr/share/games/quake`; the rerelease folder), `--quake-dir` (remembered in
  `<folder>/quake-dir`), `QUAKEVR_QUAKE_DIR`, `--flat` (`-flatscreenmode`, also in the Flatpak) and `--dry-run` (prints
  the folder and the command). Without Quake it says what to run (and in the Flatpak, the `flatpak override` for each
  Steam library the sandbox can't see).
- **The OpenXR runtime** is always the system's: the loader's ordinary search, `XR_RUNTIME_JSON`; no `XDG_CONFIG_DIRS`
  change. The game writes into the tarball's `quakevr/`.
- **Tests**: `bash Misc/linux/tarball/quakevr_sh_test.sh [launcher] [work dir]` (the Quake search on fake homes, every
  layout; a stub engine for the command line; prints `quakevr_sh_test: N passed, M failed`). The tarball was made from a
  local offline `flatpak-builder` build in WSL, unpacked in a clean folder and run with a fake Steam library (2026-11-27,
  ROUND26.md "linuxtar").

## SteamVR (GitHub #82, 2026-11-17)

Reports: Fedora 45 with a Steam Frame, SteamVR running and the headset connected first, the Flatpak (and "outside its
sandbox", how unknown): `SteamVR: not running` in the report, then `xrCreateInstance failed: (-51)` for
`~/.local/share/Steam/steamapps/common/SteamVR/steamxr_linux64.json`. Another player: SteamVR took the game once (the
Frame didn't offer it), then the same error. No hardware here: what follows is from the code, the Flatpak's sandbox
and the OpenXR loader, checked headless in WSL (a SteamVR manifest whose library is missing).

- **-51 is `XR_ERROR_RUNTIME_UNAVAILABLE`** (failure lines now name the result: `xrCreateInstance failed:
  XR_ERROR_RUNTIME_UNAVAILABLE (-51)`; before, the name came from `xrResultToString`, which needs the instance a failed
  `xrCreateInstance` doesn't have). The loader returns it when the runtime's library doesn't load (`dlopen`: a missing
  file or dependency) or its `xrNegotiateLoaderRuntimeInterface` / `xrCreateInstance` fails: SteamVR's `vrclient.so`
  fails when it can't reach its server (`vrserver`) and can't start it.
- **"SteamVR: not running" in the Flatpak** is the sandbox: it has its own PID namespace, its `/proc` lists its own
  processes only. The report now says "can't tell: the Flatpak sandbox hides the host's processes"; Auto treats it as not
  running (a sole or active SteamVR is tried anyway, as in the report). No file tells reliably that SteamVR runs
  (`openvrpaths.vrpath` lists the installs, the logs stay after it stops); asking the host (`flatpak-spawn --host`)
  needs `--talk-name=org.freedesktop.Flatpak`, a full sandbox escape: not taken. Outside a sandbox the detection reads
  every process's `comm`, its executable's name and its argv[0]/argv[1]'s (a program started through `ld.so` or a
  shell, as the Steam Linux Runtime's container may start SteamVR's) for `vrserver`, `vrcompositor`, `vrmonitor`.
- **The Flatpak's `/dev/shm`**: `--device=all` alone mounts a tmpfs of the sandbox's own there (Flatpak shares the
  host's only with `--device=shm`). OpenVR's client finds its server's shared memory there: without it, `vrclient`
  sees no server, tries to start one from inside the sandbox and fails: the likeliest cause of -51 in the Flatpak. The
  manifest has `--device=shm` from 2026-11-17; an older install: `flatpak override --user --device=shm
  io.github.vittorioromeo.QuakeVR`. The runtime report lists the sandbox's devices, whether `/dev/shm` is the host's,
  and the `/dev/shm` entries that look like SteamVR's, OpenVR's or Steam's (to learn their names from players' logs).
- **Not explained yet**: the same error outside the sandbox. The loader's own errors (which file it tried, the `dlopen`
  message) went to stderr only; they are in `qvr_openxr.txt` now, and a failed start prints them. What a player's next
  log should show: the loader's lines, and SteamVR's `~/.local/share/Steam/logs/vrclient_*.txt` (the Flatpak can't
  write it: the Steam folder is read-only there) and `vrserver.txt`.
- **A failed SteamVR start says what to try** (`runtimeUnavailable`): start SteamVR first and wait for the headset,
  then `vr_restart`; in the Flatpak without the host's `/dev/shm`, the override; the logs; Monado or WiVRn instead.
- **`vr_xr_debug 1`** (Debug > Startup, "OpenXR Loader Log"; saved): `XR_LOADER_DEBUG=all` before VR starts (on Linux
  before video init, the config read early), every loader message in `qvr_openxr.txt`. The loader reads the variable
  once, at its first message: from the next game start when VR already started. `flatpak run
  --env=XR_LOADER_DEBUG=all io.github.vittorioromeo.QuakeVR` does the same from outside.
- **A PSVR2 on SteamVR 2.19.3 (2026-11-18; Ubuntu 26.04 on a PS5's Oberon, Mesa 26.2.7, GNOME 50 on Wayland; the
  Flatpak at 5187f3e, before `--device=shm`)**: the player's logs (the bug report's zip) and what they tried:
  - **SteamVR's logs folder must be writable**: with the Steam folder read-only, `vrclient` failed to start (-51);
    writing `~/.local/share/Steam/logs` fixed it. The manifest writes that folder alone from 2026-11-18
    (`--filesystem=xdg-data/Steam/logs`); an older install: `flatpak override --user
    --filesystem=xdg-data/Steam/logs io.github.vittorioromeo.QuakeVR`. The runtime report and a failed start say
    whether the folder is writable.
  - **With `--device=shm` too, the game's side worked**: `xrCreateSession` (GLX: display, drawable, context, FBConfig and
    visual all set; the context current on the main thread for every call), the eye and panel swapchains, `OpenXR:
    started`, the session READY, then SYNCHRONIZED in SteamVR's own log of the game (`xrclient_ironwail.txt`).
  - **The sandbox makes SteamVR's client start a second set of SteamVR's helpers, as the game's children**: the game
    is PID 2 in its own PID namespace (`xrclient_ironwail.txt`: "startup with PID=2", then "Overriding pid with 758890
    (was 2)"); SteamVR's client, in the game's process, doesn't see the host's helpers and starts its own
    `vrdashboard`, `vrwebhelper` and `vrenv.sh` (which runs `vrmonitor -nokillprocess -startupexename ironwail`)
    inside the sandbox, though the host's were running (they disconnect only later); `vrserver` logs "Duplicate mailbox
    name vrwebhelper/main" and a second `vrmonitor` and `vrdashboard`. A native OpenXR game on the same machine logs
    "VR Dashboard executable already running" and starts none.
  - **The sandboxed `vrmonitor` crashes GNOME Shell (likely)**: twice, 5 ms after it connects to `vrserver`, GNOME
    Shell's journal shows a new legacy tray icon (`Shell.TrayIcon`), then an X `BadWindow` in its panel code
    (`panelMenu.js`, `panel.js`), SIGABRT and a logout (Xwayland goes with it, and the host's SteamVR); the host's own
    `vrmonitor` starting never shows that tray line. Probably the sandbox's filtered session bus leaves its Qt without
    the StatusNotifier service, so it falls back to an XEmbed tray icon (not proven). Not every time: at 08:56 and 09:20
    the sandboxed `vrmonitor` ran for ~100 s and the desktop survived. GNOME Shell aborting on a client's `BadWindow`
    is GNOME's bug (worth a report with `journalctl --user -b` and `coredumpctl info gnome-shell`).
  - **The game's crashes are SteamVR's client's, and separate from GNOME's**: they happen in runs where GNOME survived
    too (08:56, 09:20), each within a frame or two of the session's first frames, right after SteamVR's client starts
    an in-process Vulkan renderer for the panel's quad layer ("Initializing CVulkanVRRenderer", "Created remote Vulkan
    image"; the native control has none): a SIGABRT on a SteamVR thread ("Not the lock owner(0) != 105", "pthreads
    futex robust_list is corrupt", `ipcposix.cpp:217`), or a SIGSEGV at 0x60 in the first `xrBeginFrame` after
    SYNCHRONIZED (`vrclient.so`, then `libvulkan_radeon.so`; the same offsets at 5187f3e and 48f9d58). Why, the logs
    can't tell: the sandbox's thread IDs in locks shared with the host, the duplicate helpers resetting shared state, or
    a SteamVR 2.19.3 bug in the GL quad-layer path on this GPU. "No GPU timing Vulkan device" is logged by the native
    control too: not the cause.
  - **Not every Flatpak**: a player on CachyOS (2026-11-19, Discord) plays the Flatpak with SteamVR (after giving it
    `~/.local/share/Steam`), so neither the sandbox alone nor the GL quad layer alone is enough. What would decide:
    the native build on the PSVR2 machine (works: the sandbox; the same SIGSEGV: SteamVR's GL path), and the CachyOS
    player's `xrclient_ironwail.txt` (PID=2? duplicate helpers? `CVulkanVRRenderer`?) with their GPU, SteamVR version
    and desktop.
  - (The same player's Windows build under Proton, 29 October: "unsupported graphics binding type: 1000023000", the
    Win32 GL binding: Proton is no way for this GL game to SteamVR on Linux.)
- **Other runtimes for a SteamVR headset**: WiVRn streams to the standalone headsets its client runs on, Monado drives
  the headsets it has drivers for (LINUX.md, "Run"); xrizer is the other way round (OpenVR games on an OpenXR runtime),
  not needed for this game, which speaks OpenXR.

## AppImage (plan, not built)

Asked for in #82. Feasible; not built here because it needs a tool CI doesn't fetch yet (`appimagetool`, or
`linuxdeploy`, from their GitHub releases: a new download, pinned by version and checksum).

- **Why it helps VR**: an AppImage runs unsandboxed on the host: the host's `/usr/share/openxr/1` manifests and their
  libraries (Monado, WiVRn from distribution packages), the host's processes (SteamVR's detection), `/dev/shm` and
  `/tmp` as any native program. The OpenXR runtime discovery is the loader's ordinary one (`XR_RUNTIME_JSON`,
  `~/.config/openxr`, `/etc/xdg/openxr`, `/etc/openxr`): nothing to configure.
- **What it bundles**: `ironwail` (Release, clang, the ThinLTO build), `libopenxr_loader.so.1` (1.1.65, built from
  source as the Flatpak's module), `libSDL2` (or sdl2-compat) and what `ldd` shows beyond the AppImage exclude list;
  never `libGL`, `libEGL`, `libvulkan`, `libX11`, glibc (the host's driver stack). The `quakevr` game folder and the
  compiled QuakeC as the Flatpak's `build.sh` assembles them. `AppRun`: the Flatpak's `quakevr.sh` with the paths
  changed (the data folder `~/.local/share/QuakeVR/quakevr`, the same Quake search, no `XDG_CONFIG_DIRS` change).
- **Compatibility**: build on the oldest glibc to support (Ubuntu 22.04's 2.35, or the Steam Runtime 3 "sniper"
  container's 2.31), clang from apt.llvm.org there (gcc can't build vendored Zancle).
- **CI**: a second job in `flatpak.yml` (or `appimage.yml`), `ubuntu-22.04`: apt (`clang`, `cmake`, `ninja-build`,
  `libsdl2-dev`, `libx11-dev`, `libgl-dev`), the OpenXR loader and fteqcc from the same pinned sources as the Flatpak
  manifest (fteqcc: the CMake build's own, from `fteqcc.json`), `Misc/linux/flatpak/build.sh`'s steps with `/app` as an AppDir prefix, then `appimagetool` (pinned) into
  `QuakeVR-<branch>-<hash>-x86_64.AppImage`, uploaded as the run's artifact like the Flatpak. About a day's work with
  one CI iteration loop; no signing, no update information (zsync) at first.

| Permission | Why |
|---|---|
| `--socket=wayland`, `--socket=x11`, `--share=ipc` | the window; X11 always (not `fallback-x11`): SteamVR has only the GLX binding, so with it the game asks SDL for X11 (XWayland); ipc: X11 shared memory |
| `--device=all` | the GPU (dri) and the headset's and controllers' USB/HID devices |
| `--device=shm` | the host's `/dev/shm` (`--device=all` alone mounts the sandbox's own there): SteamVR's client looks there for its server |
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
| OpenXR runtime detection (`vr_xr_runtime.cpp`) | active runtime from `active_runtime.json` (XDG config dirs, `/etc`), available manifests in `/usr/share/openxr/1` and `/usr/local/share/openxr/1`, running processes from `/proc` (SteamVR's `vrserver`, `vrcompositor`, `vrmonitor`: by `comm`, the executable's name or argv[0]/argv[1]'s; in a Flatpak sandbox "can't tell"); `XR_RUNTIME_JSON` set with `setenv`. No implicit-layer switch (that reads the Windows registry). |
| OpenXR loader messages | the loader writes its errors to stderr (all its messages to stdout with `XR_LOADER_DEBUG`): caught while it loads a runtime, into `qvr_openxr.txt` ("OpenXR loader output (...)" lines), a failed `xrCreateInstance`'s errors to the console. `vr_xr_debug 1` sets `XR_LOADER_DEBUG=all`. |
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
