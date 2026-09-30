#!/usr/bin/env python3
# make_enemyguns.py -- the grunts' shotgun and the enforcers' laser rifle as weapons: progs/v_gruntgun.mdl and
# progs/v_enfrifle.mdl, cut out of Quake VR's own soldier and enforcer models (quakevr/progs/soldier.mdl, enforcer.mdl),
# as make_chainsaw.py cuts the ogres' chainsaw and make_swords.py the knights' swords out of theirs.
#
# Usage: python Misc/quakevr/make_enemyguns.py [output progs folder]
#
# Each gun is a separate piece of its monster's mesh (vertices listed below: the same lists as
# Quake/vr/vr_monstermods.cpp's knownSwords, which hides the gun in the monster's death frames: the monster drops this
# one). Taken from the monster's first frame (stand1) and laid in the weapon's own frame:
#   +x along the barrel (towards the muzzle), +z up (the gun's top), +y left;
#   the origin in the middle of the pistol grip added under the receiver, which the main hand holds (upright, the barrel
#   ahead): the weapon settings' Offset puts it in the fist (vr_weapons.inc, slots 21 and 22).
# Scaled to `length` model units (at the weapon settings' Scale 0.4: about 65 and 75 cm). The triangles, skin coordinates
# and skin are the monster's own (the whole of skin 0, as the chainsaw keeps the ogre's).
#
# Cleaned for close-up viewing (the monsters' guns were made to be seen from afar):
# - degenerate triangles (no area: the enforcer's rifle has six) dropped;
# - the normals smoothed over the welded surface of the gun alone (the monster's own were lit as part of its body);
# - holes closed (seal_mdl.py: flat caps on the open loops, slivers in the cracks);
# - a cleaner silhouette where a hand holds it: a bevelled pistol grip under the receiver (a butt plate, ribs), a trigger
#   guard and a trigger; a band where the other hand goes (the rifle: the shotgun has its own); bolt heads on the
#   receiver; the skins' edge wear (mdlpolish.py: the parts in the skin's own browns and steels, flat-shaded, as the id models).
# Nine frames (0..8, the same pose): the frames a gun's firing animation steps through.
# The muzzle and counter anchors (slots 21, 22: MuzzleAnchorVertex, WpnTextAnchorVertex) are strip-order indices of
# the cut-out vertices (the parts are appended after them): rerun improve_weapons.strip_order after changing the cut.

import math
import os
import struct
import sys

import numpy as np

import genguard
import mdlpolish as mp
from make_chainsaw import principal
from make_swords import Mdl
from mdlgen import HEADER, anorms, cross, dot, mul, norm, sub
from seal_mdl import seal

FRAMES = 9

# The pistol grip added under the receiver (model units): its length (from the butt to the receiver's underside, into
# which it sinks SINK), its depth (front to back) and width, how far it leans back (degrees), the bevel of its edges.
GRIP_LEN, GRIP_DEPTH, GRIP_WIDTH, GRIP_TILT, GRIP_BEVEL, SINK = 7.2, 3.4, 2.6, 8.0, 0.5, 0.9
MARGIN = 0.9  # room round the gun in the file's byte grid for the parts (bands, bolt heads)

GUNS = {
    'v_gruntgun.mdl': dict(
        src='soldier.mdl', counts=(555, 810), tris=116,
        verts=list(range(463, 549)),
        length=46.0,          # model units, the stock's back to the muzzle (x 0.4: about 65 cm)
        grip_x=-6.5,          # source units along the barrel from the gun's centre: the rear hand's place
        fore_x=3.0,           # and the other hand's (the band round the barrel)
        grip_mat='brown', guard_mat='blued',
        fore_band=False,      # its own band round the barrel is the foregrip
    ),
    'v_enfrifle.mdl': dict(
        src='enforcer.mdl', counts=(479, 984), tris=118,
        verts=[22, 23, 100] + list(range(400, 431)) + list(range(455, 479)),
        length=52.0,          # (x 0.4: about 75 cm)
        grip_x=-9.0,
        fore_x=6.0,
        grip_mat='brown', guard_mat='blued',
        fore_band=True,       # a band at the vented housing's front, where the other hand holds it
    ),
}


