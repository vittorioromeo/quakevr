#!/usr/bin/env python3
"""Makes quakevr/wads/id_textures.wad: every texture of id's Quake maps, read from the player's own id1 paks.

    python Misc/trenchbroom/make_id_wad.py [--quake <Quake folder>] [--out <wad>] [--list]

The textures are id's data: the WAD is made from your own install (id1/pak0.pak and pak1.pak: the maps in them, each
.bsp's texture lump), git-ignored and never committed. Maps that use it (the calibration room: worldspawn
"wad" "quakevr/wads/id_textures.wad;quakevr/wads/quakevr_dev.wad") compile with qbsp -wadpath <checkout>; their .bsp
embeds the textures it uses (the author's decision for the committed vrcalibration.bsp, round 21).

Every texture once, by name (case kept as the maps have it), the first map that has it giving it (pak0 then pak1, the
maps in name order): the same bytes every time. --list prints the names and sizes instead of writing.
"""

import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "quakevr", "wads", "id_textures.wad")
DEFAULT_QUAKE = r"C:\Program Files (x86)\Steam\steamapps\common\Quake"


def pak_files(path):
    """{name: bytes} of a .pak's files."""
    with open(path, "rb") as f:
        data = f.read()
    magic, off, size = struct.unpack("<4sii", data[:12])
    if magic != b"PACK":
        sys.exit("%s: not a pak" % path)
    out = {}
    for i in range(off, off + size, 64):
        name = data[i:i + 56].split(b"\0")[0].decode("latin1").lower()
        pos, ln = struct.unpack("<ii", data[i + 56:i + 64])
        out[name] = data[pos:pos + ln]
    return out


def bsp_miptex(bsp):
    """The (name, miptex bytes) of a BSP29 map's texture lump (textures it only names, not holds, left out)."""
    version = struct.unpack("<i", bsp[:4])[0]
    if version != 29:
        return []
    ofs, ln = struct.unpack("<ii", bsp[4 + 2 * 8: 4 + 2 * 8 + 8])
    lump = bsp[ofs:ofs + ln]
    if len(lump) < 4:
        return []
    count = struct.unpack("<i", lump[:4])[0]
    out = []
    for k in range(count):
        mo = struct.unpack("<i", lump[4 + 4 * k: 8 + 4 * k])[0]
        if mo < 0:
            continue
        name = lump[mo:mo + 16].split(b"\0")[0].decode("latin1")
        w, h, o0 = struct.unpack("<III", lump[mo + 16:mo + 28])
        if not name or o0 == 0 or w == 0 or h == 0:
            continue
        size = 40 + w * h + (w // 2) * (h // 2) + (w // 4) * (h // 4) + (w // 8) * (h // 8)
        out.append((name, lump[mo:mo + size]))
    return out


def collect(quake):
    id1 = os.path.join(quake, "id1")
    paks = sorted(n for n in os.listdir(id1) if n.lower() in ("pak0.pak", "pak1.pak"))
    if not paks:
        sys.exit("no id1/pak0.pak in %s (--quake)" % quake)
    seen, textures = set(), []
    for pak in paks:
        files = pak_files(os.path.join(id1, pak))
        for name in sorted(n for n in files if n.startswith("maps/") and n.endswith(".bsp")):
            for tex, blob in bsp_miptex(files[name]):
                if tex.lower() not in seen:
                    seen.add(tex.lower())
                    textures.append((tex, blob))
    return textures


def write_wad(textures, path):
    out = bytearray(b"WAD2" + struct.pack("<ii", len(textures), 0))
    entries = []
    for name, blob in textures:
        entries.append((len(out), len(blob), name))
        out += blob
        while len(out) % 4:
            out += b"\0"
    dir_ofs = len(out)
    for pos, size, name in entries:
        out += struct.pack("<iiibbh16s", pos, size, size, 0x44, 0, 0, name.encode("latin1"))
    struct.pack_into("<i", out, 8, dir_ofs)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--quake", default=DEFAULT_QUAKE, help="the Quake folder (its id1/pak0.pak, pak1.pak)")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--list", action="store_true", help="print the textures (name, size) instead of writing")
    args = ap.parse_args()
    textures = collect(args.quake)
    if args.list:
        for name, blob in textures:
            w, h = struct.unpack("<II", blob[16:24])
            print("%-16s %dx%d" % (name, w, h))
        return
    write_wad(textures, args.out)
    print("wrote %s (%d textures)" % (os.path.relpath(args.out, ROOT), len(textures)))


if __name__ == "__main__":
    main()
