# bsp_splice.py hull0.bsp clip.bsp out.bsp -- a BSP2 with hull 0 (nodes, leaves, faces: everything drawn and lit) from
# the first and the clipping hulls (hulls 1 and 2: clipnodes, each model's heads 1-3) from the second. Both must be
# compiles of the same .map (the same models). The second's planes are appended to the first's (its clipnodes' plane
# numbers moved past them).
#
# Why (vrstart2_gen.py, compile_map): ericw-tools 2.0's qbsp fills from the occupants (or the outside) through the
# BSP's portals; on this map some portals fail and it made air solid in hull 0 (invisible slabs with no faces: holes),
# so hull 0 comes from a -nofill run; that run's clipping hulls, unfilled, are 100 times bigger (18 million clipnodes),
# so they come from a normal one.
import struct
import sys

HEADER_LUMPS = 15
PLANES, CLIPNODES, MODELS = 1, 9, 14


def read(path):
    with open(path, "rb") as f:
        d = f.read()
    assert d[:4] == b"BSP2", "%s: not BSP2" % path
    lumps = [struct.unpack_from("<ii", d, 4 + 8 * i) for i in range(HEADER_LUMPS)]
    return d, lumps


def lump(d, lumps, i):
    o, l = lumps[i]
    return d[o:o + l]


def splice(a_path, b_path, out_path):
    da, la = read(a_path)
    db, lb = read(b_path)
    pa, pb = lump(da, la, PLANES), lump(db, lb, PLANES)
    np_a = len(pa) // 20
    cb = bytearray(lump(db, lb, CLIPNODES))
    for i in range(len(cb) // 12):
        pn, c0, c1 = struct.unpack_from("<iii", cb, i * 12)
        struct.pack_into("<iii", cb, i * 12, pn + np_a, c0, c1)
    ma, mb = bytearray(lump(da, la, MODELS)), lump(db, lb, MODELS)
    assert len(ma) == len(mb), "the two compiles have different models"
    for m in range(len(ma) // 64):
        heads = struct.unpack_from("<4i", mb, m * 64 + 36)
        h0 = struct.unpack_from("<i", ma, m * 64 + 36)[0]
        struct.pack_into("<4i", ma, m * 64 + 36, h0, heads[1], heads[2], heads[3])
    new = {PLANES: pa + pb, CLIPNODES: bytes(cb), MODELS: bytes(ma)}
    # the lumps in their order in the file, then anything after the last lump (BSPX) as it was
    order = sorted(range(HEADER_LUMPS), key=lambda i: la[i][0])
    end = max(o + l for o, l in la)
    out = bytearray(4 + 8 * HEADER_LUMPS)
    out[:4] = b"BSP2"
    offs = [None] * HEADER_LUMPS
    for i in order:
        data = new.get(i, lump(da, la, i))
        while len(out) % 4:
            out.append(0)
        offs[i] = (len(out), len(data))
        out += data
    tail = da[end:]
    if tail:
        # BSPX: its lumps' offsets are absolute; moved by the change in where the tail starts
        while len(out) % 4:
            out.append(0)
        shift = len(out) - end
        t = bytearray(tail)
        if t[:4] == b"BSPX":
            n = struct.unpack_from("<i", t, 4)[0]
            for k in range(n):
                o = 8 + k * 32 + 24
                off, ln = struct.unpack_from("<ii", t, o)
                struct.pack_into("<ii", t, o, off + shift, ln)
        out += t
    for i in range(HEADER_LUMPS):
        struct.pack_into("<ii", out, 4 + 8 * i, *offs[i])
    with open(out_path, "wb") as f:
        f.write(out)
    return len(pb) // 20, len(cb) // 12


if __name__ == "__main__":
    planes, clip = splice(*sys.argv[1:4])
    print("bsp_splice: %s: hull 0 from %s, hulls 1-2 from %s (%d planes, %d clipnodes)" % (
        sys.argv[3], sys.argv[1], sys.argv[2], planes, clip))
