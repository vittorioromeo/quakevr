#!/bin/bash
# quakevr_sh_test.sh [launcher] [work dir] -- the Linux launcher's Quake finding (Misc/linux/flatpak/quakevr.sh, the
# Flatpak's and the tarball's), on fake home folders: every Steam layout (native, libraryfolders.vdf in steamapps/ and
# config/, Debian's, Flatpak and Snap Steam), the rerelease folder, GOG/Heroic, the env and --quake-dir overrides and the
# remembered folder, no Quake at all (the message), the engine's command line (a stub engine prints it) and the
# Flatpak's override hint. Fake pak0.pak files (empty); nothing outside <work dir> (default: a new folder under /tmp)
# is read or written. Ends with "quakevr_sh_test: N passed, M failed" (exit 1 on a failure).
set -u
here=$(cd "$(dirname "$(readlink -f "$0")")" && pwd -P)
launcher=$(readlink -f "${1:-$here/../flatpak/quakevr.sh}")
work="${2:-$(mktemp -d /tmp/quakevr_sh_test.XXXXXX)}"
mkdir -p "$work"
pass=0; fail=0; case_n=0

ok()  { pass=$((pass + 1)); echo "  ok   $case_n $1"; }
bad() { fail=$((fail + 1)); echo "  FAIL $case_n $1: $2"; }

# A fresh fake home and bundle (ironwail: a stub that prints its LD_LIBRARY_PATH and arguments) per case.
new_case() {
    case_n=$((case_n + 1))
    H="$work/case$case_n/home"; B="$work/case$case_n/QuakeVR"
    mkdir -p "$H" "$B/quakevr"
    cp "$launcher" "$B/quakevr.sh"
    printf '#!/bin/sh\necho "ENGINE LD=$LD_LIBRARY_PATH ARGS=$*"\n' > "$B/ironwail"
    chmod +x "$B/quakevr.sh" "$B/ironwail"
}
pak() { mkdir -p "$1/id1" && : > "$1/id1/pak0.pak"; }
launch() { env -i HOME="$H" PATH="$PATH" "$@" 2>"$work/case$case_n/stderr"; }

