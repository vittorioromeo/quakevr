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
# - the rifle made exactly mirror-symmetric (symmetrize, mirror_triangles: its sides sat off its middle);
# - a detail pass (GUNS' cuts, details_grunt, details_enforcer: ROUND21.md, "The enemy guns' detail pass" and "The enemy
#   guns carved, the rifle symmetric"): the grooves and vents the skins paint dark carved in (carve: Blender 5.2's exact
#   boolean, headless, blender/carve_mesh.py; QVR_BLENDER names another blender.exe), small sights, a thin barrel at
#   the muzzle, the grunts' gun a wire stock.
# Nine frames (0..8, the same pose): the frames a gun's firing animation steps through.
# The anchors (slots 21, 22: WpnTextAnchorVertex, MuzzleAnchorVertex; configs keep them) stay at their indices:
# pin_anchors orders the triangles so the counter's cut-out vertex and the barrel's bore centre come at them (GUNS'
# anchors). The bounds' corner (scale_origin, about which the weapon Scale pivots: the Offsets and hotspots follow it)
# is pinned too (GUNS' origin); the script prints both.

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

# The rifle's sights (model units along the barrel): the notch just ahead of the housing's peak, the post between its
# third and fourth vents.
ENF_REAR_SIGHT_X, ENF_FRONT_SIGHT_X = 6.3, 15.6


def slot(box, depth, probe, side):
    """A recess cut into the gun (carve): inside `box` (x0, x1, y0, y1, z0, z1) the surface sunk `depth`; its walls
    take the skin's texel where a ray from `probe` (outside the gun) meets it, inwards across `side` ('+y': along -y)."""
    d = {'+y': (0.0, -1.0, 0.0), '-y': (0.0, 1.0, 0.0), 'z': (0.0, 0.0, -1.0)}[side]
    box = (box[0], box[1], min(box[2], box[3]), max(box[2], box[3]), box[4], box[5])
    return dict(box=box, depth=depth, probe=(probe, d))


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
        origin=(-29.899999618530273, -6.170675277709961, -4.599999904632568),  # the bounds' corner (vr_weapons.inc)
        anchors=(23, 7, 1829),  # WpnTextAnchorVertex (strip index, the cut-out's vertex), MuzzleAnchorVertex
        cuts=[slot((x - 0.5, x + 0.5, -3.1, 3.1, 11.0, 30.0), 0.4, (x, 2.0, 40.0), 'z')  # the receiver's top grooves
              for x in (2.0, 4.9, 7.6, 10.3, 12.7)]
             + [slot((1.75, 8.0, 2.0, 12.0, 6.85, 9.1), 0.3, (2.5, 20.0, 9.7), '+y')]    # the window (on the right)
             + [slot((24.1, 28.1, 2.0 * s, 12.0 * s, 7.0, 9.0), 0.5, (26.0, 20.0 * s, 8.0), '+y' if s > 0 else '-y')
                for s in (1.0, -1.0)],                                                       # the muzzle cone's vents
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
        sight_top=13.8,
        symmetric=True,       # mirrored exactly about y = 0 (symmetrize), on a byte grid centred on it
        origin=(-16.39794921875, -6.089954376220703, -4.599999904632568),  # the bounds' corner (vr_weapons.inc)
        anchors=(37, 14, 1813),
        # The housing's five vents, over its top and down its sides (their bottoms follow its slope), both sides;
        # the muzzle cone's two grooves, all round.
        cuts=[slot((x0, x1, 1.95 * s, 12.0 * s, 7.6 - 0.07 * (0.5 * (x0 + x1) - 9.0), 30.0), 0.5,
                   (0.5 * (x0 + x1), 20.0 * s, 8.8), '+y' if s > 0 else '-y')
              for x0, x1 in ((8.38, 9.48), (11.13, 12.08), (13.8, 14.8), (16.43, 17.48), (19.1, 20.18))
              for s in (1.0, -1.0)]
             + [slot((x0, x1, -12.0, 12.0, -12.0, 30.0), 0.35, (0.5 * (x0 + x1), 0.0, 40.0), 'z')
                for x0, x1 in ((31.1, 31.8), (32.85, 33.45))],
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


