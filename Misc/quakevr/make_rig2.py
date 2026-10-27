#!/usr/bin/env python3
# make_rig2.py -- the authored grunt rig ("rig2": docs/vr-port/ROUND22.md, "Grunt rig rebuild, stage 1"): a humanoid
# skeleton with anatomical joints, joint limits and blended skin weights (up to 4 bones a vertex), written as
# quakevr/rigs/<model>.rig2.txt for the engine (vr_rig2.cpp: vr_rig2 1, the Rig Studio's Range of Motion window).
#
# Usage: python Misc/quakevr/make_rig2.py [--preview out.png] [--quiet]
#        (writes quakevr/rigs/vrgrunt.rig2.txt and quakevr/rigs/soldier.rig2.txt: the two share the body, vertex for
#        vertex; soldier.mdl's gun is its `weapon` bone on the right hand)
#
# The method (all from the rest frame, frame 0, and the choices in SKELETON below):
# 1. Joints. Each bone's pivot and end are authored (model units, x forward, y left, z up, read off the mesh's
#    cross-sections), then each limb joint (`refine`) is moved to the middle of the mesh's cross-section through it,
#    the plane across the limb there (the bisector of its two bones), the loop of that section nearest the authored
#    point: the joint sits in the middle of the limb, wherever the authored point was.
# 2. Regions. Each vertex goes to the nearest bone segment (pivot..end) it can see from inside the mesh (the line from
#    the vertex to the segment's nearest point crosses no triangle): the arm's inside does not go to the chest beside it.
#    Then each vertex whose neighbours mostly lie in another region follows them (stray vertices), twice.
# 3. Blends. At each joint, the vertices of the parent's and the child's regions within its radius are blended by
#    where they lie along the limb (the bisector): the child's weight a smoothstep from 0 at `blend` units on the
#    parent's side to 1 at `blend` units on the child's; outside the band, rigid. The width is the joint's choice
#    (wide at the spine and the shoulders, narrow at the wrists).
# 4. Smoothing. Two passes of averaging with the neighbours (inverse edge length), each vertex half its own: the seams
#    between regions that are not parent and child (the chest beside the upper arm) blend over a ring.
# 5. Welds. The vertices at the same place in every frame (the skin's seam copies) take one set of weights (their
#    average): the surface cannot open along a seam.
# 6. At most 4 bones a vertex, none under 0.03, summing to 1 (3 decimals).
#
# Limits (degrees): a ball joint's swing is an elliptical cone in (swing x: toward `fwd`, swing y: toward `side`), each
# quadrant its own radius, measured from its neutral direction (`neutral`: the shoulders' and hips' arm or leg hanging
# straight down; the others' the rest pose's own direction); twist about the bone, from the rest pose's. A hinge's angle
# is its bend from straight (0) about `axis`, the rest pose's own bend measured.

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import check_mdl_holes as cmh  # noqa: E402
from rig2png import Canvas  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
MODELS = {'vrgrunt': 'quakevr/progs/vrgrunt.mdl', 'soldier': 'quakevr/progs/soldier.mdl'}
GUN = list(range(463, 549))  # soldier.mdl's gun (make_vrgrunt.py's list): the `weapon` bone, fixed on the right hand

