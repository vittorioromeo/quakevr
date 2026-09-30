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
# - a detail pass (details_grunt, details_enforcer: ROUND21.md, "The enemy guns' detail pass"): the ridges, vents and
#   grooves the skins paint made geometry, primitive sights, a thin barrel at the muzzle, the grunts' gun a wire stock.
# Nine frames (0..8, the same pose): the frames a gun's firing animation steps through.
# The counter anchors (slots 21, 22: WpnTextAnchorVertex) are strip-order indices of the cut-out vertices (the parts are
# appended after them): rerun improve_weapons.strip_order after changing the cut. The muzzle anchors (MuzzleAnchorVertex)
# are the thin barrels' bore centres: printed as the script runs. The stock moves the grunts' gun's bounds' corner
# (scale_origin, printed too), about which the weapon Scale pivots: its Offset and hotspots then follow (vr_weapons.inc).

import math
import os
import struct
import sys

import numpy as np

import genguard
import mdlpolish as mp
from improve_weapons import strip_order
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
        details='grunt',      # details_grunt
        stock_x=-28.6,        # the butt plate's middle (the pad behind it)
        barrel=4.2,           # the thin barrel's reach past the muzzle's face
        sight_top=15.0,
    ),
    'v_enfrifle.mdl': dict(
        src='enforcer.mdl', counts=(479, 984), tris=118,
        verts=[22, 23, 100] + list(range(400, 431)) + list(range(455, 479)),
        length=52.0,          # (x 0.4: about 75 cm)
        grip_x=-9.0,
        fore_x=6.0,
        grip_mat='brown', guard_mat='blued',
        fore_band=True,       # a band at the vented housing's front, where the other hand holds it
        details='enforcer',   # details_enforcer
        stock_x=None,
        barrel=4.6,
        sight_top=14.7,
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


# ----------------------------------------------------------------------------
# The detail pass (NOTES.md e1m1_2026-09-30_23-27/23-31/23-34, e2m1_2026-09-30_23-42): what the skins only paint
# (ridges, vents, grooves, lamps) made geometry, a thin barrel at the muzzle (the muzzle anchor at its bore), primitive
# sights, and on the grunts' gun a light wire stock. Positions are the guns' own (model units, origin in the grip),
# read off orthographic projections of the textured cut-outs.

def surface(p, x, y, z, d):
    """The old surface's point and outward normal met from (x, y, z) along d."""
    q, n, _ = p.hit((x, y, z), d)
    return q, n


def ridge(p, x, zmin, width, height, material, key, sink=0.15, shade=0.0, turn=18.0):
    """A raised rib across the gun at `x`: over the outline of its section above `zmin` (the top and the upper sides,
    clear of what is painted lower down; None: all round, a ring), `height` proud of it, `width` along the barrel.
    Corners turning less than `turn` degrees are dropped (fewer faces)."""
    ax = np.array((1.0, 0.0, 0.0))
    hull, u, v, _ = p.outline((x, 0.0, 0.0), ax, (0.0, 0.0, 1.0), width)
    c = np.array((x, 0.0, 0.0))
    pts = [c + u * a + v * b for a, b in hull]
    n = len(pts)
    mid = sum(pts) / n
    closed = zmin is None
    if closed:
        arc = list(pts)
    else:
        i0 = next(i for i in range(n) if pts[i][2] < zmin <= pts[(i + 1) % n][2])

        def cut(a, b):
            t = (zmin - a[2]) / (b[2] - a[2])
            return a + t * (b - a)

        arc = [cut(pts[i0], pts[(i0 + 1) % n])]
        k = (i0 + 1) % n
        while pts[k][2] >= zmin:
            arc.append(pts[k])
            k = (k + 1) % n
        arc.append(cut(pts[(k - 1) % n], pts[k]))
    arc = [a for i, a in enumerate(arc) if i == 0 or np.linalg.norm(a - arc[i - 1]) > 0.05]
    cos_turn = math.cos(math.radians(turn))
    changed = True
    while changed and len(arc) > (3 if closed else 2):
        changed = False
        for i in range(0 if closed else 1, len(arc) if closed else len(arc) - 1):
            d0 = arc[i] - arc[i - 1]
            d1 = arc[(i + 1) % len(arc)] - arc[i]
            if d0 @ d1 / (np.linalg.norm(d0) * np.linalg.norm(d1)) > cos_turn:
                del arc[i]
                changed = True
                break
    m = len(arc)
    segs = [(i, (i + 1) % m) for i in range(m if closed else m - 1)]

    def edge_normal(a, b):
        e = (b - a) / np.linalg.norm(b - a)
        o = 0.5 * (a + b) - mid
        o = o - e * (o @ e) - ax * (o @ ax)
        return o / np.linalg.norm(o)

    en = [edge_normal(arc[i], arc[j]) for i, j in segs]
    inner, outer = [], []
    for i, a in enumerate(arc):
        ns = [en[k] for k, (s0, s1) in enumerate(segs) if i in (s0, s1)]
        nn = sum(ns)
        nn = nn / np.linalg.norm(nn)
        mitre = 1.0 / max(0.6, min(nn @ e for e in ns))
        inner.append(a - nn * sink)
        outer.append(a + nn * height * mitre)
    h = ax * 0.5 * width
    for k, (i, j) in enumerate(segs):
        o = en[k]
        p.face([outer[i] - h, outer[j] - h, outer[j] + h, outer[i] + h], o, material, p.lit(o) + shade + 0.1, key)
        p.face([inner[i] + h, inner[j] + h, outer[j] + h, outer[i] + h], ax, material, p.lit(ax) + shade, key)
        p.face([inner[i] - h, inner[j] - h, outer[j] - h, outer[i] - h], -ax, material, p.lit(-ax) + shade, key)
    if not closed:
        for i, j in ((0, 1), (m - 1, m - 2)):
            t = arc[i] - arc[j]
            t = t / np.linalg.norm(t)
            p.face([inner[i] - h, outer[i] - h, outer[i] + h, inner[i] + h], t, material, p.lit(t) + shade, key)


def framed_slot(p, x0, x1, z0, z1, side, material, key, width=0.45, height=0.6, slats=(), ends=True):
    """A raised lip round a vent painted on the side (`side` +1: +y), and slats across it at the heights `slats`:
    the painted dark reads as a recess."""
    def at(x, z):
        return surface(p, x, side * 20.0, z, (0.0, -side, 0.0))

    def bar(a, b):
        (qa, na), (qb, nb) = a, b
        e = (qb - qa) / np.linalg.norm(qb - qa)
        up = na + nb
        p.bar(qa - e * 0.5 * width, qb + e * 0.5 * width, up, width, height, material, key)

    c = {(i, j): at(x, z) for i, x in enumerate((x0, x1)) for j, z in enumerate((z0, z1))}
    bar(c[0, 0], c[1, 0])
    bar(c[0, 1], c[1, 1])
    if ends:
        bar(c[0, 0], c[0, 1])
        bar(c[1, 0], c[1, 1])
    for z in slats:
        bar(at(x0, z), at(x1, z))


def tube(p, a, b, r, material, key, sides=6):
    """A plain rod from a to b."""
    a, b = np.asarray(a, np.float64), np.asarray(b, np.float64)
    L = np.linalg.norm(b - a)
    ax = (b - a) / L
    ref = (0.0, 0.0, 1.0) if abs(ax[2]) < 0.9 else (1.0, 0.0, 0.0)
    p.revolve(a, ax, ref, [(0.0, 0.0), (0.0, r), (L, r), (L, 0.0)], material, key, sides=sides,
              shades=[0.0, 0.1, 0.0, 0.0], skip=(0, 2))  # its ends are sunk in what it joins


def barrel_tip(p, face_x, yc, zc, length, r, collar, material, key, bore=0.35, depth=0.3, sides=6):
    """A thin barrel out of the muzzle's face (its root sunk into it, a collar round it), its bore dark. Returns the
    bore's bottom centre: the muzzle anchor's place."""
    root = face_x - 0.6
    L = face_x + length - root
    prof = [(0.0, 0.0), (0.0, collar), (1.3, collar), (1.3, r), (L, r), (L, bore), (L - depth, bore), (L - depth, 0.0)]
    p.revolve((root, yc, zc), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), prof, material, key, sides=sides,
              shades=[0, 0.05, 0.25, 0.0, 0.3, -0.35, -0.6, 0], skip=(0,))
    return np.array((root + L - depth, yc, zc))


