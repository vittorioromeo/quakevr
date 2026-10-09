#!/usr/bin/env python3
# make_ssg_open.py -- the super shotgun broken open (immersive reloading, phase 2b: docs/vr-port/RELOAD.md; QC
# vr_reload.qc; the engine's vr_view.cpp ssgParts): v_shot2.mdl cut in two at its hinge, written as
#
#   quakevr/progs/vr_ssg_frame_on_v_shot2.mdl    the frame: the grip, the trigger guard, the receiver; its front (the
#                                                standing breech) closed by a steel plate with the two firing pins
#   quakevr/progs/vr_ssg_barrels_on_v_shot2.mdl  the barrels with the fore-end; their back (the breech face) closed by a
#                                                plate with the two chambers' mouths: skin 0 both empty, 1 the left one
#                                                (model +y) loaded, 2 both loaded (the shells' brass heads and primers)
#
# Both keep the gun's vertices, poses, scale and origin (so they lie exactly on it: the engine draws them with the
# gun's place, the barrels turned about the hinge), its skin with rows added under it for the plates. The gun closed
# is drawn as v_shot2.mdl itself; only an open (or opening, closing) gun is drawn in its two parts.
#
# The cut: a triangle whose middle is in front of x = CUT is the barrels', and so is the rear sight's ring; the two tapers joining the barrels to the
# receiver above the fore-end (triangles across the cut higher than z 5) are dropped, the plates closing the holes they
# leave. Run after v_shot2.mdl changes (polish_weapons.py), then check vr_view.cpp's ssg* constants (the hinge, the
# chambers' middle) still match, and bake their normal maps: python Misc/quakevr/make_ssg_open.py, then
# python Misc/quakevr/bake_normals.py vr_ssg_frame_on_v_shot2.mdl vr_ssg_barrels_on_v_shot2.mdl

import collections
import math
import os
import random
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import genguard  # noqa: E402
import mdlpolish as mp  # noqa: E402

PROGS = os.path.join(HERE, "..", "..", "quakevr", "progs")
SRC = os.path.join(PROGS, "v_shot2.mdl")
CUT = 12.5           # model x of the cut (the receiver's front)
TAPER_Z = 5.0        # triangles across the cut above this are the tapers (dropped)
RING_BOX = (12.0, 12.7, 0.6, 6.95)  # x from, x to, |y| under, z over: the rear sight's ring (its whole piece moves)
CHAMBERS = ((1.25, 7.0), (-1.25, 7.0))  # (y, z) of the chambers' (and the firing pins') middles
CHAMBER_R = 0.6      # the chamber mouth's radius (its chamfer's outer edge)
BREECH_X = 11.72     # the standing breech plate, just in front of the receiver's open front
BREECH = [(-1.72, 7.5), (1.66, 7.5), (2.46, 6.39), (1.66, 5.36), (-1.72, 5.36), (-2.52, 6.39)]  # (y, z), its outline
PX = 12.0            # skin pixels per model unit on the plates
ROWS = 36            # skin rows added for them

# Quake palette indices (gfx/palette.lmp): the gun's blued steel is the near-blacks 1..5 and the blue-greys 32..36;
# brass from the browns 28..31 and the olive 199 (make_shell.py's); the primers' greys.
STEEL = (2, 3, 33, 2, 34, 3)
EDGE = (35, 36, 5)
BORE = (0, 0, 1)
CHAMFER = (36, 5, 35)
BRASS = (30, 31, 199, 28)


def classify(m):
    P = m.positions(0)
    T = np.array([t[1:] for t in m.tris])
    X = P[T][:, :, 0]
    C = P[T].mean(1)
    taper = (X.min(1) < CUT) & (X.max(1) > CUT) & (C[:, 2] >= TAPER_Z)
    # The rear sight's ring (a piece of its own on the barrels' rib, over the cut: x 12.14-12.6) goes with the barrels
    # (the author: it stayed on the frame and floated there as the gun broke open).
    mesh = mp.Mesh0(m)
    ring = np.zeros(len(T), bool)
    for ti in np.nonzero((C[:, 0] > RING_BOX[0]) & (C[:, 0] < RING_BOX[1]) & (np.abs(C[:, 1]) < RING_BOX[2]) &
                         (C[:, 2] > RING_BOX[3]))[0]:
        piece = mp.piece_of_tri(mesh, ti)
        if P[piece][:, 0].min() < RING_BOX[0] or P[piece][:, 0].max() > RING_BOX[1]:
            continue  # (a taper, part of the receiver's piece: not the ring)
        vs = set(piece.tolist())
        ring |= np.array([t[0] in vs for t in T])
    barrels = ((C[:, 0] > CUT) & ~taper) | ring
    frame = ~barrels & ~taper
    return P, T, frame, barrels


