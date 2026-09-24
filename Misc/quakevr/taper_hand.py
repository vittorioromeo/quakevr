#!/usr/bin/env python3
# taper_hand.py -- tapers the wrist end of the hand model (progs/hand_base.mdl), so that it stays
# inside the body's bracer cuff (make_vrbody.py) instead of poking through it.
#
# Usage: python Misc/quakevr/taper_hand.py <input hand_base.mdl> <output hand_base.mdl>
#
# Applied once to the original model (git: quakevr/progs/hand_base.mdl before the commit that
# added this script); run it on that original, not on its own output.
#
# Model space: +x towards the fingers; the wrist's centre is at (-6.86, -1.08, 1.42), the model's
# end at x -6.97. Vertices behind TAPER_START are pulled towards the wrist's axis, down to
# TAPER_END_SCALE of their distance at the very end (smoothly).

import struct
import sys

WRIST_Y, WRIST_Z = -1.08, 1.42
TAPER_START = -2.5
TAPER_END_X = -6.97
TAPER_END_SCALE = 0.7


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
    ident, version = struct.unpack_from("<4si", d, 0)
    assert ident == b"IDPO" and version == 6, "not a Quake mdl"
    scale = struct.unpack_from("<3f", d, 8)
    origin = struct.unpack_from("<3f", d, 20)
    numskins, sw, sh, nv, nt, nf = struct.unpack_from("<6i", d, 48)

    o = 84
    for _ in range(numskins):
        group, = struct.unpack_from("<i", d, o)
        o += 4
        if group == 0:
            o += sw * sh
        else:
            n, = struct.unpack_from("<i", d, o)
            o += 4 + 4 * n + n * sw * sh
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
