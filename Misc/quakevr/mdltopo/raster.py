# raster.py -- a small exact software rasteriser for alias models (numpy, orthographic, nearest texels, depth buffer,
# back faces culled as the engine does). Pixel centres are sampled exactly (edge functions), so two meshes that cover the
# same surface with the same texture mapping give the same pixels whatever their triangulation (up to float rounding at
# texel boundaries). Used by the topology audit (hidden faces, highlight shots) and by compare.py (the fix-phase gate).
import struct
import zlib

import numpy as np

LIGHT = np.array([0.45, 0.3, 0.84])
LIGHT = LIGHT / np.linalg.norm(LIGHT)


def fib_dirs(n):
    i = np.arange(n) + 0.5
    phi = np.arccos(1 - 2 * i / n)
    th = np.pi * (1 + 5 ** 0.5) * i
    return np.stack([np.cos(th) * np.sin(phi), np.sin(th) * np.sin(phi), np.cos(phi)], 1)


def cube_dirs():
    """6 axes, 12 edges, 8 corners (26)."""
    d = [(x, y, z) for x in (-1, 0, 1) for y in (-1, 0, 1) for z in (-1, 0, 1) if (x, y, z) != (0, 0, 0)]
    d = np.array(d, dtype=np.float64)
    return d / np.linalg.norm(d, axis=1, keepdims=True)


def basis(d):
    up = np.array([0.0, 0.0, 1.0]) if abs(d[2]) < 0.95 else np.array([1.0, 0.0, 0.0])
    r = np.cross(up, d)
    r /= np.linalg.norm(r)
    u = np.cross(d, r)
    return r, u


class View:
    """Orthographic camera looking along -d at a sphere (centre, radius) filling a size x size image."""

    def __init__(self, d, centre, radius, size):
        self.d = np.asarray(d, dtype=np.float64)
        self.r, self.u = basis(self.d)
        self.c = np.asarray(centre, dtype=np.float64)
        self.R = float(radius)
        self.S = int(size)

    def project(self, p):
        q = p - self.c
        k = self.S / (2 * self.R)
        x = (q @ self.r + self.R) * k
        y = (self.R - q @ self.u) * k
        z = -(q @ self.d)
        return x, y, z


EDGE_EPS = 1e-9


