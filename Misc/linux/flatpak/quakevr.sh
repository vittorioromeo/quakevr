#!/bin/sh
# quakevr.sh -- the Linux launcher (docs/vr-port/LINUX.md, "Flatpak" and "Linux tarball"), one script for both builds:
#
#   flatpak run io.github.vittorioromeo.QuakeVR [options] [engine arguments...]   (the Flatpak: /app/bin/quakevr)
#   ./quakevr.sh [options] [engine arguments...]      (the tarball QuakeVR-linux-x86_64.tar.gz: beside ironwail)
#
#   --quake-dir <folder>   the Quake install (the folder with id1/pak0.pak, or its rerelease folder); remembered
#   --flat                 flat-screen mode, no headset (the engine's -flatscreenmode, as the Windows "flat" shortcut)
#   --dry-run              print the Quake folder and the engine's command line, run nothing
#   --help                 this text
#
# Runs the engine as the Windows installer's shortcuts do: "-basedir <Quake> -basedir <Quake VR> -game quakevr".
# - <Quake>: the player's install, read in place (never written): --quake-dir (remembered), $QUAKEVR_QUAKE_DIR, the
#   remembered folder, else the Steam libraries (native, Flatpak and Snap Steam, every libraryfolders.vdf "path"), the
#   usual GOG/Heroic/Lutris folders and Debian's game-data-packager folder. A folder without id1/pak0.pak but with
#   rerelease/id1/pak0.pak (Steam's and GOG's current Quake) uses the rerelease.
# - <Quake VR>, the Flatpak: ~/.var/app/<id>/data/QuakeVR, writable: its quakevr/ holds links to the shipped game files
#   (/app/share/quakevr/quakevr, refreshed at every start) beside what the game writes (config, saves, logs, crash
#   reports, screenshots). OpenXR: ~/.config is added to XDG_CONFIG_DIRS, so the loader and the game find the active
#   runtime the host set (~/.config/openxr/1/active_runtime.json, readable through the manifest's xdg-config/openxr).
# - <Quake VR>, the tarball: the folder this script is in (the game writes into its quakevr/). The bundled OpenXR loader
#   (libopenxr_loader.so.1) is found through LD_LIBRARY_PATH; the OpenXR runtime is always the system's (Monado, WiVRn,
#   SteamVR: the loader's usual active_runtime.json search, or XR_RUNTIME_JSON).
set -u
id=io.github.vittorioromeo.QuakeVR
self=$(readlink -f "$0" 2>/dev/null || echo "$0")
here=$(cd "$(dirname "$self")" && pwd -P)

if [ -x "$here/ironwail" ] && [ -d "$here/quakevr" ]; then
    # The tarball: everything beside this script.
    mode=tarball
    engine="$here/ironwail"
    base="$here"
    remembered="$here/quake-dir"
    run="$here/quakevr.sh"
else
    mode=flatpak
    shipped=/app/share/quakevr/quakevr
    engine=/app/lib/quakevr/ironwail
    data="${XDG_DATA_HOME:-$HOME/.var/app/$id/data}"
    conf="${XDG_CONFIG_HOME:-$HOME/.var/app/$id/config}"
    base="$data/QuakeVR"
    remembered="$conf/quake-dir"
    run="flatpak run $id"
fi

say() { echo "quakevr: $*" >&2; }

# A Quake folder: the one with id1/pak0.pak (lower case, as Ironwail looks for it), or its rerelease folder.
quake_in() {
    [ -n "$1" ] || return 1
    if [ -f "$1/id1/pak0.pak" ]; then echo "$1"; return 0; fi
    if [ -f "$1/rerelease/id1/pak0.pak" ]; then echo "$1/rerelease"; return 0; fi
    return 1
}

# Steam roots (one per line): native (~/.local/share, ~/.steam links, Debian's package), Flatpak and Snap Steam.
steam_roots="$HOME/.local/share/Steam
$HOME/.steam/steam
$HOME/.steam/root
$HOME/.steam/debian-installation
$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam
$HOME/.var/app/com.valvesoftware.Steam/data/Steam
$HOME/snap/steam/common/.local/share/Steam"

# Steam libraries: each Steam root and the "path" entries of its libraryfolders.vdf (steamapps/, or config/).
steam_libraries() {
    echo "$steam_roots" | while IFS= read -r root; do
        [ -d "$root/steamapps" ] || continue
        echo "$root"
        for vdf in "$root/steamapps/libraryfolders.vdf" "$root/config/libraryfolders.vdf"; do
            [ -f "$vdf" ] && sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"[[:space:]]*$/\1/p' "$vdf"
        done
    done
}

# GOG (its installer, Heroic, Lutris, minigalaxy), hand-made folders, Debian's game-data-packager.
other_places="$HOME/GOG Games/Quake
$HOME/GOG Games/Quake The Offering
$HOME/Games/Heroic/Quake
$HOME/Games/Heroic/Quake The Offering
$HOME/Games/gog/quake
$HOME/Games/Quake
$HOME/Games/quake
$HOME/quake
$HOME/Quake
/usr/share/games/quake"

