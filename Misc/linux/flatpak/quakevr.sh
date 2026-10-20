#!/bin/sh
# quakevr -- the Flatpak's launcher (/app/bin/quakevr; docs/vr-port/LINUX.md, "Flatpak").
#
#   flatpak run io.github.vittorioromeo.QuakeVR [--quake-dir <folder with id1>] [engine arguments...]
#
# Runs the engine as the Windows installer's shortcuts do: "-basedir <Quake> -basedir <Quake VR> -game quakevr".
# - <Quake>: the player's install, read in place (never written): --quake-dir (remembered), $QUAKEVR_QUAKE_DIR, the
#   remembered folder, else the Steam libraries (native and Flatpak Steam) and the usual GOG/Heroic folders. A folder
#   without id1/pak0.pak but with rerelease/id1/pak0.pak (Steam's and GOG's current Quake) uses the rerelease.
# - <Quake VR>: ~/.var/app/<id>/data/QuakeVR, writable: its quakevr/ holds links to the shipped game files
#   (/app/share/quakevr/quakevr, refreshed at every start) beside what the game writes (config, saves, logs, crash
#   reports, screenshots).
# - OpenXR: ~/.config is added to XDG_CONFIG_DIRS, so the loader and the game find the active runtime the host set
#   (~/.config/openxr/1/active_runtime.json, readable through the manifest's xdg-config/openxr).
set -u
id=io.github.vittorioromeo.QuakeVR
shipped=/app/share/quakevr/quakevr
engine=/app/lib/quakevr/ironwail
data="${XDG_DATA_HOME:-$HOME/.var/app/$id/data}"
conf="${XDG_CONFIG_HOME:-$HOME/.var/app/$id/config}"
base="$data/QuakeVR"
remembered="$conf/quake-dir"

say() { echo "quakevr: $*" >&2; }

# A Quake folder: the one with id1/pak0.pak (lower case, as Ironwail looks for it), or its rerelease folder.
quake_in() {
    [ -n "$1" ] || return 1
    if [ -f "$1/id1/pak0.pak" ]; then echo "$1"; return 0; fi
    if [ -f "$1/rerelease/id1/pak0.pak" ]; then echo "$1/rerelease"; return 0; fi
    return 1
}

# Steam libraries: each Steam root and the "path" entries of its libraryfolders.vdf.
steam_libraries() {
    for root in "$HOME/.local/share/Steam" "$HOME/.steam/steam" "$HOME/.steam/root" \
        "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam" "$HOME/.var/app/com.valvesoftware.Steam/data/Steam"; do
        [ -d "$root/steamapps" ] || continue
        echo "$root"
        vdf="$root/steamapps/libraryfolders.vdf"
        [ -f "$vdf" ] && sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"[[:space:]]*$/\1/p' "$vdf"
    done
}

find_quake() {
    found=$(steam_libraries | while IFS= read -r lib; do quake_in "$lib/steamapps/common/Quake" && break; done)
    if [ -n "$found" ]; then echo "$found"; return 0; fi
    for d in "$HOME/GOG Games/Quake" "$HOME/GOG Games/Quake The Offering" "$HOME/Games/Heroic/Quake" \
        "$HOME/Games/Heroic/Quake The Offering" "$HOME/Games/Quake" "$HOME/Games/quake" "$HOME/quake" "$HOME/Quake"; do
        quake_in "$d" && return 0
    done
    return 1
}

# --quake-dir <folder> / --quake-dir=<folder>: used and remembered; every other argument goes to the engine.
given=""
n=$#
while [ "$n" -gt 0 ]; do
    a="$1"; shift; n=$((n - 1))
    case "$a" in
        --quake-dir=*) given="${a#--quake-dir=}" ;;
        --quake-dir) if [ "$n" -gt 0 ]; then given="$1"; shift; n=$((n - 1)); fi ;;
        *) set -- "$@" "$a" ;;
    esac
done

quake=""
if [ -n "$given" ]; then
    if quake=$(quake_in "$given"); then
        mkdir -p "$conf" && printf '%s\n' "$given" > "$remembered"
        say "Quake folder remembered: $given"
    else
        say "--quake-dir $given: no id1/pak0.pak there (nor in its rerelease folder), or the sandbox can't read it"
        quake=""
    fi
fi
[ -z "$quake" ] && [ -n "${QUAKEVR_QUAKE_DIR:-}" ] && quake=$(quake_in "$QUAKEVR_QUAKE_DIR")
if [ -z "$quake" ] && [ -f "$remembered" ]; then
    last=$(head -n 1 "$remembered")
    quake=$(quake_in "$last") || say "the remembered Quake folder $last: no id1/pak0.pak seen (outside home? see below)"
fi
[ -z "$quake" ] && quake=$(find_quake)

# The writable Quake VR folder: links to the shipped files (new ones added, ones a newer version dropped removed),
# never over a file the game wrote.
mkdir -p "$base/quakevr"
cp -rsn "$shipped/." "$base/quakevr/" 2>/dev/null
find "$base/quakevr" -xtype l -lname "$shipped/*" -delete 2>/dev/null

# OpenXR: the host's active runtime. The loader reads XDG_CONFIG_HOME (~/.var/app/<id>/config, where Flatpak shows the
# shared ~/.config/openxr too), then XDG_CONFIG_DIRS: ~/.config there as well, in case.
export XDG_CONFIG_DIRS="${XDG_CONFIG_DIRS:-/etc/xdg}:$HOME/.config"

if [ -z "$quake" ]; then
    say "Quake not found. Run: flatpak run $id --quake-dir /path/to/Quake (the folder with id1/pak0.pak)."
    say "Quake outside your home folder also needs: flatpak override --user --filesystem=/path/to/Quake:ro $id"
    # The engine's error dialog shows this as the folder it could not use.
    exec "$engine" -basedir "Quake not found: run 'flatpak run $id --quake-dir /path/to/Quake'" -game quakevr "$@"
fi
say "Quake: $quake; Quake VR: $base"
exec "$engine" -basedir "$quake" -basedir "$base" -game quakevr "$@"