class Gun:
    """The gun being built, as seal_mdl.Sealer wants it: .st, .tris ([front, a, b, c]), .frames ([name, positions,
    normal indices])."""

    def __init__(self):
        self.st, self.tris, self.frames = [], [], []


def tri_area(p, a, b, c):
    n = cross(sub(p[b], p[a]), sub(p[c], p[a]))
    return 0.5 * math.sqrt(dot(n, n))


def smooth_normals(pos, tris):
    """Per vertex: the area-weighted normals of the faces round its welded position (Quake's front faces clockwise
    seen from outside: the outward normal is minus the cross product)."""
    key = lambda p: tuple(round(x, 3) for x in p)
    acc = {}
    for _, a, b, c in tris:
        n = cross(sub(pos[b], pos[a]), sub(pos[c], pos[a]))
        for v in (a, b, c):
            k = key(pos[v])
            acc[k] = sub(acc.get(k, (0.0, 0.0, 0.0)), n)
    table = anorms()
    out = []
    for p in pos:
        n = acc.get(key(p), (0.0, 0.0, 1.0))
        n = norm(n) if dot(n, n) > 1e-12 else (0.0, 0.0, 1.0)
        out.append(max(range(len(table)), key=lambda k: dot(table[k], n)))
    return out


def write_mdl(path, gun, skin, sw, sh, lo, hi):
    """Quantizes the frames over the bounds lo..hi (room left for the parts mdlpolish adds)."""
    allp = [p for f in gun.frames for p in f[1]]
    lo = [min(lo[k], min(p[k] for p in allp)) for k in range(3)]
    hi = [max(hi[k], max(p[k] for p in allp)) for k in range(3)]
    scale = [max((hi[k] - lo[k]) / 255.0, 1e-4) for k in range(3)]
    radius = max(math.sqrt(dot(p, p)) for p in allp)
    out = bytearray(HEADER.pack(b'IDPO', 6, *scale, *lo, radius, 0.0, 0.0, 0.0,
                                1, sw, sh, len(gun.st), len(gun.tris), len(gun.frames), 0, 0, 1.0))
    out += struct.pack('<i', 0) + bytes(skin)
    for st in gun.st:
        out += struct.pack('<3i', *st)
    for t in gun.tris:
        out += struct.pack('<4i', *t)
    for name, pos, nrm in gun.frames:
        q = [[max(0, min(255, int(round((p[k] - lo[k]) / scale[k])))) for k in range(3)] for p in pos]
        bmin = bytes(min(v[k] for v in q) for k in range(3)) + b'\0'
        bmax = bytes(max(v[k] for v in q) for k in range(3)) + b'\0'
        out += struct.pack('<i', 0) + bmin + bmax + name.encode().ljust(16, b'\0')[:16]
        for v, n in zip(q, nrm):
            out += bytes((v[0], v[1], v[2], n))
    with open(path, 'wb') as f:
        f.write(out)