find_quake() {
    found=$(steam_libraries | while IFS= read -r lib; do quake_in "$lib/steamapps/common/Quake" && break; done)
    if [ -n "$found" ]; then echo "$found"; return 0; fi
    found=$(echo "$other_places" | while IFS= read -r d; do quake_in "$d" && break; done)
    if [ -n "$found" ]; then echo "$found"; return 0; fi
    return 1
}

# Launcher options; every other argument goes to the engine.
given=""
dry=0
n=$#
while [ "$n" -gt 0 ]; do
    a="$1"; shift; n=$((n - 1))
    case "$a" in
        --quake-dir=*) given="${a#--quake-dir=}" ;;
        --quake-dir) if [ "$n" -gt 0 ]; then given="$1"; shift; n=$((n - 1)); fi ;;
        --dry-run) dry=1 ;;
        --flat) set -- "$@" -flatscreenmode ;;
        --help) sed -n '2,10p' "$self" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) set -- "$@" "$a" ;;
    esac
done

quake=""
if [ -n "$given" ]; then
    if quake=$(quake_in "$given"); then
        if [ "$dry" = 0 ]; then
            mkdir -p "$(dirname "$remembered")" && printf '%s\n' "$given" > "$remembered" &&
                say "Quake folder remembered: $given"
        fi
    elif [ "$mode" = flatpak ]; then
        say "--quake-dir $given: no id1/pak0.pak there (nor in its rerelease folder), or the sandbox can't read it"
    else
        say "--quake-dir $given: no id1/pak0.pak there (nor in its rerelease folder)"
    fi
fi
[ -z "$quake" ] && [ -n "${QUAKEVR_QUAKE_DIR:-}" ] && quake=$(quake_in "$QUAKEVR_QUAKE_DIR")
if [ -z "$quake" ] && [ -f "$remembered" ]; then
    last=$(head -n 1 "$remembered")
    quake=$(quake_in "$last") || say "the remembered Quake folder $last: no id1/pak0.pak seen there now"
fi
[ -z "$quake" ] && quake=$(find_quake)

if [ "$mode" = flatpak ]; then
    if [ "$dry" = 0 ]; then
        # The writable Quake VR folder: links to the shipped files (new ones added, ones a newer version dropped
        # removed), never over a file the game wrote.
        mkdir -p "$base/quakevr"
        cp -rsn "$shipped/." "$base/quakevr/" 2>/dev/null
        find "$base/quakevr" -xtype l -lname "$shipped/*" -delete 2>/dev/null
    fi
    # OpenXR: the host's active runtime. The loader reads XDG_CONFIG_HOME (~/.var/app/<id>/config, where Flatpak shows
    # the shared ~/.config/openxr too), then XDG_CONFIG_DIRS: ~/.config there as well, in case.
    export XDG_CONFIG_DIRS="${XDG_CONFIG_DIRS:-/etc/xdg}:$HOME/.config"
else
    # The bundled OpenXR loader (libopenxr_loader.so.1 beside the engine) before the system's.
    export LD_LIBRARY_PATH="$here${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

if [ -z "$quake" ]; then
    say "Quake not found. Give its folder (the one with id1/pak0.pak, or with rerelease/id1/pak0.pak) once:"
    say "    $run --quake-dir /path/to/Quake     (remembered; or set QUAKEVR_QUAKE_DIR=/path/to/Quake)"
    say "Looked in: every Steam library's steamapps/common/Quake (libraryfolders.vdf), ~/GOG Games/Quake,"
    say "    ~/Games/Heroic/Quake, ~/Games/Quake, ~/quake, /usr/share/games/quake."
    if [ "$mode" = flatpak ]; then
        # Steam libraries the sandbox can't see (another disk: only home is shared): the override sharing each.
        steam_libraries | while IFS= read -r lib; do
            [ -d "$lib" ] || say "Steam library $lib is not visible in the sandbox: flatpak override --user --filesystem=$lib:ro $id"
        done
        say "Quake outside your home folder also needs: flatpak override --user --filesystem=/path/to/Quake:ro $id"
        say "Or use the tarball QuakeVR-linux-x86_64.tar.gz (no sandbox; LINUX.md, \"Linux tarball\")."
    fi
    if [ "$dry" = 1 ]; then echo "quake: (not found)"; exit 1; fi
    # The engine's error dialog shows this as the folder it could not use.
    exec "$engine" -basedir "Quake not found: run '$run --quake-dir /path/to/Quake'" -game quakevr "$@"
fi
if [ "$dry" = 1 ]; then
    echo "quake: $quake"
    echo "run: $engine -basedir $quake -basedir $base -game quakevr $*"
    exit 0
fi
say "Quake: $quake; Quake VR: $base"
exec "$engine" -basedir "$quake" -basedir "$base" -game quakevr "$@"