def smooth_vectors(pos, tris):
    """Per vertex: the area-weighted normals of the faces round its welded position (Quake's front faces clockwise
    seen from outside: the outward normal is minus the cross product), unit vectors."""
    key = lambda p: tuple(round(x, 3) for x in p)
    acc = {}
    for _, a, b, c in tris:
        n = cross(sub(pos[b], pos[a]), sub(pos[c], pos[a]))
        for v in (a, b, c):
            k = key(pos[v])
            acc[k] = sub(acc.get(k, (0.0, 0.0, 0.0)), n)
    out = []
    for p in pos:
        n = acc.get(key(p), (0.0, 0.0, 1.0))
        out.append(norm(n) if dot(n, n) > 1e-12 else (0.0, 0.0, 1.0))
    return out


def nearest_anorm(n):
    table = anorms()
    return max(range(len(table)), key=lambda k: dot(table[k], n))


def smooth_normals(pos, tris):
    """Per vertex: the index of Quake's normal nearest its smooth_vectors one."""
    return [nearest_anorm(n) for n in smooth_vectors(pos, tris)]


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


def symmetrize(L):
    """The cut-out made exactly mirror-symmetric about its y = 0 plane (NOTES.md vrfiringrange_2026-10-01_02-38: the
    rifle's sides sat off its middle). Every vertex has a mirror partner in the monster's mesh (its left and right
    halves are the same vertices, only placed by hand on a coarse grid, up to a unit apart); each pair is set to the
    average of the one and the other's mirror image, a vertex that is its own partner onto the plane. Returns the new
    positions and the largest mirror error they had."""
    keys = list(L)
    P = np.array([L[k] for k in keys], np.float64)
    M = P * (1.0, -1.0, 1.0)
    partner = [int(np.argmin(np.linalg.norm(P - q, axis=1))) for q in M]
    assert all(partner[partner[i]] == i for i in range(len(keys))), 'the cut-out has no clean mirror pairing'
    err = max(float(np.linalg.norm(P[partner[i]] - M[i])) for i in range(len(keys)))
    out = {}
    for i, k in enumerate(keys):
        q = 0.5 * (P[i] + M[partner[i]])
        if partner[i] == i:
            q[1] = 0.0
        out[k] = tuple(float(c) for c in q)
    return out, err, {k: keys[partner[i]] for i, k in enumerate(keys)}


def mirror_triangles(tris, pos, partner):
    """The triangulation made mirror-symmetric too (the monster's diagonals differ side to side, so its two halves'
    quads bend differently and a cut crossing a diagonal lands off its mirror image): the triangles on the right
    (+y) side kept and mirrored onto the left as back faces (the left half of the skin's back half, as the monster's
    left side has it); those across the middle kept (a quad across it is a trapezoid, flat and symmetric)."""
    right, middle = [], []
    for t in tris:
        ys = [pos[v][1] for v in t[1:]]
        if min(ys) < -1e-9 and max(ys) > 1e-9:
            middle.append(t)
        elif max(ys) > 1e-9:
            assert t[0] == 1, 'a back face on the right side'
            right.append(t)
    left = [(0, partner[a], partner[c], partner[b]) for _, a, b, c in right]
    return right + middle + left


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
    scale = spec['length'] / (max(xs) - min(xs))  # (the length as cut out: symmetrize keeps the size)
    partner = None
    if spec.get('symmetric'):
        L, spec['mirror_error'], partner = symmetrize(L)

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
    if partner:
        gun.tris = mirror_triangles(gun.tris, pos, {index[a]: index[b] for a, b in partner.items()})
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
# The detail pass (NOTES.md e1m1_2026-09-30_23-27/23-31/23-34, e2m1_2026-09-30_23-42; vrfiringrange_2026-10-01_02-38,
# 02-39): the grooves and vents the skins paint dark carved into the guns (recesses whose floors keep the painted
# dark), a thin barrel at the muzzle (the muzzle anchor at its bore), small sights, and on the grunts' gun a light wire
# stock and its lamps as lenses. Positions are the guns' own (model units, origin in the grip), read off the skins
# painted over the cut-outs (the dark runs along and round each gun).

BLENDER = os.environ.get('QVR_BLENDER', r'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe')


def surface(p, x, y, z, d):
    """The old surface's point and outward normal met from (x, y, z) along d."""
    q, n, _ = p.hit((x, y, z), d)
    return q, n


