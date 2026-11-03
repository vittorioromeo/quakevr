#!/usr/bin/env python3
"""Fails unless our progs.dat is the 32-bit format, and prints its globals count.

Our QC compiles to FTE's extended progs (version 7, secondary version FTE32: 32-bit statements and defs, so no
65,536-globals limit): `#pragma flag enable fte32` in QC/defs.qc, a flag of our FTEQCC (Tools/fteqcc/README.md). A
stock FTEQCC ignores the pragma with a warning and writes id's 16-bit version 6, which still runs today but would
silently stop compiling (or start writing FTE32 only on overflow) as the globals grow: this check makes that loud.
The engine reads both (PR_LoadProgs).

Usage: python Misc/quakevr/check_progs_format.py [progs.dat]   (default quakevr/progs.dat; from the repository's root)
Prints one line: the format and the counts. Exit 1 when it is not FTE32 (or not a progs.dat).
"""
import pathlib
import struct
import sys

PROG_VERSION = 6
PROG_EXTENDEDVERSION = 7


def _fourcc(s):
    return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24


SECONDARY16 = _fourcc(b"1FTE") ^ _fourcc(b"PROG")
SECONDARY32 = _fourcc(b"1FTE") ^ _fourcc(b"32B ")


def progs_format(data):
    """(format name, header ints) of a progs.dat's bytes: 'id16', 'fte16', 'fte32', or another description."""
    if len(data) < 60:
        return "not a progs.dat (too short)", None
    h = struct.unpack("<15i", data[:60])
    if h[0] == PROG_VERSION:
        return "id16", h
    if h[0] == PROG_EXTENDEDVERSION and len(data) >= 92:
        ext = struct.unpack("<8i", data[60:92])
        compressed, secondary = ext[6], ext[7]
        kind = {SECONDARY16: "fte16", SECONDARY32: "fte32"}.get(secondary, "version 7 kind %08x" % (secondary & 0xFFFFFFFF))
        return kind + (" compressed" if compressed else ""), h
    return "version %d" % h[0], h


def main():
    path = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "quakevr/progs.dat")
    try:
        data = path.read_bytes()
    except OSError as e:
        print("check_progs_format: %s: %s" % (path, e))
        return 1
    fmt, h = progs_format(data)
    if h is None:
        print("check_progs_format: %s: %s" % (path, fmt))
        return 1
    print("check_progs_format: %s: %s, %d globals, %d statements, %d global defs, %d field defs, %d functions" % (
        path.as_posix(), fmt, h[13], h[3], h[5], h[7], h[9]))
    if fmt != "fte32":
        print("check_progs_format: FAILED: not the 32-bit format (fte32). Compile with our FTEQCC "
              "(Tools/fteqcc/fteqcc64.exe, or one built with Tools/fteqcc/fte32.patch): QC/defs.qc's "
              "`#pragma flag enable fte32` selects it")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
