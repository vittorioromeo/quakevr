# audit.py -- topology audit of Quake alias models on their stored (quantised) vertices, every pose.
#   python audit.py [--only name,name] [--sources quakevr,id1:pak0,...] [--no-hidden]
#   python audit.py [--no-hidden] model.mdl ...       (one line of JSON per model, no inventory needed)
# Needs numpy: run it with Blender's Python ("C:/Program Files/Blender Foundation/Blender 5.2/5.2/python/bin/python.exe").
# Reads inventory.json (scratch/topo/collect.py), writes results/<key>.json per model.
# Classes (see report.py / topoaudit_report.md for the definitions and thresholds):
#   zfight       same-facing coplanar overlapping triangle pairs (gap <= 1 grid step, overlap > 2% and > 0.01 step^2):
#                same texels (>= 95% of samples) / different; coincident in every pose or some
#   dup_tris     exact duplicate triangles (same welded corners)
#   degenerate   zero area (index-, weld- or geometry-degenerate) in every pose / some pose; slivers (quality < 0.05)
#   dup_verts    same bytes in every pose: pure duplicates (same texel coords, normals) vs seam splits (needed)
#   open/nonman  welded edges used once / more than twice; flipped (two triangles walking an edge the same way)
#   cracks       T-junctions on open edges: a corner of another open edge within 1 step of the edge's inside;
#                width = its largest distance off the edge line over the poses (0 = on the line: no gap)
#   hidden       triangles never seen from 48 directions in the sampled poses (pixels or centroid depth test)
#   flat         connected coplanar (every pose, within 0.3 step), affinely moving, texture-affine regions:
#                estimated triangles saved by retriangulating each (k - (B + 2(L-1) - 2))
import argparse
import json
import os
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mdlio  # noqa: E402
import raster  # noqa: E402

RES = os.path.join(HERE, "results")


def sample_poses(P, n):
    return sorted(set(np.linspace(0, P - 1, min(n, P)).round().astype(int).tolist()))


def clip_area(A, B):
    """Area of the intersection of two 2D triangles (any winding)."""
    def ccw(T):
        a = (T[1][0] - T[0][0]) * (T[2][1] - T[0][1]) - (T[1][1] - T[0][1]) * (T[2][0] - T[0][0])
        return T if a > 0 else T[::-1]
    A, B = ccw(A), ccw(B)
    poly = list(B)
    for i in range(3):
        p, q = A[i], A[(i + 1) % 3]
        ex, ey = q[0] - p[0], q[1] - p[1]
        out = []
        n = len(poly)
        for k in range(n):
            c, d = poly[k], poly[(k + 1) % n]
            sc = ex * (c[1] - p[1]) - ey * (c[0] - p[0])
            sd = ex * (d[1] - p[1]) - ey * (d[0] - p[0])
            if sc >= 0:
                out.append(c)
            if (sc >= 0) != (sd >= 0):
                tt = sc / (sc - sd)
                out.append((c[0] + tt * (d[0] - c[0]), c[1] + tt * (d[1] - c[1])))
        poly = out
        if len(poly) < 3:
            return 0.0, []
    a = 0.0
    for k in range(len(poly)):
        a += poly[k][0] * poly[(k + 1) % len(poly)][1] - poly[(k + 1) % len(poly)][0] * poly[k][1]
    return abs(a) / 2, poly


def plane2d(n):
    n = n / np.linalg.norm(n)
    up = np.array([0, 0, 1.0]) if abs(n[2]) < 0.9 else np.array([1.0, 0, 0])
    u = np.cross(up, n)
    u /= np.linalg.norm(u)
    return u, np.cross(n, u)


def bary2(p, T):
    (x0, y0), (x1, y1), (x2, y2) = T
    d = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0)
    w1 = ((p[0] - x0) * (y2 - y0) - (p[1] - y0) * (x2 - x0)) / d
    w2 = ((x1 - x0) * (p[1] - y0) - (y1 - y0) * (p[0] - x0)) / d
    return 1 - w1 - w2, w1, w2


