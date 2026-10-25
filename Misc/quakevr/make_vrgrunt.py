#!/usr/bin/env python3
# make_vrgrunt.py -- the armed monsters' models (QC vr_armedmon.qc's table; docs/vr-port/ARMED_MONSTERS.md):
# Quake VR's own monster models without the gun they carry, one for each armed monster:
#   progs/vrgrunt.mdl     the grunt (quakevr/progs/soldier.mdl)
#   progs/vrenforcer.mdl  the enforcer (quakevr/progs/enforcer.mdl)
#
# Usage: python Misc/quakevr/make_vrgrunt.py [quakevr progs folder] [model name: vrgrunt, vrenforcer; default all]
#
# The gun is a separate piece of the monster's mesh (its vertices: the same lists as vr_monstermods.cpp's knownSwords
# and make_enemyguns.py). Its triangles are dropped; its vertices stay in every frame, unused by any triangle: the
# numbering, the frames, the skin and the ragdoll rig (vr_ragdoll.cpp) are the monster's, and the gun's line is still
# known in every frame (QC modelvertex: where the monster holds the weapon it carries, a player's weapon drawn as a
# prop, and which way it points).
#
# Adding an armed monster: a row here (its source model, its vertex and triangle counts, its gun's vertices and the
# number of triangles they make), then the rest of ARMED_MONSTERS.md.

import os
import struct
import sys

MODELS = {
    'vrgrunt': dict(src='soldier.mdl', counts=(555, 810), gun=list(range(463, 549)), gun_tris=116),
    'vrenforcer': dict(src='enforcer.mdl', counts=(479, 984),
                       gun=[22, 23, 100] + list(range(400, 431)) + list(range(455, 479)), gun_tris=118),
}


def make(progs, name, spec):
    data = open(os.path.join(progs, spec['src']), 'rb').read()
    head = list(struct.unpack_from('<4s i 3f 3f f 3f 8i f', data, 0))
    nskins, sw, sh, nverts, ntris = head[12:17]
    assert (nverts, ntris) == spec['counts'], 'not Quake VR\'s %s (%d vertices, %d triangles)' % (spec['src'], nverts,
                                                                                                  ntris)
    off = 84
    for _ in range(nskins):
        assert struct.unpack_from('<i', data, off)[0] == 0, 'a skin group'
        off += 4 + sw * sh
    off += 12 * nverts
    tris = [data[off + 16 * i:off + 16 * (i + 1)] for i in range(ntris)]
    rest = data[off + 16 * ntris:]
    gun = set(spec['gun'])
    kept = [t for t in tris if not any(v in gun for v in struct.unpack('<4i', t)[1:])]
    assert spec['gun_tris'] is None or len(tris) - len(kept) == spec['gun_tris'], \
        'the gun has %d triangles, not %d' % (len(tris) - len(kept), spec['gun_tris'])
    head[16] = len(kept)
    out = struct.pack('<4s i 3f 3f f 3f 8i f', *head) + data[84:off] + b''.join(kept) + rest
    path = os.path.join(progs, name + '.mdl')
    open(path, 'wb').write(out)
    print('%s: %d vertices, %d triangles (the gun\'s %d dropped), %d bytes' % (path, nverts, len(kept),
                                                                          len(tris) - len(kept), len(out)))


def main():
    progs = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..', 'quakevr', 'progs')
    names = [sys.argv[2]] if len(sys.argv) > 2 else list(MODELS)
    for name in names:
        make(progs, name, MODELS[name])


if __name__ == '__main__':
    main()