def front_sight(p, x, top, material, key):
    """A post on a small block, its top at `top`."""
    s, _ = surface(p, x, 0.0, 40.0, (0.0, 0.0, -1.0))
    X, Y, Z = np.eye(3)
    base_top = s[2] + 0.5
    p.box((x, 0.0, s[2] + 0.15), X, Y, Z, (0.65, 0.6, 0.35), material, key, bevel=0.15)
    p.box((x, 0.0, 0.5 * (base_top + top) - 0.1), X, Y, Z, (0.28, 0.14, 0.5 * (top - base_top) + 0.1), material, key)
    return top


def rear_sight(p, x, notch, material, key, ear=0.55):
    """A notch between two ears on a block across the top: the notch's bottom at `notch`."""
    s, _ = surface(p, x, 0.0, 40.0, (0.0, 0.0, -1.0))
    X, Y, Z = np.eye(3)
    p.box((x, 0.0, 0.5 * (s[2] - 0.2 + notch)), X, Y, Z, (0.4, 1.05, 0.5 * (notch - s[2] + 0.2)), material, key,
          bevel=0.12)
    for sy in (1.0, -1.0):
        p.box((x, sy * 0.6, notch + 0.5 * ear - 0.05), X, Y, Z, (0.18, 0.42, 0.5 * ear + 0.05), material, key)


