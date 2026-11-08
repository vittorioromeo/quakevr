# fix.py -- the automatic topology fixes (topoaudit_report.md steps 1-5), each gated by compare.py at the model's own
# noise floor (its renumbered copy's result).
#   python fix.py in.mdl out.mdl [--steps degen,dup,zsame,weld,hidden,crack] [--work dir] [--frames all|N]
# Steps (each only when compare.py still says equal; a group that differs is retried one change at a time):
#   degen   triangles of zero area in every pose (index-, weld- or geometry-degenerate): never drawn
#   dup     exact duplicate triangles (same welded corners in the same winding): the extra copies
#   zsame   static z-fight pairs with the same texels (coincident in every pose): the one the other covers
#   weld    vertices identical in everything (bytes and normal in every pose, texel coords, seam flag): merged
#   hidden  triangles never seen from 48 directions in the sampled poses
#   crack   T-junctions of width 0 (the corner on the open edge's line in every pose, at a whole texel): the long
#           edge's triangle split at that corner
# Unused vertices are kept (the box and any vertex-indexed data stay as they are; welds renumber, see the map).
# Writes out.mdl and out.mdl.fix.json: per step what changed, "tri_from" (each new triangle's index in in.mdl; a
# split's halves both name the split one), "vert_from" (each new vertex's index in in.mdl), the compare results.
# Ends with "fix: <name> ..." and exit 0 (also when nothing changed).
import argparse
import json
import os
import re
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audit  # noqa: E402
import mdlio  # noqa: E402


