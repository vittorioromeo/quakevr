#!/usr/bin/env python3
# refine_light.py -- the thunderbolt (v_light.mdl): its body's right side made the mirror image of its left, the
# stretched lower faces of both re-mapped, and the painted grooves of the side panels carved (the author's note,
# 1.0.1: "The left side of the model looks good, while the right side has some stretched textures and it doesn't look
# symmetrical ... The model also has a lot of grooves in the textures that should be inset in the geometry", as was
# done for the laser cannon: refine_laserg.py, whose machinery this uses).
#
# Usage: python Misc/quakevr/refine_light.py [model]   (default: quakevr/progs/v_light.mdl; rewrites it in place)
#        python Misc/quakevr/refine_light.py --report [model]   (the sides' stretch only, writes nothing)
# polish_weapons.py runs it (POST) on the v_light.mdl it writes, so rerunning that still gives the shipped file.
#
# The body's sides (the faces under the top bevels, |normal y| > 0.5, below z 8.65, not the front's underside):
# - the upper face of each side (the panels: lighter plates with bolts, dark slots between them) is mapped well on
#   +y (left; 4 texels a unit, anisotropy 1.05) but on -y one triangle spans the face corner to corner with its
#   texels folded onto a line (anisotropy 71: the streaks) and its triangulation leaves a crack filled by a sliver;
# - the lower face of each (sloping in to the bottom) is folded onto a few texel rows on both sides (anisotropy 17-20:
#   the long dark lines under the panels, painted by no one).
# The +y lower face is unwrapped flat (least squares conformal) into free skin and painted from the 3D surface in the
# body's dark browns (refine_laserg.paint_charts: grain along the gun, its convex edges lit and worn). The +y upper
# face's three dark slots (GROOVES, read off the paint: luminance runs) are carved GROOVE_DEPTH into it (Blender's
# exact boolean, headless: blender/carve_mesh.py, on a slab made of that face): their floors keep the slot's dark
# paint, their walls its darkest texel. The -y side is then that whole side mirrored across the body's middle plane,
# on the same texels (the skin's panel is shared, as it was), its outline the old -y vertices' places.
#
# What stays: the header (scale, origin: the weapon offsets, hotspots, sights and the magazine's seat are model space),
# every old vertex's index and every old triangle (vr_anchor.cpp's strip order: the button 57, muzzle 104 and magazine
# 655 anchors; polish_weapons.py checks them): the old side triangles stay in the file with their own vertices (used by
# nothing else) collapsed onto one point, as split_auto_pump does; the new sides are new triangles on new vertices,
# after the old ones (sharing no vertex with them, so no old strip runs into them). In every pose a new vertex is carried
# by the body's rigid motion (mdlpolish.Carrier: the kick), an outline vertex takes the old vertex's bytes.

import json
import math
import os
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import make_enemyguns as eg  # noqa: E402
import mdlpolish as mp  # noqa: E402
import refine_laserg as rl  # noqa: E402
from mdlgen import anorms  # noqa: E402

DEFAULT = os.path.join(HERE, "..", "..", "quakevr", "progs", "v_light.mdl")

BODY_AT = (11.1, 5.5, 5.8)  # a vertex of the body (its +y side's middle)
# The lower faces' paint: (texels per unit, ramp, base, spread, grain along x): the body's dark browns (as the panel's).
rl.PAINT["tbside"] = (4.0, rl.KEEL_RAMP, 0.30, 0.12, True)
# The +y upper face's dark slots (model units: x0, x1, z0, z1; the paint's runs of its darkest texels, a margin under
# the face's top edge, z 8.7, and over its fold, z 5.8-6.1): carved GROOVE_DEPTH in.
GROOVES = [(1.0, 2.9, 6.2, 8.45), (7.9, 9.9, 6.2, 8.45), (15.0, 17.0, 5.3, 8.45)]
GROOVE_DEPTH = 0.4
SLAB = 3.0  # the slab carved (the upper face pushed in this far): deeper than any cut
NOUV = -100.0  # the slab's back and sides (dropped after the carve)


