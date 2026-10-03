# knight_rig.py -- the knight's motion clusters (as vr_ragdoll.cpp derives them), to write its seed table (vr_ragdoll.cpp
# knightSeeds; ROUND21.md, "Ragdolls 3"). Needs numpy; the clusters are kept between runs in the temp folder
# (knight_labels.json):
#   python knight_rig.py               the 18 motion clusters: each one's vertices, rest-pose middle, box and neighbours
#   python knight_rig.py knight_bones_assign.json    with the clusters given to bones ({"bones": [[name, parent, [clusters]], ...]}):
#                                      the bones' rms fit, middles and boxes, and each joint's centre (least squares
#                                      between the parent's and the child's motions, pulled a little to their boundary)
# The sword's vertices (vr_monstermods.cpp knownSwords) are left out: it is a loose bone, hidden as he dies.
import os, sys, json, tempfile
import numpy as np
import mdl

SWORD = [334, 335, 336, 363, 364, 365, 524, 535, 536, 537, 538, 539, 540, 557, 558, 559, 560, 561, 562, 563, 564, 565,
         566, 582, 583, 584, 585, 586, 587, 588, 589, 590, 591, 592, 593, 594, 595, 596, 597, 598, 599, 600, 601, 602,
         603, 604, 607, 608, 609, 612, 613, 614, 615, 617, 626, 627, 628, 639, 640, 645, 646]
names, P, T = mdl.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../quakevr/progs/knight.mdl'))
nf, nv, _ = P.shape
# the sword's own vertices (not shared with other triangles: monstermods' rule)
sword = set(SWORD)
swordTri = [all(v in sword for v in t) for t in T]
own = set(SWORD)
for i, t in enumerate(T):
    if not swordTri[i]:
        for v in t:
            own.discard(v)
# welded representatives
key = {}
rep = list(range(nv))
for v in range(nv):
    k = P[:, v, :].tobytes()
    if k in key:
        rep[v] = key[k]
    else:
        key[k] = v
reps = [v for v in range(nv) if rep[v] == v and v not in own]
adj = {v: set() for v in range(nv)}
for t in T:
    c = [rep[v] for v in t]
    for k in range(3):
        a, b = c[k], c[(k + 1) % 3]
        if a != b:
            adj[a].add(b); adj[b].add(a)
X = P[:, reps, :]  # [pose, rep, 3]
R = len(reps)
idx = {v: i for i, v in enumerate(reps)}


def kabsch(a, b):
    ca, cb = a.mean(0), b.mean(0)
    H = (a - ca).T @ (b - cb)
    U, S, Vt = np.linalg.svd(H)
    d = np.sign(np.linalg.det(Vt.T @ U.T))
    D = np.diag([1, 1, d])
    Rm = Vt.T @ D @ U.T
    return Rm, cb - Rm @ ca


def fit(label, k):
    tr = {}
    for l in range(k):
        m = label == l
        if m.sum() < 3:
            continue
        tr[l] = [kabsch(X[0, m], X[p, m]) for p in range(nf)]
    return tr


def err(tr, l, i):
    if l not in tr:
        return 1e30
    e = 0
    for p in range(nf):
        Rm, t = tr[l][p]
        d = Rm @ X[0, i] + t - X[p, i]
        e += d @ d
    return e


def refine(label, k, its):
    for _ in range(its):
        tr = fit(label, k)
        changed = 0
        for i, v in enumerate(reps):
            best, be = label[i], err(tr, label[i], i)
            for u in adj[v]:
                if u in idx:
                    l = label[idx[u]]
                    if l != best:
                        e = err(tr, l, i)
                        if e < be:
                            best, be = l, e
            if best != label[i]:
                label[i] = best; changed += 1
        if not changed:
            break
    return label


traj = X.transpose(1, 0, 2).reshape(R, -1)
if len(sys.argv) < 2:
    k = 18
    centres = [0]
    dmin = np.full(R, 1e30)
    while len(centres) < k:
        d = ((traj - traj[centres[-1]]) ** 2).sum(1)
        dmin = np.minimum(dmin, d)
        centres.append(int(dmin.argmax()))
    D = np.stack([((traj - traj[c]) ** 2).sum(1) for c in centres], 1)
    label = D.argmin(1)
    label = refine(label, k, 15)
    for l in range(k):
        m = label == l
        if m.sum() == 0:
            continue
        c = X[0, m].mean(0)
        nb = sorted({int(label[idx[u]]) for i in np.where(m)[0] for u in adj[reps[i]] if u in idx} - {l})
        print('cluster %2d: %3d verts, rest centre %6.1f %6.1f %6.1f, lo %s hi %s, neighbours %s' % (
            l, m.sum(), *c, X[0, m].min(0).round(1), X[0, m].max(0).round(1), nb))
    json.dump([int(x) for x in label], open(os.path.join(tempfile.gettempdir(), 'knight_labels.json'), 'w'))
else:
    # assign: {"bone": [clusters...]} in bone order; joints by least squares between parent and child
    a = json.load(open(sys.argv[1]))
    lab0 = np.array(json.load(open(os.path.join(tempfile.gettempdir(), 'knight_labels.json'))))
    bones = a['bones']  # [[name, parent, [clusters]]]
    label = np.full(R, -1)
    for b, (nm, par, cl) in enumerate(bones):
        for c in cl:
            label[lab0 == c] = b
    label = refine(label, len(bones), 20)
    tr = fit(label, len(bones))
    rms = np.sqrt(sum(err(tr, label[i], i) for i in range(R)) / (R * nf))
    print('bones rms %.2f' % rms)
    for b, (nm, par, cl) in enumerate(bones):
        m = label == b
        c = X[0, m].mean(0)
        line = '%-11s %3d verts centre %6.1f %6.1f %6.1f lo %s hi %s' % (nm, m.sum(), *c, X[0, m].min(0).round(1), X[0, m].max(0).round(1))
        if par >= 0:
            # joint: p minimizing sum |Ra p + ta - Rb p - tb|^2 over poses (rest is identity for both)
            A, y = [], []
            for p in range(nf):
                Ra, ta = tr[par][p]; Rb, tb = tr[b][p]
                A.append(Ra - Rb); y.append(tb - ta)
            A = np.concatenate(A); y = np.concatenate(y)
            # regularize towards the boundary between them
            bnd = [X[0, i] for i in np.where(m)[0] for u in adj[reps[i]] if u in idx and label[idx[u]] == par]
            pb = np.mean(bnd, 0) if bnd else c
            lam = 0.5
            A2 = np.concatenate([A, lam * np.eye(3)]); y2 = np.concatenate([y, lam * pb])
            piv = np.linalg.lstsq(A2, y2, rcond=None)[0]
            line += ' pivot %6.1f %6.1f %6.1f (boundary %6.1f %6.1f %6.1f)' % (*piv, *pb)
        print(line)
