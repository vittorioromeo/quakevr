# make_enemyshove.py -- the grunts' and enforcers' shove (ROUND21.md, "Grunts and enforcers shove you"): six frames
# appended to Quake VR's own quakevr/progs/soldier.mdl and enforcer.mdl, after all of theirs (which stay as they
# were, byte for byte): shove1..shove6, a two-handed push with the gun held across (QC orig_mon_soldier.qc
# army_shove1.., orig_mon_enforcer.qc enf_shove1..; vr_enemyshove.qc).
#
# The models have no skeleton (vertex animation), so the frames are made from stand1 by a soft deformation:
# - the gun (the separate piece the engine also knows: vr_monstermods.cpp) moves rigidly by the frame's thrust;
# - the arms follow it, fully at the hands (the body's vertices about the grip points) and less and less up to the
#   shoulders, by their distance along the surface from the hands (geodesic, so the chest next to the hands stays);
# - the upper body leans about the hips (fully above `lean_z`, not at all below `hips_z`: the legs stay planted).
# The vertex normals are stand1's, turned by the lean (no lighting pop between frames).
#
#   python Misc/quakevr/make_enemyshove.py            writes both models (genguard: stops if one was edited by hand)
#   python Misc/quakevr/make_enemyshove.py --check    prints what it would write, writes nothing
#
# Run again, it replaces the shove frames it appended before (frames named shove*), so it can be re-tuned.

import heapq
import math
import os
import struct
import sys

import numpy as np

import genguard
from mdlgen import HEADER, anorms, read_skins

HERE = os.path.dirname(os.path.abspath(__file__))
PROGS = os.path.normpath(os.path.join(HERE, '..', '..', 'quakevr', 'progs'))

# Per frame: (lean in degrees, forward positive; the thrust (x forward, y left, z up) of the gun and hands).
# 1-3 the wind-up (the gun drawn in, leaning back: the tell), 4 the push (the blow lands on this frame's think), 5-6
# back to standing.
GRUNT_KEYS = [(-3, (-2.0, 0.0, 1.5)), (-6, (-4.0, 0.0, 2.5)), (-8, (-4.5, 0.0, 3.0)),
              (16, (12.0, 0.0, 2.0)), (10, (7.0, 0.0, 1.0)), (4, (3.0, 0.0, 0.0))]
ENFORCER_KEYS = [(-4, (-3.0, 0.0, 0.5)), (-7, (-5.0, 0.0, 1.0)), (-8, (-6.0, 0.0, 1.0)),
                 (14, (12.0, 0.0, 0.0)), (9, (7.0, 0.0, 0.0)), (3, (3.0, 0.0, 0.0))]

MODELS = [
    dict(name='soldier.mdl', counts=(555, 810), base=0, gun=list(range(463, 549)),
         hands=[((5.5, -4.0, 5.0), 3.5), ((10.5, 6.0, 2.5), 4.0)],
         falloff=(3.0, 17.0), pivot=(-2.0, 0.0, -6.0), hips_z=-10.0, lean_z=0.0, keys=GRUNT_KEYS),
    dict(name='enforcer.mdl', counts=(479, 984), base=0,
         gun=[22, 23, 100] + list(range(400, 431)) + list(range(455, 479)),
         hands=[((5.0, -10.0, 12.0), 3.5), ((3.5, 14.0, 15.0), 3.5)], falloff=(3.0, 16.0), pivot=(-2.0, 0.0, -2.0), hips_z=-6.0, lean_z=4.0, keys=ENFORCER_KEYS),
]

NAMES = ['shove%d' % (i + 1) for i in range(6)]


