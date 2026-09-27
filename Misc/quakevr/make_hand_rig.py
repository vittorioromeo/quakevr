#!/usr/bin/env python3
# make_hand_rig.py -- generates the jointed hand (Quake/vr/vr_handrig.cpp; round 21, "Hands remodelled"):
#   quakevr/progs/hand_rig.md5mesh, .md5anim   the hand as one skinned mesh
#   quakevr/progs/hand_rig.mdl                 a placeholder: Ironwail loads MD5 only as the "enhanced"
#                                              replacement of an existing .mdl
#   quakevr/progs/hand_rig_NN_00.lmp           its skins, NN = the damage level (0 clean .. 3 bloodiest)
#   Quake/vr/vr_handrig_data.inc               the rig for the engine: joints, pivots, curl frames, the
#                                              vertices and their weights, the grasp solver's spheres
#
# Usage: python Misc/quakevr/make_hand_rig.py [progs folder [include file]]
#
# The mesh is modelled here, procedurally, in the space and at the scale of the six hand models drawn before
# (progs/hand_base.mdl and the five progs/finger_*.mdl, still the fallback: vr_hand_rig 0). Their skins (and
# the bloody ones, make_bloody_hands.py) are baked onto it, so the hand keeps its look and palette.
#
# Space ("rig space"): hand_base.mdl's model space, hand model units (about 1.2 cm at the default scale): +x
# towards the fingers, +y the palm's side, +z the thumb's side. The six models' placement constants (their scale
# origins and default offsets) are kept: the engine moves each finger by what its offset cvars change from the
# defaults, as before.
#
# The hand:
#   - A palm lofted through seven cross-sections (13 vertices round) from the wrist, inside the arm's cuff, to
#     the knuckles: the wrist, the heel of the hand, the arched back, the cupped palm with its pads.
#   - Four fingers and a thumb, 8-sided tubes tapering to rounded tips, with slight knuckle bulges. Human
#     proportions: phalanges about 1 : 0.62 : 0.5, the middle finger longest, the pinky about 3/4 of it; the
#     fingers' length about the palm's width (the old ones were short on a long palm).
#   - The thumb has three segments: the metacarpal (its thick base is the ball of the thumb), the proximal and
#     the distal phalanx; it turns at the carpometacarpal joint near the wrist (the solver's opposition).
#
# The rig: three joints a finger (the thumb's CMC, MCP, IP; the fingers' MCP, PIP, DIP), each a hinge with its
# pivot inside the knuckle and its own axis (the fingers' converge a little as they close, as a fist's do). Each
# joint's turn per curl frame (0 open .. 4 the tightest fist, 5 = 3: the curls vr_view.cpp draws) is given
# below. Skinning without candy-wrapping or collapse: the ring of vertices at a joint rides a helper joint turned
# half as far (a mitred joint: both tubes meet at the bisecting plane, the ring keeps its size), the rest of a
# segment rides the segment; the palm's thumb side follows the thumb's metacarpal by weight, between helpers
# turned a quarter, half and three quarters as far (slerped: the thenar never thins).
#
# The engine (vr_handrig.cpp) computes the joints' matrices from the pose on the CPU (33 of them) and the GPU
# skins the mesh through the body's path (VR_AliasBonePoses): one draw per hand.

import math
import os
import struct
import sys

import numpy as np

from mdlgen import HEADER, anorms, read_skins

HERE = os.path.dirname(os.path.abspath(__file__))
PARTS = ["hand_base", "finger_thumb", "finger_index", "finger_middle", "finger_ring", "finger_pinky"]
FINGERS = ["thumb", "index", "middle", "ring", "pinky"]  # vr_handrig.cpp's order
FRAMES = 6  # curl frames 0..5 (vr_view.cpp: 0 open .. 4 the tightest fist; 5 is 3)

# Default placement of the six models (vr_cvars.inc): vr_fingers_*, vr_finger_<name>_*, vr_finger_base_* (hand
# model units; y is to the hand's right, the model's -y), and the fist slot's Scale (vr_weapons.inc, slot 16). The
# offsets are drawn at weapons::offsetScale(), the models at ModelTransform::k: their ratio at any world and gun
# model scale is 0.75 / (1.25 * 0.7). The engine moves each finger by what its settings change from these.
FINGERS_ALL = (-5.05, -0.1, -0.1875)
FINGER_OFFSETS = {
    "thumb": (-0.3625, 3.2625, -1.9375),
    "index": (-0.325, 0.6125, -1.825),
    "middle": (-0.3625, 0.5125, -0.3125),
    "ring": (-0.3625, 0.7, 0.65),
    "pinky": (-0.325, 1.15, 1.6375),
}
BASE_OFFSET = (0.0, 0.0, 0.0)
OFFSET_PER_MODEL = 0.75 / (1.25 * 0.7)
FIST_SCALE = 0.34

# The palm's middle as the grasp solver and the cup hotspots have used it (vr_grasp.cpp: the middle of the old
# palm's triangles facing the palm's way, the thenar's left out). Kept exactly: cups, the palm's turn and the free
# hand's reach to a cup are all measured from it, and so stay where they were tuned.
PALM_CENTRE = (-2.640542507171631, 0.05627598240971565, 0.4530821740627289)

# ----------------------------------------------------------------------------
# The shape (rig space, hand units)

SIDES = 8  # finger and thumb cross-sections

# Per finger: its knuckle's pivot (MCP), its splay (degrees towards +z, the thumb's side), its phalanges' lengths
# (MCP to PIP, PIP to DIP, DIP to the tip), its half-widths (at the MCP, PIP, DIP and near the tip), how much its
# closing axis leans (degrees: the fingers converge as they close) and its curl frames' joint angles.
FLAT = 0.88  # a finger's thickness over its width
FINGER_SPEC = {
    "index": dict(pivot=(5.45, -1.74, 3.10), splay=3.5, length=(3.15, 1.95, 1.70), half=(1.07, 1.00, 0.88, 0.75), lean=-5.0),
    "middle": dict(pivot=(5.65, -1.80, 1.28), splay=0.0, length=(3.45, 2.20, 1.80), half=(1.10, 1.03, 0.91, 0.77), lean=1.0),
    "ring": dict(pivot=(5.45, -1.76, -0.50), splay=-3.5, length=(3.25, 2.10, 1.74), half=(1.05, 0.97, 0.86, 0.73), lean=6.0),
    "pinky": dict(pivot=(4.95, -1.64, -2.12), splay=-8.0, length=(2.60, 1.58, 1.52), half=(0.92, 0.85, 0.75, 0.65), lean=12.0),
}
# Joint angles (MCP, PIP, DIP) per curl frame 0..4 (5 = 3): 0 relaxed, 4 the tightest fist.
FINGER_FRAMES = {
    "index": [(3, 6, 3), (21, 29, 16), (43, 60, 36), (70, 90, 54), (84, 100, 62)],
    "middle": [(5, 8, 4), (23, 31, 17), (45, 62, 38), (72, 92, 56), (86, 102, 64)],
    "ring": [(6, 10, 5), (25, 33, 18), (47, 64, 39), (74, 93, 57), (89, 102, 64)],
    "pinky": [(8, 13, 7), (27, 35, 19), (49, 66, 40), (77, 94, 58), (92, 100, 62)],
}

# The thumb: its carpometacarpal pivot (CMC, the metacarpal's base, near the wrist), its three segments'
# directions and lengths at rest, their half-widths (CMC, the metacarpal's middle (the ball of the thumb), MCP,
# IP, near the tip), and the point its pad faces (it closes towards it).
THUMB_CMC = np.array([-2.55, -0.35, 2.75])
THUMB_DIRS = [(0.80, 0.44, 0.40), (0.76, 0.44, 0.48), (0.88, 0.28, 0.38)]
THUMB_LENGTHS = (3.9, 2.75, 2.15)
THUMB_HALF = (1.30, 1.48, 1.08, 1.00, 0.80)
THUMB_PAD_TOWARDS = np.array([4.2, 0.9, -2.2])
# Joint angles (CMC, MCP, IP) per curl frame 0..4 (the CMC's opposition is the grasp solver's metacarpal turn).
THUMB_FRAMES = [(0, 3, 5), (4, 14, 20), (8, 27, 37), (13, 42, 55), (16, 48, 64)]
# ... and the CMC's turn across the palm per frame (about -x, as the grasp's opposition turns it): the fist's thumb
# comes over the index and middle fingers.
THUMB_ACROSS = [0, 8, 20, 36, 44]