def extract(progs, spec):
    """The gun cut out of its monster, cleaned, sealed, in its own frame (origin: the pistol grip's middle). Returns
    (gun, the source model, the degenerate triangles dropped, the sealer, the scale, the foregrip's x)."""
    m = Mdl(open(os.path.join(progs, spec['src']), 'rb').read())
    assert (m.nverts, m.ntris) == spec['counts'], 'not Quake VR\'s %s (%d vertices, %d triangles)' % (
        spec['src'], m.nverts, m.ntris)
    vs = set(spec['verts'])
    tris = [t for t in m.tris if all(v in vs for v in t[1:])]
    assert len(tris) == spec['tris'], '%s: the gun has %d triangles, not %d' % (spec['src'], len(tris), spec['tris'])

    # The gun's frame: x its longest spread (towards the muzzle: the end ahead as the monster fires, the + end of its
    # principal axis in both models), z its middle spread turned up.
    c, axes = principal([m.pos(v) for v in spec['verts']])
    X = axes[0]
    Z = axes[1] if axes[1][2] > 0 else mul(axes[1], -1.0)
    Z = norm(sub(Z, mul(X, dot(Z, X))))
    Y = cross(Z, X)
    local = lambda p: (dot(sub(p, c), X), dot(sub(p, c), Y), dot(sub(p, c), Z))
    L = {v: local(m.pos(v)) for v in spec['verts']}
    xs = [p[0] for p in L.values()]
    scale = spec['length'] / (max(xs) - min(xs))

    # The grip: under the receiver at grip_x, its top at the receiver's underside there.
    near = [p for p in L.values() if abs(p[0] - spec['grip_x']) < 2.0]
    under = min(p[2] for p in near)
    grip_top = (spec['grip_x'], 0.0, under)

    # The origin: the grip's middle, half its length (less its sink into the receiver) under the receiver, leaning back.
    t = math.radians(GRIP_TILT)
    half = 0.5 * GRIP_LEN - SINK
    centre = (-math.sin(t) * half, 0.0, -math.cos(t) * half)

    gun = Gun()
    index = {}
    pos = []
    for v in spec['verts']:
        index[v] = len(gun.st)
        gun.st.append(tuple(m.stverts[v]))
        pos.append(sub(mul(sub(L[v], grip_top), scale), centre))
    dropped = 0
    for front, a, b, cc in tris:
        t = (front, index[a], index[b], index[cc])
        if tri_area(pos, *t[1:]) < 1e-4:
            dropped += 1
            continue
        gun.tris.append(t)
    gun.frames = [['frame%d' % (f + 1), list(pos), [0] * len(pos)] for f in range(FRAMES)]
    sealer = seal(gun, (0, 0, 8, 8), None, spec['src'])
    nrm = smooth_normals(gun.frames[0][1], gun.tris)
    for f in gun.frames:
        f[2] = list(nrm)
    fore = (spec['fore_x'] - spec['grip_x']) * scale - centre[0]
    return gun, m, dropped, sealer, scale, fore


def section(P, x, width=1.0):
    """The gun's extent (z min, z max, y min, y max) over the vertices within `width` of `x` along the barrel."""
    near = [p for p in P if abs(p[0] - x) < width] or P
    return (min(p[2] for p in near), max(p[2] for p in near), min(p[1] for p in near), max(p[1] for p in near))