# expect <label> <expected Quake folder> [env assignments...] -- [launcher arguments]
expect() {
    label="$1"; want="$2"; shift 2
    envs=()
    while [ $# -gt 0 ] && [ "$1" != "--" ]; do envs+=("$1"); shift; done
    [ $# -gt 0 ] && shift
    got=$(launch "${envs[@]}" "$B/quakevr.sh" --dry-run "$@" | sed -n 's/^quake: //p')
    if [ "$got" = "$want" ]; then ok "$label"; else bad "$label" "got '$got', want '$want'"; fi
}

echo "quakevr_sh_test: $launcher (work $work)"

new_case; expect "no Quake anywhere" "(not found)"
if grep -q -- "--quake-dir /path/to/Quake" "$work/case$case_n/stderr"; then ok "no Quake: says what to run"
else bad "no Quake: says what to run" "$(head -c 300 "$work/case$case_n/stderr")"; fi

new_case; pak "$H/.local/share/Steam/steamapps/common/Quake"
expect "native Steam" "$H/.local/share/Steam/steamapps/common/Quake"

new_case; mkdir -p "$H/.local/share/Steam/steamapps/common/Quake/Id1"
: > "$H/.local/share/Steam/steamapps/common/Quake/Id1/PAK0.PAK"; pak "$H/.local/share/Steam/steamapps/common/Quake/rerelease"
expect "Steam rerelease folder" "$H/.local/share/Steam/steamapps/common/Quake/rerelease"

new_case; mkdir -p "$H/.local/share/Steam/steamapps"; pak "$work/case$case_n/disk2/SteamLibrary/steamapps/common/Quake"
printf '"libraryfolders"\n{\n\t"0"\n\t{\n\t\t"path"\t\t"%s"\n\t}\n\t"1"\n\t{\n\t\t"path"\t\t"%s"\n\t\t"label"\t\t""\n\t}\n}\n' \
    "$H/.local/share/Steam" "$work/case$case_n/disk2/SteamLibrary" > "$H/.local/share/Steam/steamapps/libraryfolders.vdf"
expect "second library (steamapps/libraryfolders.vdf)" "$work/case$case_n/disk2/SteamLibrary/steamapps/common/Quake"

new_case; mkdir -p "$H/.steam/steam/steamapps" "$H/.steam/steam/config"; pak "$work/case$case_n/games/SteamLibrary/steamapps/common/Quake"
printf '"libraryfolders"\n{\n\t"1"\n\t{\n\t\t"path"\t\t"%s"\n\t}\n}\n' "$work/case$case_n/games/SteamLibrary" \
    > "$H/.steam/steam/config/libraryfolders.vdf"
expect "second library (config/libraryfolders.vdf, ~/.steam/steam)" "$work/case$case_n/games/SteamLibrary/steamapps/common/Quake"

new_case; pak "$H/.steam/debian-installation/steamapps/common/Quake"
expect "Debian's Steam package" "$H/.steam/debian-installation/steamapps/common/Quake"

new_case; pak "$H/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/Quake"
expect "Flatpak Steam" "$H/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/Quake"

new_case; pak "$H/snap/steam/common/.local/share/Steam/steamapps/common/Quake"
expect "Snap Steam" "$H/snap/steam/common/.local/share/Steam/steamapps/common/Quake"

new_case; pak "$H/GOG Games/Quake"
expect "GOG (a folder name with a space)" "$H/GOG Games/Quake"

new_case; pak "$H/Games/Heroic/Quake/rerelease"
expect "Heroic, rerelease" "$H/Games/Heroic/Quake/rerelease"

new_case; pak "$H/GOG Games/Quake"; pak "$H/.local/share/Steam/steamapps/common/Quake"
expect "Steam before GOG" "$H/.local/share/Steam/steamapps/common/Quake"

new_case; pak "$H/.local/share/Steam/steamapps/common/Quake"; pak "$work/case$case_n/elsewhere"
expect "QUAKEVR_QUAKE_DIR before Steam" "$work/case$case_n/elsewhere" QUAKEVR_QUAKE_DIR="$work/case$case_n/elsewhere"

new_case; pak "$work/case$case_n/env"; pak "$work/case$case_n/arg"
expect "--quake-dir before QUAKEVR_QUAKE_DIR" "$work/case$case_n/arg" QUAKEVR_QUAKE_DIR="$work/case$case_n/env" -- \
    --quake-dir "$work/case$case_n/arg"
expect "--quake-dir=<folder>" "$work/case$case_n/arg" -- --quake-dir="$work/case$case_n/arg"

new_case; pak "$H/.local/share/Steam/steamapps/common/Quake"
expect "a bad --quake-dir falls back to the search" "$H/.local/share/Steam/steamapps/common/Quake" -- --quake-dir "$work/nothing"
if grep -q "no id1/pak0.pak there" "$work/case$case_n/stderr"; then ok "a bad --quake-dir is named"
else bad "a bad --quake-dir is named" "$(head -c 300 "$work/case$case_n/stderr")"; fi

# A real run (the stub engine): the command line, LD_LIBRARY_PATH on the bundle, the folder remembered.
new_case; pak "$work/case$case_n/Quake"
line=$(launch LD_LIBRARY_PATH=/opt/x "$B/quakevr.sh" --quake-dir "$work/case$case_n/Quake" -novr +map e1m1)
want="ENGINE LD=$B:/opt/x ARGS=-basedir $work/case$case_n/Quake -basedir $B -game quakevr -novr +map e1m1"
if [ "$line" = "$want" ]; then ok "engine command line and LD_LIBRARY_PATH"; else bad "engine command line" "got '$line'"; fi
expect "the remembered folder" "$work/case$case_n/Quake"

# Started through a link (~/bin/quakevr): still the tarball beside the real script.
new_case; pak "$H/quake"; mkdir -p "$H/bin"; ln -s "$B/quakevr.sh" "$H/bin/quakevr"
got=$(launch "$H/bin/quakevr" --dry-run | sed -n 's/^run: //p')
case "$got" in "$B/ironwail -basedir $H/quake -basedir $B -game quakevr"*) ok "through a link";;
    *) bad "through a link" "got '$got'";; esac

# The Flatpak's mode (no ironwail beside the script): a Steam library it can't see gets its override command.
new_case; mkdir -p "$work/case$case_n/fp" "$H/.local/share/Steam/steamapps"; cp "$launcher" "$work/case$case_n/fp/quakevr"
printf '"libraryfolders"\n{\n\t"1"\n\t{\n\t\t"path"\t\t"/mnt/not-shared/SteamLibrary"\n\t}\n}\n' \
    > "$H/.local/share/Steam/steamapps/libraryfolders.vdf"
got=$(launch sh "$work/case$case_n/fp/quakevr" --dry-run | sed -n 's/^quake: //p')
if [ "$got" = "(not found)" ] && grep -q "flatpak override --user --filesystem=/mnt/not-shared/SteamLibrary:ro" \
    "$work/case$case_n/stderr"; then ok "Flatpak: the override for an unseen library"
else bad "Flatpak: the override for an unseen library" "got '$got': $(head -c 400 "$work/case$case_n/stderr")"; fi

echo "quakevr_sh_test: $pass passed, $fail failed"
[ "$fail" = 0 ]
