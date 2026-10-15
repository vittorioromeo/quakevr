# make_voreshove.py -- the vore's two-handed shove (ROUND22.md, "The vore's two-handed shove"): seven frames appended
# to Quake VR's own quakevr/progs/shalrath.mdl, after all of its 36 (which stay as they were, byte for byte):
# shove1..shove7 (QC orig_mon_shalrath.qc shal_shove1..; vr_enemyshove.qc). Both arms drawn back, elbows low and the
# hands up by his chest (the tell), then thrust straight out at you together, his body leaning into it (the blow lands
# on shove5, the arms' full reach), and back towards his rest.
#
# The model has no skeleton (vertex animation); its ragdoll rig (vr_ragdoll.cpp shalrathSeeds) is fitted to its frames,
# and the frames here are posed by that rig's bones: each vertex goes with its bone (BONE_OF: Misc/quakevr/ragdoll/rig.py
# shalrath shalrath_bones.json's labels, as the engine gives them), every bone turned rigidly about its joint (the seeds'
# pivots), blended across a joint near it (an upper arm's vertices by the shoulder partly with the chest, a forearm's by
# the elbow partly with the upper arm, the chest's low ones partly with the pelvis) so the seams stretch smoothly:
# - the chest (with the head, the jaw and the arms) leans about its pivot with the pelvis (forward positive);
# - each upper arm turns about its shoulder the shortest way from its rest direction to the key's;
# - each forearm then about its elbow, the shortest way from where the upper arm left it to the key's direction.
# The pelvis and the legs stay as the rest pose ($attack1, the model's stand1). The normals turn with their bone.
#
#   python Misc/quakevr/make_voreshove.py            writes the model (genguard: stops if it was edited by hand)
#   python Misc/quakevr/make_voreshove.py --check    prints what it would write, writes nothing
#
# Run again, it replaces the shove frames it appended before (frames named shove*), so it can be re-tuned.

import math
import os
import sys

import numpy as np

import genguard
from make_enemyshove import Model, smoothstep
from mdlgen import anorms

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.normpath(os.path.join(HERE, '..', '..', 'quakevr', 'progs', 'shalrath.mdl'))
COUNTS = (371, 816)

# Each vertex's bone (hex digit: vr_ragdoll.cpp shalrathSeeds' order): 0 pelvis, 1 chest, 2 head, 3 jaw, 4 upperarm_l,
# 5 forearm_l, 6 upperarm_r, 7 forearm_r, 8-d the legs.
BONE_OF = (
    '45544455555555555677666777777777772d9b111131333133133331111122333331223233333233333bbbbbbbb99999999d'
    'ddddaaabaabaaaba8888888898cccccccccccc33332233322311111111111111111222222200001010000011111110110111'
    '111111111111888888800000000aa000aa0000000080111bdddd9999bbb9999ddddbbbb7755555577757755511887ddd2222'
    '22222211111111111111111444444666666646444444444666666664444444466666666')
PELVIS, CHEST = 0, 1
LEANS = {1, 4, 5, 6, 7}  # the chest and its arms (the head and jaw: HEAD)

# The rig's joints (vr_ragdoll.cpp shalrathSeeds: pivots; an arm's far end its forearm's tip), x forward, y left, z up.
CHEST_PIVOT = np.array([0.2, -1.2, 21.5])
HEAD_PIVOT = np.array([-0.8, 0.1, 40.1])
HEAD = (2, 3)  # the head and its jaw (the tendrils)
ARMS = [  # (upper arm's bone, forearm's, shoulder, elbow, hand, side: +1 left)
    (4, 5, np.array([-2.4, 12.5, 36.8]), np.array([3.0, 29.4, 23.9]), np.array([5.9, 44.1, 16.5]), 1.0),
    (6, 7, np.array([0.4, -11.6, 34.3]), np.array([4.5, -24.9, 20.6]), np.array([3.6, -49.8, 20.2]), -1.0),
]

# The arms' directions (the left's; the right's mirrored), in the chest's frame (before its lean): drawn back (elbows
# behind and below the shoulders, the forearms up and forward, the hands in by his chest), and thrust (straight ahead,
# a little in, the hands together at your chest's height).
COCK = (np.array([-0.45, 0.5, -0.74]), np.array([0.85, -0.2, 0.5]))
PEAK = (np.array([0.93, 0.05, -0.12]), np.array([0.93, -0.36, 0.0]))
REST = None  # (each arm's own rest directions)

# Per frame: (the chest's lean in degrees, forward positive; the head's nod on top of it, the same way; the arms between
# two of the poses above, 0..1). 1-3 the wind-up (leaning back, head pulled back, arms drawn back: the tell), 4 the
# thrust, 5 its full reach (the blow lands on its think), 6-7 back towards rest (shal_run1 follows).
KEYS = [(-4, -3, REST, COCK, 0.45), (-9, -7, REST, COCK, 0.85), (-11, -8, REST, COCK, 1.0),
        (5, 6, COCK, PEAK, 0.7), (12, 10, COCK, PEAK, 1.0),
        (9, 6, PEAK, REST, 0.3), (3, 2, PEAK, REST, 0.7)]
NAMES = ['shove%d' % (i + 1) for i in range(len(KEYS))]


def unit(v):
    return v / np.linalg.norm(v)