def parts(path, spec, fore):
    """The grip, trigger guard, trigger, foregrip band, bolt heads and edge wear (mdlpolish.py), in place."""
    p = mp.Polisher(path, rows=16, seed=len(spec['src']))
    P = [tuple(q) for q in p.m.positions(0)]
    texels = mp.edge_wear(p.m, p.ramps, mesh=p.mesh)
    t = math.radians(GRIP_TILT)
    up = np.array((math.sin(t), 0.0, math.cos(t)))       # the grip's axis, butt to top
    fwd = np.array((math.cos(t), 0.0, -math.sin(t)))     # its front, square to it
    side = np.array((0.0, 1.0, 0.0))
    key = p.carrier_at((0.0, 0.0, 0.5 * GRIP_LEN))
    gm, sm = spec['grip_mat'], spec['guard_mat']
    # The grip, bevelled, and its butt plate; three ribs across its front (the fingers' places).
    p.box((0.0, 0.0, 0.0), fwd, side, up, (0.5 * GRIP_DEPTH, 0.5 * GRIP_WIDTH, 0.5 * GRIP_LEN), gm, key, bevel=GRIP_BEVEL)
    butt = -up * (0.5 * GRIP_LEN + 0.18)
    p.box(butt, fwd, side, up, (0.5 * GRIP_DEPTH + 0.15, 0.5 * GRIP_WIDTH + 0.12, 0.18), sm, key, bevel=0.2)
    for k in (-1.6, 0.0, 1.6):
        c = up * k + fwd * (0.5 * GRIP_DEPTH + 0.05)
        p.box(c, fwd, up, side, (0.12, 0.28, 0.5 * GRIP_WIDTH - 0.25), gm, key, levels=(0.55, 0.45))
    # The trigger guard: a bar forward from the grip's front, a little under the receiver, and up into it.
    top = 0.5 * GRIP_LEN - SINK                     # the receiver's underside, over the grip
    front = lambda z: 0.5 * GRIP_DEPTH / math.cos(t) + z * math.tan(t)
    gz = top - 2.5
    x0, x1 = front(gz) - 0.4, front(gz) + 3.3
    p.bar((x0, 0.0, gz), (x1, 0.0, gz), (0.0, 0.0, 1.0), 0.55, 0.45, sm, key, bevel=0.1)
    p.bar((x1 - 0.2, 0.0, gz - 0.2), (x1 + 0.5, 0.0, top + 0.6), (1.0, 0.0, 0.0), 0.55, 0.45, sm, key, bevel=0.1)
    # The trigger: curved back as it comes down (two short bars).
    tx = front(top) + 1.5
    p.bar((tx, 0.0, top + 0.4), (tx - 0.1, 0.0, top - 1.0), (1.0, 0.0, 0.0), 0.35, 0.4, sm, key)
    p.bar((tx - 0.1, 0.0, top - 0.9), (tx - 0.55, 0.0, top - 1.7), (1.0, 0.0, 0.0), 0.35, 0.4, sm, key)
    # The foregrip: a band round the barrel where the other hand holds it (the grunt's gun has its own).
    if spec['fore_band']:
        zlo, zhi, _, _ = section(P, fore)
        p.band((fore, 0.0, 0.5 * (zlo + zhi)), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), 3.0, 0.3, gm)
    # Bolt heads on the receiver's sides, over the grip.
    zlo, zhi, ylo, yhi = section(P, 1.5)
    for x in (-0.5, 2.5):
        for s_ in (1.0, -1.0):
            p.stud((x, s_ * (abs(ylo) + abs(yhi) + 10.0), 0.55 * zlo + 0.45 * zhi), (0.0, -s_, 0.0), radius=0.4,
                   height=0.25, material=sm)
    tris, verts, rows = p.finish(path)
    return tris, verts, rows, texels


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    progs = os.path.join(here, '..', '..', 'quakevr', 'progs')
    out_dir = sys.argv[1] if len(sys.argv) > 1 else progs
    paths = [os.path.join(out_dir, name) for name in GUNS]
    guard = genguard.Guard('make_enemyguns.py', paths)
    for name, spec in GUNS.items():
        gun, m, dropped, sealer, scale, fore = extract(progs, spec)
        P = gun.frames[0][1]
        # Room for the parts: the grip and its butt under the gun, bands and bolt heads round it.
        lo = [min(p[k] for p in P) - MARGIN for k in range(3)]
        hi = [max(p[k] for p in P) + MARGIN for k in range(3)]
        lo[0] = min(lo[0], -0.5 * GRIP_DEPTH - 0.5 * GRIP_LEN - 1.0)
        lo[2] = min(lo[2], -0.5 * GRIP_LEN - 1.0)
        path = os.path.join(out_dir, name)
        write_mdl(path, gun, m.skin, m.sw, m.sh, lo, hi)
        tris, verts, rows, texels = parts(path, spec, fore)
        print('%s: %d vertices, %d triangles (%d degenerate dropped), scale %.3f of the %s\'s; sealer: %s' % (
            name, len(gun.st), len(gun.tris), dropped, scale, spec['src'], '; '.join(sealer.log) or 'nothing to do'))
        print('  parts: +%d triangles, +%d vertices, %d skin rows, %d texels worn; the foregrip %.1f ahead' % (
            tris, verts, rows, texels, fore))
    guard.finish()


if __name__ == '__main__':
    main()
