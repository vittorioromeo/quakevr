#!/bin/bash
# build.sh -- the quakevr module of io.github.vittorioromeo.QuakeVR.yml, run by flatpak-builder in the sandbox from its
# copy of the checkout (no .git: the manifest's dir source skips it). Compiles the QuakeC (fteqcc, the module before),
# builds the engine (Release, clang) and installs into /app:
#   /app/lib/quakevr/ironwail, ironwail.pak    the engine and its pak (found beside the executable)
#   /app/share/quakevr/quakevr/                the game folder: what git tracks under quakevr/ (less the development
#                                              files, as Windows/package-quakevr.ps1) and progs.dat. Never id's data.
#   /app/bin/quakevr                           the launcher (quakevr.sh): finds the player's Quake, runs the engine
#   the desktop entry, AppStream metadata and icon
set -euo pipefail
dest="${FLATPAK_DEST:-/app}"
id="${FLATPAK_ID:-io.github.vittorioromeo.QuakeVR}"
here=Misc/linux/flatpak
jobs="${FLATPAK_BUILDER_N_JOBS:-$(nproc)}"

echo "== QuakeC (fteqcc)"
# QC/build.sh's compile (the release flavour: no QC tests, -DQVR_NO_TESTS) without its checks (TrenchBroom FGD, operator precedence: the Windows build runs them).
(cd QC && fteqcc -DQVR_NO_TESTS -O3 -Fautoproto -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208 \
    > ../qc-build.log 2>&1) || { tail -40 qc-build.log; echo "QC compilation failed"; exit 1; }
test -f quakevr/progs.dat || { echo "no quakevr/progs.dat after the QC compile"; exit 1; }

echo "== Engine (CMake, Release, ${CC:-cc})"
# No user dirs: the launcher passes the base dirs (the Quake install, then the writable Quake VR folder).
cmake -S . -B build-flatpak -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_USERDIRS=OFF -DQVR_OPENXR=ON \
    | tee cmake-configure.log | grep -E "Quake VR|OpenXR" || true
grep -q "OpenXR backend on" cmake-configure.log || { echo "the OpenXR backend is off (no pkg-config openxr or X11)"; exit 1; }
cmake --build build-flatpak --parallel "$jobs"

echo "== Install"
install -Dm755 build-flatpak/ironwail "$dest/lib/quakevr/ironwail"
install -Dm644 Quake/ironwail.pak "$dest/lib/quakevr/ironwail.pak"

# The game folder's allowlist: the files git would track under quakevr/ (a fresh repository here: the .gitignore files
# leave out every build output, config, save, cache and dump; CI's checkout has nothing untracked), less git's own file
# and the motion recorder's verdicts (Windows/package-quakevr.ps1's $devOnly), plus the compiled QuakeC.
git init -q .
# Regular files only: a local checkout's links (a "relit" link to elsewhere: git sees a link, not the ignored folder)
# are left out, and named.
git ls-files -z --others --exclude-standard -- quakevr \
    | grep -zv -e '^quakevr/\.gitignore$' -e '^quakevr/motions/' \
    | while IFS= read -r -d '' f; do
        if [ -f "$f" ] && [ ! -L "$f" ]; then printf '%s\0' "$f"; else echo "left out (not a regular file): $f" >&2; fi
    done > game-files.lst
printf 'quakevr/progs.dat\0' >> game-files.lst
count=$(tr -cd '\0' < game-files.lst | wc -c)
[ "$count" -gt 1000 ] || { echo "the game folder's list has only $count files"; exit 1; }
mkdir -p "$dest/share/quakevr"
xargs -0 cp --parents -t "$dest/share/quakevr" < game-files.lst
chmod -R u+rwX,go+rX,go-w "$dest/share/quakevr"
tr -d '\r\n' < VERSION > "$dest/share/quakevr/VERSION"
echo "game folder: $count files, $(du -sh "$dest/share/quakevr/quakevr" | cut -f1)"

install -Dm755 "$here/quakevr.sh" "$dest/bin/quakevr"
install -Dm644 "$here/$id.desktop" "$dest/share/applications/$id.desktop"
install -Dm644 "$here/$id.metainfo.xml" "$dest/share/metainfo/$id.metainfo.xml"
# (a copy of the installer's Assets/logo_square.png)
install -Dm644 "$here/icon-512.png" "$dest/share/icons/hicolor/512x512/apps/$id.png"