class Work:
    def __init__(self, path, work, frames):
        self.path = path
        self.name = os.path.basename(path)
        self.work = work
        self.frames = frames
        self.m = mdlio.read(path)
        self.tri_from = np.arange(self.m.nt)
        self.vert_from = np.arange(self.m.nv)
        self.n = 0
        self.log = {}
        self.floor = self.compare(path, self.renum(path))
        print("fix: %s noise floor: texel px %d, shade %.4f" % (self.name, self.floor[0], self.floor[1]))

    def tmp(self, tag):
        self.n += 1
        return os.path.join(self.work, "%s_%02d_%s.mdl" % (self.name[:-4], self.n, tag))

    def renum(self, path):
        out = os.path.join(self.work, "renum")
        subprocess.run([sys.executable, os.path.join(HERE, "make_variants.py"), path, out], capture_output=True, check=True)
        return os.path.join(out, self.name[:-4] + "_renum.mdl")

    def compare(self, a, b):
        r = subprocess.run([sys.executable, os.path.join(HERE, "compare.py"), a, b, "--frames", self.frames,
                            "--max-texel-px", "1000000000", "--max-shade", "1"], capture_output=True, text=True)
        g = re.search(r"texel px (\d+), shade max ([\d.]+)", r.stdout)
        if not g:
            raise SystemExit("fix: compare failed: " + r.stdout + r.stderr)
        return int(g.group(1)), float(g.group(2))

    def passes(self, res):
        return res[0] <= self.floor[0] and res[1] <= max(0.02, self.floor[1])

    def write(self, m, tag):
        p = self.tmp(tag)
        mdlio.write(m, p)
        return p

    def current_path(self):
        return self.write(self.m, "cur")

    # candidate meshes
    def without(self, drop):
        m = mdlio.read(self.cur_bytes())
        keep = np.ones(m.nt, bool)
        keep[list(drop)] = False
        m.tris, m.facesfront = m.tris[keep].copy(), m.facesfront[keep].copy()
        return m, keep

    def cur_bytes(self):
        p = os.path.join(self.work, "_cur.mdl")
        mdlio.write(self.m, p)
        return open(p, "rb").read()

    def try_drop(self, drop, tag):
        """Drops triangles `drop` (indices into the current mesh) if the model still compares equal; returns the result."""
        m, keep = self.without(drop)
        p = self.write(m, tag)
        res = self.compare(self.path, p)
        if self.passes(res):
            self.m = mdlio.read(p)
            self.tri_from = self.tri_from[keep]
            return True, res
        return False, res

    def drop_gated(self, groups, tag):
        """groups: lists of current triangle indices, each one change. All at once, else one at a time."""
        groups = [sorted(set(g)) for g in groups if g]
        if not groups:
            return 0, []
        allt = sorted({t for g in groups for t in g})
        ok, res = self.try_drop(allt, tag)
        if ok:
            return len(allt), [res]
        results = [res]
        removed = 0
        for g in groups:
            # indices shift as earlier groups go: map through tri ids
            ids = [int(np.nonzero(self.tri_from == self._orig_of[t])[0][0]) for t in g if (self.tri_from == self._orig_of[t]).any()]
            ok, r = self.try_drop(ids, tag + "1")
            results.append(r)
            removed += len(ids) if ok else 0
        return removed, results

    def audit(self):
        p = os.path.join(self.work, "_aud.mdl")
        mdlio.write(self.m, p)
        a = audit.Audit(p, hidden=False)
        self._orig_of = self.tri_from.copy()
        return a

    # ---- steps
    def step_degen(self):
        a = self.audit()
        a.degenerate()
        idx = np.nonzero(a.degen_all)[0].tolist()
        n, res = self.drop_gated([idx] if idx else [], "degen")
        self.log["degen"] = {"found": len(idx), "removed": n, "compare": res[:1]}

    def step_dup(self):
        a = self.audit()
        a.degenerate()
        a.topology()
        W = a.W[a.m.tris]
        groups = []
        for g in a.dup_tris:
            by = {}
            for t in g:
                w = W[t].tolist()
                k = w.index(min(w))
                by.setdefault(tuple(w[k:] + w[:k]), []).append(t)
            for same in by.values():
                if len(same) > 1:
                    groups.append(same)
        found = sum(len(g) - 1 for g in groups)
        removed, res = 0, []
        if groups:
            # keep the first copy (drawn first), else the last
            n, res = self.drop_gated([g[1:] for g in groups], "dup")
            if n < found:
                a = self.audit()  # retry the rest keeping the last copy
                a.degenerate()
                a.topology()
                W = a.W[a.m.tris]
                rest = []
                for g in a.dup_tris:
                    by = {}
                    for t in g:
                        w = W[t].tolist()
                        k = w.index(min(w))
                        by.setdefault(tuple(w[k:] + w[:k]), []).append(t)
                    rest += [s[:-1] for s in by.values() if len(s) > 1]
                n2, res2 = self.drop_gated(rest, "dupl")
                n += n2
                res += res2
            removed = n
        self.log["dup"] = {"found": found, "removed": removed, "compare": res[:1]}

    def step_zsame(self):
        a = self.audit()
        a.degenerate()
        a.topology()
        a.zfight()
        groups = []
        for p in a.zpairs:
            if not (p["all"] and p["same_texels"] >= 0.95 and p["kind"] in ("exact", "crossing")):
                continue
            ta, tb = p["a"], p["b"]
            f = p["pose"]
            ara, arb = a.A2[f, ta] / 2, a.A2[f, tb] / 2
            # the covered one: the overlap is (nearly) all of it
            if p["area"] >= 0.98 * min(ara, arb):
                groups.append([tb if arb <= ara else ta])
        removed, res = self.drop_gated(groups, "zsame") if groups else (0, [])
        self.log["zsame"] = {"found": len(groups), "removed": removed, "compare": res[:1]}

    def step_hidden(self):
        p = os.path.join(self.work, "_aud.mdl")
        mdlio.write(self.m, p)
        a = audit.Audit(p, hidden=True)
        self._orig_of = self.tri_from.copy()
        a.degenerate()
        a.hidden_faces()
        hid = a.hidden_tris.tolist()
        removed, res = self.drop_gated([[t] for t in hid], "hidden") if hid else (0, [])
        self.log["hidden"] = {"found": len(hid), "removed": removed, "compare": res[:1]}

    def step_weld(self):
        m = self.m
        key = np.concatenate([m.bytes.transpose(1, 0, 2).reshape(m.nv, -1), m.normidx.T,
                              np.stack([m.s, m.t, m.onseam], 1)], 1)
        _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
        inv = inv.reshape(-1)
        rep = first[inv]  # each vertex's first identical one
        dup = rep != np.arange(m.nv)
        if not dup.any():
            self.log["weld"] = {"found": 0, "removed": 0, "compare": []}
            return
        keepv = ~dup
        newidx = np.cumsum(keepv) - 1
        c = mdlio.read(self.cur_bytes())
        c.tris = newidx[rep[m.tris]]
        c.onseam, c.s, c.t = m.onseam[keepv], m.s[keepv], m.t[keepv]
        c.bytes, c.normidx = m.bytes[:, keepv], m.normidx[:, keepv]
        p = self.write(c, "weld")
        res = self.compare(self.path, p)
        if self.passes(res):
            self.old_to_new_v = newidx[rep]  # current vertex -> new vertex
            self.vert_map_steps.append(self.old_to_new_v)
            self.vert_from = self.vert_from[keepv]
            self.m = mdlio.read(p)
            self.log["weld"] = {"found": int(dup.sum()), "removed": int(dup.sum()), "compare": [res]}
        else:
            self.log["weld"] = {"found": int(dup.sum()), "removed": 0, "compare": [res]}

    def step_crack(self):
        a = self.audit()
        a.degenerate()
        a.topology()
        m = self.m
        W = a.W
        rep = np.zeros(int(W.max()) + 1, np.int64)
        rep[W] = np.arange(m.nv)
        Q = m.pos[:, rep]
        uv = mdlio.tri_uv(m)
        splits = []  # (tri, corner c: edge c->c+1, new vertex template, t)
        done = set()
        verts = np.unique(np.array(a.open_edges)) if a.open_edges else np.zeros(0, int)
        for (wa, wb) in a.open_edges:
            (t, _), = a.edges[(wa, wb)]
            p0a, p0b = Q[0, wa], Q[0, wb]
            ab0 = p0b - p0a
            tq = ((Q[0, verts] - p0a) @ ab0) / max(float(ab0 @ ab0), 1e-30)
            dq = np.linalg.norm(Q[0, verts] - (p0a + tq[:, None] * ab0), axis=1)
            for wv in verts[(tq > 0) & (tq < 1) & (dq < 1e-9)]:
                if wv in (wa, wb):
                    continue
                pa, pb, pv = Q[:, wa], Q[:, wb], Q[:, wv]
                ab = pb - pa
                L2 = (ab ** 2).sum(-1)
                tt = ((pv - pa) * ab).sum(-1) / np.maximum(L2, 1e-30)
                if not ((tt > 0.0) & (tt < 1.0)).all():
                    continue
                w = np.linalg.norm(pv - (pa + tt[:, None] * ab), axis=-1)
                if w.max() > 1e-9 or np.abs(tt - tt[0]).max() > 1e-9:
                    continue
                if t in done:
                    continue
                # the triangle's corner order for the edge
                Wt = W[m.tris[t]].tolist()
                for c in range(3):
                    if {Wt[c], Wt[(c + 1) % 3]} == {wa, wb}:
                        break
                ia, ib = m.tris[t][c], m.tris[t][(c + 1) % 3]
                s0 = tt[0] if Wt[c] == wa else 1 - tt[0]
                st = (1 - s0) * np.array([m.s[ia], m.t[ia]], float) + s0 * np.array([m.s[ib], m.t[ib]], float)
                if np.abs(st - np.round(st)).max() > 1e-6 or m.onseam[ia] != m.onseam[ib]:
                    continue
                splits.append((t, c, int(rep[wv]), st.round().astype(int), int(m.onseam[ia])))
                done.add(t)
        found = len(splits)
        removed = 0
        res = []
        if splits:
            c_ = mdlio.read(self.cur_bytes())
            tris, ff, tf = list(m.tris.tolist()), list(m.facesfront.tolist()), list(self.tri_from.tolist())
            s, tt_, os_ = list(m.s), list(m.t), list(m.onseam)
            B, N = [m.bytes], [m.normidx]
            nv = m.nv
            for t, c, v, st, seam in splits:
                # a new vertex at v's place (its bytes and normal), the edge's texel coords
                s.append(st[0]); tt_.append(st[1]); os_.append(seam)
                B.append(m.bytes[:, v:v + 1]); N.append(m.normidx[:, v:v + 1])
                ia, ib, ic = tris[t][c], tris[t][(c + 1) % 3], tris[t][(c + 2) % 3]
                tris[t] = [ia, nv, ic]
                tris.append([nv, ib, ic]); ff.append(ff[t]); tf.append(tf[t])
                nv += 1
            c_.tris = np.array(tris); c_.facesfront = np.array(ff)
            c_.s, c_.t, c_.onseam = np.array(s), np.array(tt_), np.array(os_)
            c_.bytes, c_.normidx = np.concatenate(B, 1), np.concatenate(N, 1)
            p = self.write(c_, "crack")
            r = self.compare(self.path, p)
            res = [r]
            if self.passes(r):
                self.m = mdlio.read(p)
                self.tri_from = np.array(tf)
                self.vert_from = np.concatenate([self.vert_from, -1 - np.array([v for _, _, v, _, _ in splits])])
                removed = found
        self.log["crack"] = {"found": found, "split": removed, "compare": res}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inp")
    ap.add_argument("out")
    ap.add_argument("--steps", default="degen,dup,zsame,weld,hidden,crack")
    ap.add_argument("--work", default=None)
    ap.add_argument("--frames", default="all")
    a = ap.parse_args()
    work = a.work or os.path.join(os.path.dirname(os.path.abspath(a.out)), "fixwork")
    os.makedirs(work, exist_ok=True)
    w = Work(a.inp, work, a.frames)
    w.vert_map_steps = []
    for s in a.steps.split(","):
        getattr(w, "step_" + s)()
        print("fix: %s %s %s" % (w.name, s, json.dumps(w.log[s])))
    nt0, nv0 = mdlio.read(a.inp).nt, mdlio.read(a.inp).nv
    final = w.compare(a.inp, w.write(w.m, "final"))
    mdlio.write(w.m, a.out)
    # vertex map old -> new (welds) composed
    old_to_new = np.arange(nv0)
    for mp in w.vert_map_steps:
        old_to_new = mp[old_to_new]
    json.dump({"model": w.name, "nt": [int(nt0), int(w.m.nt)], "nv": [int(nv0), int(w.m.nv)], "floor": w.floor,
               "final": final, "steps": w.log, "tri_from": w.tri_from.tolist(), "vert_from": w.vert_from.tolist(),
               "vert_old_to_new": old_to_new.tolist()},
              open(a.out + ".fix.json", "w"))
    print("fix: %s triangles %d -> %d, vertices %d -> %d, compare texel px %d shade %.4f (floor %d %.4f) %s"
          % (w.name, nt0, w.m.nt, nv0, w.m.nv, final[0], final[1], w.floor[0], w.floor[1],
             "PASS" if w.passes(final) else "FAIL"))


if __name__ == "__main__":
    main()