class Model:
    def __init__(self, path):
        data = open(path, 'rb').read()
        self.header = list(HEADER.unpack_from(data, 0))
        nskins, sw, sh, nv, nt, nf = self.header[12:18]
        self.nv, self.nt = nv, nt
        skins, off = read_skins(data, HEADER.size, nskins, sw, sh)
        start = off
        off += 12 * nv
        self.tris = np.array([struct.unpack_from('<4i', data, off + 16 * i)[1:] for i in range(nt)])
        off += 16 * nt
        self.head = data[HEADER.size:off]  # the skins, the skin coordinates and the triangles, as they are
        assert start > HEADER.size
        self.frames = []  # (name, raw bytes of the whole frame)
        for _ in range(nf):
            (kind,) = struct.unpack_from('<i', data, off)
            assert kind == 0, 'a frame group'
            size = 4 + 8 + 16 + 4 * nv
            raw = data[off:off + size]
            name = raw[12:28].split(b'\0')[0].decode('latin-1')
            self.frames.append((name, raw))
            off += size
        assert off == len(data)
        self.scale = np.array(self.header[2:5])
        self.origin = np.array(self.header[5:8])

    def verts(self, index):
        raw = self.frames[index][1]
        v = np.frombuffer(raw[28:], dtype=np.uint8).reshape(self.nv, 4)
        return v[:, :3].astype(float) * self.scale + self.origin, v[:, 3].copy()

    def frame_bytes(self, name, pos, normals):
        q = np.rint((pos - self.origin) / self.scale)
        assert q.min() >= 0 and q.max() <= 255, 'a shove frame out of the model\'s bounds'
        q = q.astype(np.uint8)
        lo, hi = q.min(0), q.max(0)
        out = struct.pack('<i', 0) + bytes((*lo, 0)) + bytes((*hi, 0)) + name.encode().ljust(16, b'\0')
        out += np.column_stack([q, normals.astype(np.uint8)]).tobytes()
        return out

    def bytes(self):
        h = list(self.header)
        h[17] = len(self.frames)
        return HEADER.pack(*h) + self.head + b''.join(raw for _, raw in self.frames)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def geodesic(pos, tris, seeds):
    """Distance along the surface (the triangles' edges, coincident vertices welded) from the `seeds`."""
    key, rep = {}, np.zeros(len(pos), int)
    for i, p in enumerate(np.round(pos, 3)):
        rep[i] = key.setdefault(tuple(p), i)
    adj = {}
    for t in tris:
        for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            a, b = rep[a], rep[b]
            if a != b:
                length = float(np.linalg.norm(pos[a] - pos[b]))
                adj.setdefault(a, []).append((b, length))
                adj.setdefault(b, []).append((a, length))
    dist, heap = {}, [(0.0, rep[s]) for s in seeds]
    while heap:
        d, u = heapq.heappop(heap)
        if u in dist:
            continue
        dist[u] = d
        for v, length in adj.get(u, ()):
            if v not in dist:
                heapq.heappush(heap, (d + length, v))
    return np.array([dist.get(rep[i], 1e9) for i in range(len(pos))])


def rot_y(deg):
    """Turns a point about the y axis so that above the pivot it goes forward (+x) for a positive angle."""
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0.0, s], [0.0, 1.0, 0.0], [-s, 0.0, c]])


def shove_frames(m, spec):
    pos, nidx = m.verts(spec['base'])
    nv = len(pos)
    gun = np.zeros(nv, bool)
    gun[spec['gun']] = True
    body_tris = [t for t in m.tris if not gun[t].any()]
    body = np.flatnonzero(~gun)
    if spec['hands']:
        seeds = [i for i in body if any(np.linalg.norm(pos[i] - np.array(c)) <= r for c, r in spec['hands'])]
    else:
        # The body's vertices touching the gun: the hands round its grips (the enforcer's rifle, held across).
        g = pos[gun]
        seeds = [i for i in body if np.min(np.linalg.norm(g - pos[i], axis=1)) < 2.0]
    geo = geodesic(pos, body_tris, seeds)
    arm = 1.0 - smoothstep(spec['falloff'][0], spec['falloff'][1], geo)
    arm[gun] = 1.0
    lean = smoothstep(spec['hips_z'], spec['lean_z'], pos[:, 2])
    lean[gun] = 1.0
    pivot = np.array(spec['pivot'])
    table = np.array(anorms())
    base_n = table[nidx]

    out = []
    for name, (deg, thrust) in zip(NAMES, spec['keys']):
        moved = pos + arm[:, None] * np.array(thrust)
        new = np.empty_like(pos)
        normals = np.empty(nv, int)
        for i in range(nv):
            r = rot_y(deg * lean[i])
            new[i] = pivot + r @ (moved[i] - pivot)
            normals[i] = int(np.argmax(table @ (r @ base_n[i])))
        out.append((name, new, normals))
    stats = 'seeds %d, arm weight > 0.5: %d vertices, lean > 0.5: %d' % (len(seeds), int((arm[~gun] > 0.5).sum()),
                                                                         int((lean > 0.5).sum()))
    return out, stats


def main():
    check = '--check' in sys.argv
    paths = [os.path.join(PROGS, s['name']) for s in MODELS]
    guard = None if check else genguard.Guard('make_enemyshove.py', paths)
    for spec, path in zip(MODELS, paths):
        m = Model(path)
        assert (m.nv, m.nt) == spec['counts'], '%s: not Quake VR\'s own model (%d vertices, %d triangles)' % (
            spec['name'], m.nv, m.nt)
        kept = [f for f in m.frames if not f[0].startswith('shove')]
        old = len(m.frames) - len(kept)
        m.frames = kept
        frames, stats = shove_frames(m, spec)
        first = len(m.frames)
        for name, new, normals in frames:
            m.frames.append((name, m.frame_bytes(name, new, normals)))
            radius = float(np.max(np.linalg.norm(new, axis=1)))
            m.header[8] = max(m.header[8], radius)
        print('%s: %s; frames %d..%d (%s), replacing %d' % (spec['name'], stats, first, len(m.frames) - 1,
                                                             ' '.join(NAMES), old))
        if not check:
            with open(path, 'wb') as f:
                f.write(m.bytes())
    if guard:
        guard.finish()


if __name__ == '__main__':
    main()
