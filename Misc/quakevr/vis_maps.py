#!/usr/bin/env python3
# vis_maps.py -- lets Quake VR draw see-through water in id's maps, by giving them "water-vis" data.
#
# Quake's 1996 maps were vised with water as a wall: from above the water, nothing below it is in the
# potentially visible set (PVS), and the other way round. Engines therefore only draw water translucent
# (r_wateralpha < 1) in maps whose PVS lets a liquid leaf see a non-liquid one; Ironwail checks this at
# load (Mod_CheckWaterVis) and keeps id's water opaque. Re-vising the maps needs their portal files,
# i.e. recompiling them; instead, the community's "vispatch" data files hold a water-vised
# visibility lump and leaf lump for every map of id1, hipnotic and rogue (id1.vis, hipnotic.vis,
# rogue.vis; VisPatch, http://vispatch.sourceforge.net/, GPL-2 tool; the data files are from
# http://www.inside3d.com/qip/vispatch/files.htm). This script puts those two lumps into the maps.
#
# Only the visibility and leaf lumps change (the leaf lump because it holds each leaf's offset into
# the visibility data); a map is patched only if the patch's leaves are the map's own (same contents,
# bounds and faces), so a patch made for another version of a map is refused.
#
# Usage:
#   Patch the relit maps in place (run after relight_maps.py; a map already water-vised is skipped):
#     python Misc/quakevr/vis_maps.py --vis-dir <folder with id1.vis hipnotic.vis rogue.vis> [--relit quakevr/relit]
#   Patch copies of the original maps (from the game's paks) into a folder:
#     python Misc/quakevr/vis_maps.py --vis-dir <...> --quake <Quake folder> --out <folder> [--only e1m1 ...]
#   Report which maps are water-vised (like Ironwail's check at map load):
#     python Misc/quakevr/vis_maps.py --check <.bsp files or folders>

import argparse
import os
import struct
import sys

import quakepak

LUMP_TEXTURES, LUMP_VISIBILITY, LUMP_TEXINFO, LUMP_FACES = 2, 4, 6, 7
LUMP_LEAFS, LUMP_MARKSURFACES, LUMP_MODELS = 10, 11, 14
LEAF_SIZE = 28  # int contents, int visofs, short mins[3], maxs[3], ushort firstmark, nummark, byte ambient[4]

CONTENTS_EMPTY, CONTENTS_SOLID, CONTENTS_WATER, CONTENTS_SLIME, CONTENTS_LAVA = -1, -2, -3, -4, -5


def lump(data, index):
    return struct.unpack_from("<ii", data, 4 + index * 8)


def is_bsp29(data):
    return len(data) >= 124 and struct.unpack_from("<i", data, 0)[0] == 29


def read_vis_file(path):
    """{map file name (lower case, e.g. "e1m1.bsp"): (visibility lump, leaf lump)} of a vispatch file."""
    with open(path, "rb") as f:
        data = f.read()
    entries = {}
    pos = 0
    while pos + 40 <= len(data):
        name = data[pos : pos + 32].split(b"\0")[0].decode("latin-1").lower()
        length, vislen = struct.unpack_from("<ii", data, pos + 32)
        vis = data[pos + 40 : pos + 40 + vislen]
        (leaflen,) = struct.unpack_from("<i", data, pos + 40 + vislen)
        leafs = data[pos + 44 + vislen : pos + 44 + vislen + leaflen]
        entries[name] = (vis, leafs)
        pos += 36 + length
    return entries


def leaf_shape(leaf_lump):
    """The leaves without what vis computes (the visibility offset and the ambient sound levels)."""
    return [leaf_lump[i : i + 4] + leaf_lump[i + 8 : i + 24] for i in range(0, len(leaf_lump), LEAF_SIZE)]


