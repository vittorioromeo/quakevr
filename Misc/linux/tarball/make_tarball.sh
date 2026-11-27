#!/bin/bash
# make_tarball.sh <prefix> [out dir] [runtime lib dir] -- QuakeVR-linux-x86_64.tar.gz, the plain Linux bundle
# (docs/vr-port/LINUX.md, "Linux tarball"), from the Flatpak build: <prefix> is the Flatpak's /app as flatpak-builder
# left it (files/ in its build directory: CI's flatpak_app/files). The same engine, pak, OpenXR loader and game folder
# as the Flatpak, with the same launcher (Misc/linux/flatpak/quakevr.sh, which sees ironwail beside it and runs as the
# tarball's).
#
#   QuakeVR/ironwail, ironwail.pak     the engine and its pak
#   QuakeVR/libopenxr_loader.so.1      the OpenXR loader the engine links (the runtime is always the system's)
#   QuakeVR/lib*.so.N                  the music codecs the engine links, from the Flatpak's runtime (sonames differ
#                                      between distributions: FLAC 12 vs 14); everything else is the system's
#   QuakeVR/quakevr/                   the game folder (what git tracks under quakevr/, and progs.dat). Never id's data.
#   QuakeVR/quakevr.sh                 the launcher: finds Quake, runs the engine with LD_LIBRARY_PATH on this folder
#   QuakeVR/README.txt, VERSION        how to run it; the glibc it needs (read from the binaries here)
#
# <runtime lib dir>: the freedesktop runtime's lib/x86_64-linux-gnu (default: found with flatpak info, the manifest's
# runtime-version). Fails when the bundle would need a glibc newer than QVR_GLIBC_MAX (default 2.38: Ubuntu 24.04,
# Debian 13, Fedora 39) or links a library that is neither bundled nor on the system list below.
# The staged QuakeVR/ folder is left in <out dir> beside the tarball (nothing is deleted: an existing one stops it).
set -euo pipefail
prefix="${1:?usage: make_tarball.sh <prefix: files/ in the Flatpak build directory> [out dir] [runtime lib dir]}"
out="${2:-.}"
rtlib="${3:-}"
here=$(cd "$(dirname "$(readlink -f "$0")")" && pwd -P)
name=QuakeVR-linux-x86_64.tar.gz
stage="$out/QuakeVR"
glibc_max="${QVR_GLIBC_MAX:-2.38}"
# Bundled from the runtime: the engine's codec libraries and theirs.
bundled="libmpg123.so.0 libopusfile.so.0 libopus.so.0 libFLAC.so.14 libvorbisfile.so.3 libvorbis.so.0 libogg.so.0"
# The system's (never bundled: the driver stack, SDL for the desktop's video and audio, glibc, the C++ runtime, curl).
system_libs="libc.so.6 libm.so.6 libmvec.so.1 libstdc++.so.6 libgcc_s.so.1 libX11.so.6 libGLX.so.0 libOpenGL.so.0"
system_libs="$system_libs libGL.so.1 libEGL.so.1 libSDL2-2.0.so.0 libcurl.so.4 ld-linux-x86-64.so.2"

if [ -z "$rtlib" ]; then
    rv=$(sed -n "s/^runtime-version: *'\{0,1\}\([0-9.]*\)'\{0,1\}.*/\1/p" "$here/../flatpak/io.github.vittorioromeo.QuakeVR.yml")
    loc=$(flatpak info --show-location "org.freedesktop.Platform//$rv" 2>/dev/null || true)
    [ -n "$loc" ] || { echo "make_tarball: the runtime org.freedesktop.Platform//$rv is not installed (or give its lib dir)" >&2; exit 1; }
    rtlib="$loc/files/lib/x86_64-linux-gnu"
fi
for f in lib/quakevr/ironwail lib/quakevr/ironwail.pak lib/libopenxr_loader.so.1 share/quakevr/quakevr/progs.dat \
    share/quakevr/VERSION; do
    [ -e "$prefix/$f" ] || { echo "make_tarball: no $prefix/$f (the Flatpak build's files/ folder?)" >&2; exit 1; }
done
[ ! -e "$stage" ] || { echo "make_tarball: $stage exists already (move it away first)" >&2; exit 1; }
mkdir -p "$stage"

install -m755 "$prefix/lib/quakevr/ironwail" "$stage/ironwail"
install -m644 "$prefix/lib/quakevr/ironwail.pak" "$stage/ironwail.pak"
# The files the NEEDED entries name (the sonames), not the links to them.
install -m755 "$(readlink -f "$prefix/lib/libopenxr_loader.so.1")" "$stage/libopenxr_loader.so.1"
for lib in $bundled; do
    [ -e "$rtlib/$lib" ] || { echo "make_tarball: no $rtlib/$lib (a new soname in the runtime? update the list)" >&2; exit 1; }
    install -m755 "$(readlink -f "$rtlib/$lib")" "$stage/$lib"
done
cp -R "$prefix/share/quakevr/quakevr" "$stage/quakevr"
chmod -R u+rwX,go+rX,go-w "$stage/quakevr"
install -m644 "$prefix/share/quakevr/VERSION" "$stage/VERSION"
install -m755 "$here/../flatpak/quakevr.sh" "$stage/quakevr.sh"

# Every library the bundled binaries link (their NEEDED entries): bundled here or the system's.
if command -v readelf > /dev/null; then
    for f in "$stage/ironwail" "$stage"/lib*.so.*; do
        for dep in $(readelf -d "$f" | sed -n 's/.*Shared library: \[\(.*\)\].*/\1/p'); do
            [ -e "$stage/$dep" ] && continue
            case " $system_libs " in *" $dep "*) continue ;; esac
            echo "make_tarball: $(basename "$f") links $dep: neither bundled nor on the system list" >&2; exit 1
        done
    done
else
    echo "make_tarball: no readelf here: the linked libraries are not checked"
fi

# The newest glibc and libstdc++ symbol versions the bundled binaries ask for (their .dynstr names them): the oldest
# system that runs the bundle.
newest() { grep -aohE "$1" "$stage/ironwail" "$stage"/lib*.so.* | sort -uV | tail -n 1; }
glibc=$(newest 'GLIBC_2\.[0-9]+' | sed 's/GLIBC_//')
glibcxx=$(newest 'GLIBCXX_3\.4\.[0-9]+' | sed 's/GLIBCXX_//')
if [ "$(printf '%s\n%s\n' "$glibc" "$glibc_max" | sort -V | tail -n 1)" != "$glibc_max" ]; then
    echo "make_tarball: the bundle needs glibc $glibc, newer than $glibc_max (QVR_GLIBC_MAX): which symbol?" >&2
    echo "  objdump -T $stage/ironwail | grep GLIBC_$glibc" >&2
    exit 1
fi
version=$(tr -d '\r\n' < "$stage/VERSION")
sed -e "s/@VERSION@/$version/g" -e "s/@GLIBC@/${glibc:-?}/g" -e "s/@GLIBCXX@/${glibcxx:-none}/g" \
    "$here/README.txt" > "$stage/README.txt"

tar -C "$out" --owner=0 --group=0 --numeric-owner --sort=name -czf "$out/$name" QuakeVR
echo "make_tarball: $out/$name ($(du -h "$out/$name" | cut -f1), version $version): needs glibc $glibc," \
    "libstdc++ GLIBCXX ${glibcxx:-none}"