# name, parent, joint, pivot, end, refine, blend (units), radius (units), limits
#   ball:  neutral (None: the rest direction), fwd, side, swing x (min, max), swing y (min, max), twist (min, max)
#   hinge: bend direction at rest (None: the rest's own bend), range (min, max)
SKELETON = [
    dict(name='pelvis', parent=None, joint='root', pivot=(-4.8, 0.0, -2.0), end=(-4.0, 0.0, 2.5)),
    dict(name='spine', parent='pelvis', joint='ball', pivot=(-4.0, 0.0, 2.5), end=(-2.6, 0.0, 9.0), blend=3.0,
         radius=12.0, fwd=(1, 0, 0), side=(0, 1, 0), sx=(-20, 40), sy=(-25, 25), tw=(-25, 25)),
    dict(name='chest', parent='spine', joint='ball', pivot=(-2.6, 0.0, 9.0), end=(0.4, 0.2, 16.6), blend=3.0,
         radius=12.0, fwd=(1, 0, 0), side=(0, 1, 0), sx=(-15, 25), sy=(-15, 15), tw=(-20, 20)),
    dict(name='neck', parent='chest', joint='ball', pivot=(0.4, 0.2, 16.6), end=(3.0, 0.3, 19.2), blend=1.2,
         radius=6.0, fwd=(1, 0, 0), side=(0, 1, 0), sx=(-25, 35), sy=(-25, 25), tw=(-35, 35)),
    dict(name='head', parent='neck', joint='ball', pivot=(3.0, 0.3, 19.2), end=(4.7, 0.4, 26.0), blend=1.0,
         radius=6.0, fwd=(1, 0, 0), side=(0, 1, 0), sx=(-25, 25), sy=(-20, 20), tw=(-30, 30)),
    dict(name='clavicle_l', parent='chest', joint='ball', pivot=(0.5, 2.0, 15.0), end=(0.9, 9.0, 15.2), blend=2.0,
         radius=6.0, fwd=(1, 0, 0), side=(0, 0, 1), sx=(-15, 20), sy=(-10, 25), tw=(-10, 10)),
    dict(name='upperarm_l', parent='clavicle_l', joint='ball', pivot=(0.9, 9.0, 15.2), end=(2.2, 10.0, 6.0),
         blend=3.5, radius=7.5, neutral=(0, 0.17, -1), fwd=(1, 0, 0), side=(0, 1, 0), sx=(-45, 135),
         sy=(-20, 110), tw=(-70, 70)),
    dict(name='forearm_l', parent='upperarm_l', joint='hinge', pivot=(2.2, 10.0, 6.0), end=(9.6, 6.6, 2.7),
         refine=True, blend=1.6, radius=4.5, range=(0, 145)),
    dict(name='hand_l', parent='forearm_l', joint='ball', pivot=(9.6, 6.6, 2.7), end=(13.4, 6.0, 1.8), refine=True,
         blend=0.9, radius=3.5, fwd=(0, 0, 1), side=(0, -1, 0), sx=(-60, 60), sy=(-25, 25), tw=(-60, 60)),
    dict(name='clavicle_r', parent='chest', joint='ball', pivot=(0.5, -1.6, 15.0), end=(-0.3, -9.4, 15.2),
         blend=2.0, radius=6.0, fwd=(1, 0, 0), side=(0, 0, 1), sx=(-15, 20), sy=(-10, 25), tw=(-10, 10)),
    dict(name='upperarm_r', parent='clavicle_r', joint='ball', pivot=(-0.3, -9.4, 15.2), end=(-0.2, -10.4, 6.4),
         blend=3.5, radius=7.5, neutral=(0, -0.17, -1), fwd=(1, 0, 0), side=(0, -1, 0), sx=(-45, 135),
         sy=(-20, 110), tw=(-70, 70)),
    dict(name='forearm_r', parent='upperarm_r', joint='hinge', pivot=(-0.2, -10.4, 6.4), end=(5.6, -6.0, 4.4),
         refine=True, blend=1.6, radius=4.5, range=(0, 145)),
    dict(name='hand_r', parent='forearm_r', joint='ball', pivot=(5.6, -6.0, 4.4), end=(8.6, -4.3, 4.2), refine=True,
         blend=0.9, radius=3.5, fwd=(0, 0, 1), side=(0, 1, 0), sx=(-60, 60), sy=(-25, 25), tw=(-60, 60)),
    dict(name='thigh_l', parent='pelvis', joint='ball', pivot=(-5.6, 4.5, -3.0), end=(-4.8, 7.0, -13.0), blend=2.6,
         radius=8.0, neutral=(0, 0.08, -1), fwd=(1, 0, 0), side=(0, 1, 0), sx=(-25, 115), sy=(-20, 45), tw=(-35, 35)),
    dict(name='shin_l', parent='thigh_l', joint='hinge', pivot=(-4.8, 7.0, -13.0), end=(-11.5, 6.8, -20.6),
         refine=True, blend=1.8, radius=5.0, range=(0, 145)),
    dict(name='foot_l', parent='shin_l', joint='ball', pivot=(-11.5, 6.8, -20.6), end=(-4.6, 7.6, -23.4),
         refine=True, blend=1.0, radius=4.0, fwd=(0, 0, 1), side=(0, 1, 0), sx=(-30, 40), sy=(-15, 15),
         tw=(-12, 12)),
    dict(name='thigh_r', parent='pelvis', joint='ball', pivot=(-4.2, -4.6, -3.0), end=(2.6, -6.6, -11.0), blend=2.6,
         radius=8.0, neutral=(0, -0.08, -1), fwd=(1, 0, 0), side=(0, -1, 0), sx=(-25, 115), sy=(-20, 45),
         tw=(-35, 35)),
    dict(name='shin_r', parent='thigh_r', joint='hinge', pivot=(2.6, -6.6, -11.0), end=(0.0, -6.8, -20.6),
         refine=True, blend=1.8, radius=5.0, range=(0, 145)),
    dict(name='foot_r', parent='shin_r', joint='ball', pivot=(0.0, -6.8, -20.6), end=(7.4, -7.3, -23.4),
         refine=True, blend=1.0, radius=4.0, fwd=(0, 0, 1), side=(0, -1, 0), sx=(-30, 40), sy=(-15, 15),
         tw=(-12, 12)),
    dict(name='weapon', parent='hand_r', joint='fixed', pivot=(8.6, -4.3, 4.2), end=(16.0, -2.0, 4.0)),
]

