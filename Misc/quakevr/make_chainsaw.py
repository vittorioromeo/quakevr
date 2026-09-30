#!/usr/bin/env python3
# make_chainsaw.py -- the ogres' chainsaw as a weapon: progs/v_chainsaw.mdl, cut out of Quake VR's own ogre model
# (quakevr/progs/ogre.mdl), as make_swords.py cuts the knights' swords out of theirs.
#
# Usage: python Misc/quakevr/make_chainsaw.py [output progs folder]
#
# The chainsaw is the ogre's vertices 416..496 (its bar, its engine block, its rear handle and the loop of its front
# handle round the block: separate from the ogre's body, only a degenerate triangle touches it): the same list as
# Quake/vr/vr_monstermods.cpp's knownPieces, which hides it in the ogre's death frames (the ogre drops this one).
# Taken from the ogre's first frame (stand1), and laid in the weapon's own frame:
#   +x along the bar (from the engine block to the bar's tip), +z up (the front handle's loop over the block), +y left;
#   the origin in the middle of the rear handle's back bar, which the main hand holds as a pistol's grip (the bar
#   upright, the chain ahead): with the weapon settings' Scale 0.34, the hand holds it there and the bar points where
#   the controller aims (vr_weapons.inc, slot 20).
# Scaled so that it is about 82 cm long (the ogre's is a giant's). Its triangles, skin coordinates and skin are the
# ogre's own (the whole of skin 0, as the swords keep their knights').
#
# Added: the starter cord's T-handle (Quake VR's: the chainsaw starts with a physical pull, vr_chainsaw.cpp), a small
# block on the block's top left, behind the front handle, painted with the rear handle's texels. Ten frames: 0..8 the
# handle seated (the weapon frames a gun's animations may use), 9 the handle out (its vertices collapsed into the cord's
# hole): the game shows frame 9 while a hand holds the cord, and draws the handle and the cord there itself. The
# engine reads the handle from the model as it loads it: the vertices that differ between frames 0 and 9 are the
# handle, their middle in frame 0 is where the hand takes it, and the point they collapse onto is the cord's hole.
#
# Cleaned for close-up viewing (round 21, as make_enemyguns.py the grunts' and enforcers' guns): the ogre's degenerate
# triangles dropped, then `polish` (mdlpolish.py) appends a starter housing under the cord's handle, two nuts on the
# clutch cover and the skin's edge wear, keeping every old vertex, triangle and UV (the anchors: checked).

import math
import os
import struct
import sys

import genguard
import mdlpolish as mp
from improve_weapons import strip_order
from make_swords import Mdl, Out
from mdlgen import HEADER, add, anorms, cross, dot, mul, norm, sub

FIRST, LAST = 416, 496          # the chainsaw's vertices in Quake VR's ogre.mdl (stand1 .. every frame)
BAR = range(416, 434)           # the bar
REAR = range(451, 469)          # the rear handle (a loop behind the block, in the saw's upright plane)
LOOP = range(469, 489)          # the front handle's loop round the block
LENGTH = 68.0                   # model units, the rear handle's back to the bar's tip (x 0.34 x 1.1667: 27 units, 82 cm)
FRAMES = 10                     # 0..8 the handle seated; 9 the handle out
PULLED = 9

# The cord's T-handle, in the saw's frame (model units, after scaling): its middle, half its length (along x),
# half its thickness; and the cord's hole below it (on the block's top).
HANDLE_HALF = (3.7, 1.0, 1.0)