class Audit:
    def __init__(self, path, hidden=True):
        self.m = m = mdlio.read(path)
        self.path = path
        self.r = {"nv": int(m.nv), "nt": int(m.nt), "poses": int(m.pos.shape[0]), "frames": int(m.nf),
                  "skin": [int(m.sw), int(m.sh)], "step": float(m.scale.max())}
        self.step = float(m.scale.max())
        self.W = mdlio.weld(m)
        self.uv = mdlio.tri_uv(m)
        self.TP = m.pos[:, m.tris]  # (P,nt,3,3)
        self.N = mdlio.outward_normals(self.TP)  # (P,nt,3)
        self.A2 = np.linalg.norm(self.N, axis=2)  # twice area
        self.hidden = hidden

    def run(self):
        t0 = time.time()
        self.degenerate()
        self.duplicates()
        self.topology()
        self.cracks()
        self.zfight()
        if self.hidden:
            self.hidden_faces()
        self.zsummary()
        self.removable()
        regs, remv = self.flat_regions, self.removable_v
        self.removable(0.5, 0.5, "flat_loose")
        self.flat_regions, self.removable_v = regs, remv
        self.r["secs"] = round(time.time() - t0, 1)
        return self.r

    # ---- degenerate / slivers
    def degenerate(self):
        m, W = self.m, self.W
        T = m.tris
        idx_deg = (T[:, 0] == T[:, 1]) | (T[:, 1] == T[:, 2]) | (T[:, 0] == T[:, 2])
        WT = W[T]
        weld_deg = ((WT[:, 0] == WT[:, 1]) | (WT[:, 1] == WT[:, 2]) | (WT[:, 0] == WT[:, 2])) & ~idx_deg
        zero = self.A2 < 1e-4 * self.step ** 2  # (P,nt)
        geo_all = zero.all(0) & ~idx_deg & ~weld_deg
        geo_some = zero.any(0) & ~zero.all(0)
        P0 = self.TP[0]
        l2 = ((P0[:, [1, 2, 0]] - P0) ** 2).sum(2).sum(1)
        q = 2 * np.sqrt(3) * self.A2[0] / np.maximum(l2, 1e-30)
        sliver = (q < 0.05) & ~zero[0]
        self.degen_all = idx_deg | weld_deg | geo_all
        self.r["degenerate"] = {"index": int(idx_deg.sum()), "weld": int(weld_deg.sum()), "zero_area_all": int(geo_all.sum()),
                                "zero_area_some": int(geo_some.sum()), "sliver": int(sliver.sum()),
                                "where": [self.loc(t) for t in np.nonzero(self.degen_all)[0][:8]]}

    def loc(self, t):
        c = self.TP[0, t].mean(0)
        return [int(t), [round(float(x), 2) for x in c]]

    # ---- duplicate vertices
    def duplicates(self):
        m, W = self.m, self.W
        nweld = int(W.max()) + 1
        key = np.stack([W, m.s, m.t, m.onseam != 0], 1)
        k2 = np.concatenate([key, m.normidx.T], 1)
        pure_pos_uv = m.nv - len(np.unique(key, axis=0))
        pure_all = m.nv - len(np.unique(k2, axis=0))
        self.r["dup_verts"] = {"welded": nweld, "extra": int(m.nv - nweld), "seam_splits": int(m.nv - nweld - pure_pos_uv),
                               "same_uv": int(pure_pos_uv), "same_uv_and_normals": int(pure_all)}

    # ---- edges
    def topology(self):
        W = self.W[self.m.tris]
        keep = ~self.degen_all
        d = {}
        for t in np.nonzero(keep)[0]:
            for c in range(3):
                a, b = int(W[t, c]), int(W[t, (c + 1) % 3])
                d.setdefault((min(a, b), max(a, b)), []).append((t, a < b))
        openE = [e for e, u in d.items() if len(u) == 1]
        nonman = [e for e, u in d.items() if len(u) > 2]
        flipped = [e for e, u in d.items() if len(u) == 2 and u[0][1] == u[1][1]]
        self.edges = d
        self.open_edges = openE
        # dup tris
        ks = {}
        for t in np.nonzero(keep)[0]:
            ks.setdefault(tuple(sorted(W[t].tolist())), []).append(int(t))
        dups = [v for v in ks.values() if len(v) > 1]
        self.dup_tris = dups
        self.r["edges"] = {"open": len(openE), "nonmanifold": len(nonman), "flipped": len(flipped)}
        self.r["dup_tris"] = {"groups": len(dups), "extra": sum(len(v) - 1 for v in dups),
                              "where": [self.loc(v[0]) for v in dups[:6]]}

    # ---- cracks (T-junctions on open edges)
    def cracks(self):
        if not self.open_edges:
            self.r["cracks"] = {"count": 0, "on_line": 0, "max_width_steps": 0.0, "where": []}
            return
        m, W = self.m, self.W
        rep = np.zeros(int(W.max()) + 1, np.int64)
        rep[W] = np.arange(m.nv)
        Q = m.pos[:, rep]  # (P,nweld,3)
        E = np.array(self.open_edges)
        verts = np.unique(E)
        tol = 1.0 * self.step
        found = []
        A0, B0 = Q[0, E[:, 0]], Q[0, E[:, 1]]
        for c0 in range(0, len(E), 512):
            a, b = A0[c0:c0 + 512, None], B0[c0:c0 + 512, None]
            v = Q[0, verts][None]
            ab = b - a
            L2 = (ab ** 2).sum(-1)
            t = ((v - a) * ab).sum(-1) / np.maximum(L2, 1e-30)
            dist = np.linalg.norm(v - (a + t[..., None] * ab), axis=-1)
            ok = (t > 0.02) & (t < 0.98) & (dist < tol)
            ok &= verts[None] != E[c0:c0 + 512, 0, None]
            ok &= verts[None] != E[c0:c0 + 512, 1, None]
            for ei, vi in zip(*np.nonzero(ok)):
                found.append((c0 + ei, verts[vi]))
        out = []
        for ei, v in found:
            a, b = E[ei]
            pa, pb, pv = Q[:, a], Q[:, b], Q[:, v]
            ab = pb - pa
            t = ((pv - pa) * ab).sum(-1) / np.maximum((ab ** 2).sum(-1), 1e-30)
            w = np.linalg.norm(pv - (pa + t[:, None] * ab), axis=-1)
            if (w < tol).all() and ((t > 0.0) & (t < 1.0)).all():
                out.append((float(w.max()) / self.step, [round(float(x), 2) for x in pv[0]]))
        out.sort(key=lambda x: -x[0])
        self.r["cracks"] = {"count": len(out), "on_line": sum(1 for w, _ in out if w < 1e-6),
                            "max_width_steps": round(out[0][0], 2) if out else 0.0,
                            "where": [[round(w, 2), p] for w, p in out[:8]]}

    # ---- coplanar overlaps
    def zfight(self):
        m = self.m
        nt = m.nt
        P = self.TP.shape[0]
        ok = ~self.degen_all
        dtol = 1.0 * self.step
        cands = set()
        for f in sample_poses(P, 12):
            T = self.TP[f]
            n = self.N[f] / np.maximum(self.A2[f], 1e-30)[:, None]
            good = ok & (self.A2[f] > 1e-4 * self.step ** 2)
            ids = np.nonzero(good)[0]
            lo, hi = T[ids].min(1) - dtol, T[ids].max(1) + dtol
            nn = n[ids]
            c = T[ids].mean(1)
            for c0 in range(0, len(ids), 384):
                sl = slice(c0, c0 + 384)
                ov = ((lo[sl, None] <= hi[None]) & (hi[sl, None] >= lo[None])).all(-1)
                ov &= (nn[sl] @ nn.T) > 0.95
                ov &= np.arange(c0, c0 + ov.shape[0])[:, None] < np.arange(len(ids))[None]
                ii, jj = np.nonzero(ov)
                if not len(ii):
                    continue
                ii = ii + c0
                # vertices of j on i's plane and vice versa
                dj = np.abs(np.einsum("kd,kcd->kc", nn[ii], T[ids[jj]] - c[ii][:, None])).max(1)
                di = np.abs(np.einsum("kd,kcd->kc", nn[jj], T[ids[ii]] - c[jj][:, None])).max(1)
                sel = (dj < dtol) & (di < dtol)
                for a, b in zip(ids[ii[sel]], ids[jj[sel]]):
                    cands.add((int(a), int(b)))
        pairs = []
        dupset = {tuple(sorted(v[:2])) for v in self.dup_tris}
        minarea = 0.01 * self.step ** 2
        for a, b in cands:
            best = (0.0, None, None)
            nover = 0
            gap = 0.0
            hmin, hmax = 1e30, -1e30
            for f in range(P):
                Ta, Tb = self.TP[f, a], self.TP[f, b]
                n = self.N[f, a]
                if self.A2[f, a] < 1e-12:
                    continue
                u, v = plane2d(n)
                ca = [(float(p @ u), float(p @ v)) for p in Ta]
                cb = [(float(p @ u), float(p @ v)) for p in Tb]
                nu = n / np.linalg.norm(n)
                ar, poly = clip_area(ca, cb)
                small = min(self.A2[f, a], self.A2[f, b]) / 2
                if not (ar > 0.02 * small and ar > minarea):
                    continue
                # b's height over a's plane across the overlap (+: b outside a)
                nb = self.N[f, b] / max(self.A2[f, b], 1e-30)
                den = float(nb @ nu)
                if abs(den) < 0.5:
                    continue
                X = np.array([[pu, pv] for pu, pv in poly]) @ np.stack([u, v]) + float(nu @ Ta[0]) * nu
                hh = ((Tb[0] - X) @ nb) / den
                if np.abs(hh).max() < dtol:
                    nover += 1
                    gap = max(gap, float(np.abs(hh).max()))
                    hmin, hmax = min(hmin, float(hh.min())), max(hmax, float(hh.max()))
                    if ar > best[0]:
                        best = (ar, f, (ca, cb, poly))
            if not nover:
                continue
            e = 0.05 * self.step
            kind = "exact" if max(-hmin, hmax) < e else ("crossing" if hmin < -e and hmax > e else "near")
            ar, f, (ca, cb, poly) = best
            same = self.same_texels(a, b, ca, cb, poly)
            pairs.append({"a": a, "b": b, "area": round(ar, 3), "poses": nover, "all": nover == P,
                          "gap_steps": round(gap / self.step, 2), "same_texels": same, "kind": kind,
                          "exact_dup": (min(a, b), max(a, b)) in dupset,
                          "at": [round(float(x), 2) for x in self.TP[f, a].mean(0)], "pose": f})
        pairs.sort(key=lambda p: -p["area"])
        self.zpairs = pairs
        same = [p for p in pairs if p["same_texels"] >= 0.95]
        self.r["zfight"] = {"pairs": len(pairs), "same_texels": len(same), "diff_texels": len(pairs) - len(same),
                            "all_poses": sum(p["all"] for p in pairs), "exact_dup": sum(p["exact_dup"] for p in pairs),
                            "area": round(sum(p["area"] for p in pairs), 2),
                            "tris": len({p["a"] for p in pairs} | {p["b"] for p in pairs}),
                            "top": pairs[:10]}

    def zsummary(self):
        hid = set(getattr(self, "hidden_tris", np.zeros(0, int)).tolist())
        z = self.r["zfight"]
        for p in self.zpairs:
            p["hidden"] = p["a"] in hid or p["b"] in hid
        vis = [p for p in self.zpairs if not p["hidden"]]
        risk = [p for p in vis if p["kind"] in ("exact", "crossing")]
        z["exact"] = sum(p["kind"] == "exact" for p in vis)
        z["crossing"] = sum(p["kind"] == "crossing" for p in vis)
        z["near"] = sum(p["kind"] == "near" for p in vis)
        z["hidden_pairs"] = len(self.zpairs) - len(vis)
        z["risk_same"] = sum(p["same_texels"] >= 0.95 for p in risk)
        z["risk_diff"] = len(risk) - z["risk_same"]
        z["risk_area"] = round(sum(p["area"] for p in risk), 2)
        # coincident in every pose (built that way: topology) vs only in some (animation passing through)
        z["risk_static_same"] = sum(p["all"] and p["same_texels"] >= 0.95 for p in risk)
        z["risk_static_diff"] = sum(p["all"] and p["same_texels"] < 0.95 for p in risk)
        z["risk_transient"] = sum(not p["all"] for p in risk)
        z["near_static"] = sum(p["all"] for p in vis if p["kind"] == "near")
        z["list"] = [[p["a"], p["b"], p["kind"][0], p["same_texels"], int(p["all"]), p["poses"], p["area"], int(p["hidden"])]
                     for p in self.zpairs]
        order = sorted(self.zpairs, key=lambda p: (p["hidden"], p["kind"] == "near", -p["area"]))
        z["top"] = order[:12]

    def same_texels(self, a, b, ca, cb, poly):
        rng = np.random.default_rng(a * 7919 + b)
        pts = []
        for k in range(1, len(poly) - 1):
            for _ in range(12):
                r1, r2 = rng.random(2)
                if r1 + r2 > 1:
                    r1, r2 = 1 - r1, 1 - r2
                p0, p1, p2 = np.array(poly[0]), np.array(poly[k]), np.array(poly[k + 1])
                pts.append(p0 + r1 * (p1 - p0) + r2 * (p2 - p0))
        if not pts:
            return 0.0
        sk = self.m.skin0
        eq = 0
        for p in pts:
            ta = bary2(p, ca)
            tb = bary2(p, cb)
            ua = ta[0] * self.uv[a, 0] + ta[1] * self.uv[a, 1] + ta[2] * self.uv[a, 2]
            ub = tb[0] * self.uv[b, 0] + tb[1] * self.uv[b, 1] + tb[2] * self.uv[b, 2]
            xa, ya = int(np.clip(np.floor(ua[0]), 0, self.m.sw - 1)), int(np.clip(np.floor(ua[1]), 0, self.m.sh - 1))
            xb, yb = int(np.clip(np.floor(ub[0]), 0, self.m.sw - 1)), int(np.clip(np.floor(ub[1]), 0, self.m.sh - 1))
            eq += sk[ya, xa] == sk[yb, xb]
        return round(eq / len(pts), 3)

    # ---- hidden faces
    def hidden_faces(self, ndirs=48, size=320, nposes=8):
        m = self.m
        P = self.TP.shape[0]
        c, R = raster.framing(m)
        seen = np.zeros(m.nt, bool)
        D = raster.fib_dirs(ndirs)
        for f in sample_poses(P, nposes):
            T = self.TP[f]
            for d in D:
                view = raster.View(d, c, R, size)
                x, y, z = view.project(T.reshape(-1, 3))
                x, y, z = x.reshape(-1, 3), y.reshape(-1, 3), z.reshape(-1, 3)
                tid, dep, _, _ = raster.raster(x, y, z, size, None, R * 1e-8)
                seen[tid[tid >= 0]] = True
                # centroid depth test for front-facing triangles too small to own a pixel
                front = (self.N[f] @ d) > 0
                cand = np.nonzero(front & ~seen)[0]
                if len(cand):
                    cx, cy, cz = x[cand].mean(1), y[cand].mean(1), z[cand].mean(1)
                    ix = np.clip(cx.astype(np.int64), 0, size - 1)
                    iy = np.clip(cy.astype(np.int64), 0, size - 1)
                    zb = dep[iy * size + ix]
                    vis = cz <= zb + 0.5 * self.step
                    seen[cand[vis]] = True
        hid = np.nonzero(~seen & ~self.degen_all)[0]
        self.hidden_tris = hid
        coll = int((self.A2[:, hid].max(0) < 0.01 * self.step ** 2).sum()) if len(hid) else 0
        self.r["hidden"] = {"count": int(len(hid)), "area": round(float(self.A2[0, hid].sum() / 2), 2),
                            "collapsed": coll, "real": int(len(hid)) - coll,
                            "where": [self.loc(t) for t in hid[:8]]}

    # ---- flat regions
