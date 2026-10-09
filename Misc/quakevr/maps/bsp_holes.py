# bsp_holes.py -- a ray test for missing faces in a compiled Quake BSP (BSP2 or BSP29): rays from random open points
# (empty or water) to the first change of contents in the world's hull 0; the point where a ray meets that change must
# lie on one of the world's faces on that plane. A point on none is a hole (a face qbsp lost: ericw-tools' "sides not
# found", a sliver of sky or void seen through the ground). Pure Python (no numpy), spread over the CPU's cores.
# A face that is there but in no leaf's list of faces (marksurfaces) is never drawn either (the renderer draws the
# faces its visible leaves list): a hole the ray test above can't see (vrstart 2026-10-09, a terrain triangle by the
# trees on the hill; qbsp 0.18.1 put it on its nearly coplanar neighbour's plane and listed only the neighbour). So
# every world face is checked against the leaves' lists ("unlisted faces", exact, over the whole map), and each ray's
# hit as the renderer would draw it ("undrawn hits": no face holding the point is listed by a leaf the ray's start
# leaf can see, by vis's PVS; a face listed only by a neighbouring leaf is common and fine, both are seen).
#
#   python Misc/quakevr/maps/bsp_holes.py quakevr/maps/vrstart.bsp [--rays 120000] [--seed 1] [--focus x0,y0,z0,x1,y1,z1]
#
# Prints the hits (a hole's point, the plane, the two contents) clustered by 64-unit cell, the unlisted faces, and
# "holes: N" (missing-face hits, undrawn hits and unlisted faces together).
import argparse
import math
import multiprocessing
import os
import random
import struct
import sys

CONTENTS = {-1: "empty", -2: "solid", -3: "water", -4: "slime", -5: "lava", -6: "sky"}