def welded(P):
    key, weld = {}, np.zeros(len(P), int)
    for i, p in enumerate(P):
        weld[i] = key.setdefault(tuple(np.round(p, 3)), len(key))
    return weld


def rear_loop(P, T, sel):
    """The barrels' back: the loop of their open edges behind x 13.2 and above z 4.9 (where the tapers were)."""
    weld = welded(P)
    rep = {}
    for i in range(len(P)):
        rep.setdefault(weld[i], i)
    cnt = collections.Counter()
    for t in T[sel]:
        a, b, c = weld[t]
        for e in ((a, b), (b, c), (c, a)):
            cnt[tuple(sorted(e))] += 1
    inside = lambda w: P[rep[w]][0] < 13.2 and P[rep[w]][2] > 4.9  # noqa: E731
    adj = collections.defaultdict(list)
    for (a, b), n in cnt.items():
        if n == 1 and inside(a) and inside(b):
            adj[a].append(b)
            adj[b].append(a)
    best = []
    seen = set()
    for s in adj:
        if s in seen:
            continue
        loop, prev, cur = [s], None, s
        seen.add(s)
        while True:
            nxt = [n for n in adj[cur] if n != prev and n not in seen]
            if not nxt:
                break
            prev, cur = cur, nxt[0]
            loop.append(cur)
            seen.add(cur)
        if len(loop) > len(best):
            best = loop
    return [P[rep[w]] for w in best]


def plate(m, outline, outward, carrier, s0, t0, ymax, zmax):
    """A fan of triangles over `outline` (3D points, a loop) round its middle, facing `outward`, carried per pose by
    `carrier`; skin coordinates from (y, z): s = s0 + (ymax - y) * PX, t = t0 + (zmax - z) * PX."""
    n = m.num_poses()
    pts = [np.asarray(p, np.float64) for p in outline]
    mid = np.mean(pts, axis=0)
    out = np.asarray(outward, np.float64)

    def vert(p):
        s = s0 + (ymax - p[1]) * PX
        t = t0 + (zmax - p[2]) * PX
        return m.add_vertex(round(s), round(t), [carrier.place(p, k) for k in range(n)],
                            [carrier.turn(out, k) for k in range(n)])

    c = vert(mid)
    ids = [vert(p) for p in pts]
    tris = []
    for i in range(len(ids)):
        a, b = ids[i], ids[(i + 1) % len(ids)]
        pa, pb = pts[i], pts[(i + 1) % len(pts)]
        if mp.tri_normal(mid, pa, pb) @ out < 0:
            a, b = b, a
        tris.append((1, c, a, b))
    return tris


def paint_plate(img, sw, s0, t0, w, h, ymax, zmax, inside, pixel):
    for t in range(t0, t0 + h):
        for s in range(s0, s0 + w):
            y = ymax - (s - s0 + 0.5) / PX
            z = zmax - (t - t0 + 0.5) / PX
            img[t * sw + s] = pixel(y, z, inside(y, z))


def edge_distance(poly, y, z):
    d = 1e9
    for i in range(len(poly)):
        (ay, az), (by, bz) = poly[i], poly[(i + 1) % len(poly)]
        ey, ez = by - ay, bz - az
        k = max(0.0, min(1.0, ((y - ay) * ey + (z - az) * ez) / (ey * ey + ez * ez)))
        d = min(d, math.hypot(y - ay - k * ey, z - az - k * ez))
    return d


def inside_poly(poly, y, z):
    c = False
    for i in range(len(poly)):
        (ay, az), (by, bz) = poly[i], poly[(i + 1) % len(poly)]
        if (az > z) != (bz > z) and y < ay + (z - az) * (by - ay) / (bz - az):
            c = not c
    return c


def steel(rng, poly, y, z):
    if edge_distance(poly, y, z) < 0.14:
        return rng.choice(EDGE)
    return rng.choice(STEEL)


def chamber_pixel(rng, poly, y, z, loaded):
    """The breech face: blued steel, the chambers' mouths (a bright chamfer round a black bore), or a loaded one's
    brass head (its rim a shade darker) with its primer."""
    for i, (cy, cz) in enumerate(CHAMBERS):
        r = math.hypot(y - cy, z - cz)
        if r < CHAMBER_R:
            if loaded[i]:
                if r < 0.17:
                    return 10 if rng.random() < 0.6 else 9
                if r < 0.23:
                    return 4
                if r > CHAMBER_R - 0.09:
                    return 28 if rng.random() < 0.7 else 30
                return rng.choice(BRASS)
            if r > CHAMBER_R - 0.12:
                return rng.choice(CHAMFER)
            return rng.choice(BORE)
    return steel(rng, poly, y, z)


