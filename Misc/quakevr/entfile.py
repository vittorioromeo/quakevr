#!/usr/bin/env python3
# entfile.py -- the external entity files (.ent) of Quake VR's own maps, pinned to their .bsp.
#
# The engine (Ironwail's Mod_LoadEntities, gl_model.c) loads maps/<map>@<crc>.ent in place of the .bsp's own
# entities, where <crc> is the CRC-16 of the .bsp's entity lump (4 lowercase hex digits), else a plain
# maps/<map>.ent. A plain one applies to whatever .bsp has that name: an old one left over in a player's folder
# (an older install copied over, a zip) put the old hub's entities on the new island's vrstart.bsp
# ("Mod_LoadModel: *28 not found"). So the .ent files we ship carry the CRC of the .bsp they were written for: a
# .bsp with other entities ignores them. A tool that rewrites a .bsp's entity lump renames its .ent (write()).
#
#   python Misc/quakevr/entfile.py [--maps quakevr/maps] [names...]   prints each map's pinned .ent name

import argparse
import glob
import os
import struct


def _table():
    table = []
    for i in range(256):
        crc = i << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
        table.append(crc & 0xFFFF)
    return table


CRC_TABLE = _table()


def crc_block(data):
    """Quake's CRC_Block (crc.c): CRC-16/CCITT, starting at 0xffff."""
    crc = 0xFFFF
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ CRC_TABLE[(crc >> 8) ^ b]
    return crc


def entity_crc(bsp_data):
    """The CRC the engine names the .bsp's .ent by: of its entity lump less the last byte (0 for an empty lump)."""
    offset, length = struct.unpack_from("<ii", bsp_data, 4)
    return crc_block(bsp_data[offset : offset + length - 1]) if length > 0 else 0


def pinned_name(name, bsp_data):
    return "%s@%04x.ent" % (name, entity_crc(bsp_data))


def find(maps, name):
    """The map's .ent in `maps`: its pinned one (any CRC), else a plain <name>.ent, else None."""
    pinned = sorted(glob.glob(os.path.join(glob.escape(maps), glob.escape(name) + "@*.ent")))
    if len(pinned) > 1:
        raise SystemExit("%s: more than one pinned .ent: %s" % (name, ", ".join(pinned)))
    if pinned:
        return pinned[0]
    plain = os.path.join(maps, name + ".ent")
    return plain if os.path.isfile(plain) else None


def write(maps, name, bsp_data, text):
    """Writes the map's .ent pinned to `bsp_data` (the .bsp as it now is) and moves away the one it had, if its name
    differs (renamed, so that git sees the rename). Returns the path written."""
    path = os.path.join(maps, pinned_name(name, bsp_data))
    old = find(maps, name)
    if old and os.path.normcase(os.path.abspath(old)) != os.path.normcase(os.path.abspath(path)):
        os.replace(old, path)
    with open(path, "w", newline="\n") as f:
        f.write(text)
    return path


def main():
    parser = argparse.ArgumentParser(description="Prints the pinned .ent name of Quake VR's maps.")
    parser.add_argument("--maps", default=os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "maps"))
    parser.add_argument("names", nargs="*")
    args = parser.parse_args()
    names = args.names or sorted(os.path.splitext(os.path.basename(p))[0]
                                 for p in glob.glob(os.path.join(args.maps, "*.bsp")))
    for name in names:
        with open(os.path.join(args.maps, name + ".bsp"), "rb") as f:
            data = f.read()
        current = find(args.maps, name)
        want = pinned_name(name, data)
        if current or args.names:
            state = "ok" if current and os.path.basename(current) == want else "now %s" % (
                os.path.basename(current) if current else "none")
            print("%-24s %s (%s)" % (name, want, state))


if __name__ == "__main__":
    main()