def raster(x, y, z, size, keep=None, depth_quantum=0.0, cull=True, tiekeys=()):
    """x, y, z: (n, 3) per-corner screen coords. Returns (tri id buffer (-1 empty), depth, w1, w2) as flat S*S arrays.
    Front faces only (screen-space signed area < 0, see project's axes)."""
    S = size
    n = len(x)
    e1x, e1y = x[:, 1] - x[:, 0], y[:, 1] - y[:, 0]
    e2x, e2y = x[:, 2] - x[:, 0], y[:, 2] - y[:, 0]
    den = e1x * e2y - e1y * e2x
    ok = (den < -1e-12) if cull else (np.abs(den) > 1e-12)
    if keep is not None:
        ok &= keep
    i0 = np.ceil(x.min(1) - 0.5).astype(np.int64)
    i1 = np.floor(x.max(1) - 0.5).astype(np.int64)
    j0 = np.ceil(y.min(1) - 0.5).astype(np.int64)
    j1 = np.floor(y.max(1) - 0.5).astype(np.int64)
    i0 = np.maximum(i0, 0)
    j0 = np.maximum(j0, 0)
    i1 = np.minimum(i1, S - 1)
    j1 = np.minimum(j1, S - 1)
    w = i1 - i0 + 1
    h = j1 - j0 + 1
    ok &= (w > 0) & (h > 0)
    idx = np.nonzero(ok)[0]
    ext = np.maximum(w[idx], h[idx])
    bucket = np.ceil(np.log2(np.maximum(ext, 1))).astype(np.int64)
    pix_l, dep_l, tri_l, a_l, b_l = [], [], [], [], []
    for bk in np.unique(bucket):
        B = 1 << int(bk)
        sel = idx[bucket == bk]
        per = max(1, (1 << 21) // (B * B))
        o = np.arange(B)
        for c0 in range(0, len(sel), per):
            t = sel[c0: c0 + per]
            X = i0[t][:, None, None] + o[None, None, :]
            Y = j0[t][:, None, None] + o[None, :, None]
            px = X + 0.5
            py = Y + 0.5
            x0, y0 = x[t, 0][:, None, None], y[t, 0][:, None, None]
            x1, y1 = x[t, 1][:, None, None], y[t, 1][:, None, None]
            x2, y2 = x[t, 2][:, None, None], y[t, 2][:, None, None]
            dd = den[t][:, None, None]
            w1 = ((px - x0) * (y2 - y0) - (py - y0) * (x2 - x0)) / dd
            w2 = ((x1 - x0) * (py - y0) - (y1 - y0) * (px - x0)) / dd
            w0 = 1.0 - w1 - w2
            # top-left fill rule: a pixel centre on an edge belongs to one side only (a shared edge is walked in
            # opposite directions by its two triangles), so the result does not depend on triangle order or numbering
            ins = (X <= i1[t][:, None, None]) & (Y <= j1[t][:, None, None])
            for wk, (ca, cb) in ((w0, (1, 2)), (w1, (2, 0)), (w2, (0, 1))):
                dx = (x[t, cb] - x[t, ca])[:, None, None]
                dy = (y[t, cb] - y[t, ca])[:, None, None]
                tl = (dy > 0) | ((dy == 0) & (dx < 0))
                ins &= (wk > EDGE_EPS) | ((wk >= -EDGE_EPS) & tl)
            ti, yy, xx = np.nonzero(ins)
            if not len(ti):
                continue
            W1 = w1[ti, yy, xx]
            W2 = w2[ti, yy, xx]
            tt = t[ti]
            Z = (1 - W1 - W2) * z[tt, 0] + W1 * z[tt, 1] + W2 * z[tt, 2]
            pix_l.append(Y[ti, yy, 0] * S + X[ti, 0, xx])
            dep_l.append(Z)
            tri_l.append(tt)
            a_l.append(W1)
            b_l.append(W2)
    tid = np.full(S * S, -1, np.int64)
    dep = np.full(S * S, np.inf)
    W1b = np.zeros(S * S)
    W2b = np.zeros(S * S)
    if pix_l:
        pix = np.concatenate(pix_l)
        dep_a = np.concatenate(dep_l)
        tri = np.concatenate(tri_l)
        # equal depths (coplanar overlaps: z-fighting) resolve by the triangle's place (its centroid), not its number
        cx = x.mean(1)[tri]
        cy = y.mean(1)[tri]
        cz = z.mean(1)[tri]
        dq = np.round(dep_a / depth_quantum) if depth_quantum else dep_a
        extra = tuple(k[tri] for k in tiekeys)  # e.g. texture centroid: coincident twins differ only there
        order = np.lexsort((tri,) + extra + (cz, cy, cx, dq, pix))
        ps = pix[order]
        first = np.r_[True, ps[1:] != ps[:-1]]
        o = order[first]
        tid[pix[o]] = tri[o]
        dep[pix[o]] = dep_a[o]
        W1b[pix[o]] = np.concatenate(a_l)[o]
        W2b[pix[o]] = np.concatenate(b_l)[o]
    return tid, dep, W1b, W2b


class Shaded:
    """Per-pixel texel index and Gouraud shade of a model pose seen from a view."""

    def __init__(self, m, uv, anorm_table):
        self.m = m
        self.uv = uv  # (nt,3,2)
        self.an = anorm_table

    def render(self, pose, view, keep=None):
        m = self.m
        P = m.pos[pose][m.tris]  # (nt,3,3)
        x, y, z = view.project(P.reshape(-1, 3))
        nt = len(m.tris)
        x, y, z = x.reshape(nt, 3), y.reshape(nt, 3), z.reshape(nt, 3)
        tid, dep, w1, w2 = raster(x, y, z, view.S, keep, view.R * 1e-8, True, (self.uv[:, :, 1].mean(1), self.uv[:, :, 0].mean(1)))
        hit = tid >= 0
        t = tid[hit]
        a, b = w1[hit], w2[hit]
        u = (1 - a - b) * self.uv[t, 0, 0] + a * self.uv[t, 1, 0] + b * self.uv[t, 2, 0]
        v = (1 - a - b) * self.uv[t, 0, 1] + a * self.uv[t, 1, 1] + b * self.uv[t, 2, 1]
        cu = np.clip(np.floor(u).astype(np.int64), 0, m.sw - 1)
        cv = np.clip(np.floor(v).astype(np.int64), 0, m.sh - 1)
        texel = np.full(view.S * view.S, -1, np.int64)
        texel[hit] = m.skin0[cv, cu]
        ni = m.normidx[pose][m.tris[t]]  # (k,3)
        sh = np.clip(self.an[ni] @ LIGHT, 0, 1) * 0.6 + 0.4  # (k,3)
        shade = np.zeros(view.S * view.S)
        shade[hit] = (1 - a - b) * sh[:, 0] + a * sh[:, 1] + b * sh[:, 2]
        return tid, dep, texel, shade


def rgb(texel, shade, pal, S):
    img = np.zeros((S * S, 3), np.float64)
    hit = texel >= 0
    img[hit] = pal[texel[hit]] * shade[hit, None]
    img[~hit] = (40, 40, 48)
    return np.clip(img, 0, 255).astype(np.uint8).reshape(S, S, 3)


def write_png(path, img):
    img = np.ascontiguousarray(img, dtype=np.uint8)
    h, w = img.shape[:2]
    raw = b"".join(b"\0" + img[r].tobytes() for r in range(h))

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def framing(*models):
    pts = np.concatenate([m.pos.reshape(-1, 3) for m in models])
    lo, hi = pts.min(0), pts.max(0)
    c = (lo + hi) / 2
    R = np.linalg.norm(pts - c, axis=1).max() * 1.02 + 1e-6
    return c, R
