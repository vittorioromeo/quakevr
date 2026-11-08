# zffix.py in.mdl out.mdl [--skip a:b,...] [--force a:b=keep,...]
# (pairs from zfpairs.py, read from $ZF_WORK or the current folder; run with Blender's Python, like audit.py)
# Decides each static different-texel z-fight pair and fixes the clear ones:
#   invisible  fewer than MINVIS contested pixels whose two looks differ visibly (RGB >= 16): left as is
#   fold       one triangle inside the other and sharing an edge with it: left for Vittorio (removing the inside one
#              opened a hole in every model tried: v_light, vrpouch), unless --force a:b=drop/<kept>
#   detail     one triangle inside the other, not sharing an edge: the inside one nudged out
#   glued/edge partial overlap with no shared vertex and at most MAXQUIET visibly differing px (the two looks nearly
#              the same): the smaller part's face nudged out
#   his        anything else (both look intended, or a nudge is blocked): left, listed for Vittorio
# Gate the result with zfgate.py, check_mdl_holes.py and anchors.py check.
# Nudge: the winner's flat patch (coplanar triangles connected through welded vertices) moves rigidly by the smallest
# whole-grid-step vector that puts it >= 1 step in front of the loser, per pose. Writes out.mdl + out.mdl.zf.json.
import json, os, sys, itertools
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import zflib, mdlio, audit  # noqa

MINVIS = 8
MAXQUIET = 150
inp, outp = sys.argv[1], sys.argv[2]
opts = dict(zip(sys.argv[3::2], sys.argv[4::2]))
skip = {tuple(int(x) for x in s.split(":")) for s in opts.get("--skip", "").split(",") if s}
force = {}
for s in opts.get("--force", "").split(","):
    if s:
        ab, k = s.split("=")
        force[tuple(int(x) for x in ab.split(":"))] = k