def corner_uv(gun, sw, tri):
    """A triangle's corners' skin coordinates (texels; the back half's for an on-seam vertex of a back face)."""
    front = tri[0]
    out = []
    for v in tri[1:]:
        on, s, t = gun.st[v]
        out.append((float(s + (sw // 2 if on and not front else 0)), float(t)))
    return out


def ray_uv(P, faces, uvs, origin, d):
    """The skin coordinates where the ray first meets the mesh (Moller-Trumbore over every face)."""
    o, d = np.asarray(origin, np.float64), np.asarray(d, np.float64)
    best = None
    for (a, b, c), uv in zip(faces, uvs):
        pa, pb, pc = P[a], P[b], P[c]
        e1, e2 = pb - pa, pc - pa
        h = np.cross(d, e2)
        det = e1 @ h
        if abs(det) < 1e-12:
            continue
        f = 1.0 / det
        s = o - pa
        u = f * (s @ h)
        q = np.cross(s, e1)
        v = f * (d @ q)
        t = f * (e2 @ q)
        if u < 0 or v < 0 or u + v > 1 or t <= 1e-6 or (best and t >= best[0]):
            continue
        best = (t, np.array((1 - u - v, u, v)) @ np.array(uv))
    assert best, 'the probe %s along %s misses the gun' % (origin, d)
    return best[1]


def inner_mesh(W, faces, depth):
    """The welded mesh's vertices moved `depth` in, every face plane round a vertex moved in by the same (least
    squares over the planes: exact where three or fewer meet): the floor of a recess that follows the surface."""
    planes = [[] for _ in W]
    for f in faces:
        n = np.cross(W[f[1]] - W[f[0]], W[f[2]] - W[f[0]])
        a = np.linalg.norm(n)
        if a < 1e-9:
            continue
        n = n / a  # outward: the faces are counter-clockwise seen from outside
        for v in f:
            if all(float(n @ m) < 0.9999 for m in planes[v]):
                planes[v].append(n)
    out = []
    for v, ns in enumerate(planes):
        A = np.array(ns)
        delta = np.linalg.lstsq(A, -depth * np.ones(len(ns)), rcond=None)[0]
        if np.linalg.norm(delta) > 3.0 * depth:  # a needle-sharp corner: straight in along the mean normal
            m = A.sum(axis=0)
            delta = -depth * m / np.linalg.norm(m)
        out.append(W[v] + delta)
    return np.array(out)


def split_slivers(V, faces, uvs, cut):
    """Removes the triangles with no area (three corners in a line, where the boolean's polygons had a corner on an
    edge) without leaving a T-junction: the triangle across the sliver's long edge is split at its middle corner
    (that triangle's skin coordinates interpolated there). Counter-clockwise faces; returns how many went."""
    gone = 0
    while True:
        area = [np.linalg.norm(np.cross(V[f[1]] - V[f[0]], V[f[2]] - V[f[0]])) for f in faces]
        bad = next((i for i, a in enumerate(area) if a < 1e-6), None)
        if bad is None:
            return gone
        f = faces[bad]
        k = max(range(3), key=lambda j: np.linalg.norm(V[f[(j + 1) % 3]] - V[f[j]]))
        a, c, b = f[k], f[(k + 1) % 3], f[(k + 2) % 3]   # the long edge a -> c, b in its middle
        other = next((i for i, g in enumerate(faces) if i != bad and any(g[j] == c and g[(j + 1) % 3] == a
                                                                         for j in range(3))), None)
        assert other is not None, 'a sliver with nothing across its long edge'
        g, gu = faces[other], uvs[other]
        j = next(j for j in range(3) if g[j] == c and g[(j + 1) % 3] == a)
        d, ud = g[(j + 2) % 3], gu[(j + 2) % 3]
        uc, ua = gu[j], gu[(j + 1) % 3]
        t = np.linalg.norm(V[b] - V[c]) / np.linalg.norm(V[a] - V[c])
        ub = [uc[0] + t * (ua[0] - uc[0]), uc[1] + t * (ua[1] - uc[1])]
        new_f = [[c, b, d], [b, a, d]]
        new_u = [[uc, ub, ud], [ub, ua, ud]]
        keep = [i for i in range(len(faces)) if i not in (bad, other)]
        cut_other = cut[other]
        faces[:] = [faces[i] for i in keep] + new_f
        uvs[:] = [uvs[i] for i in keep] + new_u
        cut[:] = [cut[i] for i in keep] + [cut_other, cut_other]
        gone += 1


def interpolated_normal(q, P, tris, N):
    """The normals N of the vertices P interpolated at q over the triangle q lies on (the nearest)."""
    best = None
    for _, a, b, c in tris:
        pa, pb, pc = P[a], P[b], P[c]
        n = np.cross(pb - pa, pc - pa)
        nn = n @ n
        if nn < 1e-12:
            continue
        d = abs((q - pa) @ n) / math.sqrt(nn)
        r = q - pa
        w1 = np.cross(r, pc - pa) @ n / nn
        w2 = np.cross(pb - pa, r) @ n / nn
        w = np.array((1.0 - w1 - w2, w1, w2))
        out = -min(0.0, w.min())  # how far outside the triangle
        score = d + out
        if best is None or score < best[0]:
            w = np.clip(w, 0.0, None)
            best = (score, w / w.sum() @ N[[a, b, c]])
    n = best[1]
    return n / np.linalg.norm(n)


def carve(gun, skin, sw, cuts, name):
    """The cuts (dicts: box (x0, x1, y0, y1, z0, z1), depth, probe (a ray's origin and direction onto the painted
    vent: the recess's walls take that texel)) taken out of the gun's mesh by Blender (blender/carve_mesh.py,
    headless). The cut-out's own vertices keep their indices and the triangles left whole their order (the counter
    anchors are strip-order indices of the cut-out's vertices); the recesses' faces are flat-shaded. Returns the
    triangles cut away and added."""
    import json
    import subprocess
    import tempfile
    P = np.array(gun.frames[0][1], np.float64)
    weld, W, wi = {}, [], []
    for p in P:
        k = tuple(np.round(p, 4))
        if k not in weld:
            weld[k] = len(W)
            W.append(p)
        wi.append(weld[k])
    W = np.array(W)
    faces = [(wi[a], wi[c], wi[b]) for _, a, b, c in gun.tris]  # Quake's clockwise to counter-clockwise
    uvs = []
    for tri in gun.tris:
        u = corner_uv(gun, sw, tri)
        uvs.append([u[0], u[2], u[1]])
    job = {'verts': W.tolist(), 'faces': faces, 'uvs': uvs, 'cuts': []}
    lum = mp.palette() @ np.array((0.3, 0.59, 0.11))
    for c in cuts:
        # The walls: the darkest texel within two of where the probe meets the painted vent.
        u, v = (int(np.floor(x)) for x in ray_uv(W, faces, uvs, *c['probe']))
        u, v = min(((a, b) for a in range(u - 2, u + 3) for b in range(v - 2, v + 3)),
                   key=lambda q: (lum[skin[q[1] * sw + q[0]]], abs(q[0] - u) + abs(q[1] - v)))
        job['cuts'].append({'box': list(c['box']), 'inner': inner_mesh(W, faces, c['depth']).tolist(),
                            'wall_uv': [u + 0.5, v + 0.5]})
    script = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'blender', 'carve_mesh.py')
    with tempfile.TemporaryDirectory() as tmp:
        src, dst = os.path.join(tmp, 'in.json'), os.path.join(tmp, 'out.json')
        with open(src, 'w') as f:
            json.dump(job, f)
        r = subprocess.run([BLENDER, '-b', '--factory-startup', '--python-exit-code', '1', '-P', script, '--', src, dst],
                           capture_output=True, text=True)
        assert r.returncode == 0 and os.path.exists(dst), '%s: Blender failed:\n%s' % (name, (r.stdout + r.stderr)[-3000:])
        with open(dst) as f:
            res = json.load(f)
        if os.environ.get('QVR_CARVE_KEEP'):  # a folder to keep Blender's input and output in (debugging)
            import shutil
            for a, b in ((src, 'in'), (dst, 'out')):
                shutil.copy(a, os.path.join(os.environ['QVR_CARVE_KEEP'], '%s_%s.json' % (name, b)))
    V = np.array(res['verts'], np.float64)
    slivers = split_slivers(V, res['faces'], res['uvs'], res['cut'])

    # The old vertices by place and skin coordinates (an on-seam one twice: its front's and its back's).
    old = {}
    for v, p in enumerate(P):
        on, s, t = gun.st[v]
        k = tuple(np.round(p, 3))
        old.setdefault((k, s, t), []).append((v, 1 if on else None))
        if on:
            old.setdefault((k, s + sw // 2, t), []).append((v, 0))
    old_tris = {tuple(t[1:]): i for i, t in enumerate(gun.tris)}
    old_list = list(gun.tris)
    st, pos = [list(x) for x in gun.st], [tuple(float(c) for c in p) for p in P]
    new_key = {}

    def vertex(p, s, t, tag):
        k = (tuple(np.round(p, 4)), s, t, tag)
        if k not in new_key:
            new_key[k] = len(st)
            st.append([0, s, t])
            pos.append(tuple(float(c) for c in p))
        return new_key[k]

    kept, added, recess = {}, [], []
    for f, uv, is_cut in zip(res['faces'], res['uvs'], res['cut']):
        f = (f[0], f[2], f[1])  # counter-clockwise back to Quake's clockwise
        uv = (uv[0], uv[2], uv[1])
        sts = [(int(math.floor(u + 1e-3)), int(math.floor(v + 1e-3))) for u, v in uv]
        if is_cut:
            n = np.cross(V[f[1]] - V[f[0]], V[f[2]] - V[f[0]])
            tag = tuple(np.round(n / (np.linalg.norm(n) or 1.0), 3))
            recess.append((1,) + tuple(vertex(V[i], s, t, tag) for i, (s, t) in zip(f, sts)))
            continue
        # An old vertex where one is (with its skin coordinates), else a new one; the face's front flag as its
        # on-seam corners need it.
        cand = [old.get((tuple(np.round(V[i], 3)), s, t), []) for i, (s, t) in zip(f, sts)]
        fronts = {c[0][1] for c in cand if c and c[0][1] is not None}
        front = fronts.pop() if len(fronts) == 1 else 1
        ids = []
        for i, (s, t), c in zip(f, sts, cand):
            m = [v for v, fl in c if fl is None or fl == front]
            ids.append(m[0] if m else vertex(V[i], s, t, None))
        tri = (front,) + tuple(ids)
        if tuple(ids) in old_tris and gun.tris[old_tris[tuple(ids)]][0] == front:
            kept[old_tris[tuple(ids)]] = tri
        else:
            added.append(tri)
    cut_away = len(gun.tris) - len(kept)
    body = [kept[i] for i in sorted(kept)] + added
    gun.st = st
    gun.tris = body + recess
    # The surface shaded as before it was cut: a new vertex on it takes the old vertices' normals interpolated over
    # the old triangle it lies on (its faces' own normals would shade wedges across the old smooth faces); the
    # recesses flat.
    old_n = np.array(smooth_vectors([tuple(p) for p in P], old_list))
    nrm = [None] * len(pos)
    for v in range(len(P)):
        nrm[v] = nearest_anorm(tuple(old_n[v]))
    for _, *vs in body:
        for v in vs:
            if nrm[v] is None:
                nrm[v] = nearest_anorm(tuple(interpolated_normal(np.array(pos[v]), P, old_list, old_n)))
    for _, a, b, c in recess:
        k = nearest_anorm(norm(mul(cross(sub(pos[b], pos[a]), sub(pos[c], pos[a])), -1.0)))
        for v in (a, b, c):
            nrm[v] = k
    nrm = [0 if n is None else n for n in nrm]
    gun.frames = [[fr[0], list(pos), list(nrm)] for fr in gun.frames]
    return cut_away, len(added) + len(recess), slivers


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


def front_sight(p, x, top, material, key, base=(0.65, 0.6, 0.35), post=(0.28, 0.14), bevel=0.15):
    """A post on a small block, its top at `top`."""
    s, _ = surface(p, x, 0.0, 40.0, (0.0, 0.0, -1.0))
    X, Y, Z = np.eye(3)
    base_top = s[2] - 0.2 + 2.0 * base[2]
    p.box((x, 0.0, s[2] - 0.2 + base[2]), X, Y, Z, base, material, key, bevel=bevel)
    p.box((x, 0.0, 0.5 * (base_top + top) - 0.1), X, Y, Z, (post[0], post[1], 0.5 * (top - base_top) + 0.1), material,
          key)
    return top


def rear_sight(p, x, notch, material, key, ear=0.55, block=(0.4, 1.05), ears=(0.18, 0.42), gap=0.6):
    """A notch between two ears on a block across the top: the notch's bottom at `notch`."""
    s, _ = surface(p, x, 0.0, 40.0, (0.0, 0.0, -1.0))
    X, Y, Z = np.eye(3)
    p.box((x, 0.0, 0.5 * (s[2] - 0.2 + notch)), X, Y, Z, (block[0], block[1], 0.5 * (notch - s[2] + 0.2)), material,
          key, bevel=0.3 * block[0])
    for sy in (1.0, -1.0):
        p.box((x, sy * gap, notch + 0.5 * ear - 0.05), X, Y, Z, (ears[0], ears[1], 0.5 * ear + 0.05), material, key)


def details_grunt(p, spec, P, key):
    """The grunts' burst gun (its grooves, window and muzzle vents carved: GUNS' cuts): its lamps as lenses, sights,
    a wire stock, a thin barrel."""
    for x, z, mat in ((9.8, 8.9, 'red'), (11.8, 9.0, 'bronze')):
        q, n = surface(p, x, 20.0, z, (0.0, -1.0, 0.0))
        p.box(q, (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), n, (0.5, 0.55, 0.3), mat, key)
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
    """The enforcers' rifle (its vents and the muzzle cone's grooves carved: GUNS' cuts): small, low sights (NOTES.md
    vrfiringrange_2026-10-01_02-39: a notch just over the housing's peak, a thin post between the vents level with the
    notch's bottom, the line along the barrel), a thin barrel."""
    s, _ = surface(p, ENF_REAR_SIGHT_X, 0.0, 40.0, (0.0, 0.0, -1.0))
    notch = s[2] + 0.2
    rear_sight(p, ENF_REAR_SIGHT_X, notch, 'blued', key, ear=0.3, block=(0.2, 0.55), ears=(0.09, 0.14), gap=0.38)
    front_sight(p, ENF_FRONT_SIGHT_X, notch, 'blued', key, base=(0.26, 0.24, 0.12), post=(0.13, 0.07), bevel=0.06)
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


def pin_anchors(path, text_index, text_vertex, muzzle_index, muzzle):
    """Keeps the weapon settings' anchors (strip-order indices: configs keep them, so a changed index would leave a
    config's counter and muzzle elsewhere): orders the file's triangles so that strip-order index `text_index` is
    the cut-out's vertex `text_vertex` (where the counter was) and `muzzle_index` a vertex at the bore's bottom centre.
    The strip builder (improve_weapons.strip_order, vr_anchor.cpp's) joins triangles only through shared vertices,
    so a run of one piece's triangles gives the same strips wherever it stands: pieces filling the strip order up to
    the counter's index (a subset sum), the body (its triangles rotated to start at one with the counter's vertex),
    more pieces up to the muzzle index, the barrel, then the rest. Returns the pieces moved."""
    m = mp.Model(path)
    T = list(m.tris)
    Q = m.positions(0)
    root = list(range(len(m.st)))

    def find(v):
        while root[v] != v:
            root[v] = root[root[v]]
            v = root[v]
        return v

    for _, a, b, c in T:
        for x in (b, c):
            ra, rx = find(a), find(x)
            if ra != rx:
                root[rx] = ra
    pieces = {}
    for i, t in enumerate(T):
        pieces.setdefault(find(t[1]), []).append(i)
    pieces = list(pieces.values())
    bore = {v for v in range(len(Q)) if np.linalg.norm(Q[v] - muzzle) < 0.15}
    body = next(pc for pc in pieces if any(text_vertex in T[i][1:] for i in pc))
    barrel = next(pc for pc in pieces if any(set(T[i][1:]) & bore for i in pc))
    assert body is not barrel
    pool = [pc for pc in pieces if pc is not body and pc is not barrel]
    sizes = [len(strip_order([T[i] for i in pc])) for pc in pool]
    ob = strip_order([T[i] for i in barrel])

    def subset(target, avoid=()):
        reach = {0: []}  # strip entries -> the pool pieces giving them
        for k, n in enumerate(sizes):
            if k in avoid:
                continue
            for total, used in list(reach.items()):
                if total + n <= target and total + n not in reach:
                    reach[total + n] = used + [k]
        return reach.get(target)

    # The counter: the body's triangles rotated to start at one with its vertex (in the first strip's first corners),
    # after pool pieces filling the strip order up to there; the muzzle: more pool pieces, then the barrel.
    for s0 in [k for k, i in enumerate(body) if text_vertex in T[i][1:]]:
        lead = body[s0:] + body[:s0]
        o = strip_order([T[i] for i in lead])
        for k in [k for k, v in enumerate(o) if v == text_vertex and k <= text_index]:
            before = subset(text_index - k)
            if before is None:
                continue
            start = text_index - k + len(o)
            for kb in [kb for kb, v in enumerate(ob) if v in bore]:
                if muzzle_index - kb - start < 0:
                    continue
                middle = subset(muzzle_index - kb - start, set(before))
                if middle is not None:
                    break
            else:
                continue
            break
        else:
            continue
        break
    else:
        raise AssertionError('%s: no order of the pieces keeps the anchors %d and %d' % (path, text_index, muzzle_index))
    chosen = set(before) | set(middle)
    order = ([i for k in before for i in pool[k]] + lead + [i for k in middle for i in pool[k]] + barrel
             + [i for k, pc in enumerate(pool) if k not in chosen for i in pc])
    m.tris = [T[i] for i in order]
    full = strip_order(m.tris)
    assert full[text_index] == text_vertex and full[muzzle_index] in bore
    m.write(path)
    return len(chosen)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    progs = os.path.join(here, '..', '..', 'quakevr', 'progs')
    out_dir = sys.argv[1] if len(sys.argv) > 1 else progs
    paths = [os.path.join(out_dir, name) for name in GUNS]
    guard = genguard.Guard('make_enemyguns.py', paths)
    for name, spec in GUNS.items():
        gun, m, dropped, sealer, scale, fore = extract(progs, spec)
        cut_away, carved, slivers = carve(gun, m.skin, m.sw, spec['cuts'], name)
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
        # The bounds' corner (scale_origin, about which the weapon Scale pivots) kept where the weapon settings have it.
        assert all(lo[k] >= spec['origin'][k] - 1e-3 for k in range(3)), '%s: the gun leaves its bounds %s' % (name, lo)
        lo = list(spec['origin'])
        if spec.get('symmetric'):  # y = 0 a step of the byte grid (127): its mirror image is on the grid too
            assert max(abs(p[1]) for p in P) < -lo[1]
            hi[1] = lo[1] - 255.0 * lo[1] / 127.0
        path = os.path.join(out_dir, name)
        write_mdl(path, gun, m.skin, m.sw, m.sh, lo, hi)
        tris, verts, rows, texels, muzzle = parts(path, spec, fore)
        # The anchors kept at their indices (WpnTextAnchorVertex, MuzzleAnchorVertex: a vertex at the bore's bottom
        # centre).
        moved = pin_anchors(path, *spec['anchors'], muzzle)
        done = mp.Model(path)
        Q = done.positions(0)
        order = strip_order(done.tris)
        print('%s: %d triangles, %d vertices; scale_origin %s; muzzle anchor %d at %s (the bore: %s); counter anchor '
              '%d at %s (%d pieces moved ahead of the barrel)' % (
                  name, len(done.tris), len(done.st), np.round(done.origin, 4).tolist(), spec['anchors'][2],
                  np.round(Q[order[spec['anchors'][2]]], 2).tolist(), np.round(muzzle, 2).tolist(), spec['anchors'][0],
                  np.round(Q[order[spec['anchors'][0]]], 2).tolist(), moved))
        print('%s: %d vertices, %d triangles (%d degenerate dropped), scale %.3f of the %s\'s; sealer: %s' % (
            name, len(gun.st), len(gun.tris), dropped, scale, spec['src'], '; '.join(sealer.log) or 'nothing to do'))
        print('  carved: %d triangles cut away, %d added (%d cuts, %d slivers split away); parts: +%d triangles, +%d vertices, %d skin rows, '
              '%d texels worn; the foregrip %.1f ahead' % (cut_away, carved, len(spec['cuts']), slivers, tris, verts, rows, texels,
                                                           fore))
        if spec.get('symmetric'):
            M = Q * (1.0, -1.0, 1.0)
            err = max(float(np.min(np.linalg.norm(Q - q, axis=1))) for q in M)
            print('  mirror error (every vertex to the nearest to its mirror image): %.6f (the cut-out had %.3f)' % (
                err, spec['mirror_error']))
    guard.finish()


if __name__ == '__main__':
    main()
