#!/usr/bin/env python3
# taper_hand.py -- tapers the wrist end of the hand model (progs/hand_base.mdl), so that it stays
# inside the body's bracer cuff (make_vrbody.py) instead of poking through it.
#
# Usage: python Misc/quakevr/taper_hand.py <input hand_base.mdl> <output hand_base.mdl>
#
# Run it on the original model, not on its own output: the original is
#   git show 03ed2b93~1:quakevr/progs/hand_base.mdl > hand_base_orig.mdl
# and then make_bloody_hands.py again (the original has only its own skin).
#
# Model space: +x towards the fingers; the wrist's centre is at (-6.86, -1.08, 1.42), the model's
# end at x -6.97. Vertices behind TAPER_START are pulled towards the wrist's axis, down to
# TAPER_END_SCALE of their distance at the very end (smoothly).

import struct
import sys

from mdlgen import HEADER, read_skins

WRIST_Y, WRIST_Z = -1.08, 1.42
TAPER_START = -1.5
TAPER_END_X = -6.97
TAPER_END_SCALE = 0.55


def taper(x, y, z):
    if x >= TAPER_START:
        return x, y, z
    t = min(1.0, (TAPER_START - x) / (TAPER_START - TAPER_END_X))
    t = t * t * (3.0 - 2.0 * t)  # smoothstep
    k = 1.0 + (TAPER_END_SCALE - 1.0) * t
    return x, WRIST_Y + (y - WRIST_Y) * k, WRIST_Z + (z - WRIST_Z) * k


def main():
    src, dst = sys.argv[1], sys.argv[2]
    d = bytearray(open(src, "rb").read())
    h = HEADER.unpack_from(d, 0)
    assert h[0] == b"IDPO" and h[1] == 6, "not a Quake mdl"
    scale, origin = h[2:5], h[5:8]
    numskins, sw, sh, nv, nt, nf = h[12:18]

    _, o = read_skins(d, HEADER.size, numskins, sw, sh)
    o += nv * 12 + nt * 16

    def vertices(at):
        changed = 0
        for i in range(nv):
            p = at + i * 4
            v = [d[p + k] * scale[k] + origin[k] for k in range(3)]
            t = taper(*v)
            for k in range(3):
                b = int(round((t[k] - origin[k]) / scale[k]))
                b = max(0, min(255, b))
                if b != d[p + k]:
                    changed += 1
                d[p + k] = b
        return changed

    changed = 0
    for _ in range(nf):
        kind, = struct.unpack_from("<i", d, o)
        o += 4
        if kind == 0:
            o += 24  # bboxmin, bboxmax, name
            changed += vertices(o)
            o += nv * 4
        else:
            n, = struct.unpack_from("<i", d, o)
            o += 4 + 8 + 4 * n
            for _ in range(n):
                o += 24
                changed += vertices(o)
                o += nv * 4

    assert o == len(d), "unexpected trailing data"
    open(dst, "wb").write(d)
    print("%s: %d frames, %d vertices, %d coordinates moved -> %s" % (src, nf, nv, changed, dst))


if __name__ == "__main__":
    main()
