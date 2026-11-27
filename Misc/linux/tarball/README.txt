Quake VR: UNLEASHED @VERSION@ for Linux (x86-64): the plain tarball
=================================================================

The same game as the Flatpak (io.github.vittorioromeo.QuakeVR), unsandboxed: it loads your system's OpenXR runtime
(Monado, WiVRn, Envision's Monado, SteamVR) directly, with your system's libraries. Use it when the Flatpak can't load
your runtime (a runtime built for a newer glibc than the Flatpak's, "GLIBC_2.xx not found", or one installed from your
distribution's packages, which the sandbox can't see).

You need your own Quake (Steam, GOG, or any folder with id1/pak0.pak). Nothing of id Software's is in here.

Run
---
    tar xzf QuakeVR-linux-x86_64.tar.gz
    cd QuakeVR
    ./quakevr.sh

quakevr.sh finds Quake in every Steam library (native, Flatpak and Snap Steam; libraryfolders.vdf), ~/GOG Games/Quake,
~/Games/Heroic/Quake, ~/Games/Quake, ~/quake and /usr/share/games/quake. Steam's and GOG's current Quake uses its
"rerelease" folder. Anywhere else, give the folder once (it is remembered in ./quake-dir):

    ./quakevr.sh --quake-dir /path/to/Quake        (or: QUAKEVR_QUAKE_DIR=/path/to/Quake ./quakevr.sh)

./quakevr.sh --flat plays on the monitor without a headset. Every other argument goes to the engine (for example
+map e1m1). ./quakevr.sh --dry-run prints the Quake folder it found and the command it would run. It runs:

    LD_LIBRARY_PATH=<this folder> ./ironwail -basedir <Quake> -basedir <this folder> -game quakevr

The game writes its config, saves, logs and screenshots into ./quakevr/ here. Your Quake folder is never written.

VR
--
The OpenXR runtime is your system's: the active one (~/.config/openxr/1/active_runtime.json, /etc/xdg/openxr/1/,
/etc/openxr/1/) or XR_RUNTIME_JSON=/path/to/manifest.json. Only the OpenXR loader is bundled
(libopenxr_loader.so.1, 1.1.65). SteamVR: start SteamVR first; the game uses X11 (XWayland on Wayland) with it.

What it needs from your system
------------------------------
glibc @GLIBC@ or newer (Ubuntu 24.04, Debian 13, Fedora 39 or newer; current Arch and openSUSE Tumbleweed),
libstdc++ with GLIBCXX_@GLIBCXX@ (gcc 13 or newer), SDL2 (libSDL2-2.0.so.0: sdl2 or sdl2-compat), OpenGL
(libGL/libOpenGL, mesa or your GPU driver), libX11 and libcurl. Most desktops have them all; on a minimal system:
    Arch: sdl2-compat libglvnd libx11 curl          Debian/Ubuntu: libsdl2-2.0-0 libopengl0 libglx0 libx11-6 libcurl4t64
    Fedora: sdl2-compat libglvnd-opengl libglvnd-glx libX11 libcurl
The music codecs (mpg123, Opus, FLAC, Vorbis) are bundled here.

More: https://github.com/vittorioromeo/quakevr (docs/vr-port/LINUX.md).