def with_lumps(data, replacements):
    """The .bsp with the given lumps ({index: bytes}) appended at its end and pointed to (the old ones
    are left unused where they were)."""
    out = bytearray(data)
    for index, blob in replacements.items():
        while len(out) % 4:
            out.append(0)
        offset = len(out)
        out += blob
        struct.pack_into("<ii", out, 4 + index * 8, offset, len(blob))
    while len(out) % 4:
        out.append(0)
    return bytes(out)


def vispatch(data, entry):
    """The map with the patch's visibility and leaf lumps, or None if the patch is not for this map."""
    if not is_bsp29(data):
        return None
    vis, leafs = entry
    ofs, length = lump(data, LUMP_LEAFS)
    if len(leafs) != length or leaf_shape(leafs) != leaf_shape(data[ofs : ofs + length]):
        return None
    return with_lumps(data, {LUMP_VISIBILITY: vis, LUMP_LEAFS: leafs})


def decompress_vis(data, visofs, row):
    vofs, vlen = lump(data, LUMP_VISIBILITY)
    if visofs < 0 or vlen == 0:
        return b"\xff" * row
    out = bytearray()
    p = vofs + visofs
    while len(out) < row:
        c = data[p]
        if c:
            out.append(c)
            p += 1
        else:
            out += b"\0" * data[p + 1]
            p += 2
    return bytes(out[:row])