# Each bone's thickness (units: about its limb's radius there), for the regions: a vertex goes to the nearest capsule
SMOOTH_ALPHA = 0.6  # the smoothing's pull to the joints' blends (smaller: wider seams)
SMOOTH_ITERS = 30
THICK = dict(pelvis=7.0, spine=7.0, chest=6.5, neck=3.5, head=5.0, clavicle=3.0, upperarm=3.0, forearm=2.3, hand=1.8,
             thigh=3.6, shin=2.8, foot=2.4)


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))


def norm(a):
    n = length(a)
    return mul(a, 1.0 / n) if n > 1e-9 else (0.0, 0.0, 0.0)


def smoothstep(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def closest_on_segment(p, a, b):
    ab = sub(b, a)
    t = dot(sub(p, a), ab) / max(dot(ab, ab), 1e-9)
    return add(a, mul(ab, min(max(t, 0.0), 1.0)))


def seg_hits_tri(o, e, a, b, c):
    """Whether the segment o..e crosses triangle abc (Moller-Trumbore), away from its ends."""
    d = sub(e, o)
    e1, e2 = sub(b, a), sub(c, a)
    p = cross(d, e2)
    det = dot(e1, p)
    if abs(det) < 1e-12:
        return False
    inv = 1.0 / det
    s = sub(o, a)
    u = dot(s, p) * inv
    if u < 0 or u > 1:
        return False
    q = cross(s, e1)
    v = dot(d, q) * inv
    if v < 0 or u + v > 1:
        return False
    t = dot(e2, q) * inv
    return 0.02 < t < 0.98


class Mesh:
    def __init__(self, path):
        tris, frames = cmh.read(path)
        self.tris = [t[1:] for t in tris]
        self.rest = frames[0]
        self.nv = len(self.rest)
        topo = cmh.Topology(tris, frames)
        self.weld = topo.weld
        self.used = sorted({v for t in self.tris for v in t})
        nb = {v: {} for v in range(self.nv)}
        for t in self.tris:
            for k in range(3):
                a, b = t[k], t[(k + 1) % 3]
                d = max(length(sub(self.rest[a], self.rest[b])), 0.05)
                nb[a][b] = 1.0 / d
                nb[b][a] = 1.0 / d
        self.nbrs = nb
        # (welded copies are neighbours: the smoothing crosses the seams)
        groups = {}
        for v, w in enumerate(self.weld):
            groups.setdefault(w, []).append(v)
        self.groups = [g for g in groups.values() if len(g) > 1]
        for g in self.groups:
            for a in g:
                for b in g:
                    if a != b:
                        nb[a][b] = 1.0

    def section_centre(self, point, normal):
        """The centroid of the loop of the mesh's section by the plane (point, normal) nearest `point`."""
        segs = []
        for t in self.tris:
            p = [self.rest[i] for i in t]
            cut = []
            for k in range(3):
                a, b = p[k], p[(k + 1) % 3]
                da, db = dot(sub(a, point), normal), dot(sub(b, point), normal)
                if (da < 0) != (db < 0):
                    s = da / (da - db)
                    cut.append(add(a, mul(sub(b, a), s)))
            if len(cut) == 2:
                segs.append(cut)
        if not segs:
            return point
        # Group the segments into loops by shared end points (rounded)
        key = lambda q: tuple(round(c, 3) for c in q)
        parent = {}

        def find(x):
            while parent.setdefault(x, x) != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x
        for a, b in segs:
            parent[find(key(a))] = find(key(b))
        loops = {}
        for a, b in segs:
            loops.setdefault(find(key(a)), []).append((a, b))
        # the plane's axes (for the containment test)
        ax = norm(cross(normal, (0, 0, 1) if abs(normal[2]) < 0.9 else (1, 0, 0)))
        ay = cross(normal, ax)

        def inside(segl):
            px, py = dot(point, ax), dot(point, ay)
            odd = False
            for a, b in segl:
                ax_, ay_, bx_, by_ = dot(a, ax), dot(a, ay), dot(b, ax), dot(b, ay)
                if (ay_ > py) != (by_ > py) and px < ax_ + (py - ay_) * (bx_ - ax_) / (by_ - ay_):
                    odd = not odd
            return odd
        best, bestd = None, 1e30
        for segl in loops.values():
            if not inside(segl):
                continue
            # the area centroid of the loop's polygon (a fan from its mean point, in the plane)
            m = (0.0, 0.0, 0.0)
            for a, b in segl:
                m = add(m, add(a, b))
            m = mul(m, 0.5 / len(segl))
            area, cen = 0.0, (0.0, 0.0, 0.0)
            for a, b in segl:
                ar = abs(dot(cross(sub(a, m), sub(b, m)), normal)) * 0.5
                area += ar
                cen = add(cen, mul(add(add(a, b), m), ar / 3.0))
            c = mul(cen, 1.0 / area) if area > 1e-6 else m
            # the smallest loop round the authored point (a limb's, not the body's round it)
            if area < bestd:
                best, bestd = c, area
        return best if best is not None else point


def build(mesh, quiet=False):
    bones = [dict(b) for b in SKELETON]
    index = {b['name']: i for i, b in enumerate(bones)}
    for b in bones:
        b['pidx'] = index[b['parent']] if b['parent'] else -1
    # 1. joints refined to their sections' middles
    for b in bones:
        if not b.get('refine'):
            continue
        p = bones[b['pidx']]
        pd = norm(sub(b['pivot'], p['pivot']))
        cd = norm(sub(b['end'], b['pivot']))
        n = norm(add(pd, cd))
        c = mesh.section_centre(b['pivot'], n)
        if not quiet:
            print('  %-11s pivot %s -> %s (%.2f)' % (b['name'], fmt(b['pivot']), fmt(c), length(sub(c, b['pivot']))))
        b['pivot'] = c
        p['end'] = c if p['joint'] != 'root' else p['end']
    # ends of the bones that end at a child's pivot follow it (a chain's)
    for b in bones:
        kids = [k for k in bones if k['pidx'] == index[b['name']] and k['joint'] != 'fixed']
        if len(kids) == 1 and b['joint'] != 'root':
            b['end'] = kids[0]['pivot']
    # 2. regions
    segbones = [i for i, b in enumerate(bones) if b['joint'] != 'fixed']
    region = [-1] * mesh.nv
    tri_pos = [tuple(mesh.rest[i] for i in t) for t in mesh.tris]
    for v in mesh.used:
        p = mesh.rest[v]
        # (the nearest capsule's surface: a bone's segment less its limb's thickness, so the wide torso's sides are its own)
        cands = sorted(segbones, key=lambda i: length(sub(closest_on_segment(p, bones[i]['pivot'], bones[i]['end']), p)) -
                       THICK.get(bones[i]['name'].rstrip('_lr').rstrip('_'), 2.5))
        region[v] = cands[0]
        def sees(q):
            lo = [min(p[k], q[k]) - 0.01 for k in range(3)]
            hi = [max(p[k], q[k]) + 0.01 for k in range(3)]
            for t, tp in zip(mesh.tris, tri_pos):
                if mesh.weld[v] in (mesh.weld[t[0]], mesh.weld[t[1]], mesh.weld[t[2]]):
                    continue
                if any(min(c[k] for c in tp) > hi[k] or max(c[k] for c in tp) < lo[k] for k in range(3)):
                    continue
                if seg_hits_tri(p, q, *tp):
                    return False
            return True
        for i in cands[:4]:
            # (seen from the nearest point of the bone or points either side of it: a vertex in a hollow, between a
            # thumb and a fist, still sees its own bone)
            a, b = bones[i]['pivot'], bones[i]['end']
            ab = sub(b, a)
            t0 = dot(sub(p, a), ab) / max(dot(ab, ab), 1e-9)
            if any(sees(add(a, mul(ab, min(max(t0 + dt, 0.0), 1.0)))) for dt in (0.0, -0.2, 0.2, -0.4, 0.4)):
                region[v] = i
                break
    for _ in range(2):
        new = list(region)
        for v in mesh.used:
            votes = {}
            for u, w in mesh.nbrs[v].items():
                if region[u] >= 0:
                    votes[region[u]] = votes.get(region[u], 0) + 1
            if votes:
                top = max(votes, key=votes.get)
                if top != region[v] and votes[top] > 0.6 * sum(votes.values()) and votes.get(region[v], 0) <= 1:
                    new[v] = top
        region = new
    # the gun: the weapon bone
    for v in GUN:
        if v < mesh.nv and v not in set(mesh.used):
            region[v] = index['weapon']
    for v in range(mesh.nv):
        if region[v] < 0:
            region[v] = index['weapon'] if v in GUN else 0
    # 3. joint blends
    weights = [{region[v]: 1.0} for v in range(mesh.nv)]
    for ci, b in enumerate(bones):
        if b['joint'] in ('root', 'fixed'):
            continue
        pi = b['pidx']
        p = bones[pi]
        cd = norm(sub(b['end'], b['pivot']))
        pd = norm(sub(b['pivot'], p['pivot'])) if p['joint'] != 'root' else cd
        u = norm(add(pd, cd))
        w = b['blend']
        # (the parent's region and its parent's: the chest round a shoulder blends with the arm as the clavicle's does,
        # so the armpit and the shoulder's front and back have no rigid seam)
        gpi = p['pidx']
        for v in mesh.used:
            if region[v] not in (pi, ci, gpi):
                continue
            x = mesh.rest[v]
            if length(sub(x, b['pivot'])) > b['radius']:
                continue
            s = dot(sub(x, b['pivot']), u)
            if abs(s) >= w:
                continue
            wc = smoothstep((s + w) / (2 * w))
            base = {pi: 1.0} if region[v] == ci else weights[v]
            out = {bo: ww * (1.0 - wc) for bo, ww in base.items()}
            out[ci] = out.get(ci, 0.0) + wc
            weights[v] = out
    # 4. smoothing: a screened diffusion of the blended weights over the surface (each vertex pulled to its step-3
    # weights by SMOOTH_ALPHA, to its neighbours' average by 1): every seam between regions, the joints' bands too,
    # eases over a few rings
    anchor = weights
    for _ in range(SMOOTH_ITERS):
        new = []
        for v in range(mesh.nv):
            if v not in mesh.nbrs or not mesh.nbrs[v] or region[v] == index['weapon']:
                new.append(anchor[v])
                continue
            acc = {}
            tot = 0.0
            for u, k in mesh.nbrs[v].items():
                for bo, ww in weights[u].items():
                    acc[bo] = acc.get(bo, 0.0) + k * ww
                tot += k
            a = SMOOTH_ALPHA
            out = {bo: ww / tot / (1 + a) for bo, ww in acc.items()}
            for bo, ww in anchor[v].items():
                out[bo] = out.get(bo, 0.0) + a * ww / (1 + a)
            new.append(out)
        weights = new
    # 5. welds
    for g in mesh.groups:
        acc = {}
        for v in g:
            for bo, ww in weights[v].items():
                acc[bo] = acc.get(bo, 0.0) + ww / len(g)
        for v in g:
            weights[v] = dict(acc)
    # 6. at most 4, none under 0.03, rounded
    out = []
    for v in range(mesh.nv):
        items = sorted(weights[v].items(), key=lambda kv: -kv[1])[:4]
        items = [(bo, ww) for bo, ww in items if ww >= 0.03] or items[:1]
        tot = sum(ww for _, ww in items)
        items = [(bo, round(ww / tot, 3)) for bo, ww in items]
        drift = round(1.0 - sum(ww for _, ww in items), 3)
        items[0] = (items[0][0], round(items[0][1] + drift, 3))
        out.append(items)
    return bones, region, out


def frame_of(b):
    """A ball joint's neutral direction and its swing axes' directions (fwd, side) made orthonormal."""
    rest = norm(sub(b['end'], b['pivot']))
    n = norm(b['neutral']) if b.get('neutral') else rest
    f = norm(sub(b['fwd'], mul(n, dot(b['fwd'], n))))
    s = norm(sub(b['side'], add(mul(n, dot(b['side'], n)), mul(f, dot(b['side'], f)))))
    return n, f, s


def hinge_axis(b, parent):
    pd = norm(sub(b['pivot'], parent['pivot']))
    cd = norm(sub(b['end'], b['pivot']))
    a = cross(pd, cd)
    return norm(a), math.degrees(math.atan2(length(a), dot(pd, cd)))


def fmt(p):
    return '%.3f %.3f %.3f' % tuple(0.0 if abs(c) < 5e-4 else c for c in p)


def write(path, model, bones, weights):
    lines = ['qvrrig2 1', 'model %s' % model,
             '# Made by Misc/quakevr/make_rig2.py (its header: the method); hand-editable. Model units, x forward,',
             '# y left, z up; degrees. bone <name> <parent|-> <root|ball|hinge|fixed> pivot <xyz> end <xyz>',
             '#   ball: neutral <xyz> fwd <xyz> side <xyz> swingx <min max> swingy <min max> twist <min max>',
             '#   hinge: axis <xyz> range <min max> (its bend from straight; the rest pose\'s own bend measured)',
             '# w <vertex> <bone> <weight> ... (up to 4, summing to 1)']
    for b in bones:
        s = 'bone %s %s %s pivot %s end %s' % (b['name'], b['parent'] or '-', b['joint'], fmt(b['pivot']), fmt(b['end']))
        if b['joint'] == 'ball':
            n, f, sd = frame_of(b)
            s += ' neutral %s fwd %s side %s swingx %g %g swingy %g %g twist %g %g' % (
                fmt(n), fmt(f), fmt(sd), b['sx'][0], b['sx'][1], b['sy'][0], b['sy'][1], b['tw'][0], b['tw'][1])
        elif b['joint'] == 'hinge':
            a, bend = hinge_axis(b, bones[b['pidx']])
            s += ' axis %s range %g %g' % (fmt(a), b['range'][0], b['range'][1])
            b['restbend'] = bend
        lines.append(s)
    for v, items in enumerate(weights):
        lines.append('w %d %s' % (v, ' '.join('%s %g' % (bones[bo]['name'], ww) for bo, ww in items)))
    with open(path, 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


COLORS = [(230, 230, 230), (230, 160, 60), (240, 220, 60), (160, 220, 240), (240, 120, 200), (90, 200, 90),
          (60, 160, 255), (40, 220, 200), (200, 120, 255), (90, 200, 90), (255, 90, 90), (255, 160, 40),
          (200, 120, 255), (60, 120, 255), (120, 255, 120), (255, 255, 120), (255, 60, 60), (255, 140, 200),
          (255, 255, 120), (128, 128, 128)]


def preview(mesh, bones, region, weights, path):
    S, W, H, cy = 12, 420, 640, 300
    c = Canvas(3 * W, H)
    views = [(lambda p: (W / 2 + S * p[1], cy - S * p[2], p[0]), (1, 0, 0)),
             (lambda p: (W + W / 2 + S * p[0], cy - S * p[2], -p[1]), (0, -1, 0)),
             (lambda p: (2 * W + W / 2 - S * p[0], cy - S * p[2], p[1]), (0, 1, 0))]
    for view, d in views:
        order = sorted(mesh.tris, key=lambda t: sum(view(mesh.rest[i])[2] for i in t))
        for t in order:
            a, b, e = (mesh.rest[i] for i in t)
            n = cross(sub(b, a), sub(e, a))
            sh = dot(n, d) / max(length(n), 1e-9)
            if sh <= 0:
                continue
            col = [0.0, 0.0, 0.0]
            for i in t:
                for bo, ww in weights[i]:
                    for k in range(3):
                        col[k] += COLORS[bo % len(COLORS)][k] * ww / 3.0
            k = 0.35 + 0.65 * sh
            c.tri(view(a), view(b), view(e), tuple(int(x * k) for x in col))
        for b in bones:
            pa, pe = view(b['pivot']), view(b['end'])
            c.line(pa[0], pa[1], pe[0], pe[1], (255, 255, 255))
            c.dot(pa[0], pa[1], (255, 30, 30), 3)
    c.save(path)


def main():
    args = sys.argv[1:]
    quiet = '--quiet' in args
    prev = args[args.index('--preview') + 1] if '--preview' in args else None
    # Built on the armed grunt's mesh (no gun triangles: the gun's vertices are the weapon bone's), written for both
    mesh = Mesh(os.path.join(ROOT, MODELS['vrgrunt']))
    if not quiet:
        print('vrgrunt: %d vertices, %d triangles' % (mesh.nv, len(mesh.tris)))
    bones, region, weights = build(mesh, quiet)
    for name, rel in MODELS.items():
        os.makedirs(os.path.join(ROOT, 'quakevr', 'rigs'), exist_ok=True)
        out = os.path.join(ROOT, 'quakevr', 'rigs', name + '.rig2.txt')
        write(out, 'progs/%s.mdl' % name, bones, weights)
        blended = sum(1 for v in mesh.used if len(weights[v]) > 1)
        if not quiet:
            print('  wrote %s (%d bones; %d of %d drawn vertices blended)' % (os.path.relpath(out, ROOT), len(bones),
                                                                            blended, len(mesh.used)))
            for b in bones:
                if b['joint'] == 'hinge':
                    print('  %-11s rest bend %.1f' % (b['name'], b['restbend']))
        if prev and name == 'vrgrunt':
            preview(mesh, bones, region, weights, prev)
            preview(mesh, bones, region, [[(region[v], 1.0)] for v in range(mesh.nv)], prev.replace('.png', '_regions.png'))


if __name__ == '__main__':
    main()