# The palm's cross-sections, wrist to knuckles: x (for the distal ones plus the knuckle line's arc, below), the
# ulnar and radial edges (z), and the back's and the palm's heights (y) with the back's arch and the palm's hollow.
PALM_STATIONS = [
    dict(x=-5.90, zu=-0.45, zr=3.25, back=-2.40, palm=0.30, arch=0.00, hollow=0.00, arc=0.0),  # inside the forearm
    dict(x=-4.40, zu=-0.70, zr=3.50, back=-2.50, palm=0.45, arch=0.05, hollow=0.00, arc=0.0),  # the wrist
    dict(x=-2.50, zu=-1.95, zr=3.95, back=-2.70, palm=0.90, arch=0.15, hollow=0.10, arc=0.0),  # the heel of the hand
    dict(x=0.10, zu=-2.90, zr=4.00, back=-2.82, palm=0.55, arch=0.22, hollow=0.30, arc=0.0),
    dict(x=2.70, zu=-3.02, zr=3.92, back=-2.95, palm=0.18, arch=0.22, hollow=0.22, arc=0.4),
    dict(x=-1.25, zu=-2.95, zr=3.82, back=-3.00, palm=-0.25, arch=0.16, hollow=0.10, arc=1.0, knuckle=-1.25),  # knuckles
    dict(x=0.00, zu=-2.65, zr=3.55, back=-2.72, palm=-0.80, arch=0.10, hollow=0.00, arc=1.0, knuckle=-0.25),  # front
]
PALM_FRONT = np.array([5.95, -1.85, 0.55])  # the front cap's middle
WRIST = (-4.40, -1.05, 1.40)  # the wrist's middle: where the arm meets the hand (vr_view.cpp drawnHand)
# Round the section (u across from the ulnar edge 0 to the radial 1; v -1 the back, +1 the palm): the back's
# vertices over the four metacarpals, the palm's over the pads.
PALM_TEMPLATE = [(0.0, 0.0), (0.035, -0.72), (0.145, -1.0), (0.37, -1.0), (0.62, -1.0), (0.88, -1.0), (0.975, -0.68),
                 (1.0, 0.05), (0.94, 0.78), (0.72, 1.0), (0.45, 1.0), (0.20, 1.0), (0.04, 0.74)]
THENAR_REACH = 2.8  # how far from the thumb's metacarpal the palm follows it (falling off)
THENAR_MOST = 0.75