def removable(self, tol_steps=0.3, uv_tol=0.02, key="flat"):
    """Over-complex flat surfaces as zero-error decimation: a welded vertex whose removal (a half-edge collapse onto a
    neighbour) changes neither the surface nor its texture mapping in any pose:
      interior: all its triangles in one plane and one affine texture chart (closed fan)       -> 2 triangles fewer
      crease:   its fan is two such charts meeting along a straight line it lies on           -> 2 triangles fewer
      border:   one chart, on an open edge that runs straight through it                       -> 1 triangle fewer
    Every pose: the charts stay planar and move affinely (rigidly or stretched as one). Sequential removals along a line or
    over a plane all stay valid, so the sum is the estimate (the fix phase does them one by one, each gated)."""
    m = self.m
    W = self.W[m.tris]
    P = self.TP.shape[0]
    tol = tol_steps * self.step
    ok = ~self.degen_all & (self.A2.min(0) > 1e-4 * self.step ** 2)
    unit = self.N / np.maximum(self.A2, 1e-30)[..., None]  # (P,nt,3)
    E = np.stack([self.TP[0, :, 1] - self.TP[0, :, 0], self.TP[0, :, 2] - self.TP[0, :, 0]], -1)  # (nt,3,2)
    UVd = np.stack([self.uv[:, 1] - self.uv[:, 0], self.uv[:, 2] - self.uv[:, 0]], -1)  # (nt,2,2)
    good = ok.copy()
    J = np.zeros((m.nt, 2, 3))
    J[good] = UVd[good] @ np.linalg.pinv(E[good])
    rep = np.zeros(int(self.W.max()) + 1, np.int64)
    rep[self.W] = np.arange(m.nv)
    Q = m.pos[:, rep]  # (P,nweld,3)
    inc = {}
    for t in np.nonzero(ok)[0]:
        for k in range(3):
            inc.setdefault(int(W[t, k]), []).append((int(t), k))
    openv = set(v for e in self.open_edges for v in e)
    bad_tris = set(np.nonzero(~ok)[0].tolist())

    def same_chart(t, s):
        d = np.abs(np.einsum("pd,pcd->pc", unit[:, t], self.TP[:, s] - self.TP[:, t, 0][:, None])).max()
        if d > tol:
            return False
        pred = self.uv[t, 0][None] + (self.TP[0, s] - self.TP[0, t, 0]) @ J[t].T
        return np.abs(pred - self.uv[s]).max() <= uv_tol

    def moves_affinely(vs):
        X = Q[:, vs]  # (P,k,3)
        if len(vs) <= 3 or P == 1:
            return True
        X0 = np.concatenate([X[0], np.ones((len(vs), 1))], 1)
        Pi = np.linalg.pinv(X0)
        sol = np.einsum("ik,pkd->pid", Pi, X)
        res = np.abs(np.einsum("ki,pid->pkd", X0, sol) - X).max()
        return res <= tol

    rem = {}
    why = {}
    normal_safe = {}
    for v, lst in inc.items():
        # any degenerate triangle on v: leave it to the degenerate fix first
        if len(lst) < 2:
            continue
        nxt = {}
        tri_of = {}
        for t, k in lst:
            x, y = int(W[t, (k + 1) % 3]), int(W[t, (k + 2) % 3])
            if x in nxt:
                nxt = None
                break
            nxt[x] = y
            tri_of[x] = t
        if nxt is None:
            continue
        starts = [x for x in nxt if x not in set(nxt.values())]
        closed = not starts
        if not closed and (len(starts) != 1 or v not in openv):
            continue
        x0 = starts[0] if starts else next(iter(nxt))
        order, x = [], x0
        while x in nxt and len(order) <= len(nxt):
            order.append(tri_of[x])
            x = nxt[x]
            if x == x0:
                break
        if len(order) != len(lst):
            continue  # several fans (non-manifold)
        # split into chart runs
        runs = [[order[0]]]
        for t in order[1:]:
            if same_chart(runs[-1][0], t):
                runs[-1].append(t)
            else:
                runs.append([t])
        if closed and len(runs) > 1 and same_chart(runs[-1][0], runs[0][0]):
            runs[0] = runs.pop() + runs[0]
        ring = sorted(set(W[order].ravel().tolist()))
        saves = 0
        if closed and len(runs) == 1:
            saves = 2
        elif closed and len(runs) == 2:
            # crease vertices: the corners shared by the two runs other than v
            sa = set(W[runs[0]].ravel().tolist()) & set(W[runs[1]].ravel().tolist())
            sa.discard(v)
            if len(sa) == 2:
                u, w = sorted(sa)
                if self.on_segment(Q, v, u, w, tol):
                    saves = 2
        elif not closed and len(runs) == 1:
            u, w = x0, x  # the two open edges' far ends
            if self.on_segment(Q, v, u, w, tol):
                saves = 1
        why[("closed" if closed else "open") + "_runs%d" % min(len(runs), 3)] = why.get(("closed" if closed else "open") + "_runs%d" % min(len(runs), 3), 0) + 1
        if not saves:
            continue
        if not all(moves_affinely(sorted(set(W[r].ravel().tolist()))) for r in runs):
            why["not_affine_motion"] = why.get("not_affine_motion", 0) + 1
            continue
        rem[v] = saves
        ids = np.unique(m.tris[order])
        normal_safe[v] = bool((m.normidx[:, ids] == m.normidx[:, ids[:1]]).all()) or len(runs) == 2
    # components of removable vertices (for locating)
    par = {v: v for v in rem}

    def find(x):
        while par[x] != x:
            par[x] = par[par[x]]
            x = par[x]
        return x
    for (a, b) in self.edges:
        if a in rem and b in rem:
            par[find(a)] = find(b)
    comp = {}
    for v in rem:
        comp.setdefault(find(v), []).append(v)
    regs = []
    for vs in comp.values():
        regs.append({"vertices": len(vs), "saved": sum(rem[v] for v in vs),
                     "at": [round(float(x), 2) for x in Q[0, vs].mean(0)], "verts": [int(v) for v in vs[:200]]})
    regs.sort(key=lambda r: -r["saved"])
    self.flat_regions = regs
    saved = sum(rem.values())
    self.removable_v = rem
    self.r[key] = {"removable_vertices": len(rem), "saved": saved,
                      "saved_normal_safe": sum(s for v, s in rem.items() if normal_safe[v]),
                      "interior": sum(1 for v, s in rem.items() if s == 2 and v not in openv),
                      "regions": len(regs), "why": why, "top": [{k: v for k, v in r.items() if k != "verts"} for r in regs[:8]]}


