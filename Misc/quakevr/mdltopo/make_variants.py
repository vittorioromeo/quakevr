# make_variants.py -- test inputs for compare.py: python make_variants.py model.mdl outdir
#   <stem>_renum.mdl  same mesh, vertices permuted, triangles reversed, corners rotated  (must compare equal)
#   <stem>_flip.mdl   one flat quad (two coplanar triangles, continuous texture) with its diagonal flipped (equal)
#   <stem>_broken.mdl one vertex moved 3 grid steps in every pose (must differ)
#   <stem>_drop.mdl   one visible triangle removed (must differ)
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mdlio  # noqa: E402


def main():
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    stem = os.path.basename(src)[:-4]
    m = mdlio.read(src)
    # renumbered
    r = mdlio.read(src)
    perm = np.random.default_rng(1).permutation(m.nv)  # new index k holds old vertex perm[k]
    inv = np.argsort(perm)
    r.onseam, r.s, r.t = m.onseam[perm], m.s[perm], m.t[perm]
    r.bytes, r.normidx = m.bytes[:, perm], m.normidx[:, perm]
    r.tris = np.roll(inv[m.tris], 1, axis=1)[::-1].copy()
    r.facesfront = m.facesfront[::-1].copy()
    mdlio.write(r, os.path.join(out, stem + "_renum.mdl"))
    # broken: the vertex nearest the model's +x end, moved 3 steps
    b = mdlio.read(src)
    v = int(np.argmax(m.bytes[0, :, 0]))
    b.bytes = b.bytes.copy()
    b.bytes[:, v, 2] = np.clip(b.bytes[:, v, 2] + 3, 0, 255)
    mdlio.write(b, os.path.join(out, stem + "_broken.mdl"))
    # dropped triangle: the largest one
    d = mdlio.read(src)
    P = m.pos[0][m.tris]
    area = np.linalg.norm(mdlio.outward_normals(P), axis=1)
    k = int(np.argmax(area))
    d.tris = np.delete(m.tris, k, 0)
    d.facesfront = np.delete(m.facesfront, k)
    mdlio.write(d, os.path.join(out, stem + "_drop.mdl"))
    # flip: two triangles sharing an edge (same vertex indices: same texture side), coplanar in every pose, whose
    # union is a convex quad; replace the shared diagonal by the other one
    W = m.tris
    edges = {}
    for t in range(m.nt):
        for c in range(3):
            a, bb = W[t, c], W[t, (c + 1) % 3]
            edges.setdefault((min(a, bb), max(a, bb)), []).append((t, c))
    best = None
    for (a, bb), uses in edges.items():
        if len(uses) != 2:
            continue
        (t1, c1), (t2, c2) = uses
        if W[t2, c2] != W[t1, (c1 + 1) % 3] or m.facesfront[t1] != m.facesfront[t2]:
            continue
        o1 = W[t1, (c1 + 2) % 3]
        o2 = W[t2, (c2 + 2) % 3]
        quad = m.pos[:, [a, o1, bb, o2]]  # (P,4,3)
        n = np.cross(quad[:, 1] - quad[:, 0], quad[:, 3] - quad[:, 0])
        nn = np.linalg.norm(n, axis=1)
        if (nn < 1e-9).any():
            continue
        dist = np.abs(np.einsum("pk,pk->p", quad[:, 2] - quad[:, 0], n / nn[:, None]))
        if dist.max() > 1e-6:
            continue
        # convex: both diagonals split each other
        q = quad[0]
        d1 = np.cross(q[2] - q[0], q[1] - q[0]) @ np.cross(q[2] - q[0], q[3] - q[0])
        d2 = np.cross(q[3] - q[1], q[0] - q[1]) @ np.cross(q[3] - q[1], q[2] - q[1])
        if d1 >= 0 or d2 >= 0:
            continue
        # texture affine across the quad: o2's texel coords predicted by t1's mapping (exactly)
        uv = mdlio.tri_uv(m)
        cu = {W[t1, k]: uv[t1, k] for k in range(3)}
        cu2 = {W[t2, k]: uv[t2, k] for k in range(3)}
        if any(np.abs(cu[k] - cu2[k]).max() > 0 for k in (a, bb)):
            continue
        e1, e2, e3 = q[2] - q[0], q[1] - q[0], q[3] - q[0]
        M = np.stack([e1, e2], 1)
        coef = np.linalg.lstsq(M, e3, rcond=None)[0]
        pred = cu[a] + coef[0] * (cu[bb] - cu[a]) + coef[1] * (cu[o1] - cu[a])
        if np.abs(pred - cu2[o2]).max() > 1e-6:
            continue
        ar = nn[0]
        if best is None or ar > best[0]:
            best = (ar, t1, c1, t2, c2, a, bb, o1, o2)
    if best:
        _, t1, c1, t2, c2, a, bb, o1, o2 = best
        f = mdlio.read(src)
        f.tris = f.tris.copy()
        # t1 = (x, y, o1) with edge x->y = tri order; rebuild keeping winding: t1' = (o1, X, o2), t2' = (o2, Y, o1)
        x, y = W[t1, c1], W[t1, (c1 + 1) % 3]
        f.tris[t1] = (o1, x, o2)
        f.tris[t2] = (o2, y, o1)
        mdlio.write(f, os.path.join(out, stem + "_flip.mdl"))
        print("flip: triangles %d,%d (area %.2f)" % (t1, t2, best[0] / 2))
    else:
        print("flip: no flat convex quad found")
    print("variants written to", out, "(moved vertex %d, dropped triangle %d)" % (v, k))


main()