def water_vis(data):
    """Which liquids the map is vised for as see-through ("water", "tele", "slime", "lava"), and which
    liquids it has; Ironwail's Mod_CheckWaterVis: a liquid is see-through if one of its leaves sees a
    leaf of other contents."""
    tofs, _ = lump(data, LUMP_TEXTURES)
    (nummip,) = struct.unpack_from("<i", data, tofs)
    names = []
    for i in range(nummip):
        (mip,) = struct.unpack_from("<i", data, tofs + 4 + i * 4)
        names.append("" if mip < 0 else data[tofs + mip : tofs + mip + 16].split(b"\0")[0].decode("latin-1").lower())
    tiofs, tilen = lump(data, LUMP_TEXINFO)
    texinfo_mip = [struct.unpack_from("<i", data, tiofs + i * 40 + 32)[0] for i in range(tilen // 40)]
    fofs, flen = lump(data, LUMP_FACES)
    face_kind = []
    for i in range(flen // 20):
        (ti,) = struct.unpack_from("<h", data, fofs + i * 20 + 10)
        name = names[texinfo_mip[ti]] if 0 <= ti < len(texinfo_mip) and 0 <= texinfo_mip[ti] < len(names) else ""
        if not name.startswith("*") or name.startswith(("*lava", "*slime")):
            face_kind.append(None)  # only water and teleporter faces tell a water leaf's kind
        else:
            face_kind.append("tele" if name.startswith("*tele") else "water")
    mofs, mlen = lump(data, LUMP_MARKSURFACES)
    marks = struct.unpack_from("<%dH" % (mlen // 2), data, mofs)
    lofs, llen = lump(data, LUMP_LEAFS)
    leafs = [struct.unpack_from("<ii6hHH", data, lofs + i * LEAF_SIZE) for i in range(llen // LEAF_SIZE)]
    (visleafs,) = struct.unpack_from("<i", data, lump(data, LUMP_MODELS)[0] + 52)
    row = (visleafs + 7) >> 3

    found, transparent = set(), set()
    for i in range(1, min(visleafs + 1, len(leafs))):
        contents, visofs = leafs[i][0], leafs[i][1]
        first, count = leafs[i][8], leafs[i][9]
        if contents == CONTENTS_WATER:
            kind = next((face_kind[marks[first + j]] for j in range(count) if face_kind[marks[first + j]]), None)
            if kind is None:
                continue
        elif contents == CONTENTS_SLIME:
            kind = "slime"
        elif contents == CONTENTS_LAVA:
            kind = "lava"
        else:
            continue
        found.add(kind)
        if kind in transparent:
            continue
        vis = decompress_vis(data, visofs, row)
        for j in range(visleafs):
            if vis[j >> 3] & (1 << (j & 7)) and j + 1 < len(leafs) and leafs[j + 1][0] != contents:
                transparent.add(kind)
                break
    return transparent, found


def describe(data):
    if not is_bsp29(data):
        return "not BSP29, not checked"
    transparent, found = water_vis(data)
    if not found:
        return "no liquids"
    order = ["water", "tele", "slime", "lava"]
    yes = [k for k in order if k in transparent]
    no = [k for k in order if k in found and k not in transparent]
    return "vised for " + (", ".join(yes) if yes else "nothing") + ("" if not no else "; opaque: " + ", ".join(no))


def check(paths):
    files = []
    for path in paths:
        if os.path.isdir(path):
            files += sorted(os.path.join(path, f) for f in os.listdir(path) if f.lower().endswith(".bsp"))
        else:
            files.append(path)
    for path in files:
        with open(path, "rb") as f:
            print("%s: %s" % (path, describe(f.read())))
    return 0


def load_patches(vis_dir, game):
    for name in (game + ".vis", os.path.join(game, "vispatch.dat")):
        path = os.path.join(vis_dir, name)
        if os.path.isfile(path):
            return read_vis_file(path)
    return None


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--vis-dir", help="folder with <game>.vis (or <game>/vispatch.dat) vispatch files")
    parser.add_argument("--games", nargs="+", default=["id1", "hipnotic", "rogue"])
    parser.add_argument("--relit", default=os.path.normpath(os.path.join(here, "..", "..", "quakevr", "relit")),
                        help="relit maps to patch in place (relight_maps.py's --out)")
    parser.add_argument("--quake", help="Quake folder: patch copies of its maps into --out instead")
    parser.add_argument("--out", help="with --quake: where to write <game>/maps/<map>.bsp")
    parser.add_argument("--only", nargs="*", help="map names (e1m1 ...)")
    parser.add_argument("--check", nargs="+", metavar="PATH", help="report the water-vis of .bsp files or folders")
    args = parser.parse_args()

    if args.check:
        return check(args.check)
    if not args.vis_dir:
        parser.error("--vis-dir is required (or --check)")
    if args.quake and not args.out:
        parser.error("--quake needs --out")

    patched = skipped = refused = 0
    for game in args.games:
        patches = load_patches(args.vis_dir, game)
        if patches is None:
            print("%s: no %s.vis in %s, skipped" % (game, game, args.vis_dir))
            continue
        if args.quake:
            game_dir = os.path.join(args.quake, game)
            if not os.path.isdir(game_dir):
                continue
            maps = {}
            for pak in quakepak.game_paks(game_dir):
                maps.update((os.path.basename(n), d) for n, d in quakepak.read_pak(pak).items()
                            if n.startswith("maps/") and n.endswith(".bsp"))
            out_dir = os.path.join(args.out, game, "maps")
        else:
            out_dir = os.path.join(args.relit, game, "maps")
            if not os.path.isdir(out_dir):
                continue
            maps = {}
            for f in os.listdir(out_dir):
                if f.lower().endswith(".bsp"):
                    with open(os.path.join(out_dir, f), "rb") as h:
                        maps[f.lower()] = h.read()

        for name, data in sorted(maps.items()):
            base = os.path.splitext(name)[0]
            if args.only and base not in args.only:
                continue
            entry = patches.get(name)
            if entry is None:
                continue
            if not args.quake and is_bsp29(data) and "water" in water_vis(data)[0]:
                skipped += 1
                continue
            result = vispatch(data, entry)
            if result is None:
                refused += 1
                print("%s/%s: the patch is for another version of the map, left alone" % (game, base))
                continue
            os.makedirs(out_dir, exist_ok=True)
            with open(os.path.join(out_dir, base + ".bsp"), "wb") as f:
                f.write(result)
            patched += 1
            print("%s/%s: %s" % (game, base, describe(result)))

    print("%d maps water-vised, %d already were, %d refused" % (patched, skipped, refused))
    return 1 if refused else 0


if __name__ == "__main__":
    sys.exit(main())
