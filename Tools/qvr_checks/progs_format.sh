#!/bin/sh
# progs_format.sh [progs.dat]: `qvr_checks progs_format` where the Windows tool isn't built (the flatpak, QC/build.sh
# on Linux or macOS): fails unless progs.dat is FTE's 32-bit format (version 7, secondary version FTE32, not
# compressed: QC/defs.qc's `#pragma flag enable fte32`, a flag of our FTEQCC: Tools/fteqcc/README.md).
# (Header ints, little-endian: 0 the version, 52 the globals, 84 compressed, 88 the secondary version.)
f="${1:-quakevr/progs.dat}"
v=$(od -An -tu4 -N4 "$f" 2>/dev/null | tr -d ' ')
g=$(od -An -tu4 -j52 -N4 "$f" 2>/dev/null | tr -d ' ')
c=$(od -An -tu4 -j84 -N4 "$f" 2>/dev/null | tr -d ' ')
s=$(od -An -tx4 -j88 -N4 "$f" 2>/dev/null | tr -d ' ')
# FTE32: "1FTE" ^ "32B " as little-endian ints
if [ "$v" = 7 ] && [ "$c" = 0 ] && [ "$s" = 65167402 ]; then
    echo "check_progs_format: $f: fte32, $g globals"
    exit 0
fi
echo "check_progs_format: $f: FAILED: not the 32-bit format (fte32; version ${v:-?}). Compile with our FTEQCC" \
    "(Tools/fteqcc/fteqcc64.exe, or one built with Tools/fteqcc/fte32.patch): QC/defs.qc's" \
    "\`#pragma flag enable fte32\` selects it"
exit 1