def details_grunt(p, spec, P, key):
    """The grunts' burst gun: ribs over the receiver's top (between its painted dark ones), the window's frame, the
    lamps, the muzzle cone's vents lipped and slatted, bolts on the clamp band, sights, a wire stock, a thin barrel."""
    for x in (2.4, 4.8, 7.2, 9.6, 12.0):
        ridge(p, x, 10.5, 0.8, 0.35, 'bronze', key)
    # The window on the right (+y) side, framed; its two lamps as lenses.
    framed_slot(p, 1.5, 8.3, 5.9, 10.3, 1.0, 'blued', key, width=0.4, height=0.5)
    for x, z, mat in ((9.8, 8.9, 'red'), (11.8, 9.0, 'bronze')):
        q, n = surface(p, x, 20.0, z, (0.0, -1.0, 0.0))
        p.box(q, (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), n, (0.5, 0.55, 0.3), mat, key)
    # The vents painted on both sides of the muzzle cone.
    for side in (1.0, -1.0):
        framed_slot(p, 24.8, 28.4, 7.0, 9.7, side, 'bronze', key, width=0.35, height=0.5, slats=(8.35,), ends=False)
    # Sights: a notch at the receiver's back, a post ahead of the clamp band (level with the notch's bottom).
    rear_sight(p, -3.5, 14.35, 'blued', key)
    front_sight(p, 22.4, 14.45, 'blued', key)
    # The stock: a mount on the back, two rods straight back and one down, a butt plate and its pad.
    X, Y, Z = np.eye(3)
    sx = spec['stock_x']
    p.box((-16.4, 0.0, 8.1), X, Y, Z, (0.55, 1.9, 2.2), 'blued', key)
    for y in (1.25, -1.25):
        tube(p, (-16.2, y, 9.3), (sx + 0.4, y, 9.3), 0.42, 'blued', key)
    tube(p, (-16.2, 0.0, 6.6), (sx + 0.4, 0.0, 4.4), 0.42, 'blued', key)
    p.box((sx, 0.0, 7.0), X, Y, Z, (0.45, 1.75, 3.35), 'blued', key, bevel=0.35)
    p.box((sx - 0.65, 0.0, 7.0), X, Y, Z, (0.22, 1.6, 3.2), 'black', key)
    return barrel_tip(p, *spec['muzzle_face'], length=4.2, r=0.72, collar=1.05, material='blued', key=key)


