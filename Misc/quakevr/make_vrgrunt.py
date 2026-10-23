#!/usr/bin/env python3
# make_vrgrunt.py -- the armed grunt's model (QC vr_armedgrunt.qc; docs/vr-port/ROUND22.md, "Armed grunts"):
# progs/vrgrunt.mdl, Quake VR's own grunt (quakevr/progs/soldier.mdl) without its gun.
#
# Usage: python Misc/quakevr/make_vrgrunt.py [quakevr progs folder]
#
# The gun is a separate piece of the grunt's mesh (vertices 463..548: the same list as vr_monstermods.cpp's knownSwords
# and make_enemyguns.py). Its 116 triangles are dropped; its vertices stay in every frame, unused by any triangle: the
# numbering, the frames, the skin and the ragdoll rig (vr_ragdoll.cpp: 555 vertices) are the grunt's, and the gun's
# line is still known in every frame (QC modelvertex: where the grunt holds the weapon it carries, a player's weapon
# drawn as a prop, and which way it points).

import os
import struct
import sys

GUN = range(463, 549)
COUNTS = (555, 810)


def main():
    progs = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..', 'quakevr', 'progs')
    data = open(os.path.join(progs, 'soldier.mdl'), 'rb').read()
    head = list(struct.unpack_from('<4s i 3f 3f f 3f 8i f', data, 0))
    nskins, sw, sh, nverts, ntris = head[12:17]
    assert (nverts, ntris) == COUNTS, 'not Quake VR\'s soldier.mdl (%d vertices, %d triangles)' % (nverts, ntris)
    off = 84
    for _ in range(nskins):
        assert struct.unpack_from('<i', data, off)[0] == 0, 'a skin group'
        off += 4 + sw * sh
    off += 12 * nverts
    tris = [data[off + 16 * i:off + 16 * (i + 1)] for i in range(ntris)]
    rest = data[off + 16 * ntris:]
    gun = set(GUN)
    kept = [t for t in tris if not any(v in gun for v in struct.unpack('<4i', t)[1:])]
    assert len(tris) - len(kept) == 116, 'the gun has %d triangles, not 116' % (len(tris) - len(kept))
    head[16] = len(kept)
    out = struct.pack('<4s i 3f 3f f 3f 8i f', *head) + data[84:off] + b''.join(kept) + rest
    path = os.path.join(progs, 'vrgrunt.mdl')
    open(path, 'wb').write(out)
    print('%s: %d vertices, %d triangles (the gun\'s %d dropped), %d bytes' % (path, nverts, len(kept),
                                                                          len(tris) - len(kept), len(out)))


if __name__ == '__main__':
    main()
