#!/bin/bash
# make_tarball.sh <prefix> [out dir] -- QuakeVR-linux-x86_64.tar.gz, the plain Linux bundle (docs/vr-port/LINUX.md,
# "Linux tarball"), from the Flatpak build: <prefix> is the Flatpak's /app as flatpak-builder left it (its build
# directory's files/: CI's flatpak_app/files). The same engine, pak, OpenXR loader and game folder as the Flatpak, with
# the same launcher (Misc/linux/flatpak/quakevr.sh, which sees ironwail beside it and runs as the tarball's).
#
#   QuakeVR/ironwail, ironwail.pak     the engine and its pak
#   QuakeVR/libopenxr_loader.so.1      the OpenXR loader the engine links (the runtime is always the system's)
#   QuakeVR/quakevr/                   the game folder (what git tracks under quakevr/, and progs.dat). Never id's data.
#   QuakeVR/quakevr.sh                 the launcher: finds Quake, runs the engine with LD_LIBRARY_PATH on this folder
#   QuakeVR/README.txt, VERSION        how to run it; the glibc it needs (read from the binaries here)
#
# The staged QuakeVR/ folder is left in <out dir> beside the tarball (nothing is deleted: an existing one stops it).
set -euo pipefail
prefix="${1:?usage: make_tarball.sh <prefix: the Flatpak build dir's files/> [out dir]}"
out="${2:-.}"
here=$(cd "$(dirname "$(readlink -f "$0")")" && pwd -P)
name=QuakeVR-linux-x86_64.tar.gz
stage="$out/QuakeVR"

for f in lib/quakevr/ironwail lib/quakevr/ironwail.pak lib/libopenxr_loader.so.1 share/quakevr/quakevr/progs.dat \
    share/quakevr/VERSION; do
    [ -e "$prefix/$f" ] || { echo "make_tarball: no $prefix/$f (the Flatpak build's files/ folder?)" >&2; exit 1; }
done
[ ! -e "$stage" ] || { echo "make_tarball: $stage exists already (move it away first)" >&2; exit 1; }
mkdir -p "$stage"

install -m755 "$prefix/lib/quakevr/ironwail" "$stage/ironwail"
install -m644 "$prefix/lib/quakevr/ironwail.pak" "$stage/ironwail.pak"
# The file the engine's NEEDED entry names (the loader's soname), not the links to it.
install -m755 "$(readlink -f "$prefix/lib/libopenxr_loader.so.1")" "$stage/libopenxr_loader.so.1"
cp -R "$prefix/share/quakevr/quakevr" "$stage/quakevr"
chmod -R u+rwX,go+rX,go-w "$stage/quakevr"
install -m644 "$prefix/share/quakevr/VERSION" "$stage/VERSION"
install -m755 "$here/../flatpak/quakevr.sh" "$stage/quakevr.sh"

# The newest glibc and libstdc++ symbol versions the bundled binaries ask for (their .dynstr names them): the oldest
# system that runs the bundle.
newest() { grep -aohE "$1" "$stage/ironwail" "$stage/libopenxr_loader.so.1" | sort -uV | tail -n 1; }
glibc=$(newest 'GLIBC_2\.[0-9]+' | sed 's/GLIBC_//')
glibcxx=$(newest 'GLIBCXX_3\.4\.[0-9]+' | sed 's/GLIBCXX_//')
version=$(tr -d '\r\n' < "$stage/VERSION")
sed -e "s/@VERSION@/$version/g" -e "s/@GLIBC@/${glibc:-?}/g" -e "s/@GLIBCXX@/${glibcxx:-none}/g" \
    "$here/README.txt" > "$stage/README.txt"

tar -C "$out" --owner=0 --group=0 --numeric-owner --sort=name -czf "$out/$name" QuakeVR
echo "make_tarball: $out/$name ($(du -h "$out/$name" | cut -f1), version $version): needs glibc $glibc," \
    "libstdc++ GLIBCXX ${glibcxx:-none}"