def unit(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def axis_angle(axis, deg):
    """A rotation matrix."""
    a = unit(axis)
    t = math.radians(deg)
    k = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + math.sin(t) * k + (1 - math.cos(t)) * (k @ k)


def quat(r):
    """(x, y, z, w) of a rotation matrix."""
    t = np.trace(r)
    if t > 0:
        s = math.sqrt(t + 1.0) * 2
        q = ((r[2, 1] - r[1, 2]) / s, (r[0, 2] - r[2, 0]) / s, (r[1, 0] - r[0, 1]) / s, 0.25 * s)
    elif r[0, 0] > r[1, 1] and r[0, 0] > r[2, 2]:
        s = math.sqrt(1.0 + r[0, 0] - r[1, 1] - r[2, 2]) * 2
        q = (0.25 * s, (r[0, 1] + r[1, 0]) / s, (r[0, 2] + r[2, 0]) / s, (r[2, 1] - r[1, 2]) / s)
    elif r[1, 1] > r[2, 2]:
        s = math.sqrt(1.0 + r[1, 1] - r[0, 0] - r[2, 2]) * 2
        q = ((r[0, 1] + r[1, 0]) / s, 0.25 * s, (r[1, 2] + r[2, 1]) / s, (r[0, 2] - r[2, 0]) / s)
    else:
        s = math.sqrt(1.0 + r[2, 2] - r[0, 0] - r[1, 1]) * 2
        q = ((r[0, 2] + r[2, 0]) / s, (r[1, 2] + r[2, 1]) / s, 0.25 * s, (r[1, 0] - r[0, 1]) / s)
    q = np.array(q)
    q /= np.linalg.norm(q)
    return q if q[3] >= 0 else -q


def qmat(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return np.array([aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
                     aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz])


def slerp(a, b, t):
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = float(a @ b)
    if d < 0:
        b, d = -b, -d
    if d > 0.9995:
        q = a + t * (b - a)
        return q / np.linalg.norm(q)
    th = math.acos(d)
    return (math.sin((1 - t) * th) * a + math.sin(t * th) * b) / math.sin(th)


IDQ = np.array([0.0, 0.0, 0.0, 1.0])

# ----------------------------------------------------------------------------
# The rig


class Rig:
    """Pivots, hinge axes and turns per curl frame of the 15 joints (thumb first, vr_handrig.cpp's order)."""

    def __init__(self):
        self.pivots = np.zeros((5, 3, 3))
        self.axes = np.zeros((5, 3, 3))
        self.turns = np.zeros((5, FRAMES, 3, 4))
        self.frames = {}
        self.fingers = {}  # per finger: its segments' frames at rest (origin, x along, y towards the palm, z)

        # The thumb.
        dirs = [unit(d) for d in THUMB_DIRS]
        pts = [THUMB_CMC]
        for d, ln in zip(dirs, THUMB_LENGTHS):
            pts.append(pts[-1] + d * ln)
        segs = []
        for k in range(3):
            x = dirs[k]
            y = THUMB_PAD_TOWARDS - pts[k]
            y = unit(y - x * (y @ x))
            segs.append((pts[k], x, y, np.cross(x, y)))
        self.fingers["thumb"] = dict(points=pts, segments=segs)
        for k in range(3):
            self.pivots[0, k] = pts[k]
            self.axes[0, k] = np.cross(segs[k][1], segs[k][2])  # closes the segment towards its pad
        self.frames["thumb"] = THUMB_FRAMES

        # The fingers.
        for fi, name in enumerate(FINGERS[1:], 1):
            spec = FINGER_SPEC[name]
            s = math.radians(spec["splay"])
            x = np.array([math.cos(s), 0.0, math.sin(s)])
            y = np.array([0.0, 1.0, 0.0])
            z = np.cross(x, y)
            pts = [np.array(spec["pivot"], float)]
            for ln in spec["length"]:
                pts.append(pts[-1] + x * ln)
            lean = math.radians(spec["lean"])
            self.fingers[name] = dict(points=pts, segments=[(pts[k], x, y, z) for k in range(3)])
            for k in range(3):
                self.pivots[fi, k] = pts[k]
                self.axes[fi, k] = unit(z * math.cos(lean) + x * math.sin(lean)) if k == 0 else z
            self.frames[name] = FINGER_FRAMES[name]

        for fi, name in enumerate(FINGERS):
            fr = self.frames[name]
            for f in range(FRAMES):
                angles = fr[3 if f == 5 else f]
                for k in range(3):
                    r = axis_angle(self.axes[fi, k], angles[k])
                    if fi == 0 and k == 0:
                        r = axis_angle((-1.0, 0.0, 0.0), THUMB_ACROSS[3 if f == 5 else f]) @ r
                    self.turns[fi, f, k] = quat(r)

    def turn(self, fi, k, curl):
        c = min(max(curl, 0.0), FRAMES - 1.0)
        a = min(int(c), FRAMES - 2)
        return slerp(self.turns[fi, a, k], self.turns[fi, a + 1, k], c - a)

    def segments(self, fi, curls, metacarpal=IDQ, shift=np.zeros(3)):
        """Each segment's transform (rotation, translation) at these curls, and each joint's turn."""
        out = [(np.eye(3), np.asarray(shift, float))]
        turns = []
        for k in range(3):
            q = self.turn(fi, k, curls[k])
            if fi == 0 and k == 0:
                q = qmul(metacarpal, q)
            r = qmat(q)
            p = self.pivots[fi, k]
            pr, pt = out[k]
            out.append((pr @ r, pr @ (p - r @ p) + pt))
            turns.append(q)
        return out, turns


# The joints of the MD5: 0 the palm; then per finger its three segments; then per finger per joint a helper turned
# half as far as the joint (the ring of vertices there); then the thumb's CMC turned a quarter and three quarters
# as far (the thenar).
PALM, SEGMENT, PART = 0, 1, 2


def joint_table():
    joints = [("palm", PALM, 0, 0, 0.0)]
    for fi, name in enumerate(FINGERS):
        for b in range(1, 4):
            joints.append(("%s_%d" % (name, b), SEGMENT, fi, b, 1.0))
    for fi, name in enumerate(FINGERS):
        for k in range(3):
            joints.append(("%s_%d_half" % (name, k + 1), PART, fi, k, 0.5))
    joints.append(("thumb_1_quarter", PART, 0, 0, 0.25))
    joints.append(("thumb_1_three_quarters", PART, 0, 0, 0.75))
    return joints


JOINTS = joint_table()


def seg_joint(fi, b):
    return 1 + fi * 3 + (b - 1)


def half_joint(fi, k):
    return 16 + fi * 3 + k


THENAR_JOINTS = [(0.0, 0), (0.25, 31), (0.5, half_joint(0, 0)), (0.75, 32), (1.0, seg_joint(0, 1))]


def joint_poses(rig, curls, metacarpal=IDQ, shifts=None):
    """Every joint's transform at a pose (as vr_handrig.cpp computes them): curls[5][3]."""
    shifts = shifts if shifts is not None else np.zeros((5, 3))
    seg, tur = [], []
    for fi in range(5):
        s, t = rig.segments(fi, curls[fi], metacarpal, shifts[fi])
        seg.append(s)
        tur.append(t)
    out = []
    for name, kind, fi, idx, frac in JOINTS:
        if kind == PALM:
            out.append((np.eye(3), np.zeros(3)))
        elif kind == SEGMENT:
            out.append(seg[fi][idx])
        else:
            r = qmat(slerp(IDQ, tur[fi][idx], frac))
            p = rig.pivots[fi, idx]
            pr, pt = seg[fi][idx]
            out.append((pr @ r, pr @ (p - r @ p) + pt))
    return out, seg


# ----------------------------------------------------------------------------
# The mesh


class Mesh:
    def __init__(self):
        self.pos = []  # bind positions
        self.infl = []  # [(joint, weight)]
        self.part = []  # 0 palm, 1..5 the fingers (thumb first)
        self.bone = []  # the finger's segment the vertex mostly rides (0 for the palm)
        self.centre = []  # the middle of its cross-section (the inside, for checks)
        self.tris = []  # (a, b, c) counter-clockwise seen from outside, per corner uv
        self.tuv = []
        self.tpart = []

    def vertex(self, p, infl, part, bone=0, centre=None):
        self.pos.append(np.asarray(p, float))
        self.centre.append(np.asarray(p if centre is None else centre, float))
        self.infl.append(infl)
        self.part.append(part)
        self.bone.append(bone)
        return len(self.pos) - 1

    def tri(self, v, uv, part):
        self.tris.append(tuple(v))
        self.tuv.append([tuple(x) for x in uv])
        self.tpart.append(part)


def ellipse_ring(c, x, y, z, half, back, palm, n=SIDES, start=0.5):
    """n points round centre c in the plane (y, z): half-width `half` along z, `back` towards -y, `palm` towards
    +y. The first point sits `start` of a step past +z (the thumb's side), going towards +y (the palm)."""
    pts = []
    for j in range(n):
        a = 2 * math.pi * (j + start) / n
        cz, sy = math.cos(a), math.sin(a)
        pts.append(c + z * (half * cz) + y * ((palm if sy > 0 else back) * sy))
    return pts


def loft(mesh, rings, infl, part, bone, u0v0, texel_len, caps):
    """Tube through `rings` (lists of points, same count), per ring its influences and bone; texture: u round
    the ring (its mean perimeter), v along (the rings' centres' distance); `caps` = (start apex or None, end apex
    or None) with their influences and bones. Returns the texture extent (u, v) in hand units."""
    n = len(rings[0])
    centres = [np.mean(r, axis=0) for r in rings]
    per = np.mean([sum(np.linalg.norm(r[(j + 1) % n] - r[j]) for j in range(n)) for r in rings])
    vs = [0.0]
    for i in range(1, len(rings)):
        vs.append(vs[-1] + np.linalg.norm(centres[i] - centres[i - 1]))
    cap0 = caps[0]
    v_off = np.linalg.norm(cap0[0] - centres[0]) if cap0 else 0.0
    ids = [[mesh.vertex(p, infl[i], part, bone[i], centres[i]) for p in r] for i, r in enumerate(rings)]
    us = [per * j / n for j in range(n + 1)]
    for i in range(len(rings) - 1):
        for j in range(n):
            j1 = (j + 1) % n
            a, b, c, d = ids[i][j], ids[i][j1], ids[i + 1][j1], ids[i + 1][j]
            ua, ub = us[j], us[j + 1]
            va, vb = v_off + vs[i], v_off + vs[i + 1]
            mesh.tri((a, c, b), [(ua, va), (ub, vb), (ub, va)], part)
            mesh.tri((a, d, c), [(ua, va), (ua, vb), (ub, vb)], part)
    # Caps: fans to an apex (its texture: a row past the ring, the apex's u per triangle).
    for which, cap in ((0, caps[0]), (1, caps[1])):
        if not cap:
            continue
        apex_p, apex_infl, apex_bone = cap
        ring = 0 if which == 0 else len(rings) - 1
        a = mesh.vertex(apex_p, apex_infl, part, apex_bone, centres[ring])
        vr = v_off + vs[ring]
        va = 0.0 if which == 0 else vr + np.linalg.norm(apex_p - centres[ring])
        for j in range(n):
            j1 = (j + 1) % n
            p, q = ids[ring][j], ids[ring][j1]
            um = 0.5 * (us[j] + us[j + 1])
            if which == 0:
                mesh.tri((a, p, q), [(um, va), (us[j], vr), (us[j + 1], vr)], part)
            else:
                mesh.tri((a, q, p), [(um, va), (us[j + 1], vr), (us[j], vr)], part)
    v_end = v_off + vs[-1] + (np.linalg.norm(caps[1][0] - centres[-1]) if caps[1] else 0.0)
    return per, v_end


def arc_x(u):
    """The knuckle line's x at u across the palm (0 the ulnar edge, 1 the radial): the fingers' MCPs."""
    zs = [(FINGER_SPEC[f]["pivot"][2], FINGER_SPEC[f]["pivot"][0]) for f in ("pinky", "ring", "middle", "index")]
    s0 = PALM_STATIONS[5]
    z = s0["zu"] + u * (s0["zr"] - s0["zu"])
    zz = [p[0] for p in zs]
    xx = [p[1] for p in zs]
    if z <= zz[0]:
        return xx[0] - 0.25 * (zz[0] - z)
    if z >= zz[-1]:
        return xx[-1] - 0.15 * (z - zz[-1])
    return float(np.interp(z, zz, xx))


def palm_point(st, u, v):
    """A point of a palm cross-section: u across (0 ulnar .. 1 radial), v -1 the back .. +1 the palm."""
    z = st["zu"] + u * (st["zr"] - st["zu"])
    w = 1.0 - (2 * u - 1) ** 2  # 1 in the middle, 0 at the edges
    back = st["back"] - st["arch"] * w
    palm = st["palm"] - st["hollow"] * w * (1.0 - 0.6 * u)  # hollow towards the ulnar side, the thenar full
    if u > 0.55:
        palm += 0.25 * st["hollow"] * (u - 0.55) / 0.45  # the thenar's slope
    if u < 0.25:
        palm += 0.35 * st["hollow"] * (0.25 - u) / 0.25  # the hypothenar
    mid = 0.5 * (back + palm)
    y = mid + v * (0.5 * (palm - back)) if v >= 0 else mid - v * (0.5 * (back - palm))
    x = st["x"]
    if "knuckle" in st:
        x = arc_x(u) + st["knuckle"]
        if v < -0.9 and st["knuckle"] < 0:
            y -= 0.12  # the knuckles' ridge over each metacarpal
    return np.array([x, y, z])


def thenar_weight(rig, p):
    c = rig.fingers["thumb"]["points"][0]
    m = rig.fingers["thumb"]["points"][1]
    d = m - c
    ln = np.linalg.norm(d)
    d /= ln
    along = float(np.clip((p - c) @ d / ln, 0.0, 1.0))
    off = np.linalg.norm((p - c) - ((p - c) @ d) * d)
    w = along ** 0.8 * max(0.0, 1.0 - off / THENAR_REACH) ** 2
    return 0.0 if w < 0.03 else min(THENAR_MOST, w)


def thenar_influences(w):
    if w <= 0.0:
        return [(0, 1.0)]
    for (fa, ja), (fb, jb) in zip(THENAR_JOINTS, THENAR_JOINTS[1:]):
        if w <= fb:
            t = (w - fa) / (fb - fa)
            return [(ja, 1.0 - t), (jb, t)] if t > 1e-4 else [(ja, 1.0)]
    return [(THENAR_JOINTS[-1][1], 1.0)]


def build_mesh(rig):
    mesh = Mesh()
    islands = {}

    # The palm.
    rings = [[palm_point(st, u, v) for u, v in PALM_TEMPLATE] for st in PALM_STATIONS]
    first = len(mesh.pos)
    infl = [[None] * len(r) for r in rings]
    wrist_apex = np.mean(rings[0], axis=0) - np.array([0.35, 0.0, 0.0])
    islands["palm"] = loft(mesh, rings, [[(0, 1.0)]] * len(rings), 0, [0] * len(rings), None, None,
                           ((wrist_apex, [(0, 1.0)], 0), (PALM_FRONT, [(0, 1.0)], 0)))
    for i in range(first, len(mesh.pos)):
        mesh.infl[i] = thenar_influences(thenar_weight(rig, mesh.pos[i]))

    # The fingers.
    for fi, name in enumerate(FINGERS[1:], 1):
        spec = FINGER_SPEC[name]
        pts = rig.fingers[name]["points"]
        _, x, y, z = rig.fingers[name]["segments"][0]
        h = spec["half"]
        l1, l2, l3 = spec["length"]
        at = lambda t: pts[0] + x * t
        # (distance along, half-width, back, palm, influences, bone)
        sections = [
            (0.0, h[0], FLAT * h[0] * 1.06, FLAT * h[0], [(half_joint(fi, 0), 1.0)], 1),  # MCP
            (0.5 * l1, 0.5 * (h[0] + h[1]) * 0.97, FLAT * 0.5 * (h[0] + h[1]) * 0.94, FLAT * 0.5 * (h[0] + h[1]) * 1.06,
             [(seg_joint(fi, 1), 1.0)], 1),
            (l1, h[1] * 1.03, FLAT * h[1] * 1.08, FLAT * h[1], [(half_joint(fi, 1), 1.0)], 2),  # PIP
            (l1 + l2, h[2] * 1.02, FLAT * h[2] * 1.06, FLAT * h[2], [(half_joint(fi, 2), 1.0)], 3),  # DIP
            (l1 + l2 + 0.45 * l3, 0.5 * (h[2] + h[3]), FLAT * 0.5 * (h[2] + h[3]) * 0.88, FLAT * 0.5 * (h[2] + h[3]) * 1.1,
             [(seg_joint(fi, 3), 1.0)], 3),
            (l1 + l2 + 0.84 * l3, h[3] * 0.84, FLAT * h[3] * 0.72, FLAT * h[3] * 0.95, [(seg_joint(fi, 3), 1.0)], 3),
        ]
        rings = [ellipse_ring(at(t), x, y, z, hw, bk, pm) for t, hw, bk, pm, _, _ in sections]
        rig.fingers[name]["profile"] = [(at(t), hw, pm) for t, hw, bk, pm, _, _ in sections]
        base = (at(-0.62), [(half_joint(fi, 0), 1.0)], 1)
        tip = (at(l1 + l2 + l3) + y * (0.18 * h[3]), [(seg_joint(fi, 3), 1.0)], 3)
        islands[name] = loft(mesh, rings, [s[4] for s in sections], fi + 1, [s[5] for s in sections], None, None, (base, tip))

    # The thumb.
    tp = rig.fingers["thumb"]["points"]
    ts = rig.fingers["thumb"]["segments"]
    h = THUMB_HALF

    def frame_at(k0, k1, t):
        """A section's frame between segments k0 and k1 (their mean at a joint)."""
        x = unit(ts[k0][1] + ts[k1][1])
        y = ts[k0][2] + ts[k1][2]
        y = unit(y - x * (y @ x))
        return x, y, np.cross(x, y)

    l1, l2, l3 = THUMB_LENGTHS
    sections = []
    x, y, z = frame_at(0, 0, 0)
    sections.append((tp[0] + x * 0.0, (x, y, z), h[0], FLAT * h[0] * 0.95, FLAT * h[0] * 1.05, [(half_joint(0, 0), 1.0)], 1))
    sections.append((tp[0] + x * (0.5 * l1) + y * 0.35 - z * 0.25, (x, y, z), h[1], FLAT * h[1] * 0.8, FLAT * h[1] * 1.3,
                     [(seg_joint(0, 1), 1.0)], 1))
    x, y, z = frame_at(0, 1, 0)
    sections.append((tp[1], (x, y, z), h[2], FLAT * h[2] * 1.08, FLAT * h[2] * 1.02, [(half_joint(0, 1), 1.0)], 2))
    x, y, z = frame_at(1, 2, 0)
    sections.append((tp[2], (x, y, z), h[3], FLAT * h[3] * 1.06, FLAT * h[3], [(half_joint(0, 2), 1.0)], 3))
    x, y, z = ts[2][1], ts[2][2], ts[2][3]
    mid = 0.5 * (h[3] + h[4])
    sections.append((tp[2] + x * (0.45 * l3), (x, y, z), mid, FLAT * mid * 0.86, FLAT * mid * 1.1, [(seg_joint(0, 3), 1.0)], 3))
    sections.append((tp[2] + x * (0.84 * l3), (x, y, z), h[4] * 0.84, FLAT * h[4] * 0.72, FLAT * h[4] * 0.95,
                     [(seg_joint(0, 3), 1.0)], 3))
    rings = [ellipse_ring(c, fx, fy, fz, hw, bk, pm) for c, (fx, fy, fz), hw, bk, pm, _, _ in sections]
    rig.fingers["thumb"]["profile"] = [(c, hw, pm) for c, _, hw, bk, pm, _, _ in sections]
    base = (tp[0] - ts[0][1] * 0.7, [(half_joint(0, 0), 1.0)], 1)
    tip = (tp[3] + ts[2][2] * (0.18 * h[4]), [(seg_joint(0, 3), 1.0)], 3)
    islands["thumb"] = loft(mesh, rings, [s[5] for s in sections], 1, [s[6] for s in sections], None, None, (base, tip))
    return mesh, islands


def check_winding(mesh):
    """Every triangle's normal points away from its cross-sections' middles (a check of the lofts)."""
    P = np.array(mesh.pos)
    C = np.array(mesh.centre)
    bad = 0
    for a, b, c in mesh.tris:
        n = np.cross(P[b] - P[a], P[c] - P[a])
        if n @ ((P[a] + P[b] + P[c]) / 3 - (C[a] + C[b] + C[c]) / 3) < 0:
            bad += 1
    return bad


# ----------------------------------------------------------------------------
# The old models (their placement constants, and their skins to bake)


def read_mdl(path):
    d = open(path, "rb").read()
    h = HEADER.unpack_from(d, 0)
    assert h[0] == b"IDPO" and h[1] == 6, path
    scale, origin = np.array(h[2:5]), np.array(h[5:8])
    numskins, sw, sh, nv, nt, nf = h[12:18]
    skins, o = read_skins(d, HEADER.size, numskins, sw, sh)
    st = [struct.unpack_from("<3i", d, o + 12 * i) for i in range(nv)]
    o += 12 * nv
    tris = [struct.unpack_from("<4i", d, o + 16 * i) for i in range(nt)]
    o += 16 * nt
    frames = []
    for _ in range(nf):
        (kind,) = struct.unpack_from("<i", d, o)
        assert kind == 0, "%s: frame groups not supported" % path
        o += 4 + 8 + 16
        v = np.frombuffer(d, np.uint8, nv * 4, o).reshape(nv, 4)
        o += nv * 4
        frames.append(v[:, :3] * scale + origin)
    for s in skins:
        assert struct.unpack_from("<i", s, 0)[0] == 0, "%s: skin groups not supported" % path
    return dict(origin=origin, skins=[np.frombuffer(s[4:], np.uint8).reshape(sh, sw) for s in skins], sw=sw, sh=sh,
                st=st, tris=tris, frames=frames)


def offset_in_model(offset):
    """A finger offset cvar triple as a displacement in model space (y is to the right: the model's -y)."""
    return np.array([offset[0], -offset[1], offset[2]]) * OFFSET_PER_MODEL


def placement(models, name):
    """Where vr_view.cpp draws a finger model relative to hand_base.mdl at the default settings, in
    hand_base's model space (vr_handrig.cpp computes the same at the current settings)."""
    if name == "hand_base":
        return np.zeros(3)
    finger = name[len("finger_"):]
    off = offset_in_model(np.add(FINGERS_ALL, FINGER_OFFSETS[finger])) - offset_in_model(BASE_OFFSET)
    return (off + (1 - FIST_SCALE) * (models[name]["origin"] - models["hand_base"]["origin"])) / FIST_SCALE


# ----------------------------------------------------------------------------
# The skin: painted here, in the old skin's palette ramp and with its grain, its shading and details laid out on
# this mesh (the old skin is a painting of a whole hand in another pose: projected onto this one, its painted fingers
# and shadows landed in the wrong places). The blood as make_bloody_hands.py paints the six models'.

SKIN_RAMP = (112, 127)  # the skin's palette ramp (dark .. light)
GRAIN = [(140, 20, 255, 56), (110, 70, 186, 118)]  # hand_base.mdl's skin 0: plain skin, the forearm's back and front


def layout_islands(islands, width=512):
    """Places each part's island (hand units -> texels) on the skin: the palm across the top, the fingers and
    the thumb in a row below. Returns {name: (x, y, texels per unit)}, the skin's size."""
    pad = 3
    for dens in np.arange(22.0, 8.0, -0.5):
        pu, pv = islands["palm"]
        pw, ph = int(math.ceil(pu * dens)), int(math.ceil(pv * dens))
        x, y = pad, pad
        layout = {"palm": (x, y, dens)}
        if pw + 2 * pad > width:
            continue
        row_y = y + ph + 2 * pad
        x = pad
        row_h = 0
        ok = True
        for name in ["thumb", "index", "middle", "ring", "pinky"]:
            u, v = islands[name]
            w, h = int(math.ceil(u * dens)), int(math.ceil(v * dens))
            if x + w + pad > width:
                ok = False
                break
            layout[name] = (x, row_y, dens)
            x += w + 2 * pad
            row_h = max(row_h, h)
        if not ok:
            continue
        height = row_y + row_h + pad
        if height <= width:
            size = 256 if height <= 256 else 512
            return layout, (width, size)
    raise RuntimeError("the islands don't fit")


def vertex_normals(mesh):
    """Smooth normals (by area, welded by position), as the engine computes the MD5's."""
    P = np.array(mesh.pos)
    key = [tuple(np.round(p, 5)) for p in P]
    acc = {}
    for a, b, c in mesh.tris:
        n = np.cross(P[b] - P[a], P[c] - P[a])
        for v in (a, b, c):
            acc[key[v]] = acc.get(key[v], 0) + n
    return np.array([unit(acc[k]) for k in key])


def rasterize(mesh, layout, size):
    """Per texel of the skin a triangle covers: its place and normal on the mesh at rest, and its part."""
    W, H = size
    P = np.array(mesh.pos)
    N = vertex_normals(mesh)
    pos_map = np.zeros((H, W, 3))
    nrm_map = np.zeros((H, W, 3))
    part_map = np.full((H, W), -1, int)
    for (a, b, c), uvs, part in zip(mesh.tris, mesh.tuv, mesh.tpart):
        ox, oy, dens = layout["palm" if part == 0 else FINGERS[part - 1]]
        pts = np.array([(ox + u * dens, oy + v * dens) for u, v in uvs])
        (x0, y0), (x1, y1), (x2, y2) = pts
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 1e-9:
            continue
        xs = np.arange(max(0, int(pts[:, 0].min()) - 1), min(W, int(pts[:, 0].max()) + 2))
        ys = np.arange(max(0, int(pts[:, 1].min()) - 1), min(H, int(pts[:, 1].max()) + 2))
        gx, gy = np.meshgrid(xs + 0.5, ys + 0.5)
        l0 = ((x1 - gx) * (y2 - gy) - (x2 - gx) * (y1 - gy)) / area
        l1 = ((x2 - gx) * (y0 - gy) - (x0 - gx) * (y2 - gy)) / area
        l2 = 1 - l0 - l1
        inside = (l0 >= -0.02) & (l1 >= -0.02) & (l2 >= -0.02)
        iy, ix = np.nonzero(inside)
        w = np.stack([l0[iy, ix], l1[iy, ix], l2[iy, ix]], 1)
        pos_map[ys[iy], xs[ix]] = w @ np.array([P[a], P[b], P[c]])
        n = w @ np.array([N[a], N[b], N[c]])
        nrm_map[ys[iy], xs[ix]] = n / np.linalg.norm(n, axis=1, keepdims=True)
        part_map[ys[iy], xs[ix]] = part
    return pos_map, nrm_map, part_map


def grain_of(models):
    """The old skin's grain: two plain patches' levels in the ramp less their local mean (about +-1.5 steps)."""
    lo, hi = SKIN_RAMP
    out = []
    for x0, y0, x1, y1 in GRAIN:
        patch = np.clip(models["hand_base"]["skins"][0][y0:y1, x0:x1].astype(float), lo, hi)
        k = 3
        pad = np.pad(patch, k, mode="reflect")
        mean = np.zeros_like(patch)
        for dy in range(-k, k + 1):
            for dx in range(-k, k + 1):
                mean += pad[k + dy:k + dy + patch.shape[0], k + dx:k + dx + patch.shape[1]]
        raw = models["hand_base"]["skins"][0][y0:y1, x0:x1]
        g = patch - mean / (2 * k + 1) ** 2
        out.append(np.where((raw > lo + 1) & (raw < hi - 1), np.clip(g, -1.8, 1.3), 0.0))  # not the painting's lines
    h = min(g.shape[0] for g in out)
    return np.concatenate([g[:h] for g in out], 1)


def bombed(grain, H, W, cell=22, seed=7):
    """The grain over the whole skin without visible repeats: overlapping cells, each from a random place in it,
    blended (and rescaled to the grain's own strength)."""
    gh, gw = grain.shape
    gy, gx = np.mgrid[0:H, 0:W]
    total = np.zeros((H, W))
    norm = np.zeros((H, W))
    rng = np.random.default_rng(seed)
    offs = rng.integers(0, 1 << 16, size=(2, 2, H // cell + 2, W // cell + 2, 2))
    for i, sy in enumerate((0, cell // 2)):
        for j, sx in enumerate((0, cell // 2)):
            cy, cx = (gy + sy) // cell, (gx + sx) // cell
            fy, fx = ((gy + sy) % cell + 0.5) / cell, ((gx + sx) % cell + 0.5) / cell
            w = (1 - np.abs(2 * fy - 1)) * (1 - np.abs(2 * fx - 1))
            o = offs[i, j, cy, cx]
            total += w * grain[(gy + o[..., 0]) % gh, (gx + o[..., 1]) % gw]
            norm += w * w
    return total / np.sqrt(np.maximum(norm, 1e-6))


def mottle(H, W, scale=28, seed=3):
    """Smooth value noise (-1..1), for the skin's broad unevenness."""
    rng = np.random.default_rng(seed)
    g = rng.uniform(-1, 1, size=(H // scale + 3, W // scale + 3))
    gy, gx = np.mgrid[0:H, 0:W] / scale
    iy, ix = gy.astype(int), gx.astype(int)
    fy, fx = gy - iy, gx - ix
    fy, fx = fy * fy * (3 - 2 * fy), fx * fx * (3 - 2 * fx)
    return ((g[iy, ix] * (1 - fx) + g[iy, ix + 1] * fx) * (1 - fy) + (g[iy + 1, ix] * (1 - fx) + g[iy + 1, ix + 1] * fx) * fy)


def finger_coords(rig, name, p, n):
    """A finger's texels: their segment (0..2), their distance past its joint, how much they face the back (+1) or
    the palm (-1), and sideways (-1..1)."""
    pts = rig.fingers[name]["points"]
    segs = rig.fingers[name]["segments"]
    seg = np.zeros(len(p), int)
    for k in (1, 2):
        seg[(p - pts[k]) @ segs[k][1] > 0] = k
    along = np.zeros(len(p))
    side = np.zeros(len(p))
    lateral = np.zeros(len(p))
    for k in range(3):
        m = seg == k
        _, x, y, z = segs[k]
        along[m] = (p[m] - pts[k]) @ x
        side[m] = -(n[m] @ y)
        lateral[m] = n[m] @ z
    return seg, along, side, lateral


def lines(d, centres, width):
    """1 on thin lines across at distances `centres` (hand units), falling off over `width`."""
    out = np.zeros_like(d)
    for c in centres:
        out = np.maximum(out, np.clip(1.0 - np.abs(d - c) / width, 0.0, 1.0))
    return out


def base_level(side):
    return 118.7 - 1.0 * side  # the palm's side a little lighter than the back


def finger_levels(rig, name, p, n):
    seg, along, side, lateral = finger_coords(rig, name, p, n)
    L = base_level(side)
    back = np.clip((side - 0.35) / 0.3, 0, 1)
    front = np.clip((-side - 0.3) / 0.3, 0, 1)
    pts = rig.fingers[name]["points"]
    lengths = [np.linalg.norm(pts[k + 1] - pts[k]) for k in range(3)]
    thumb = name == "thumb"
    L = L - 1.4 * lateral ** 2 * np.where(thumb & (seg == 0), 0.0, 1.0)  # form: the sides darker (not the thenar)
    # Distances past the second and third joints (negative before them).
    d1 = np.where(seg == 1, along, np.where(seg == 0, along - lengths[0], 9.0))
    d2 = np.where(seg == 2, along, np.where(seg == 1, along - lengths[1], 9.0))
    # The knuckles on the back: a highlight and their wrinkles.
    L += back * 0.6 * np.exp(-(d1 / 0.35) ** 2)
    L -= back * 1.1 * lines(d1, (-0.16, 0.0, 0.15) if not thumb else (-0.12, 0.04), 0.05)
    L -= back * 0.9 * lines(d2, (-0.08, 0.07), 0.045)
    # The joints' creases on the palm's side.
    L -= front * 1.5 * lines(d1, (-0.05, 0.05), 0.04)
    L -= front * 1.4 * lines(d2, (0.0,), 0.045)
    if not thumb:
        d0 = np.where(seg == 0, along, 9.0)
        L -= front * 1.3 * lines(d0, (1.05,), 0.05)  # the finger's root crease
        # between the fingers, where they leave the palm
        L -= 2.4 * np.clip(1.0 - d0 / 1.3, 0, 1) * np.clip(np.abs(lateral) - 0.35, 0, 1) / 0.65
    # The fingertip's pad.
    L += front * 0.4 * ((seg == 2) & (along > 0.3 * lengths[2]))
    # The nail: the back of the distal segment, from 40% of it to the tip: lighter, a darker rim, a light free edge.
    l3 = lengths[2]
    a = (along - 0.40 * l3) / (0.62 * l3)
    nail = (seg == 2) & (a > 0) & (a < 1.05) & (side > 0.5)
    rim = nail & ((side < 0.63) | (a < 0.08))
    edge = nail & (a > 0.86) & ~rim
    L = np.where(nail, L + 1.1, L)
    L = np.where(edge, L + 1.3, L)
    L = np.where(rim, base_level(side) - 1.3, L)
    return L


def palm_levels(rig, p, n):
    side = -n[:, 1]
    x = p[:, 0]
    xs = [st["x"] for st in PALM_STATIONS[:5]] + [4.4, 5.3]
    zu = np.interp(x, xs, [st["zu"] for st in PALM_STATIONS])
    zr = np.interp(x, xs, [st["zr"] for st in PALM_STATIONS])
    u = np.clip((p[:, 2] - zu) / (zr - zu), 0, 1)
    L = base_level(side)
    back = np.clip((side - 0.3) / 0.3, 0, 1)
    front = np.clip((-side - 0.3) / 0.3, 0, 1)
    knuckles = np.array([arc_x(t) for t in u])
    # The back: the tendons over the metacarpals, fading in from the wrist, and the knuckles.
    fade = np.clip((x + 2.5) / 3.0, 0, 1)
    for uf in (0.145, 0.37, 0.62, 0.88):
        L += back * fade * (0.55 * np.exp(-((u - uf) / 0.04) ** 2) - 0.2 * np.exp(-((u - uf - 0.11) / 0.05) ** 2))
        L += back * 0.8 * np.exp(-((u - uf) / 0.06) ** 2 - ((x - knuckles + 0.9) / 0.5) ** 2)
    # The palm's side: its creases (the heart line under the fingers, the head line across, the life line round the
    # ball of the thumb) and the wrist's.
    heart = knuckles - 1.7 - 0.5 * np.clip(u - 0.1, 0, 1)
    L -= front * 1.5 * (u < 0.78) * lines(x - heart, (0.0,), 0.07)
    head = 1.5 - 1.8 * u
    L -= front * 1.3 * ((u > 0.3) & (u < 0.98)) * lines(x - head, (0.0,), 0.07)
    life_u = 0.62 + 0.08 * np.clip(-x - 0.5, 0, 3) - 0.03 * np.clip(x - 0.5, 0, 2)
    L -= front * 1.3 * ((x < 2.0) & (x > -3.3)) * lines(u - life_u, (0.0,), 0.02)
    L -= front * 1.1 * lines(x, (-4.0, -4.35), 0.06)
    # The pads lighter (the ulnar side's); darker into the forearm and round the fingers' roots.
    L += front * 0.35 * np.exp(-((u - 0.12) / 0.12) ** 2 - ((x + 1.5) / 1.8) ** 2)
    L -= 0.8 * np.clip((-4.6 - x) / 1.0, 0, 1)
    L -= 0.5 * np.clip((x - knuckles + 0.2) / 0.5, 0, 1)
    return L


def paint(rig, pos_map, nrm_map, part_map, grain):
    """The skin's level (a float in the palette ramp) per texel."""
    H, W = part_map.shape
    level = np.zeros((H, W))
    for part in range(6):
        sel = part_map == part
        if sel.any():
            p, n = pos_map[sel], nrm_map[sel]
            level[sel] = palm_levels(rig, p, n) if part == 0 else finger_levels(rig, FINGERS[part - 1], p, n)
            if part == 1:
                # The ball of the thumb where it meets the palm: painted as the palm there (no seam where they cross),
                # the thumb's own from past the middle of its metacarpal.
                c, m = rig.fingers["thumb"]["points"][:2]
                t = np.clip(((p - c) @ unit(m - c)) / np.linalg.norm(m - c), 0, 1)
                w = np.clip((t - 0.45) / 0.4, 0, 1)
                level[sel] = w * level[sel] + (1 - w) * palm_levels(rig, p, n)
    return level + 1.1 * bombed(grain, H, W) + 0.8 * mottle(H, W)


def to_palette(level):
    """Levels to palette indices: the skin's ramp, and below it the brown ramp's darks (as the old skin's shadows)."""
    lo, hi = SKIN_RAMP
    L = np.round(level)
    skin = np.clip(L, lo + 2, hi - 1)
    brown = np.clip(23 - (lo + 2 - L), 18, 23)  # 23 (75, 55, 27) sits just under 114 (71, 51, 27)
    return np.where(L >= lo + 2, skin, brown)


def bleed(img, part_map, level):
    """The damage level's blood, as make_bloody_hands.py paints the six models' (per island, their marks)."""
    from make_bloody_hands import mark_texel, marks_for
    H, W = part_map.shape
    out = img.copy()
    for part in range(6):
        used = (part_map == part).ravel()
        levels = marks_for(PARTS[part] if part == 0 else "finger_" + FINGERS[part - 1], used, W)
        marks = sum(levels[:level], [])
        ys, xs = np.nonzero(part_map == part)
        for y, x in zip(ys, xs):
            m = mark_texel(x + 0.5, y + 0.5, marks)
            if m is not None:
                out[y, x] = m
    return out


def dilate(img, covered, steps=4):
    """Grows the islands into their neighbours (filtering and mipmaps never reach an empty texel)."""
    img = img.copy()
    covered = covered.copy()
    for _ in range(steps):
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            src = np.roll(np.roll(img, dy, 0), dx, 1)
            cov = np.roll(np.roll(covered, dy, 0), dx, 1)
            take = ~covered & cov
            img[take] = src[take]
            covered |= take
    return img


# ----------------------------------------------------------------------------
# The grasp solver's spheres (vr_grasp.cpp)


def solver_spheres(rig, mesh):
    """Each segment as a row of spheres along its palm's side, as wide as it is (their palm's side flush with the
    skin, from the sections the mesh was lofted through); the palm's side as spheres under its skin; the ball of
    the thumb's (the thenar) likewise."""
    segs = []
    for fi, name in enumerate(FINGERS):
        pts = rig.fingers[name]["points"]
        cum = np.concatenate([[0.0], np.cumsum([np.linalg.norm(pts[k + 1] - pts[k]) for k in range(3)])])
        prof = rig.fingers[name]["profile"]

        def param(c):
            """A point's place along the finger (the pivots' polyline)."""
            best = (1e9, 0.0)
            for k in range(3):
                d = pts[k + 1] - pts[k]
                t = float(np.clip((c - pts[k]) @ d / (d @ d), 0.0, 1.0))
                e = np.linalg.norm(pts[k] + d * t - c)
                if e < best[0]:
                    best = (e, cum[k] + t * np.linalg.norm(d))
            return best[1]

        ps = [param(c) for c, _, _ in prof]
        hws = [hw for _, hw, _ in prof]
        pms = [pm for _, _, pm in prof]
        for b in (1, 2, 3):
            _, x, y, z = rig.fingers[name]["segments"][b - 1]
            ln = cum[b] - cum[b - 1]
            if fi == 0 and b == 1:
                continue  # the thumb's metacarpal is the ball of the thumb: the thenar's spheres (the palm's place)
            lo, hi = 0.0, ln
            n = max(2, int(math.ceil((hi - lo) / 0.8)) + 1)
            for i in range(n):
                t = lo + (hi - lo) * i / (n - 1)
                hw = float(np.interp(cum[b - 1] + t, ps, hws))
                pm = float(np.interp(cum[b - 1] + t, ps, pms))
                r = min(0.95 * hw, 1.1)
                if b == 3 and i == n - 1:
                    t = hi - 0.6 * r  # the fingertip's, inside its round end
                segs.append((fi, b, pts[b - 1] + x * t + y * (pm - r), r))
    palm, thenar = [], []
    radius = 0.45
    for x in np.arange(-4.2, 5.2, 1.4):
        for u in np.arange(0.07, 0.97, 0.17):
            # the palm's side at (x, u): interpolate the sections
            st = [palm_point(s, u, 1.0) for s in PALM_STATIONS]
            xs = [p[0] for p in st]
            if x < xs[1] or x > xs[5]:
                continue
            p = np.array([np.interp(x, xs, [q[i] for q in st]) for i in range(3)])
            p[0] = x
            c = p - np.array([0.0, radius, 0.0])
            if thenar_weight(rig, p) > 0.0:
                thenar.append((c, radius))
            else:
                palm.append((c, radius))
    # the ball of the thumb itself: along the metacarpal's palm side
    o, x, y, z = rig.fingers["thumb"]["segments"][0]
    ln = np.linalg.norm(rig.fingers["thumb"]["points"][1] - o)
    for t in np.arange(0.25, 0.95, 0.2):
        thenar.append((o + x * (t * ln) + y * (FLAT * THUMB_HALF[1] * 1.1 - 0.6), 0.6))
    return segs, palm, thenar


# ----------------------------------------------------------------------------
# Output


def lmp(path, pixels):
    h, w = pixels.shape
    with open(path, "wb") as f:
        f.write(struct.pack("<ii", w, h) + pixels.tobytes())


def write_placeholder_mdl(path):
    # A single tiny triangle; only its name matters (see the header comment).
    skin_w, skin_h = 8, 8
    data = bytearray(HEADER.pack(b"IDPO", 6, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0,
                                 1, skin_w, skin_h, 3, 1, 1, 0, 0, 1.0))
    data += struct.pack("<i", 0) + bytes(skin_w * skin_h)
    for sv in ((0, 0, 0), (0, 4, 0), (0, 0, 4)):
        data += struct.pack("<3i", *sv)
    data += struct.pack("<4i", 1, 0, 1, 2)
    data += struct.pack("<i", 0)
    data += bytes((0, 0, 0, 0)) + bytes((1, 1, 1, 0))
    data += b"hand_rig".ljust(16, b"\0")
    for v in ((0, 0, 0), (1, 0, 0), (0, 1, 0)):
        data += bytes((v[0], v[1], v[2], 0))
    with open(path, "wb") as f:
        f.write(data)


def fmt(v):
    return " ".join("%.6f" % c for c in v)


def cfloat(x):
    s = "%.7g" % x
    if "e" not in s and "." not in s:
        s += ".0"
    return s + "f"


def cvec(v):
    return "{" + ", ".join(cfloat(c) for c in v) + "}"


def joint_bind(rig, j):
    name, kind, fi, idx, frac = JOINTS[j]
    if kind == PALM:
        return np.zeros(3)
    return rig.pivots[fi, idx - 1 if kind == SEGMENT else idx]


def build(progs):
    models = {n: read_mdl(os.path.join(progs, n + ".mdl")) for n in PARTS}
    shifts = {n: placement(models, n) for n in PARTS}
    rig = Rig()
    mesh, islands = build_mesh(rig)
    return models, shifts, rig, mesh, islands


def main():
    progs = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "quakevr", "progs")
    inc = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "Quake", "vr", "vr_handrig_data.inc")
    models, shifts, rig, mesh, islands = build(progs)
    bad = check_winding(mesh)
    assert bad == 0, "%d triangles face inwards" % bad
    P = np.array(mesh.pos)
    print("mesh: %d vertices, %d triangles (palm %d, fingers %s)" % (
        len(P), len(mesh.tris), sum(1 for p in mesh.tpart if p == 0),
        [sum(1 for p in mesh.tpart if p == i) for i in range(1, 6)]))

    # The skin.
    layout, size = layout_islands(islands)
    W, H = size
    pos_map, nrm_map, part_map = rasterize(mesh, layout, size)
    level = paint(rig, pos_map, nrm_map, part_map, grain_of(models))
    clean = np.where(part_map >= 0, to_palette(level), 0).astype(np.uint8)
    print("skin: %dx%d, %.1f texels a hand unit, %d%% used" % (W, H, layout["palm"][2], 100 * (part_map >= 0).mean()))
    for dmg in range(4):
        img = clean if dmg == 0 else bleed(clean, part_map, dmg)
        lmp(os.path.join(progs, "hand_rig_%02d_00.lmp" % dmg), dilate(img, part_map >= 0))

    # MD5 vertices: one per (position, texture coordinate).
    verts, vkey = [], {}
    tris_out = []
    for (a, b, c), uvs, part in zip(mesh.tris, mesh.tuv, mesh.tpart):
        name = "palm" if part == 0 else FINGERS[part - 1]
        ox, oy, dens = layout[name]
        t = []
        for v, (u, vv) in zip((a, b, c), uvs):
            st = ((ox + u * dens) / W, (oy + vv * dens) / H)
            key = (v, round(st[0], 6), round(st[1], 6))
            if key not in vkey:
                vkey[key] = len(verts)
                verts.append((v, st))
            t.append(vkey[key])
        tris_out.append((t[0], t[2], t[1]))  # clockwise seen from outside, as Quake's models are drawn
    weights, vinfo = [], []
    for v, st in verts:
        vinfo.append((st, len(weights), len(mesh.infl[v])))
        for j, w in mesh.infl[v]:
            weights.append((j, w, P[v] - joint_bind(rig, j)))

    with open(os.path.join(progs, "hand_rig.md5mesh"), "w", newline="\n") as fo:
        fo.write("MD5Version 10\ncommandline \"Misc/quakevr/make_hand_rig.py\"\n\n")
        fo.write("numJoints %d\nnumMeshes 1\n\njoints {\n" % len(JOINTS))
        for j, (name, kind, fi, idx, frac) in enumerate(JOINTS):
            fo.write("\t\"%s\"\t%d ( %s ) ( 0.000000 0.000000 0.000000 )\n" % (name, -1 if j == 0 else 0, fmt(joint_bind(rig, j))))
        fo.write("}\n\nmesh {\n\tshader \"hand_rig\"\n\n")
        fo.write("\tnumverts %d\n" % len(verts))
        for i, (st, start, count) in enumerate(vinfo):
            fo.write("\tvert %d ( %.6f %.6f ) %d %d\n" % (i, st[0], st[1], start, count))
        fo.write("\n\tnumtris %d\n" % len(tris_out))
        for i, t in enumerate(tris_out):
            fo.write("\ttri %d %d %d %d\n" % (i, t[0], t[1], t[2]))
        fo.write("\n\tnumweights %d\n" % len(weights))
        for i, (j, w, p) in enumerate(weights):
            fo.write("\tweight %d %d %.6f ( %s )\n" % (i, j, w, fmt(p)))
        fo.write("}\n")

    with open(os.path.join(progs, "hand_rig.md5anim"), "w", newline="\n") as fo:
        # One frame holding the bind pose (the engine poses the hand: VR_AliasBonePoses).
        fo.write("MD5Version 10\ncommandline \"Misc/quakevr/make_hand_rig.py\"\n\n")
        fo.write("numFrames 1\nnumJoints %d\nframeRate 24\nnumAnimatedComponents %d\n\n" % (len(JOINTS), 6 * len(JOINTS)))
        fo.write("hierarchy {\n")
        for i, (name, *_rest) in enumerate(JOINTS):
            fo.write("\t\"%s\"\t%d 63 %d\n" % (name, -1 if i == 0 else 0, 6 * i))
        fo.write("}\n\nbounds {\n\t( %s ) ( %s )\n}\n\nbaseframe {\n" % (fmt(P.min(0)), fmt(P.max(0))))
        local = [joint_bind(rig, j) if j == 0 else joint_bind(rig, j) - joint_bind(rig, 0) for j in range(len(JOINTS))]
        for lp in local:
            fo.write("\t( %s ) ( 0.000000 0.000000 0.000000 )\n" % fmt(lp))
        fo.write("}\n\nframe 0 {\n")
        for lp in local:
            fo.write("\t%s 0.000000 0.000000 0.000000\n" % fmt(lp))
        fo.write("}\n")
    write_placeholder_mdl(os.path.join(progs, "hand_rig.mdl"))

    # The engine's tables.
    segs, palm_spheres, thenar_spheres = solver_spheres(rig, mesh)
    palm_ids = [i for i in range(len(P)) if mesh.part[i] == 0]
    finger_ids = []
    first = []
    for fi in range(5):
        first.append(len(finger_ids))
        finger_ids += [i for i in range(len(P)) if mesh.part[i] == fi + 1]
    first.append(len(finger_ids))
    pidx = {v: i for i, v in enumerate(palm_ids)}
    fidx = {v: i for i, v in enumerate(finger_ids)}
    ptris = [(pidx[a], pidx[c], pidx[b]) for (a, b, c), part in zip(mesh.tris, mesh.tpart) if part == 0]
    ftris = [(fidx[a], fidx[c], fidx[b]) for (a, b, c), part in zip(mesh.tris, mesh.tpart) if part > 0]

    def infl_c(v):
        inf = mesh.infl[v] + [(0, 0.0)] * (4 - len(mesh.infl[v]))
        return "%d, {%s}, {%s}" % (len(mesh.infl[v]), ", ".join(str(j) for j, _ in inf), ", ".join(cfloat(w) for _, w in inf))

    L = []
    L.append("// vr_handrig_data.inc -- generated by Misc/quakevr/make_hand_rig.py; do not edit.")
    L.append("// The jointed hand (vr_handrig.cpp): hand_base.mdl's model space, hand model units.")
    L.append("")
    L.append("inline constexpr int numJoints = %d; // in progs/hand_rig.md5mesh" % len(JOINTS))
    L.append("inline constexpr int numVertices = %d; // the fingers' (the thumb's first)" % len(finger_ids))
    L.append("inline constexpr int numFrames = %d;" % FRAMES)
    L.append("inline constexpr float fistScale = %s; // the fist slot's Scale the rig was placed at" % cfloat(FIST_SCALE))
    L.append("inline constexpr float offsetPerModel = %s; // weapons::offsetScale() / ModelTransform::k" % cfloat(OFFSET_PER_MODEL))
    L.append("inline constexpr float baseScaleOrigin[3] = %s; // hand_base.mdl's" % cvec(models["hand_base"]["origin"]))
    L.append("inline constexpr float palmCentre[3] = %s; // the palm's middle (cups, the palm's turn): the old hand's" % cvec(PALM_CENTRE))
    L.append("inline constexpr float wrist[3] = %s; // the wrist's middle: where the arm meets the hand" % cvec(WRIST))
    L.append("")
    L.append("// Per finger (thumb, index, middle, ring, pinky): the old finger model's scale origin, and where it sat relative")
    L.append("// to hand_base.mdl at the default settings (the engine moves a finger by what its settings change from these).")
    L.append("inline constexpr float fingerScaleOrigin[5][3] = {%s};" % ", ".join(cvec(models["finger_" + f]["origin"]) for f in FINGERS))
    L.append("inline constexpr float fingerBindShift[5][3] = {%s};" % ", ".join(cvec(shifts["finger_" + f]) for f in FINGERS))
    L.append("inline constexpr float fingerDefaultOffset[5][3] = {%s}; // vr_fingers_* + vr_finger_<name>_*" %
             ", ".join(cvec(np.add(FINGERS_ALL, FINGER_OFFSETS[f])) for f in FINGERS))
    L.append("")
    L.append("// Joints per finger: 1 the knuckle (the thumb's CMC, near the wrist), 2, 3: their pivots, their hinge axes, and")
    L.append("// their turns relative to the segment before, per curl frame (x, y, z, w).")
    L.append("inline constexpr float pivots[5][3][3] = {%s};" % ", ".join(
        "{" + ", ".join(cvec(p) for p in rig.pivots[f]) + "}" for f in range(5)))
    L.append("inline constexpr float axes[5][3][3] = {%s};" % ", ".join(
        "{" + ", ".join(cvec(p) for p in rig.axes[f]) + "}" for f in range(5)))
    L.append("inline constexpr float turns[5][numFrames][3][4] = {")
    for f in range(5):
        L.append("    {" + ", ".join("{" + ", ".join(cvec(rig.turns[f, fr, k]) for k in range(3)) + "}" for fr in range(FRAMES)) + "},")
    L.append("};")
    L.append("")
    L.append("// The MD5's joints: the palm; a finger's segment (finger, segment 1..3); or part of a finger's joint's turn")
    L.append("// (finger, joint 0..2, the share of its turn), after the segment before it.")
    L.append("enum JointKind : unsigned char { PalmJoint, SegmentJoint, PartJoint };")
    L.append("struct Joint")
    L.append("{")
    L.append("    const char* name;")
    L.append("    JointKind kind;")
    L.append("    unsigned char finger, index;")
    L.append("    float share;")
    L.append("};")
    kinds = ["PalmJoint", "SegmentJoint", "PartJoint"]
    L.append("inline constexpr Joint joints[numJoints] = {%s};" % ", ".join(
        "{\"%s\", %s, %d, %d, %s}" % (n, kinds[k], fi, idx, cfloat(fr)) for n, k, fi, idx, fr in JOINTS))
    L.append("")
    L.append("// The fingers' vertices: finger, the segment it rides most (1..3), its place at rest, and its joints (up to 4)")
    L.append("// and their weights.")
    L.append("struct Vertex")
    L.append("{")
    L.append("    unsigned char finger, bone, count;")
    L.append("    short joint[4];")
    L.append("    float weight[4];")
    L.append("    float pos[3];")
    L.append("};")
    L.append("inline constexpr Vertex vertices[numVertices] = {")
    for v in finger_ids:
        L.append("    {%d, %d, %s, %s}," % (mesh.part[v] - 1, mesh.bone[v], infl_c(v), cvec(P[v])))
    L.append("};")
    L.append("inline constexpr int firstVertex[6] = {%s};" % ", ".join(str(x) for x in first))
    L.append("inline constexpr int numFingerTriangles = %d; // by vertex (vertices[]), clockwise seen from outside" % len(ftris))
    L.append("inline constexpr unsigned short fingerTriangles[numFingerTriangles][3] = {%s};" % ", ".join("{%d, %d, %d}" % t for t in ftris))
    L.append("")
    L.append("// The palm's vertices: at rest, the share of the thumb's metacarpal turn it follows (the thenar), its joints.")
    L.append("inline constexpr int numPalmVertices = %d;" % len(palm_ids))
    L.append("struct PalmVertex")
    L.append("{")
    L.append("    float pos[3];")
    L.append("    float thumb;")
    L.append("    unsigned char count;")
    L.append("    short joint[4];")
    L.append("    float weight[4];")
    L.append("};")
    L.append("inline constexpr PalmVertex palmVertices[numPalmVertices] = {%s};" % ", ".join(
        "{%s, %s, %s}" % (cvec(P[v]), cfloat(thenar_weight(rig, P[v])), infl_c(v)) for v in palm_ids))
    L.append("inline constexpr int numPalmTriangles = %d;" % len(ptris))
    L.append("inline constexpr unsigned short palmTriangles[numPalmTriangles][3] = {%s};" % ", ".join("{%d, %d, %d}" % t for t in ptris))
    L.append("")
    L.append("// The grasp solver's spheres (vr_grasp.cpp), at rest: each finger segment's row along its palm's side, the palm's")
    L.append("// side, the ball of the thumb.")
    L.append("struct SegmentSphere")
    L.append("{")
    L.append("    unsigned char finger, bone;")
    L.append("    float c[3];")
    L.append("    float r;")
    L.append("};")
    L.append("inline constexpr int numSegmentSpheres = %d;" % len(segs))
    L.append("inline constexpr SegmentSphere segmentSpheres[numSegmentSpheres] = {%s};" % ", ".join(
        "{%d, %d, %s, %s}" % (f, b, cvec(c), cfloat(r)) for f, b, c, r in segs))
    L.append("inline constexpr int numPalmSpheres = %d;" % len(palm_spheres))
    L.append("inline constexpr float palmSpheres[numPalmSpheres][4] = {%s};" % ", ".join(
        cvec(list(c) + [r]) for c, r in palm_spheres))
    L.append("inline constexpr int numThenarSpheres = %d;" % len(thenar_spheres))
    L.append("inline constexpr float thenarSpheres[numThenarSpheres][4] = {%s};" % ", ".join(
        cvec(list(c) + [r]) for c, r in thenar_spheres))
    with open(inc, "w", newline="\n") as fo:
        fo.write("\n".join(L) + "\n")

    print("hand_rig: %d joints, %d vertices, %d triangles, %d weights; spheres: %d on the segments, %d palm, %d thenar"
          % (len(JOINTS), len(verts), len(tris_out), len(weights), len(segs), len(palm_spheres), len(thenar_spheres)))
    print("-> %s, %s" % (os.path.normpath(progs), os.path.normpath(inc)))


if __name__ == "__main__":
    main()