def rot_y(deg):
    """Turns a point about the y axis so that above the pivot it goes forward (+x) for a positive angle."""
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0.0, s], [0.0, 1.0, 0.0], [-s, 0.0, c]])


def between(a, b):
    """The shortest rotation taking direction a to direction b."""
    a, b = unit(a), unit(b)
    axis = np.cross(a, b)
    s, c = np.linalg.norm(axis), float(np.dot(a, b))
    if s < 1e-9:
        return np.eye(3)
    k = axis / s
    K = np.array([[0.0, -k[2], k[1]], [k[2], 0.0, -k[0]], [-k[1], k[0], 0.0]])
    return np.eye(3) + s * K + (1.0 - c) * (K @ K)


def mirror(pose, side):
    return tuple(d * np.array([1.0, side, 1.0]) for d in pose)


def bones(lean, nod, a, b, f):
    """Each bone's rigid motion (R, t: p' = R p + t) for a key."""
    L = rot_y(lean)
    lean_t = CHEST_PIVOT - L @ CHEST_PIVOT
    out = {k: (np.eye(3), np.zeros(3)) for k in range(14)}
    for k in LEANS:
        out[k] = (L, lean_t)
    N = rot_y(nod)
    for k in HEAD:
        out[k] = (L @ N, L @ (HEAD_PIVOT - N @ HEAD_PIVOT) + lean_t)
    for upper, fore, sh, el, hand, side in ARMS:
        rest = (el - sh, hand - el)
        pa = rest if a is None else mirror(a, side)
        pb = rest if b is None else mirror(b, side)
        du = unit((1 - f) * unit(pa[0]) + f * unit(pb[0]))
        df = unit((1 - f) * unit(pa[1]) + f * unit(pb[1]))
        Ru = between(rest[0], du)
        el2 = sh + Ru @ (el - sh)
        Rf = between(Ru @ rest[1], df) @ Ru
        # (in the chest's frame: p -> sh + Ru (p - sh), the forearm's p -> el2 + Rf (p - el); then the lean)
        out[upper] = (L @ Ru, L @ (sh - Ru @ sh) + lean_t)
        out[fore] = (L @ Rf, L @ (el2 - Rf @ el) + lean_t)
    return out


def weights(pos, bone):
    """Each vertex's second bone and its weight (the blend across a joint near it); (-1, 0) for none."""
    other = np.full(len(pos), -1)
    w = np.zeros(len(pos))
    for i, (p, b) in enumerate(zip(pos, bone)):
        if b == CHEST:
            other[i], w[i] = PELVIS, 1.0 - smoothstep(CHEST_PIVOT[2] - 2.0, CHEST_PIVOT[2] + 4.0, p[2])
        for upper, fore, sh, el, hand, side in ARMS:
            if b == upper:
                t = np.dot(p - sh, unit(el - sh))
                other[i], w[i] = CHEST, 1.0 - smoothstep(-2.0, 6.0, t)
            elif b == fore:
                t = np.dot(p - el, unit(hand - el))
                other[i], w[i] = upper, 1.0 - smoothstep(-2.0, 4.0, t)
    return other, w


def shove_frames(m):
    pos, nidx = m.verts(0)
    bone = np.array([int(c, 16) for c in BONE_OF])
    assert len(bone) == len(pos)
    other, w = weights(pos, bone)
    table = np.array(anorms())
    base_n = table[nidx]
    out = []
    for name, key in zip(NAMES, KEYS):
        T = bones(*key)
        new = np.empty_like(pos)
        normals = np.empty(len(pos), int)
        for i, p in enumerate(pos):
            R, t = T[bone[i]]
            q = R @ p + t
            if other[i] >= 0 and w[i] > 0:
                R2, t2 = T[other[i]]
                q = (1 - w[i]) * q + w[i] * (R2 @ p + t2)
                if w[i] > 0.5:
                    R = R2
            new[i] = q
            normals[i] = int(np.argmax(table @ (R @ base_n[i])))
        out.append((name, new, normals))
    blended = int(((other >= 0) & (w > 0.01)).sum())
    return out, 'blended across a joint: %d vertices' % blended


def main():
    check = '--check' in sys.argv
    guard = None if check else genguard.Guard('make_voreshove.py', [PATH])
    m = Model(PATH)
    assert (m.nv, m.nt) == COUNTS, 'shalrath.mdl: not Quake VR\'s own model (%d vertices, %d triangles)' % (m.nv, m.nt)
    kept = [fr for fr in m.frames if not fr[0].startswith('shove')]
    old = len(m.frames) - len(kept)
    m.frames = kept
    frames, stats = shove_frames(m)
    first = len(m.frames)
    for name, new, normals in frames:
        m.frames.append((name, m.frame_bytes(name, new, normals)))
        m.header[8] = max(m.header[8], float(np.max(np.linalg.norm(new, axis=1))))
        hands = [new[np.array([int(c, 16) for c in BONE_OF]) == fore] for _, fore, _, _, _, _ in ARMS]
        print('  %s: hands at %s and %s' % (name, hands[0].mean(0).round(1), hands[1].mean(0).round(1)))
    print('shalrath.mdl: %s; frames %d..%d (%s), replacing %d' % (stats, first, len(m.frames) - 1, ' '.join(NAMES), old))
    if not check:
        with open(PATH, 'wb') as f:
            f.write(m.bytes())
    if guard:
        guard.finish()


if __name__ == '__main__':
    main()