def principal(pts):
    """The centre and the principal axes (the largest spread first) of `pts`."""
    n = len(pts)
    c = mul(tuple(sum(p[i] for p in pts) for i in range(3)), 1.0 / n)
    cov = [[sum((p[i] - c[i]) * (p[j] - c[j]) for p in pts) for j in range(3)] for i in range(3)]
    axes = []
    for _ in range(3):
        a = (1.0, 0.7, 0.3)
        for _ in range(200):
            a = tuple(sum(cov[i][j] * a[j] for j in range(3)) for i in range(3))
            for b in axes:
                a = sub(a, mul(b, dot(a, b)))
            a = norm(a)
        axes.append(a)
    return c, axes


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    progs = os.path.join(here, '..', '..', 'quakevr', 'progs')
    out_dir = sys.argv[1] if len(sys.argv) > 1 else progs
    m = Mdl(open(os.path.join(progs, 'ogre.mdl'), 'rb').read())
    assert (m.nverts, m.ntris) == (497, 1290), 'not Quake VR\'s ogre.mdl (%d vertices, %d triangles)' % (m.nverts, m.ntris)
    verts = list(range(FIRST, LAST + 1))
    tris = [t for t in m.tris if all(FIRST <= v <= LAST for v in t[1:])]

    # The saw's frame: x along the bar (away from the block), y the rear handle's plane's normal, z up (towards the
    # front handle's loop's middle's side it arches to: its top).
    block = [m.pos(v) for v in verts if v not in BAR and v not in REAR and v not in LOOP]
    bc = mul(tuple(sum(p[i] for p in block) for i in range(3)), 1.0 / len(block))
    c, axes = principal([m.pos(v) for v in BAR])
    X = axes[0] if dot(sub(c, bc), axes[0]) > 0 else mul(axes[0], -1.0)
    _, raxes = principal([m.pos(v) for v in REAR])
    Y = norm(sub(raxes[2], mul(X, dot(raxes[2], X))))
    Z = cross(X, Y)
    lc = mul(tuple(sum(m.pos(v)[i] for v in LOOP) for i in range(3)), 1.0 / len(LOOP))
    if dot(sub(lc, bc), Z) < 0:
        Z = mul(Z, -1.0)
    Y = cross(Z, X)

    def local(p):
        d = sub(p, bc)
        return (dot(d, X), dot(d, Y), dot(d, Z))

    L = {v: local(m.pos(v)) for v in verts}
    # The grip: the rear handle's back bar (its vertices within 2 ogre units of its rearmost).
    back = min(L[v][0] for v in REAR)
    bar = [L[v] for v in REAR if L[v][0] < back + 2.0]
    grip = mul(tuple(sum(p[i] for p in bar) for i in range(3)), 1.0 / len(bar))
    tip = max(L[v][0] for v in BAR)
    scale = LENGTH / (tip - back)

    out = Out()
    index = {}
    for v in verts:
        p = mul(sub(L[v], grip), scale)
        index[v] = out.vert(p, m.stverts[v], None)
    degenerate = 0
    for front, a, b, cc in tris:
        n = cross(sub(out.p[index[b]], out.p[index[a]]), sub(out.p[index[cc]], out.p[index[a]]))
        if dot(n, n) < 1e-6:
            degenerate += 1  # no area (the ogre's slivers where its pieces met): dropped
            continue
        out.tris.append((front, index[a], index[b], index[cc]))
    saw_tris = len(out.tris)

    # The block's top and left (for the handle): its vertices, in the saw's frame.
    blk = [out.p[index[v]] for v in verts if v not in BAR and v not in REAR and v not in LOOP]
    top = max(p[2] for p in blk)
    left = max(p[1] for p in blk)
    loop = [out.p[index[v]] for v in LOOP]
    rear_of_loop = min(p[0] for p in loop)
    blk_rear = min(p[0] for p in blk)
    hx, hy, hz = HANDLE_HALF
    # On the block's top, at its left edge, between the block's back and the front handle's loop.
    mid = ((blk_rear + rear_of_loop) * 0.5, left - hy - 0.5, top + hz + 0.4)
    hole = (mid[0], mid[1], top - 0.6)
    # The rear handle's texels for it (the plastic): the middle of the rear handle's triangles' skin coordinates.
    rs = [m.stverts[v] for v in REAR]
    s0 = min(s for _, s, _ in rs)
    t0 = min(t for _, _, t in rs)
    s1 = max(s for _, s, _ in rs)
    t1 = max(t for _, _, t in rs)
    region = (s0 + (s1 - s0) // 4, t0 + (t1 - t0) // 4, s0 + 3 * (s1 - s0) // 4, t0 + 3 * (t1 - t0) // 4)
    first_handle = len(out.p)
    ring = lambda w: (w, mid[1] - hy, mid[1] + hy, mid[2] - hz, mid[2] + hz)
    out.loft([ring(mid[0] - hx), ring(mid[0] + hx)], 'x', region)
    handle = range(first_handle, len(out.p))

    # Normals: the saw's from its faces (clockwise front faces), the handle's its faces' own.
    acc = {i: (0.0, 0.0, 0.0) for i in range(first_handle)}
    for _, a, b, cc in out.tris[:saw_tris]:
        n = cross(sub(out.p[b], out.p[a]), sub(out.p[cc], out.p[a]))
        for i in (a, b, cc):
            acc[i] = add(acc[i], mul(n, -1.0))
    for i, n in acc.items():
        out.n[i] = norm(n) if dot(n, n) > 1e-12 else (0.0, 0.0, 1.0)
    for i, (onseam, s, t) in enumerate(out.st):
        out.st[i] = (onseam, s, t)

    normals = anorms()
    nidx = [max(range(len(normals)), key=lambda k: dot(normals[k], n)) for n in out.n]
    pulled = [hole if i in handle else p for i, p in enumerate(out.p)]
    poses = [out.p] * (FRAMES - 1) + [pulled]
    lo = [min(p[i] for p in out.p) for i in range(3)]
    hi = [max(p[i] for p in out.p) for i in range(3)]
    qscale = [max((hi[i] - lo[i]) / 255.0, 1e-4) for i in range(3)]
    radius = max(math.sqrt(dot(p, p)) for p in out.p)

    data = bytearray(HEADER.pack(b'IDPO', 6, *qscale, *lo, radius, 0.0, 0.0, 0.0,
                                 1, m.sw, m.sh, len(out.p), len(out.tris), FRAMES, 0, 0, 1.0))
    data += struct.pack('<i', 0) + bytes(m.skin)
    for st in out.st:
        data += struct.pack('<3i', *st)
    for t in out.tris:
        data += struct.pack('<4i', *t)
    for f, pose in enumerate(poses):
        q = [[max(0, min(255, round((p[k] - lo[k]) / qscale[k]))) for k in range(3)] for p in pose]
        bmin = bytes([min(v[k] for v in q) for k in range(3)] + [0])
        bmax = bytes([max(v[k] for v in q) for k in range(3)] + [0])
        name = ('pulled' if f == PULLED else 'frame%d' % (f + 1)).encode().ljust(16, b'\0')
        data += struct.pack('<i', 0) + bmin + bmax + name + b''.join(bytes(v + [nidx[i]]) for i, v in enumerate(q))

    path = os.path.join(out_dir, 'v_chainsaw.mdl')
    guard = genguard.Guard('make_chainsaw.py', [path])
    open(path, 'wb').write(data)
    tipv = max(range(first_handle), key=lambda i: out.p[i][0])
    order = strip_order(out.tris)
    blk_verts = [index[v] for v in verts if v not in BAR and v not in REAR and v not in LOOP]
    added = polish(path, out.p, [index[v] for v in BAR], blk_verts, hole, mid)
    after = strip_order(mp.Model(path).tris)
    assert after[:len(order)] == order, 'the polish moved the anchors'
    guard.finish()
    print('v_chainsaw.mdl: %d vertices, %d triangles (%d degenerate dropped), %d frames; scale %.3f of the ogre\'s' % (
        len(out.p), len(out.tris), degenerate, FRAMES, scale))
    print('  polish: +%d triangles, +%d vertices, %d skin rows, %d texels worn' % added)
    print('  the bar\'s tip (the muzzle): vertex %d at %.2f %.2f %.2f, anchor %d (slot 20\'s MuzzleAnchorVertex)' % (
        tipv, *out.p[tipv], order.index(tipv)))
    print('  the cord\'s handle at %.2f %.2f %.2f, its hole at %.2f %.2f %.2f' % (*mid, *hole))
    print('  the front handle\'s top: x %.2f..%.2f, z %.2f' % (min(p[0] for p in loop), max(p[0] for p in loop),
                                                           max(p[2] for p in loop)))
    print('  bounds %.1f %.1f %.1f .. %.1f %.1f %.1f' % (*lo, *hi))


def polish(path, P, bar, block, hole, handle):
    """The close-up pass (mdlpolish.py, as polish_weapons.py the guns'): the skin's edge wear; two nuts holding the bar on
    the clutch cover (the block's right side); the starter's housing under the cord's T-handle (it floated beside the
    block's sloping back, nothing under it): a bevelled box on the block's left, the handle seated on its top. Every old
    vertex, triangle and UV stays (anchors, hotspots, the handle and its hole read from frames 0 and 9: the new vertices
    are the same in every frame)."""
    p = mp.Polisher(path, rows=8, seed=14)
    texels = mp.edge_wear(p.m, p.ramps, mesh=p.mesh)
    bx0 = min(P[v][0] for v in bar)
    bz = sum(P[v][2] for v in bar) / len(bar)
    ymin = min(P[v][1] for v in block)
    # The nuts, on the block's right side over the bar's root.
    for dz in (-1.4, 1.4):
        p.stud((bx0 - 2.5, ymin - 20.0, bz + dz), (0.0, 1.0, 0.0), radius=0.9, height=0.55, material='blued')
    # The starter's housing: from inside the block up to the handle's underside, as long as most of the handle.
    hx, hy, hz = HANDLE_HALF
    top = handle[2] - hz - 0.05
    bottom = hole[2] - 6.0
    key = p.carrier_at((hole[0] + 3.0, 0.0, hole[2] - 5.0))
    p.box((handle[0], handle[1] - 0.6, 0.5 * (top + bottom)), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0),
          (hx - 0.9, hy + 0.9, 0.5 * (top - bottom)), 'steel', key, bevel=0.45, levels=(0.22, 0.3))
    tris, verts, rows = p.finish(path)
    return tris, verts, rows, texels


if __name__ == '__main__':
    main()
