# rig.py -- a monster's motion clusters (as vr_ragdoll.cpp derives them), to write its seed table (vr_ragdoll.cpp
# seedTables; ROUND21.md, "Ragdolls 3", "Ragdolls 4"). knight_rig.py made general: any of Quake VR's monster models, its
# dropped weapon hidden in its death frames as vr_monstermods.cpp hides it. Needs numpy; the clusters are kept between
# runs in the temp folder (<model>_labels.json):
#   python rig.py ogre [k]              the k (18) motion clusters: each one's vertices, rest-pose middle, box, neighbours
#   (a seed table of more clusters, SeedTable::clusters: run "rig.py <model> <k>" first, its bones.json those clusters)
#   python rig.py ogre bones.json       with the clusters given to bones ({"bones": [[name, parent, [clusters]], ...]}):
#                                       the bones' rms fit, middles and boxes, and each joint's centre (least squares
#                                       between the parent's and the child's motions, pulled a little to their boundary)
#   python rig.py ogre frames           the frame names (the death animations' first and last)
#   RIG_PAK=<qbase>/hipnotic/pak0.pak python rig.py grem ...   a mission pack's monster (not in quakevr/progs): its pak
#   python rig.py ogre draw out.png [pose] [bones]   the last run's clusters (or bones) in colour on the pose (0), seen
#                                       from his right side (x to the right) and from the front (his left to the right)
# A bone's list: its clusters, or "pN" a whole piece of its own (with "wholepieces": true, SeedTable::wholePieces: the
# clusters only the body piece's; the centroid's legs, arms and pincers, meshes of their own moving alike).
# A bone's flags after its capsule: 'tip' (its end its far tip, not its child's pivot), 'boundary' (its pivot the
# boundary's middle with its parent, not the motions' fit).
# A seed's keepHinge (a hinge whose axis the rest pose's sideways lean would turn: the rottweiler's legs) is set by hand.
# The loose pieces (a piece some pose hides, all of it at one point: the dropped weapon) are left out.
import os, sys, json, tempfile
import numpy as np
import mdl

# vr_monstermods.cpp knownSwords: the weapon's vertices, its death frames (None: by name: death*, bdeath*, fdeath*)
KNOWN = {
    'knight': ([334, 335, 336, 363, 364, 365, 524, 535, 536, 537, 538, 539, 540, 557, 558, 559, 560, 561, 562, 563, 564,
                565, 566, 582, 583, 584, 585, 586, 587, 588, 589, 590, 591, 592, 593, 594, 595, 596, 597, 598, 599, 600,
                601, 602, 603, 604, 607, 608, 609, 612, 613, 614, 615, 617, 626, 627, 628, 639, 640, 645, 646], (76, 96)),
    'hknight': ([43, 45, 46, 47, 48, 526, 527, 531, 532] +  # (the blade, and its guard: a piece of its own in his hand)
                [42, 44] + list(range(49, 59)) + list(range(332, 366)) + [525, 528, 529, 530], None),
    'ogre': (list(range(416, 497)), None),
    'soldier': (list(range(463, 549)), (8, 28)),
    'enforcer': ([22, 23, 100] + list(range(400, 431)) + list(range(455, 479)), None),
    'grem': (list(range(85, 123)), (104, 123)),  # (Hipnotic's gremlin: the stolen gun, tucked in his body but in the g* frames)
}

# The models whose every piece but the body is loose (vr_ragdoll.cpp SeedTable::looseWeapons: hidden in the ragdoll):
# the player's axe and gun, each in his hands or on his back by the frame (ROUND21.md, "Player ragdolls").
LOOSE_PIECES = {'player'}

