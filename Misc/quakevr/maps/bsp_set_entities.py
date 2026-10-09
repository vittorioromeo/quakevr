# bsp_set_entities.py -- edits a compiled BSP's entity lump in place of a recompile: a text replaced in it (or the
# lump replaced by a file), the lumps after it moved along, BSPX lumps (the light grid) included. Every other byte is
# kept: the geometry, the lightmaps, the .lit and .lux beside it stay valid.
#
#   python Misc/quakevr/maps/bsp_set_entities.py quakevr/maps/vrstart.bsp --replace "\"worldtext\" \"TORCH SIDE\"" "\"worldtext\" \"FLASHLIGHT\nSIDE\""
#   python Misc/quakevr/maps/bsp_set_entities.py <bsp> --from-file <entities.txt>
#
# (vrstart's and the tutorial's text changed so, 2026-10-09, their generators saying the same: vrstart_gen.py,
# vrtutorial_gen.py. Check a lump against its .map: every entity's keys the same.)
import argparse
import struct
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bsp")
    ap.add_argument("--replace", nargs=2, metavar=("OLD", "NEW"), help="a text in the entity lump and its replacement "
                    "(\\n written as a backslash and n, as in a .map)")
    ap.add_argument("--from-file", help="the whole entity lump from this file")
    args = ap.parse_args()
    data = bytearray(open(args.bsp, "rb").read())
    magic = bytes(data[:4])
    if magic not in (b"BSP2", b"2PSB") and struct.unpack_from("<i", data, 0)[0] != 29:
        sys.exit("not a BSP: %r" % magic)
    lumps = [list(struct.unpack_from("<ii", data, 4 + 8 * i)) for i in range(15)]
    end = max(o + l for o, l in lumps)
    bspx = (end + 3) & ~3
    bspx_lumps = []
    if data[bspx:bspx + 4] == b"BSPX":
        n = struct.unpack_from("<i", data, bspx + 4)[0]
        bspx_lumps = [list(struct.unpack_from("<24sii", data, bspx + 8 + 32 * k)) for k in range(n)]
    ofs, ln = lumps[0]
    ents = bytes(data[ofs:ofs + ln])
    if args.from_file:
        new = open(args.from_file, "rb").read().rstrip(b"\0") + b"\0"
    else:
        old_s, new_s = (a.encode("latin1") for a in args.replace)
        count = ents.count(old_s)
        if count != 1:
            sys.exit("the text is in the entity lump %d times (not once)" % count)
        new = ents.replace(old_s, new_s)
    # the lumps after it keep their alignment: the new lump padded (with spaces before its end) to a multiple of 4 more
    body = new.rstrip(b"\0")
    while (len(body) + 1 - ln) % 4:
        body += b" "
    new = body + b"\0"
    delta = len(new) - ln
    data[ofs:ofs + ln] = new
    lumps[0][1] = len(new)
    for i in range(1, 15):
        if lumps[i][0] > ofs:
            lumps[i][0] += delta
    for l in bspx_lumps:
        if l[1] > ofs:
            l[1] += delta
    for i, (o, l) in enumerate(lumps):
        struct.pack_into("<ii", data, 4 + 8 * i, o, l)
    if bspx_lumps:
        at = bspx + delta
        assert data[at:at + 4] == b"BSPX", "the BSPX header moved unexpectedly"
        for k, (name, o, l) in enumerate(bspx_lumps):
            struct.pack_into("<24sii", data, at + 8 + 32 * k, name, o, l)
    open(args.bsp, "wb").write(bytes(data))
    print("%s: the entity lump %d -> %d bytes, %d lumps and %d BSPX lumps moved by %d" % (
        args.bsp, ln, len(new), sum(1 for o, _ in lumps[1:] if o > ofs), len(bspx_lumps), delta))


if __name__ == "__main__":
    main()