def side_tris(m, P, find, root):
    """The body's two sides: {+1: [triangles], -1: [...]}, each closed over the triangles whose corners all lie on it
    (the -y side's sliver)."""
    out = {1: [], -1: []}
    for i, t in enumerate(m.tris):
        if find(t[1]) != root:
            continue
        n, a = rl.tri_n(P, t)
        c = P[list(t[1:])].mean(0)
        if abs(n[1]) > 0.5 and a > 5.0 and c[2] < 8.65 and n[2] > -0.7:
            out[1 if n[1] > 0 else -1].append(i)
    for k in (1, -1):
        vs = {v for i in out[k] for v in m.tris[i][1:]}
        out[k] += [i for i, t in enumerate(m.tris) if i not in out[k] and set(t[1:]) <= vs]
        out[k].sort()
    return out


def stretch(m, P, tris, corners):
    """Area-weighted (texels per unit, anisotropy) of these triangles."""
    a_sum = d_sum = an_sum = 0.0
    for i in tris:
        A = P[list(m.tris[i][1:])]
        U = np.array([(s, t) for _, s, t in corners[i]], float)
        e1, e2 = A[1] - A[0], A[2] - A[0]
        n = np.cross(e1, e2)
        a = np.linalg.norm(n) / 2
        if a < 1e-6:
            continue
        x = e1 / np.linalg.norm(e1)
        y = np.cross(n / (2 * a), x)
        f1, f2 = U[1] - U[0], U[2] - U[0]
        if abs(f1[0] * f2[1] - f1[1] * f2[0]) < 1e-9:
            dens, an = 0.0, 50.0
        else:
            J = np.array([f1, f2]).T @ np.linalg.inv(np.array([[e1 @ x, e2 @ x], [e1 @ y, e2 @ y]]))
            s = np.linalg.svd(J, compute_uv=False)
            dens, an = math.sqrt(abs(np.linalg.det(J))), min(50.0, s[0] / s[1])
        a_sum, d_sum, an_sum = a_sum + a, d_sum + dens * a, an_sum + an * a
    return d_sum / a_sum, an_sum / a_sum


def slab(P, m, tris, corners):
    """The faces as a closed slab (their copy SLAB in along their mean normal n, the outline's walls along n): welded
    vertices (the front's first k), counter-clockwise faces, corner skin coordinates (NOUV off the face), k, n."""
    weld, W, wi = {}, [], {}
    for i in tris:
        for v in m.tris[i][1:]:
            k = tuple(np.round(P[v], 4))
            if k not in weld:
                weld[k] = len(W)
                W.append(P[v])
            wi[v] = weld[k]
    faces = [(wi[a], wi[c], wi[b]) for _, a, b, c in (m.tris[i] for i in tris)]  # clockwise -> counter-clockwise
    uvs = []
    for i in tris:
        u = [(float(s), float(t)) for _, s, t in corners[i]]
        uvs.append([u[0], u[2], u[1]])
    n = sum(rl.tri_n(P, m.tris[i])[0] * rl.tri_n(P, m.tris[i])[1] for i in tris)
    n = n / np.linalg.norm(n)
    k = len(W)
    W = np.array(W + [w - n * SLAB for w in W])
    no = [(NOUV, NOUV)] * 3
    edges = {}
    for f in faces:
        for j in range(3):
            edges[(f[j], f[(j + 1) % 3])] = edges.get((f[j], f[(j + 1) % 3]), 0) + 1
    outline = [e for e in edges if (e[1], e[0]) not in edges]
    for a, b, c in list(faces):
        faces.append((a + k, c + k, b + k))
        uvs.append(no)
    for a, b in outline:
        faces += [(b, a, a + k), (b, a + k, b + k)]
        uvs += [no, no]
    return W, faces, uvs, k, n


