# zflib.py -- helpers for the different-texel static z-fight pass (renders, contested pixels, components)
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mdlio, raster, compare  # noqa

AN = mdlio.anorms()
PAL = mdlio.palette().astype(np.float64)


def tri_area(m, pose=0):
    P = m.pos[pose][m.tris]
    return np.linalg.norm(mdlio.outward_normals(P), axis=-1) / 2


def components(m):
    """connected components of triangles through welded vertex positions"""
    W = mdlio.weld(m)
    par = list(range(m.nt))

    def f(x):
        while par[x] != x:
            par[x] = par[par[x]]
            x = par[x]
        return x
    first = {}
    for t, tri in enumerate(m.tris):
        for v in tri:
            w = int(W[v])
            if w in first:
                ra, rb = f(first[w]), f(t)
                if ra != rb:
                    par[ra] = rb
            else:
                first[w] = t
    return np.array([f(t) for t in range(m.nt)]), W


class Scene:
    def __init__(self, m, frame_with=None, size=384, views="cube"):
        self.m = m
        self.S = raster.Shaded(m, mdlio.tri_uv(m), AN)
        self.c, self.R = raster.framing(*(frame_with or [m]))
        self.V = compare.views(views)
        self.size = size

    def view(self, vi):
        return raster.View(self.V[vi], self.c, self.R, self.size)

    def render(self, pose, vi, keep=None):
        return self.S.render(pose, self.view(vi), keep)


def contested(sc, a, b, poses, tol):
    """per (pose, view): pixels where the pair's winner matters: (pixel idx, texel if a wins, texel if b wins)"""
    m = sc.m
    out = {}
    na = np.ones(m.nt, bool); na[a] = False
    nb = np.ones(m.nt, bool); nb[b] = False
    for f in poses:
        P = m.pos[f][m.tris]
        n = mdlio.outward_normals(P[a])
        n = n / max(np.linalg.norm(n), 1e-30)
        for vi in range(len(sc.V)):
            if float(n @ sc.V[vi]) >= -0.02:  # back facing (outward_normals points into the screen for front faces): culled
                continue
            tid, dep, tex, sh = sc.render(f, vi)
            sel = (tid == a) | (tid == b)
            if not sel.any():
                continue
            t2, d2, x2, s2 = sc.render(f, vi, na)
            t3, d3, x3, s3 = sc.render(f, vi, nb)
            ca = (tid == a) & (t2 == b) & (np.abs(d2 - dep) < tol)  # a shown, b right there
            cb = (tid == b) & (t3 == a) & (np.abs(d3 - dep) < tol)
            idx = np.nonzero(ca | cb)[0]
            if not len(idx):
                continue
            ta = np.where(ca[idx], tex[idx], x3[idx])  # texel when a wins
            tb = np.where(ca[idx], x2[idx], tex[idx])  # texel when b wins
            sha = np.where(ca[idx], sh[idx], s3[idx])
            shb = np.where(ca[idx], s2[idx], sh[idx])
            out[(f, vi)] = (idx, ta, tb, sha, shb)
    return out