def breech_pixel(rng, y, z):
    """The standing breech: blued steel with the two firing pins (a dark hole in a bright bushing) where the chambers
    close on it, and the extractor's slot below between them."""
    for cy, cz in CHAMBERS:
        r = math.hypot(y - cy, z - cz)
        if r < 0.11:
            return 1
        if r < 0.3:
            return rng.choice(CHAMFER)
    if abs(y) < 0.45 and 6.3 < z < 6.48:
        return 1
    return steel(rng, BREECH, y, z)


def build():
    out_b = os.path.join(PROGS, "vr_ssg_barrels_on_v_shot2.mdl")
    out_f = os.path.join(PROGS, "vr_ssg_frame_on_v_shot2.mdl")
    guard = genguard.Guard("make_ssg_open.py", [out_b, out_f])
    base = mp.Model(SRC)
    P, T, frame_sel, barrel_sel = classify(base)
    loop = rear_loop(P, T, barrel_sel)
    cap_yz = [(p[1], p[2]) for p in loop]
    print("v_shot2.mdl: %d triangles: the frame %d, the barrels %d, %d tapers dropped; the barrels' back %d points" %
          (len(T), frame_sel.sum(), barrel_sel.sum(), len(T) - frame_sel.sum() - barrel_sel.sum(), len(loop)))

    # The plates' places in the added rows: the breech face at the left, the standing breech right of it.
    cy = [p[0] for p in cap_yz]
    cz = [p[1] for p in cap_yz]
    cap_ymax, cap_zmax = max(cy) + 0.1, max(cz) + 0.1
    cap_w = int(math.ceil((cap_ymax - min(cy) + 0.1) * PX)) + 1
    cap_h = int(math.ceil((cap_zmax - min(cz) + 0.1) * PX)) + 1
    by = [p[0] for p in BREECH]
    bz = [p[1] for p in BREECH]
    br_ymax, br_zmax = max(by) + 0.1, max(bz) + 0.1
    br_w = int(math.ceil((br_ymax - min(by) + 0.1) * PX)) + 1
    br_h = int(math.ceil((br_zmax - min(bz) + 0.1) * PX)) + 1
    assert max(cap_h, br_h) <= ROWS and cap_w + br_w + 4 <= base.sw, (cap_w, cap_h, br_w, br_h)

    # The barrels.
    bm = mp.Model(SRC)
    t0 = bm.grow_skin(ROWS)
    barrel_verts = np.unique(T[barrel_sel].ravel())
    carrier = mp.Carrier(bm, barrel_verts)
    mid = np.mean(loop, axis=0)
    cap = plate(bm, loop, (-1.0, 0.0, 0.0), carrier, 0, t0, cap_ymax, cap_zmax)
    bm.tris = [base.tris[i] for i in np.nonzero(barrel_sel)[0]] + cap
    poly = cap_yz
    skins = []
    for k, loaded in enumerate(((False, False), (True, False), (True, True))):
        rng = random.Random(4100)  # (the same steel on every skin)
        img = bytearray(bm.skin(0))
        paint_plate(img, bm.sw, 0, t0, cap_w, cap_h, cap_ymax, cap_zmax, lambda y, z: inside_poly(poly, y, z),
                    lambda y, z, inside: chamber_pixel(rng, poly, y, z, loaded) if inside or edge_distance(poly, y, z) < 0.3
                    else rng.choice(STEEL))
        skins.append([0, b"", [img]])
    bm.skins = skins
    bm.h[12] = len(skins)  # (the header's skin count)
    bm.write(out_b)

    # The frame.
    fm = mp.Model(SRC)
    t0f = fm.grow_skin(ROWS)
    front = [i for i in np.unique(T[frame_sel].ravel()) if P[i][0] > 9.0]
    fcarrier = mp.Carrier(fm, front)
    outline = [(BREECH_X, y, z) for y, z in BREECH]
    s0 = cap_w + 4
    breech = plate(fm, outline, (1.0, 0.0, 0.0), fcarrier, s0, t0f, br_ymax, br_zmax)
    fm.tris = [base.tris[i] for i in np.nonzero(frame_sel)[0]] + breech
    rng = random.Random(4200)
    img = fm.skin(0)
    paint_plate(img, fm.sw, s0, t0f, br_w, br_h, br_ymax, br_zmax, lambda y, z: True,
                lambda y, z, inside: breech_pixel(rng, y, z))
    fm.write(out_f)
    print("the breech face's middle (the load point): %.2f %.2f %.2f" % tuple(mid))
    for p in (out_f, out_b):
        print("%s: %d bytes" % (os.path.normpath(p), os.path.getsize(p)))
    guard.finish()


if __name__ == "__main__":
    build()