def on_segment(self, Q, v, u, w, tol):
    a, b, p = Q[:, u], Q[:, w], Q[:, v]
    ab = b - a
    L2 = (ab ** 2).sum(-1)
    if (L2 < 1e-12).any():
        return False
    t = ((p - a) * ab).sum(-1) / L2
    d = np.linalg.norm(p - (a + t[:, None] * ab), axis=-1)
    return bool((d <= tol).all() and (t > 0).all() and (t < 1).all() and np.ptp(t) < 0.01)


Audit.removable = removable
Audit.on_segment = on_segment


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--sources", default="")
    ap.add_argument("--no-hidden", action="store_true")
    ap.add_argument("--skip-done", action="store_true", help="skip models whose result is already complete")
    ap.add_argument("models", nargs="*", help="model files to audit directly (prints their results as JSON)")
    a = ap.parse_args()
    if a.models:
        for p in a.models:
            r = Audit(p, hidden=not a.no_hidden).run()
            for k in ("zfight", "flat", "flat_loose"):
                r.get(k, {}).pop("top", None)
            print(os.path.basename(p), json.dumps(r, default=lambda o: o.item() if hasattr(o, "item") else str(o)))
        return
    inv = json.load(open(os.path.join(HERE, "inventory.json")))
    os.makedirs(RES, exist_ok=True)
    only = set(x for x in a.only.split(",") if x)
    srcs = set(x for x in a.sources.split(",") if x)
    for key, e in inv["models"].items():
        if only and not any(o == key or key.endswith("/" + o) or key.endswith("_" + o) for o in only):
            continue
        if srcs and e["source"] not in srcs:
            continue
        if e.get("overridden_by_quakevr"):
            continue
        out = os.path.join(RES, key.replace("/", "__").replace(":", "_") + ".json")
        if a.skip_done and os.path.exists(out):
            try:
                if "real" in json.load(open(out)).get("hidden", {}):
                    continue
            except ValueError:
                pass
        try:
            au = Audit(e["path"], hidden=not a.no_hidden)
            r = au.run()
        except Exception as ex:  # a broken file is a finding, not a crash
            r = {"error": repr(ex)}
        r["key"] = key
        json.dump(r, open(out, "w"), indent=1, default=lambda o: o.item() if hasattr(o, "item") else str(o))
        z = r.get("zfight", {})
        print("%-40s nt %5s zf %4s/%-4s hid %4s crk %3s deg %3s flat-saved %4s %ss" % (
            key, r.get("nt"), z.get("same_texels"), z.get("diff_texels"), r.get("hidden", {}).get("count"),
            r.get("cracks", {}).get("count"), sum(r.get("degenerate", {"x": 0}).get(k, 0) for k in ("index", "weld", "zero_area_all")),
            r.get("flat", {}).get("saved"), r.get("secs")), flush=True)


if __name__ == "__main__":
    main()