class Bsp:
    def __init__(self, path):
        with open(path, "rb") as f:
            d = f.read()
        magic = d[:4]
        self.bsp2 = magic == b"BSP2"
        if not self.bsp2:
            assert struct.unpack_from("<i", d, 0)[0] == 29, magic
        lumps = [struct.unpack_from("<ii", d, 4 + 8 * i) for i in range(15)]

        def lump(i):
            o, l = lumps[i]
            return d[o:o + l]

        pl = lump(1)
        self.planes = [struct.unpack_from("<4f", pl, i * 20) for i in range(len(pl) // 20)]
        vx = lump(3)
        self.verts = [struct.unpack_from("<3f", vx, i * 12) for i in range(len(vx) // 12)]
        nd = lump(5)
        self.nodes = []
        if self.bsp2:
            for i in range(len(nd) // 44):
                p, c0, c1 = struct.unpack_from("<iii", nd, i * 44)
                self.nodes.append((p, c0, c1))
        else:
            for i in range(len(nd) // 24):
                p, c0, c1 = struct.unpack_from("<ihh", nd, i * 24)
                self.nodes.append((p, c0, c1))
        lf = lump(10)
        sz = 44 if self.bsp2 else 28
        self.leafc = [struct.unpack_from("<i", lf, i * sz)[0] for i in range(len(lf) // sz)]
        ms = lump(11)
        msz = 4 if self.bsp2 else 2
        marks = struct.unpack_from("<%d%s" % (len(ms) // msz, "I" if self.bsp2 else "H"), ms, 0)
        # each leaf's listed faces (marksurfaces; world face numbers)
        self.leafmarks = []
        for i in range(len(lf) // sz):
            fm, nm = struct.unpack_from("<II" if self.bsp2 else "<HH", lf, i * sz + (32 if self.bsp2 else 20))
            self.leafmarks.append(frozenset(marks[fm:fm + nm]))
        self.listers = {}  # world face number -> the leaves listing it
        for L, m in enumerate(self.leafmarks):
            for f in m:
                self.listers.setdefault(f, []).append(L)
        self.visofs = [struct.unpack_from("<i", lf, i * sz + 4)[0] for i in range(len(lf) // sz)]
        self.vis = lump(4)
        fc = lump(7)
        ed = lump(12)
        se = lump(13)
        if self.bsp2:
            edges = [struct.unpack_from("<II", ed, i * 8) for i in range(len(ed) // 8)]
        else:
            edges = [struct.unpack_from("<HH", ed, i * 4) for i in range(len(ed) // 4)]
        surfedges = struct.unpack_from("<%di" % (len(se) // 4), se, 0)
        md = lump(14)
        mins = struct.unpack_from("<3f", md, 0)
        self.head = struct.unpack_from("<i", md, 36)[0]
        self.visleafs = struct.unpack_from("<i", md, 52)[0]
        firstface, numfaces = struct.unpack_from("<ii", md, 56)
        self.firstface = firstface
        fsz = 28 if self.bsp2 else 20
        self.faces = []
        for i in range(firstface, firstface + numfaces):
            if self.bsp2:
                pn, side, fe, ne = struct.unpack_from("<iiii", fc, i * fsz)
            else:
                pn, side, fe, ne = struct.unpack_from("<hhih", fc, i * fsz)
            poly = []
            for k in range(fe, fe + ne):
                e = surfedges[k]
                poly.append(self.verts[edges[e][0]] if e >= 0 else self.verts[edges[-e][1]])
            self.faces.append((pn, poly))
        # the world's faces in a grid of CELL-unit cells (by their bounds): a hit is looked up by where it is, and a face
        # counts if its plane is the hit's (within a degree and half a unit) and it holds the point
        self.grid = {}
        self.fdata = []
        for pn, poly in self.faces:
            fi = len(self.fdata)
            self.fdata.append(self.prep(pn, poly))
            lo = [min(q[i] for q in poly) - 1 for i in range(3)]
            hi = [max(q[i] for q in poly) + 1 for i in range(3)]
            C = self.CELL
            for cx in range(int(lo[0] // C), int(hi[0] // C) + 1):
                for cy in range(int(lo[1] // C), int(hi[1] // C) + 1):
                    for cz in range(int(lo[2] // C), int(hi[2] // C) + 1):
                        self.grid.setdefault((cx, cy, cz), []).append(fi)

    CELL = 128

    def prep(self, pn, poly):
        n = self.planes[pn][:3]
        ax = max(range(3), key=lambda i: abs(n[i]))
        u, v = [(1, 2), (0, 2), (0, 1)][ax]
        p2 = [(q[u], q[v]) for q in poly]
        xs = [q[0] for q in p2]
        ys = [q[1] for q in p2]
        return (self.planes[pn], u, v, p2, min(xs), min(ys), max(xs), max(ys))

    def leaf(self, p):
        n = self.head
        while n >= 0:
            pn, c0, c1 = self.nodes[n]
            a, b, c, dd = self.planes[pn]
            n = c0 if a * p[0] + b * p[1] + c * p[2] - dd >= 0 else c1
        return -n - 1

    def contents(self, p):
        return self.leafc[self.leaf(p)]

    def unlisted_faces(self):
        """The world faces no leaf lists (never drawn): [(face index, its centre)]."""
        listed = set()
        for m in self.leafmarks:
            listed.update(m)
        out = []
        for fi, (pn, poly) in enumerate(self.faces):
            if fi + self.firstface not in listed:
                out.append((fi, tuple(sum(q[i] for q in poly) / len(poly) for i in range(3))))
        return out

    def trace(self, a, b, c0):
        """The first point of a..b whose contents are not c0: (t, planenum, contents) or None."""
        stack = [(self.head, a, b, 0.0, 1.0, -1)]
        nodes, planes, leafc = self.nodes, self.planes, self.leafc
        while stack:
            n, p, q, tp, tq, pin = stack.pop()
            while n >= 0:
                pn, f, bk = nodes[n]
                x, y, z, dd = planes[pn]
                dp = x * p[0] + y * p[1] + z * p[2] - dd
                dq = x * q[0] + y * q[1] + z * q[2] - dd
                if dp >= 0 and dq >= 0:
                    n = f
                    continue
                if dp < 0 and dq < 0:
                    n = bk
                    continue
                fr = dp / (dp - dq)
                m = (p[0] + (q[0] - p[0]) * fr, p[1] + (q[1] - p[1]) * fr, p[2] + (q[2] - p[2]) * fr)
                tm = tp + (tq - tp) * fr
                near, far = (f, bk) if dp >= 0 else (bk, f)
                stack.append((far, m, q, tm, tq, pn))
                n, q, tq = near, m, tm
            c = leafc[-n - 1]
            if c != c0:
                return (tp, pin, c, p)
        return None

    def sees(self, a, b):
        """Whether leaf a's PVS has leaf b (no vis data, or a leaf without: everything)."""
        o = self.visofs[a]
        if o < 0 or not self.vis or b == 0:
            return True
        want = (b - 1) >> 3
        vis = self.vis
        i = 0
        while i <= want:
            if vis[o]:
                if i == want:
                    return bool(vis[o] & (1 << ((b - 1) & 7)))
                i += 1
                o += 1
            else:
                i += vis[o + 1]
                o += 2
        return False

    def drawn_from(self, pn, p, start):
        """Whether a face holding p (on plane pn) is listed by a leaf that leaf `start` sees: the renderer draws it."""
        for fi in self.faces_at(pn, p):
            for L in self.listers.get(fi + self.firstface, ()):
                if L == start or self.sees(start, L):
                    return True
        return False

    def on_face(self, pn, p, eps=0.25):
        """Whether a world face on plane pn holds p."""
        for _ in self.faces_at(pn, p, eps):
            return True
        return False

    def faces_at(self, pn, p, eps=0.25):
        """The world faces (indices into self.faces) on plane pn that hold p."""
        hn = self.planes[pn]
        C = self.CELL
        for fi in self.grid.get((int(p[0] // C), int(p[1] // C), int(p[2] // C)), ()):
            pl, u, v, p2, x0, y0, x1, y1 = self.fdata[fi]
            if abs(pl[0] * hn[0] + pl[1] * hn[1] + pl[2] * hn[2]) < 0.9998:
                continue
            if abs(pl[0] * p[0] + pl[1] * p[1] + pl[2] * p[2] - pl[3]) > 0.5:
                continue
            x, y = p[u], p[v]
            if x < x0 - eps or x > x1 + eps or y < y0 - eps or y > y1 + eps:
                continue
            # inside one of the triangles of its fan from the first corner (as the engine draws it: a face with
            # T-junctions' corners may double back on itself, its fan still covers it), within eps of their edges
            ax_, ay_ = p2[0]
            for i in range(1, len(p2) - 1):
                if _in_tri(x, y, ax_, ay_, p2[i][0], p2[i][1], p2[i + 1][0], p2[i + 1][1], eps):
                    yield fi
                    break


def _in_tri(x, y, ax, ay, bx, by, cx, cy, eps):
    area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
    if area < 0:
        bx, by, cx, cy = cx, cy, bx, by
        area = -area
    for (px, py, qx, qy) in ((ax, ay, bx, by), (bx, by, cx, cy), (cx, cy, ax, ay)):
        L = math.hypot(qx - px, qy - py)
        if L < 1e-9:
            continue
        if ((qx - px) * (y - py) - (qy - py) * (x - px)) / L < -eps:
            return False
    return area > 1e-9 or False


BSP = None


def _init(path):
    global BSP
    BSP = Bsp(path)


def _work(job):
    seed, n, box, focus = job
    rnd = random.Random(seed)
    b = BSP
    hits = []
    done = 0
    lo, hi = focus if focus else box
    while done < n:
        p = (rnd.uniform(lo[0], hi[0]), rnd.uniform(lo[1], hi[1]), rnd.uniform(lo[2], hi[2]))
        c0 = b.contents(p)
        if c0 not in (-1, -3):
            continue
        z = rnd.uniform(-1, 1)
        a = rnd.uniform(0, 2 * math.pi)
        r = math.sqrt(1 - z * z)
        dvec = (r * math.cos(a), r * math.sin(a), z)
        q = (p[0] + dvec[0] * 16384, p[1] + dvec[1] * 16384, p[2] + dvec[2] * 16384)
        done += 1
        h = b.trace(p, q, c0)
        if h is None:
            continue
        t, pn, c, hp = h
        if pn < 0:
            continue
        if not b.on_face(pn, hp):
            hits.append((hp, pn, c0, c, "missing"))
            continue
        # drawn: a face holding the point listed by a leaf the start's leaf sees
        if not b.drawn_from(pn, hp, b.leaf(p)):
            hits.append((hp, pn, c0, c, "undrawn"))
    return hits


def main():
    ap = argparse.ArgumentParser(description="A ray test for missing faces (holes) in a compiled Quake BSP.")
    ap.add_argument("bsp")
    ap.add_argument("--rays", type=int, default=120000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--focus", default=None, help="x0,y0,z0,x1,y1,z1: the start points' box (default: the world's)")
    ap.add_argument("--show", type=int, default=40, help="list at most this many holes' cells")
    args = ap.parse_args()
    b = Bsp(args.bsp)
    vs = b.verts
    box = (tuple(min(v[i] for v in vs) for i in range(3)), tuple(max(v[i] for v in vs) for i in range(3)))
    focus = None
    if args.focus:
        f = [float(x) for x in args.focus.split(",")]
        focus = (tuple(f[:3]), tuple(f[3:]))
    jobs = max(1, (os.cpu_count() or 4))
    per = args.rays // (jobs * 4) + 1 if args.rays > 0 else 0
    work = [(args.seed * 100003 + i, per, box, focus) for i in range(jobs * 4)]
    res = []
    if per:  # (--rays 0: the unlisted faces alone)
        with multiprocessing.Pool(jobs, _init, (args.bsp,)) as pool:
            res = pool.map(_work, work)
    hits = [h for r in res for h in r]
    cells = {}
    for hp, pn, c0, c, kind in hits:
        k = (kind, int(hp[0] // 64), int(hp[1] // 64), int(hp[2] // 64))
        cells.setdefault(k, []).append((hp, pn, c0, c))
    print("%s: %d rays, %d world faces, %d leaves" % (args.bsp, per * len(work), len(b.faces), len(b.leafc)))
    for k, hs in sorted(cells.items(), key=lambda kv: -len(kv[1]))[:args.show]:
        hp, pn, c0, c = hs[0]
        pl = b.planes[pn]
        print("  %4d %s hits at (%.0f %.0f %.0f) plane (%.3f %.3f %.3f) %.1f  %s -> %s" % (
            len(hs), k[0], hp[0], hp[1], hp[2], pl[0], pl[1], pl[2], pl[3], CONTENTS.get(c0, c0), CONTENTS.get(c, c)))
    unl = b.unlisted_faces()
    for fi, cen in unl[:args.show]:
        print("  unlisted face %d at (%.0f %.0f %.0f), %d corners" % (fi, cen[0], cen[1], cen[2], len(b.faces[fi][1])))
    nmiss = sum(1 for h in hits if h[4] == "missing")
    print("missing-face hits: %d, undrawn hits: %d, unlisted faces: %d" % (nmiss, len(hits) - nmiss, len(unl)))
    print("holes: %d (%d places)" % (len(hits) + len(unl), len(cells) + len(unl)))


if __name__ == "__main__":
    main()