name = sys.argv[1]
mpath = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../quakevr/progs/%s.mdl' % name)
if not os.path.exists(mpath) and os.environ.get('RIG_PAK'):
    # A mission pack's monster (the gremlin's grem, the mummy): read from its pak (RIG_PAK=<...>/hipnotic/pak0.pak)
    import struct
    pak = open(os.environ['RIG_PAK'], 'rb').read()
    _, off, ln = struct.unpack_from('<4sii', pak, 0)
    for i in range(ln // 64):
        fn, o, l = struct.unpack_from('<56sii', pak, off + i * 64)
        if fn.split(b'\0')[0].decode() == 'progs/%s.mdl' % name:
            mpath = pak[o:o + l]
names, P, T = mdl.load(mpath)
P = P.astype(np.float64)
nf, nv, _ = P.shape
if len(sys.argv) > 2 and sys.argv[2] == 'frames':
    print(' '.join('%d:%s' % (i, x) for i, x in enumerate(names)))
    sys.exit()

# vr_monstermods.cpp: the weapon's own vertices collapsed onto the hilt (the vertices it shares) in the death frames
if name in KNOWN:
    verts, deaths = KNOWN[name]
    vin = set(verts)
    wtri = [all(v in vin for v in t) for t in T]
    inW, inRest = set(), set()
    for i, t in enumerate(T):
        (inW if wtri[i] else inRest).update(int(v) for v in t)
    own = sorted(inW - inRest)
    shared = sorted(inW & inRest)
    anchor = shared or own
    for f in range(nf):
        death = deaths[0] <= f <= deaths[1] if deaths else names[f].startswith(('death', 'bdeath', 'fdeath'))
        if death:
            P[f, own] = P[f, anchor].mean(0)  # (onto the anchor's middle)
    print('weapon: %d own vertices, %d shared (hidden in its death frames)' % (len(own), len(shared)))

# welded representatives (the same place in every frame), pieces, the loose ones (vr_ragdoll.cpp derive)
key, rep = {}, list(range(nv))
for v in range(nv):
    k = P[:, v, :].tobytes()
    if k in key:
        rep[v] = key[k]
    else:
        key[k] = v
adj = {v: set() for v in range(nv)}
for t in T:
    if len(set(int(v) for v in t)) < 3:
        continue  # (a triangle with a repeated corner draws nothing and joins nothing: the ogre's chainsaw to his hand)
    c = [rep[v] for v in t]
    for k in range(3):
        a, b = c[k], c[(k + 1) % 3]
        if a != b:
            adj[a].add(b); adj[b].add(a)
allReps = [v for v in range(nv) if rep[v] == v]
piece = {}
for v in allReps:
    if v in piece:
        continue
    pid = len(set(piece.values()))
    st = [v]; piece[v] = pid
    while st:
        u = st.pop()
        for w in adj[u]:
            if w not in piece:
                piece[w] = pid; st.append(w)
sizes = {}
for v, p in piece.items():
    sizes[p] = sizes.get(p, 0) + 1
body = max(sizes, key=sizes.get)
bodyR = [v for v in allReps if piece[v] == body]
loose = set()
for p, n in sizes.items():
    if p == body or n < 4:
        continue
    pv = [v for v in allReps if piece[v] == p]
    gap = max(np.sqrt(((P[f, pv][:, None, :] - P[f, bodyR][None, :, :]) ** 2).sum(2).min()) for f in range(0, nf, 2))
    hidden = any((P[f, pv].max(0) - P[f, pv].min(0)).max() < 0.5 for f in range(nf))
    hidden = hidden or name in LOOSE_PIECES  # (the player's axe and gun: hidden in his ragdoll, SeedTable::looseWeapons)
    print('piece %d: %d places, gap %.1f, %s' % (p, n, gap, 'loose (hidden)' if hidden else 'body'))
    if hidden:
        loose.update(pv)
reps = [v for v in allReps if v not in loose]
X = P[:, reps, :]  # [pose, rep, 3]
R = len(reps)
idx = {v: i for i, v in enumerate(reps)}
print('%d vertices, %d frames, %d places (%d loose)' % (nv, nf, len(allReps), len(loose)))


def kabsch(a, b):
    ca, cb = a.mean(0), b.mean(0)
    H = (a - ca).T @ (b - cb)
    U, S, Vt = np.linalg.svd(H)
    d = np.sign(np.linalg.det(Vt.T @ U.T))
    Rm = Vt.T @ np.diag([1, 1, d]) @ U.T
    return Rm, cb - Rm @ ca


def fit(label, k):
    tr = {}
    for l in range(k):
        m = label == l
        if m.sum() >= 3:
            tr[l] = [kabsch(X[0, m], X[p, m]) for p in range(nf)]
    return tr


def errs(tr, l):
    # every representative's error under cluster l's motion
    if l not in tr:
        return np.full(R, 1e30)
    e = np.zeros(R)
    for p in range(nf):
        Rm, t = tr[l][p]
        d = X[0] @ Rm.T + t - X[p]
        e += (d * d).sum(1)
    return e


def refine(label, k, its):
    for _ in range(its):
        tr = fit(label, k)
        E = np.stack([errs(tr, l) for l in range(k)], 1)
        changed = 0
        new = label.copy()
        for i, v in enumerate(reps):
            best = label[i]
            for u in adj[v]:
                if u in idx:
                    l = label[idx[u]]
                    if E[i, l] < E[i, best]:
                        best = l
            if best != label[i]:
                new[i] = best; changed += 1
        label = new
        if not changed:
            break
    return label


def draw(lab, path, pose):
    from PIL import Image, ImageDraw
    pal = [(230, 25, 75), (60, 180, 75), (255, 225, 25), (0, 130, 200), (245, 130, 48), (145, 30, 180), (70, 240, 240),
           (240, 50, 230), (210, 245, 60), (250, 190, 212), (0, 128, 128), (220, 190, 255), (170, 110, 40), (255, 250, 200),
           (128, 0, 0), (170, 255, 195), (128, 128, 0), (255, 215, 180), (0, 0, 128), (128, 128, 128)]
    V = P[pose]
    lv = np.array([lab[idx[rep[v]]] if rep[v] in idx else -1 for v in range(nv)])
    lo, hi = V.min(0) - 2, V.max(0) + 2
    s = 8.0
    W = int((hi[0] - lo[0]) * s + (hi[1] - lo[1]) * s) + 20
    H = int((hi[2] - lo[2]) * s) + 10
    img = Image.new('RGB', (W, H), (255, 255, 255))
    d = ImageDraw.Draw(img)
    views = [(lambda p: ((p[0] - lo[0]) * s, (hi[2] - p[2]) * s), lambda p: p[1]),
             (lambda p: ((hi[0] - lo[0]) * s + 20 + (hi[1] - p[1]) * s, (hi[2] - p[2]) * s), lambda p: -p[0])]
    for proj, depth in views:  # side: from his right (-y), nearer is lower y; front: from +x
        order = sorted(range(len(T)), key=lambda i: -np.mean([depth(V[v]) for v in T[i]]))
        for i in order:
            t = T[i]
            l = np.bincount([lv[v] + 1 for v in t]).argmax() - 1
            c = pal[l % len(pal)] if l >= 0 else (200, 200, 200)
            d.polygon([proj(V[v]) for v in t], fill=c, outline=tuple(int(x * 0.6) for x in c))
        for l in sorted(set(lv) - {-1}):
            m = lv == l
            x, y = proj(V[m].mean(0))
            d.text((x - 4, y - 5), str(l), fill=(0, 0, 0))
    img.save(path)
    print('drawn', path, img.size)


traj = X.transpose(1, 0, 2).reshape(R, -1)
lpath = os.path.join(tempfile.gettempdir(), '%s_labels.json' % name)
if len(sys.argv) > 3 and sys.argv[2] == 'draw':
    bones = len(sys.argv) > 5
    lab = json.load(open(lpath.replace('_labels', '_bones') if bones else lpath))
    draw(lab, sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 0)
elif len(sys.argv) < 3 or sys.argv[2].isdigit():
    k = int(sys.argv[2]) if len(sys.argv) > 2 else 18
    centres = [0]
    dmin = np.full(R, 1e30)
    while len(centres) < k:
        d = ((traj - traj[centres[-1]]) ** 2).sum(1)
        dmin = np.minimum(dmin, d)
        centres.append(int(dmin.argmax()))
    D = np.stack([((traj - traj[c]) ** 2).sum(1) for c in centres], 1)
    label = refine(D.argmin(1), k, 30)
    tr = fit(label, k)
    E = np.stack([errs(tr, l) for l in range(k)], 1)
    print('clusters rms %.2f' % np.sqrt(E[np.arange(R), label].sum() / (R * nf)))
    for l in range(k):
        m = label == l
        if m.sum() == 0:
            continue
        c = X[0, m].mean(0)
        nb = sorted({int(label[idx[u]]) for i in np.where(m)[0] for u in adj[reps[i]] if u in idx} - {l})
        print('cluster %2d: %3d verts, rest centre %6.1f %6.1f %6.1f, lo %s hi %s, neighbours %s' % (
            l, m.sum(), *c, X[0, m].min(0).round(1), X[0, m].max(0).round(1), nb))
    json.dump([int(x) for x in label], open(lpath, 'w'))
else:
    a = json.load(open(sys.argv[2]))
    lab0 = np.array(json.load(open(lpath)))
    bones = a['bones']  # [[name, parent, [clusters]]]
    label = np.full(R, -1)
    # "wholepieces" (SeedTable::wholePieces): the clusters only the body's piece's; every other piece one bone's, named
    # "pN" (rig.py's piece N) in its list (the centroid's legs, arms and pincers: meshes of their own moving alike)
    whole = a.get('wholepieces', False)
    pieceOf = np.array([piece[v] for v in reps])
    for b, bone in enumerate(bones):
        for c in bone[2]:
            if isinstance(c, str):
                label[pieceOf == int(c[1:])] = b
            else:
                label[(lab0 == c) & ((pieceOf == piece[bodyR[0]]) if whole else True)] = b
    label = refine(label, len(bones), 40)
    json.dump([int(x) for x in label], open(lpath.replace('_labels', '_bones'), 'w'))
    tr = fit(label, len(bones))
    E = np.stack([errs(tr, l) for l in range(len(bones))], 1)
    print('bones rms %.2f' % np.sqrt(E[np.arange(R), label].sum() / (R * nf)))
    centre, pivot = {}, {}
    for b, bone in enumerate(bones):
        nm, par = bone[0], bone[1]
        m = label == b
        c = centre[b] = X[0, m].mean(0)
        pivot[b] = c
        line = '%-11s rms %.2f %3d verts centre %6.1f %6.1f %6.1f lo %s hi %s' % (nm, np.sqrt(E[m, b].sum() / max(m.sum() * nf, 1)), m.sum(), *c, X[0, m].min(0).round(1), X[0, m].max(0).round(1))
        if par >= 0 and b in tr and par in tr:
            A, y = [], []
            for p in range(nf):
                Ra, ta = tr[par][p]; Rb, tb = tr[b][p]
                A.append(Ra - Rb); y.append(tb - ta)
            A = np.concatenate(A); y = np.concatenate(y)
            bnd = [X[0, i] for i in np.where(m)[0] for u in adj[reps[i]] if u in idx and label[idx[u]] == par]
            pb = np.mean(bnd, 0) if bnd else c
            lam = 0.5
            piv = pivot[b] = np.linalg.lstsq(np.concatenate([A, lam * np.eye(3)]), np.concatenate([y, lam * pb]), rcond=None)[0]
            if 'boundary' in bone[5:]:
                pivot[b] = pb  # (its pivot the boundary's middle: the fit's was off it, the motions too alike: the fiend's ankles)
            line += ' pivot %6.1f %6.1f %6.1f (boundary %6.1f %6.1f %6.1f)' % (*piv, *pb)
        print(line)
    # The seed table (vr_ragdoll.cpp Seed): a bone's 4th entry its joint ("root", ["ball", cone, twist], ["hinge", flex,
    # x, y, z]), its 5th a capsule's radius. Its end: its first child's pivot, else its pivot mirrored through its middle.
    # The engine gives each cluster to the bone whose seed centre is nearest its middle: check it gets the json's.
    for b, bone in enumerate(bones):
        for cl in bone[2]:
            if isinstance(cl, str):
                mid = X[0, pieceOf == int(cl[1:])].mean(0)
            else:
                sel = (lab0 == cl) & ((pieceOf == piece[bodyR[0]]) if whole else True)
                if not sel.any():
                    print('WARNING: cluster %d (%s) has no vertices%s' % (cl, bone[0], ' in the body piece' if whole else ''))
                    continue
                mid = X[0, sel].mean(0)
            near = min(range(len(bones)), key=lambda k: ((centre[k] - mid) ** 2).sum())
            if near != b:
                print('WARNING: cluster %s (%s) is nearer the centre of %s' % (cl, bone[0], bones[near][0]))
    f = lambda v: '{%s}' % ', '.join(('%.1ff' % x).replace('-0.0f', '0.f').replace('.0f', '.f') for x in v)
    print('constexpr Seed %sSeeds[] = {' % name)
    for b, bone in enumerate(bones):
        nm, par, joint = bone[0], bone[1], bone[3] if len(bone) > 3 else ['ball', 45, 30]
        cap = bone[4] if len(bone) > 4 else 0
        kids = [k for k, o in enumerate(bones) if o[1] == b]
        tip = 'tip' in bone[5:]  # (its end its far tip, not its child's pivot: the dog's head, his jaw)
        end = pivot[kids[0]] if kids and not tip else 2 * centre[b] - pivot[b]
        piv = pivot[b] if par >= 0 else centre[b]
        low = X[0, label == b][:, 2].min()
        if cap > 0 and not kids and end[2] - cap < low and piv[2] - end[2] > 1e-3:
            end = piv + (end - piv) * min(1, (piv[2] - (low + cap)) / (piv[2] - end[2]))  # (its capsule not below its foot)
        if joint == 'root':
            j = 'Joint::Root, %s, %s, %s, %s, 0.f, 0.f, 0.f, {}' % (f(centre[b]), f(piv), f(end), '%.1ff' % cap)
        elif joint[0] == 'ball':
            j = 'Joint::Ball, %s, %s, %s, %.1ff, %.1ff, %.1ff, 0.f, {}' % (f(centre[b]), f(piv), f(end), cap, joint[1], joint[2])
        else:
            j = 'Joint::Hinge, %s, %s, %s, %.1ff, 0.f, 0.f, %.1ff, %s' % (f(centre[b]), f(piv), f(end), cap, joint[1], f(joint[2:5]))
        print('    {"%s", %d, %s},' % (nm, par, j.replace('.0f', '.f')))
    print('};')