def carve_slab(W, faces, uvs, k, n, m, skin, c):
    """GROOVES out of the slab (blender/carve_mesh.py). Returns [(corner positions, corner skin coordinates, cut)] of
    the front: the face with its recesses. The recesses' floor (each cut's `inner`) is the slab with its front moved
    GROOVE_DEPTH in along n: its walls in the slab's own (not moved in as eg.inner_mesh would: a cut near the outline
    would go deeper there), so a cut is the box's part in front of that floor only."""
    lum = mp.palette() @ np.array((0.3, 0.59, 0.11))
    fq = [f for f, uv in zip(faces, uvs) if uv[0][0] > NOUV / 2]
    fu = [uv for uv in uvs if uv[0][0] > NOUV / 2]
    job = {"verts": W.tolist(), "faces": [list(f) for f in faces], "uvs": uvs, "cuts": [], "dissolve": True}
    inner = W.copy()
    inner[:k] -= n * GROOVE_DEPTH
    inner[k:] += n * 0.05
    for x0, x1, z0, z1 in GROOVES:
        x0, x1 = (rl.snap_axis(x, m.origin[0], m.scale[0]) for x in (x0, x1))
        z0, z1 = (rl.snap_axis(z, m.origin[2], m.scale[2]) for z in (z0, z1))
        ys = []
        for x in (x0, x1):
            for z in (z0, z1):
                hit = [mp.ray_tri(np.array((x, c + 20.0, z)), np.array((0.0, -1.0, 0.0)), *(W[list(f)]))
                       for f in fq]
                ys += [c + 20.0 - h for h in hit if h is not None]
        assert len(ys) >= 4, "a groove off the face: %s" % ((x0, x1, z0, z1),)
        xm, zm = 0.5 * (x0 + x1), 0.5 * (z0 + z1)
        u, v = (int(np.floor(q)) for q in eg.ray_uv(W, fq, fu, (xm, c + 20.0, zm), (0.0, -1.0, 0.0)))
        u, v = min(((a, b) for a in range(u - 2, u + 3) for b in range(v - 2, v + 3)),
                   key=lambda q: (lum[skin[q[1] * m.sw + q[0]]], abs(q[0] - u) + abs(q[1] - v)))
        job["cuts"].append({"box": [x0, x1, min(ys) - GROOVE_DEPTH - 0.15, c + 20.0, z0, z1],
                            "inner": inner.tolist(), "wall_uv": [u + 0.5, v + 0.5]})
    script = os.path.join(HERE, "blender", "carve_mesh.py")
    with tempfile.TemporaryDirectory() as tmp:
        src, dst = os.path.join(tmp, "in.json"), os.path.join(tmp, "out.json")
        with open(src, "w") as f:
            json.dump(job, f)
        r = subprocess.run([eg.BLENDER, "-b", "--factory-startup", "--python-exit-code", "1", "-P", script, "--", src,
                            dst], capture_output=True, text=True)
        assert r.returncode == 0 and os.path.exists(dst), "Blender failed:\n%s" % (r.stdout + r.stderr)[-3000:]
        with open(dst) as f:
            res = json.load(f)
    V = np.array(res["verts"], np.float64)
    eg.split_slivers(V, res["faces"], res["uvs"], res["cut"])
    out = []
    for f, uv, is_cut in zip(res["faces"], res["uvs"], res["cut"]):
        if min(q for corner in uv for q in corner) < NOUV / 2:
            continue  # the slab's back and sides
        f, uv = (f[0], f[2], f[1]), (uv[0], uv[2], uv[1])  # back to clockwise
        out.append(([V[k] for k in f], [(int(math.floor(a + 1e-3)), int(math.floor(b + 1e-3))) for a, b in uv],
                    bool(is_cut)))
    return out