name = os.path.basename(inp)
J = json.load(open(os.path.join(os.environ.get("ZF_WORK", "."), name + ".pairs.json")))
m = mdlio.read(inp)
P = m.pos.shape[0]
comp, W = zflib.components(m)
csize = np.bincount(comp, minlength=m.nt)
step = float(m.scale.max())
sc = zflib.Scene(m)
poses = sorted(set([0, P // 2, P - 1]))
TP = m.pos[:, m.tris]
FN = -mdlio.outward_normals(TP)
FN = FN / np.maximum(np.linalg.norm(FN, axis=-1, keepdims=True), 1e-30)  # front normals (P, nt, 3)
tw = W[m.tris]  # welded corners


def patch(t):
    """coplanar triangles connected to t through welded vertices (every pose)"""
    byw = {}
    for i, c in enumerate(tw):
        for w in c:
            byw.setdefault(int(w), []).append(i)
    seen, todo = {t}, [t]
    while todo:
        x = todo.pop()
        for w in tw[x]:
            for y in byw[int(w)]:
                if y in seen:
                    continue
                if (np.einsum("pd,pd->p", FN[:, y], FN[:, t]) > 0.995).all():
                    d = np.einsum("pcd,pd->pc", TP[:, y] - TP[:, t, :1], FN[:, t])
                    if np.abs(d).max() < 0.3 * step:
                        seen.add(y)
                        todo.append(y)
    return seen


def overlap_cov(a, b, f):
    Ta, Tb = m.pos[f][m.tris[a]], m.pos[f][m.tris[b]]
    u, v = audit.plane2d(FN[f, a])
    ar, _ = audit.clip_area([(x @ u, x @ v) for x in Ta], [(x @ u, x @ v) for x in Tb])
    Aa, Ab = zflib.tri_area(m, f)[[a, b]]
    return ar / max(Aa, 1e-12), ar / max(Ab, 1e-12)


def vis_count(con):
    n = 0
    for idx, ta, tb, sha, shb in con.values():
        ca = zflib.PAL[ta] * sha[:, None]
        cb = zflib.PAL[tb] * shb[:, None]
        n += int(np.count_nonzero(np.abs(ca - cb).max(1) >= 16))
    return n


dec = []
for q in J["pairs"]:
    a, b = q["a"], q["b"]
    r = dict(a=a, b=b, kind=q["kind"], at=q["at"])
    con = zflib.contested(sc, a, b, poses, 1.5 * step)
    r["px"] = sum(len(v[0]) for v in con.values())
    r["vis"] = vis_count(con)
    cova, covb = overlap_cov(a, b, q["pose"])
    shared = len(set(tw[a].tolist()) & set(tw[b].tolist()))
    r.update(cova=round(cova, 3), covb=round(covb, 3), shared=shared, ca=int(csize[comp[a]]), cb=int(csize[comp[b]]),
             samecomp=bool(comp[a] == comp[b]))
    if (a, b) in skip:
        r["act"] = "his"; r["why"] = "skipped by hand"
    elif (a, b) in force:
        k = force[(a, b)]
        r["act"], r["keep"] = k.split("/")[0], int(k.split("/")[1])
        r["why"] = "by hand"
    elif r["vis"] < MINVIS:
        r["act"] = "invisible"
    elif max(cova, covb) >= 0.98 and shared >= 2:
        x, y = (a, b) if cova >= covb else (b, a)
        r["act"], r["why"] = "his", "fold: inside the other and sharing an edge with it, removing it opens a hole"
    elif max(cova, covb) >= 0.95 and shared <= 1:
        x, y = (a, b) if cova >= covb else (b, a)
        r["act"], r["keep"] = "detail", x
    elif shared == 0 and r["vis"] <= MAXQUIET:
        # partial overlap, both intended, the two looks nearly the same: the smaller part's face goes in front
        if r["ca"] != r["cb"]:
            r["keep"] = a if r["ca"] < r["cb"] else b
        else:
            pa, pb = patch(a), patch(b)
            ar = zflib.tri_area(m)
            r["keep"] = a if ar[list(pa)].sum() <= ar[list(pb)].sum() else b
        r["act"] = "glued" if not r["samecomp"] else "edge"
    else:
        r["act"] = "his"
        r["why"] = "partial overlap, both look intended, the choice shows" if shared == 0 else "partial overlap of faces sharing a vertex"
    r["con"] = con
    dec.append(r)

# nudges: group by patch of the kept triangle
moves = {}  # frozenset(patch) -> list of decisions
drops = set()
for r in dec:
    if r["act"] == "fold" or r["act"] == "drop":
        drops.add(r.get("drop", r["b"] if r.get("keep") == r["a"] else r["a"]))
    if r["act"] in ("detail", "glued", "edge", "nudge"):
        k = r["keep"]
        l = r["b"] if k == r["a"] else r["a"]
        pk, pl = patch(k), patch(l)
        wk = set(tw[list(pk)].ravel().tolist())
        wl = set(tw[list(pl)].ravel().tolist())
        if pk & pl or wk & wl:
            r["act"], r["why"] = "his", "the kept face shares vertices with the other face: a nudge would tear it"
            continue
        r["patch"], r["lose"] = sorted(pk), l
        moves.setdefault(frozenset(pk), []).append(r)

# a face can't both move in front and stay behind a face that moves too (both would move: still coplanar)
movedset = set().union(*moves.keys()) if moves else set()
for pk in list(moves):
    rs = moves[pk]
    if any(set(patch(r["lose"])) & movedset for r in rs):
        for r in rs:
            r["act"], r["why"] = "his", "layers conflict: the face behind is itself moved in front of another"
        del moves[pk]
newb = m.bytes.copy()
moved_tris = set()
for pk, rs in moves.items():
    k = rs[0]["keep"]
    ws = set(tw[list(pk)].ravel().tolist())
    vsel = np.isin(W, list(ws))
    shift = np.zeros((P, 3), int)
    ok = True
    for f in range(P):
        n = FN[f, k]
        need = 0.0
        for r in rs:
            h = (TP[f, r["lose"]] - TP[f, k, 0]) @ n
            need = max(need, float(h.max()))
        need += 0.9 * float(m.scale.min())
        best = None
        for s in itertools.product(range(-2, 3), repeat=3):
            d = np.array(s) * m.scale
            if d @ n < need:
                continue
            tang = np.linalg.norm(d - (d @ n) * n)
            key = (round(tang / step, 3), float(d @ d))
            if best is None or key < best[0]:
                best = (key, s)
        if best is None:
            ok = False
            break
        shift[f] = best[1]
        nb = newb[f][vsel].astype(int) + shift[f]
        if nb.min() < 0 or nb.max() > 255:
            ok = False
            break
    if not ok:
        for r in rs:
            r["act"], r["why"] = "his", "no whole-step nudge fits the grid"
        continue
    for f in range(P):
        newb[f][vsel] = (newb[f][vsel].astype(int) + shift[f]).astype(np.uint8)
    moved_tris |= set(np.nonzero(vsel[m.tris].any(1))[0].tolist())
    for r in rs:
        r["shift"] = shift[0].tolist()

m.bytes = newb
keep = np.ones(m.nt, bool)
keep[list(drops)] = False
tri_from = np.nonzero(keep)[0]
m.tris = m.tris[keep]
m.facesfront = m.facesfront[keep]
mdlio.write(m, outp)
for r in dec:
    r.pop("con")
json.dump({"decisions": dec, "tri_from": tri_from.tolist(), "moved_tris": sorted(moved_tris), "dropped": sorted(drops)},
          open(outp + ".zf.json", "w"), default=lambda o: o.item() if hasattr(o, "item") else str(o), indent=0)
from collections import Counter
print("fixzf:", name, dict(Counter(r["act"] for r in dec)), "moved tris", len(moved_tris), "dropped", len(drops))
for r in dec:
    print("  %d:%d %s keep=%s vis=%d px=%d cov=%.2f/%.2f sh=%d comp=%d/%d %s %s" % (
        r["a"], r["b"], r["act"], r.get("keep", "-"), r["vis"], r["px"], r["cova"], r["covb"], r["shared"], r["ca"], r["cb"],
        r.get("shift", ""), r.get("why", "")))