def details_enforcer(p, spec, P, key):
    """The enforcers' rifle: fins between the housing's painted vents and a spine over them, raised rings between the
    muzzle cone's painted grooves, sights, a thin barrel."""
    for x in (9.35, 11.95, 14.55, 17.15):
        ridge(p, x, 6.6, 0.9, 0.35, 'bronze', key)
    X, Y, Z = np.eye(3)
    s0, _ = surface(p, 12.0, 0.0, 40.0, (0.0, 0.0, -1.0))
    p.box((13.25, 0.0, s0[2] + 0.1), X, Y, Z, (4.6, 0.55, 0.3), 'bronze', key)
    for x in (31.15, 32.9):
        ridge(p, x, None, 0.75, 0.22, 'brown', key)
    rear_sight(p, 5.4, 14.05, 'blued', key)
    front_sight(p, 20.6, 14.15, 'blued', key)
    return barrel_tip(p, *spec['muzzle_face'], length=4.6, r=0.85, collar=1.25, material='blued', key=key)


def parts(path, spec, fore):
    """The grip, trigger guard, trigger, foregrip band, bolt heads and edge wear (mdlpolish.py), and the detail pass,
    in place. Returns also the muzzle (the barrel's bore)."""
    p = mp.Polisher(path, rows=28, seed=len(spec['src']))
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
    muzzle = globals()['details_' + spec['details']](p, spec, P, key)
    tris, verts, rows = p.finish(path)
    return tris, verts, rows, texels, muzzle


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
        # The detail pass: the barrel ahead of the muzzle's face (its middle: the face's), the sights over the top,
        # the stock behind.
        xmax = max(p[0] for p in P)
        face = [p for p in P if p[0] >= xmax - 0.6]
        spec['muzzle_face'] = (sum(p[0] for p in face) / len(face), 0.5 * (min(p[1] for p in face) + max(p[1] for p in face)),
                               0.5 * (min(p[2] for p in face) + max(p[2] for p in face)))
        hi[0] = max(hi[0], spec['muzzle_face'][0] + spec['barrel'] + 0.3)
        hi[2] = max(hi[2], spec['sight_top'] + 0.3)
        if spec['stock_x'] is not None:
            lo[0] = min(lo[0], spec['stock_x'] - 1.3)
        path = os.path.join(out_dir, name)
        write_mdl(path, gun, m.skin, m.sw, m.sh, lo, hi)
        tris, verts, rows, texels, muzzle = parts(path, spec, fore)
        # The muzzle anchor (MuzzleAnchorVertex): the strip-order index of a vertex at the bore's bottom centre.
        done = mp.Model(path)
        Q = done.positions(0)
        order = strip_order(done.tris)
        near = min(range(len(order)), key=lambda i: float(np.linalg.norm(Q[order[i]] - muzzle)))
        print('%s: %d triangles, %d vertices; scale_origin %s; muzzle anchor %d at %s (the bore: %s)' % (
            name, len(done.tris), len(done.st), np.round(done.origin, 4).tolist(), near,
            np.round(Q[order[near]], 2).tolist(), np.round(muzzle, 2).tolist()))
        print('%s: %d vertices, %d triangles (%d degenerate dropped), scale %.3f of the %s\'s; sealer: %s' % (
            name, len(gun.st), len(gun.tris), dropped, scale, spec['src'], '; '.join(sealer.log) or 'nothing to do'))
        print('  parts: +%d triangles, +%d vertices, %d skin rows, %d texels worn; the foregrip %.1f ahead' % (
            tris, verts, rows, texels, fore))
    guard.finish()


if __name__ == '__main__':
    main()