def fix(path, report_only=False):
    m = mp.Model(path)
    P = m.positions(0)
    find = rl.welded_components(m, P)
    root = find(min(range(len(P)), key=lambda v: np.linalg.norm(P[v] - np.array(BODY_AT))))
    sides = side_tris(m, P, find, root)
    corners = [[(v, m.st[v][1], m.st[v][2]) for v in t[1:]] for t in m.tris]
    upper = [i for i in sides[1] if rl.tri_n(P, m.tris[i])[0][2] > 0.0]
    lower = [i for i in sides[1] if rl.tri_n(P, m.tris[i])[0][2] <= 0.0]
    report = {"+y upper": upper, "+y lower": lower,
              "-y upper": [i for i in sides[-1] if rl.tri_n(P, m.tris[i])[0][2] > 0.0],
              "-y lower": [i for i in sides[-1] if rl.tri_n(P, m.tris[i])[0][2] <= 0.0]}
    print("%s, before:" % os.path.basename(path))
    for k, tris in report.items():
        print("  %-9s %d triangles: %.2f texels/unit, anisotropy %.2f" % ((k, len(tris)) + stretch(m, P, tris, corners)))
    if report_only:
        return
    assert (len(upper), len(lower), len(sides[-1])) == (3, 2, 5), "the body's sides changed: %s" % sides
    dead = sorted(set(sides[1]) | set(sides[-1]))
    dead_v = sorted({v for i in dead for v in m.tris[i][1:]})
    for i, t in enumerate(m.tris):
        assert i in dead or not set(t[1:]) & set(dead_v), "triangle %d shares a vertex with the sides" % i
    assert all(m.st[v][0] == 0 for v in dead_v) and all(m.tris[i][0] == 1 for i in dead)
    assert len(m.skins) == 1 and m.skins[0][0] == 0, "one skin expected"
    ys_p = [P[v][1] for i in sides[1] for v in m.tris[i][1:]]
    ys_m = [P[v][1] for i in sides[-1] for v in m.tris[i][1:]]
    c = 0.5 * (max(ys_p) + min(ys_m))  # the body's middle plane (its widest points: +-6.6/6.9 at the bottom front)

    # 1. The +y lower face re-mapped and painted.
    jobs = [("tbside", chart) for chart in rl.make_charts(P, lower, m)]
    H, placed = rl.layout(m, P, corners, jobs)
    if H > m.sh:
        m.grow_skin(H - m.sh)
    key = lambda v: tuple(np.round(P[v], 3))
    for part, chart, st in placed:
        for i in chart:
            corners[i] = [(v, st[key(v)][0], st[key(v)][1]) for v in m.tris[i][1:]]
    skin = m.skin()
    rl.paint_charts(skin, m.sw, m.sh, m, P, placed)

    # 2. The +y upper face carved.
    W, faces, uvs, k, n = slab(P, m, upper, corners)
    carved = carve_slab(W, faces, uvs, k, n, m, m.skin(), c)
    patch = carved + [([P[v] for v in m.tris[i][1:]], [(s, t) for _, s, t in corners[i]], False) for i in lower]
    old_keys = {tuple(np.round(P[v], 3)) for i in sides[1] for v in m.tris[i][1:]}
    outline = {}
    for pos, _, _ in patch:
        for j in range(3):
            e = (tuple(np.round(pos[j], 3)), tuple(np.round(pos[(j + 1) % 3], 3)))
            outline[e] = outline.get(e, 0) + 1
    loose = {a for a, b in outline if (b, a) not in outline} - old_keys
    assert not loose, "the carved side's outline has new corners (T-junctions): %s" % sorted(loose)

    # 3. The -y side: the +y side mirrored, its outline on the old -y vertices.
    minus_v = sorted({v for i in sides[-1] for v in m.tris[i][1:]})
    plus_v = sorted({v for i in sides[1] for v in m.tris[i][1:]})

    def mirrored(p):
        q = np.array((p[0], 2 * c - p[1], p[2]))
        w = min(minus_v, key=lambda v: np.linalg.norm(P[v] - q))
        return (P[w], w) if np.linalg.norm(P[w] - q) < 0.2 else (q, None)

    def on_old(p):
        w = min(plus_v, key=lambda v: np.linalg.norm(P[v] - p))
        return w if np.linalg.norm(P[w] - p) < 1e-3 else None

    tris = []  # (positions, skin coordinates, cut, old vertex per corner or None)
    for pos, st, cut in patch:
        tris.append((pos, st, cut, [on_old(p) for p in pos]))
        mp_ = [mirrored(p) for p in pos]
        tris.append(([mp_[0][0], mp_[2][0], mp_[1][0]], [st[0], st[2], st[1]], cut, [mp_[0][1], mp_[2][1], mp_[1][1]]))
    hit = {tuple(np.round(P[w], 3)) for _, _, _, ws in tris for w in ws if w is not None}
    miss = [v for v in minus_v if tuple(np.round(P[v], 3)) not in hit]
    assert not miss, "old -y vertices the mirrored side misses: %s" % miss

    # 4. Normals: a recess's faces flat; the rest smoothed over the side's faces within 30 degrees.
    fn = []
    for pos, _, _, _ in tris:
        n = np.cross(pos[2] - pos[0], pos[1] - pos[0])
        a = np.linalg.norm(n)
        fn.append((n / a if a > 1e-9 else n, a))
    acc = {}
    for (pos, _, cut, _), (n, a) in zip(tris, fn):
        if not cut:
            for p in pos:
                acc.setdefault(tuple(np.round(p, 3)), []).append((n, a))

    def vnormal(p, n, cut):
        if cut:
            return n
        s = sum(a * q for q, a in acc[tuple(np.round(p, 3))] if q @ n > math.cos(math.radians(30.0)))
        return s / np.linalg.norm(s)

    # 5. The old sides' vertices collapsed (their triangles vanish; indices, and so the strip order, unchanged): onto
    # the one that is an anchor (the two-handed grip's, 230: the -y side's bottom front corner, where the mirrored side
    # has a corner too), which stays where it is.
    import polish_weapons  # (here: it imports this module)
    from improve_weapons import strip_order
    order = strip_order(m.tris)
    kept = sorted({order[a] for a in polish_weapons.slot_anchors().get(os.path.basename(path), ())} & set(dead_v))
    assert len(kept) <= 1, "anchors on more than one of the sides' vertices: %s" % kept
    first = kept[0] if kept else dead_v[0]
    poses = rl.pose_arrays(m)
    blocks = m.pose_blocks()
    for p, blk in enumerate(blocks):
        vb = blk[2] if blk[0] == "simple" else blk[1]
        for v in dead_v:
            if v != first:
                vb[4 * v:4 * v + 4] = bytes(poses[p][first])
    carrier = mp.Carrier(m, [v for i, t in enumerate(m.tris) if find(t[1]) == root and i not in dead for v in t[1:]])
    table = np.array(anorms())
    vid = {}

    def vertex(p, s, t, n, old):
        k = (tuple(np.round((np.asarray(p) - m.origin) / m.scale).astype(int)), s, t, int(np.argmax(table @ n)))
        if k not in vid:
            if old is not None:
                per_pose = [poses[q][old, :3].astype(np.float64) * m.scale + m.origin for q in range(len(poses))]
            else:
                per_pose = [carrier.place(p, q) for q in range(len(poses))]
            vid[k] = m.add_vertex(s, t, per_pose, [carrier.turn(n, q) for q in range(len(poses))])
        return vid[k]

    new = []
    for (pos, st, cut, olds), (n, _) in zip(tris, fn):
        new.append((1,) + tuple(vertex(p, s, t, vnormal(p, n, cut), o) for p, (s, t), o in zip(pos, st, olds)))
    m.tris = list(m.tris) + new
    after = [[(v, m.st[v][1], m.st[v][2]) for v in t[1:]] for t in m.tris]
    m_pos = np.array([poses[0][v, :3] * m.scale + m.origin for v in range(m.old_nv)] +
                     [np.asarray(q[0]) for q in m.new_pos])
    plus_new = [len(m.tris) - len(new) + k for k in range(0, len(new), 2)]
    minus_new = [len(m.tris) - len(new) + k for k in range(1, len(new), 2)]
    print("%s, after: %d -> %d triangles (%d collapsed), %d -> %d vertices, skin %dx%d" % (os.path.basename(path),
        len(m.tris) - len(new), len(m.tris), len(dead), m.old_nv, len(m.st), m.sw, m.sh))
    pm = mp.Model.__new__(mp.Model)  # (stretch() reads tris and positions only)
    pm.tris = m.tris
    for k, tris_k in (("+y side", plus_new), ("-y side", minus_new)):
        print("  %-9s %d triangles: %.2f texels/unit, anisotropy %.2f" % ((k, len(tris_k)) +
                                                                          stretch(pm, m_pos, tris_k, after)))
    m.write(path)


def main():
    args = [a for a in sys.argv[1:] if a != "--report"]
    fix(args[0] if args else DEFAULT, "--report" in sys.argv)


if __name__ == "__main__":
    main()
