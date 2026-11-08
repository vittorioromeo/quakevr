# zfgate.py orig.mdl new.mdl zfjson [--size 256] : the z-fight fix gate.
# target = orig rendered with each fixed pair's kept triangle winning its contested pixels; the new model must equal it
# except on pixels the moved (nudged) triangles cover in either render (their outline shifts by a grid step).
# Also counts contested pixels where the new render does not show the kept surface's texel.
import json, os, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import zflib, mdlio  # noqa

A, B = mdlio.read(sys.argv[1]), mdlio.read(sys.argv[2])
Z = json.load(open(sys.argv[3]))
size = int(sys.argv[5]) if len(sys.argv) > 5 else 256
fixed = [r for r in Z["decisions"] if r["act"] not in ("his", "invisible")]
tri_from = np.array(Z["tri_from"])
moved = np.zeros(A.nt, bool)
moved[Z["moved_tris"]] = True
sa = zflib.Scene(A, frame_with=[A, B], size=size)
sb = zflib.Scene(B, frame_with=[A, B], size=size)
step = float(A.scale.max())
P = A.pos.shape[0]
tot = bad = wrong = cont = unexpl = 0
maxsh = 0.0
for f in range(P):
    for vi in range(len(sa.V)):
        t0, d0, x0, s0 = sa.render(f, vi)
        t1, d1, x1, s1 = sb.render(f, vi)
        tgt, tsh = x0.copy(), s0.copy()
        want = {}
        keep_of = {}
        for r in fixed:
            con = zflib.contested(sa, r["a"], r["b"], [f], 1.5 * step)
            for (ff, v2), (idx, ta, tb, sha, shb) in con.items():
                if v2 != vi:
                    continue
                k = r["keep"]
                tgt[idx] = ta if k == r["a"] else tb
                tsh[idx] = sha if k == r["a"] else shb
                for i, t in zip(idx.tolist(), (ta if k == r["a"] else tb).tolist()):
                    want[i] = t
                    keep_of[i] = k
        o1 = np.where(t1 >= 0, tri_from[np.maximum(t1, 0)], -1)
        allow = ((t0 >= 0) & moved[np.maximum(t0, 0)]) | ((o1 >= 0) & moved[np.maximum(o1, 0)])
        al2 = allow.reshape(size, size)
        for _ in range(2):
            al2 = al2 | np.roll(al2, 1, 0) | np.roll(al2, -1, 0) | np.roll(al2, 1, 1) | np.roll(al2, -1, 1)
        allow = al2.reshape(-1)
        diff = x1 != tgt
        # a moved face's texels and outline slide by under a grid step: a pixel is explained when the target shows
        # its texel within one pixel
        T2 = np.pad(tgt.reshape(size, size), 2, constant_values=-2)
        X2 = x1.reshape(size, size)
        expl = np.zeros((size, size), bool)
        for dy in range(5):
            for dx in range(5):
                expl |= T2[dy:dy + size, dx:dx + size] == X2
        expl = expl.reshape(-1)
        tot += int(diff.sum())
        bad += int((diff & ~allow).sum())
        unexpl += int((diff & ~expl).sum())
        both = (x1 >= 0) & (tgt >= 0) & ~allow
        if both.any():
            maxsh = max(maxsh, float(np.abs(s1 - tsh)[both].max()))
        if want:
            ii = np.array(list(want.keys()))
            got = o1[ii]
            wt = np.array([keep_of[i] for i in ii])
            wrong += int(np.count_nonzero((got != wt) & ~((got >= 0) & moved[np.maximum(got, 0)])))
            cont += len(ii)
ok = bad == 0 and unexpl == 0 and maxsh <= 0.02 and (wrong == 0 or Z["moved_tris"])
print("gate: %s %d poses x %d views: diff px %d (outside moved faces %d, not a 1-px slide %d), shade max %.4f outside, contested px %d, kept surface not shown %d -> %s"
      % (os.path.basename(sys.argv[2]), P, len(sa.V), tot, bad, unexpl, maxsh, cont, wrong, "PASS" if ok else "FAIL"))
