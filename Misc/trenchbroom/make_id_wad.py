#!/usr/bin/env python3
"""Builds a texture WAD of id's own map textures from the player's Quake install (never committed).

    python Misc/trenchbroom/make_id_wad.py [--quake <Quake folder>] [--out <wad>] [--list]

Reads every map in id1's paks (and the mission packs' hipnotic/pak0.pak and rogue/pak0.pak, when installed), takes
each texture the maps embed (by name; the first one found wins: id1, then hipnotic, then rogue) and writes them, as
they are (8-bit, Quake's palette, their mip levels), into quakevr/wads/id_textures.wad (git-ignored: id's data is not
in the repository). Maps made with it (vrstart: Misc/quakevr/maps/vrstart_gen.py) name it in their worldspawn's
`wad`; qbsp finds it with `-wadpath <checkout>`. A .bsp compiled from such a map embeds id's textures: the author
decided that the hub maps may ship so (MAPPING.md, "Textures and id's data").

Deterministic: the same install gives the same bytes (textures sorted by name).
"""

import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DEFAULT_QUAKE = r"C:\Program Files (x86)\Steam\steamapps\common\Quake"
OUT = os.path.join(ROOT, "quakevr", "wads", "id_textures.wad")


def pak_files(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, off, size = struct.unpack_from("<4sii", data, 0)
    if magic != b"PACK":
        return
    for i in range(off, off + size, 64):
        name = data[i:i + 56].split(b"\0")[0].decode("latin1").lower()
        pos, ln = struct.unpack_from("<ii", data, i + 56)
        yield name, data[pos:pos + ln]


def bsp_textures(bsp):
    """(name, miptex bytes) of every texture a BSP embeds (version 29 or BSP2)."""
    version = struct.unpack_from("<i", bsp, 0)[0]
    if version not in (29, 0x32505342, 0x50534232):  # 29, "BSP2", "2PSB"
        return
    off, ln = struct.unpack_from("<ii", bsp, 4 + 2 * 8)  # lump 2: textures
    if ln <= 0:
        return
    lump = bsp[off:off + ln]
    n = struct.unpack_from("<i", lump, 0)[0]
    for k in range(n):
        o = struct.unpack_from("<i", lump, 4 + 4 * k)[0]
        if o < 0:
            continue
        name = lump[o:o + 16].split(b"\0")[0].decode("latin1")
        w, h = struct.unpack_from("<II", lump, o + 16)
        offs = struct.unpack_from("<4I", lump, o + 24)
        if not name or w == 0 or h == 0 or w > 4096 or h > 4096 or offs[0] == 0:
            continue
        size = 40 + w * h + (w // 2) * (h // 2) + (w // 4) * (h // 4) + (w // 8) * (h // 8)
        blob = bytearray(lump[o:o + size])
        if len(blob) != size:
            continue
        # the offsets as they are in a WAD's miptex (relative to the miptex: the same as in the BSP)
        yield name, bytes(blob)


def collect(quake):
    sources = []
    id1 = os.path.join(quake, "id1")
    for name in sorted(os.listdir(id1)):
        if name.lower().endswith(".pak"):
            sources.append(("id1", os.path.join(id1, name)))
    for pack in ("hipnotic", "rogue"):
        d = os.path.join(quake, pack)
        if os.path.isdir(d):
            for name in sorted(os.listdir(d)):
                if name.lower().endswith(".pak"):
                    sources.append((pack, os.path.join(d, name)))
    if not sources:
        sys.exit("no paks in %s" % id1)
    textures = {}
    origin = {}
    for game, path in sources:
        for fname, blob in pak_files(path):
            if not (fname.startswith("maps/") and fname.endswith(".bsp")):
                continue
            for name, tex in bsp_textures(blob):
                key = name.lower()
                if key not in textures:
                    textures[key] = (name, tex)
                    origin[key] = "%s:%s" % (game, fname)
    return textures, origin


def build_wad(textures):
    out = bytearray(b"WAD2" + struct.pack("<ii", len(textures), 0))
    entries = []
    for key in sorted(textures):
        name, blob = textures[key]
        entries.append((len(out), len(blob), name))
        out += blob
        while len(out) % 4:
            out += b"\0"
    dir_ofs = len(out)
    for pos, size, name in entries:
        out += struct.pack("<iiibbh16s", pos, size, size, 0x44, 0, 0, name.encode("latin1")[:15])
    struct.pack_into("<i", out, 8, dir_ofs)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--quake", default=DEFAULT_QUAKE, help="the Quake folder (with id1, and hipnotic/rogue if owned)")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--list", action="store_true", help="print each texture, its size and the map it came from")
    args = ap.parse_args()
    textures, origin = collect(args.quake)
    data = build_wad(textures)
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(data)
    if args.list:
        for key in sorted(textures):
            name, blob = textures[key]
            w, h = struct.unpack_from("<II", blob, 16)
            print("%-16s %4dx%-4d %s" % (name, w, h, origin[key]))
    print("wrote %s: %d textures, %d bytes" % (os.path.relpath(args.out, ROOT), len(textures), len(data)))


if __name__ == "__main__":
    main()
