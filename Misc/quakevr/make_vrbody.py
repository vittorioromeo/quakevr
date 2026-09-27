#!/usr/bin/env python3
# make_vrbody.py -- generates the prototype skinned body of Quake VR's avatar (Quake/vr/vr_avatar.cpp),
# in three builds (vrbody_lean, vrbody (athletic), vrbody_brawny; vr_body_build picks one):
#   quakevr/progs/<build>.md5mesh, .md5anim  the skeleton and a low-poly body, in the bind pose
#   quakevr/progs/<build>.mdl                a placeholder: Ironwail loads MD5 only as the
#                                            "enhanced" replacement of an existing .mdl
#   quakevr/progs/vrbody_NN_00.tga           their skins (Quake palette colours): skin
#                                            armour * 4 + damage (vr_view.cpp picks it from the
#                                            player's armour and health): damage 0..3 adds scratches
#                                            and blood, armour 1..3 (green, yellow, red) plates the
#                                            torso
#
# Usage: python Misc/quakevr/make_vrbody.py [output progs folder]
#
# Conventions the engine relies on (vr_avatar.cpp):
# - Units are Quake units at vr_world_scale 1 (1 m = 1 / 0.0381 units), feet at z = 0, facing +x,
#   +y is the body's left. The bind pose is a person whose eyes are at 1.646 m.
# - Each bone's local +x points along the bone towards its child (up the spine, down the limbs,
#   forward along the foot). Its local +z is the bone's "hint": forward for the spine and legs (knees
#   bend forward), back for the arms (elbows bend back), up for the clavicles and feet.
# - The engine finds bones by name.

import math
import os
import struct
import sys

from mdlgen import HEADER, add, cross, dot, mul, norm, sub
from mdlpolish import palette

UNITS = 1.0 / 0.0381  # Quake units per metre at vr_world_scale 1

# ----------------------------------------------------------------------------
# Bases and quaternions


def frame(direction, hint):
    """Orthonormal basis (x, y, z): x along `direction`, z towards `hint`."""
    x = norm(direction)
    z = norm(sub(hint, mul(x, dot(hint, x))))
    y = cross(z, x)
    return (x, y, z)


def to_local(basis, v):  # basis^T v
    return (dot(basis[0], v), dot(basis[1], v), dot(basis[2], v))


def quat_from_basis(basis):
    # Rotation matrix with the basis vectors as columns.
    (xx, xy, xz), (yx, yy, yz), (zx, zy, zz) = basis
    m00, m01, m02 = xx, yx, zx
    m10, m11, m12 = xy, yy, zy
    m20, m21, m22 = xz, yz, zz
    tr = m00 + m11 + m22
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        w, x, y, z = 0.25 * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s
    elif m00 > m11 and m00 > m22:
        s = math.sqrt(1.0 + m00 - m11 - m22) * 2
        w, x, y, z = (m21 - m12) / s, 0.25 * s, (m01 + m10) / s, (m02 + m20) / s
    elif m11 > m22:
        s = math.sqrt(1.0 + m11 - m00 - m22) * 2
        w, x, y, z = (m02 - m20) / s, (m01 + m10) / s, 0.25 * s, (m12 + m21) / s
    else:
        s = math.sqrt(1.0 + m22 - m00 - m11) * 2
        w, x, y, z = (m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25 * s
    n = math.sqrt(w * w + x * x + y * y + z * z)
    w, x, y, z = w / n, x / n, y / n, z / n
    if w > 0:  # MD5 stores x y z and rebuilds w as -sqrt(1 - x^2 - y^2 - z^2)
        w, x, y, z = -w, -x, -y, -z
    return (x, y, z)


def mat_mul_basis(parent, child):
    """parent^T * child, as a basis (the child's rotation in the parent's space)."""
    return tuple(to_local(parent, axis) for axis in child)

# ----------------------------------------------------------------------------
# Skeleton (metres)


UP = (0.0, 0.0, 1.0)
FWD = (1.0, 0.0, 0.0)
BACK = (-1.0, 0.0, 0.0)

joints = []  # (name, parent index, position)
index = {}


def joint(name, parent, pos):
    index[name] = len(joints)
    joints.append([name, index[parent] if parent else -1, pos])


joint("pelvis", None, (0.0, 0.0, 0.95))
joint("spine", "pelvis", (-0.01, 0.0, 1.10))
joint("chest", "spine", (-0.01, 0.0, 1.28))
joint("neck", "chest", (-0.01, 0.0, 1.47))
joint("head", "neck", (0.0, 0.0, 1.57))
for side, sy in (("l", 1.0), ("r", -1.0)):
    # A-pose: arms 30 degrees out from vertical.
    shoulder = (-0.01, 0.19 * sy, 1.42)
    elbow = add(shoulder, (0.0, 0.29 * math.sin(math.radians(30)) * sy, -0.29 * math.cos(math.radians(30))))
    wrist = add(elbow, (0.0, 0.26 * math.sin(math.radians(30)) * sy, -0.26 * math.cos(math.radians(30))))
    joint("clavicle_" + side, "chest", (0.0, 0.03 * sy, 1.43))
    joint("upperarm_" + side, "clavicle_" + side, shoulder)
    joint("forearm_" + side, "upperarm_" + side, elbow)
    joint("hand_" + side, "forearm_" + side, wrist)
for side, sy in (("l", 1.0), ("r", -1.0)):
    joint("thigh_" + side, "pelvis", (0.0, 0.09 * sy, 0.92))
    joint("calf_" + side, "thigh_" + side, (0.0, 0.09 * sy, 0.50))
    joint("foot_" + side, "calf_" + side, (-0.02, 0.09 * sy, 0.08))

TOE = {"l": (0.15, 0.09, 0.02), "r": (0.15, -0.09, 0.02)}


def bone_basis(name):
    pos = joints[index[name]][2]
    child = {
        "pelvis": "spine", "spine": "chest", "chest": "neck", "neck": "head",
        "clavicle_l": "upperarm_l", "upperarm_l": "forearm_l", "forearm_l": "hand_l",
        "clavicle_r": "upperarm_r", "upperarm_r": "forearm_r", "forearm_r": "hand_r",
        "thigh_l": "calf_l", "calf_l": "foot_l", "thigh_r": "calf_r", "calf_r": "foot_r",
    }
    if name == "head":
        return frame(UP, FWD)
    if name.startswith("hand_"):
        forearm = joints[index["forearm_" + name[-1]]][2]
        return frame(sub(pos, forearm), BACK)
    if name.startswith("foot_"):
        return frame(sub(TOE[name[-1]], pos), UP)
    direction = sub(joints[index[child[name]]][2], pos)
    if name.startswith("clavicle_"):
        return frame(direction, UP)
    if name.startswith(("upperarm_", "forearm_")):
        return frame(direction, BACK)
    return frame(direction, FWD)  # spine chain and legs


bases = {name: bone_basis(name) for name, _, _ in joints}

# ----------------------------------------------------------------------------
# Mesh: lofted rings of 8 vertices, with caps

SIDES = 8
PAD = 2.0 / 128  # texture coordinates kept off each block's edges (4 texels of the 256 skins)

# Skin blocks (u0, v0, u1, v1): skin (the arms), the head, the torso ("leather": the ranger's vest, belt and the top of
# his trousers), the legs ("cloth": trousers and thigh plates), the feet and the boots' shafts, the
# bracers.
BLOCKS = {
    "skin": (0.0, 0.0, 0.5, 0.4),
    "head": (0.0, 0.4, 0.5, 0.5),
    "leather": (0.5, 0.0, 1.0, 0.5),
    "cloth": (0.0, 0.5, 0.5, 1.0),
    "boots": (0.5, 0.5, 0.625, 1.0),
    "shaft": (0.625, 0.5, 0.75, 1.0),
    "bracer": (0.75, 0.5, 1.0, 1.0),
}

verts = []  # (position, [(joint index, bias)], (s, t))
tris = []


def add_vert(pos, weights, st):
    verts.append((pos, weights, st))
    return len(verts) - 1


def loft(rings, block, cap_start=True, cap_end=True, sides=SIDES, per=1, power=2.0, vrange=(0.0, 1.0)):
    """rings: list of (centre, u axis, v axis, radius along u, radius along v, weights). `per`: the loft is drawn
    with that many spans between each two rings given (a number, or one per gap), the rings in between on a smooth
    curve through them (refine()); the texture runs along the rings given as it would without them. `power`: the
    rings' shape, 2 an ellipse, more a fuller, squarer one (a superellipse through the same extremes). `vrange`: the
    part of the block's height the texture runs along (a band painted from one row of the torso's)."""
    rings, vs = refine(rings, per)
    u0, v0, u1, v1 = BLOCKS[block]
    # Keep away from the block edges so filtering does not bleed into the next block.
    u0, v0, u1, v1 = u0 + PAD, v0 + PAD, u1 - PAD, v1 - PAD
    # Front faces are clockwise seen from outside (Ironwail: glFrontFace(GL_CW)), which the
    # triangles below are when the loft runs along u x v (the direction the rings go around);
    # otherwise go around the other way.
    along = sub(rings[-1][0], rings[0][0])
    if dot(cross(rings[0][1], rings[0][2]), along) < 0:
        rings = [(c, ua, mul(va, -1.0), ru, rv, ws) for c, ua, va, ru, rv, ws in rings]
    # Each ring has one vertex more than it has sides: the last is the first again, at u = 1, so that
    # the texture wraps round once (the engine's normals weld vertices by position).
    first = len(verts)
    n = sides + 1
    for r, (centre, ua, va, ru, rv, weights) in enumerate(rings):
        for k in range(n):
            a = 2 * math.pi * (k % sides) / sides
            ca, sa = math.cos(a), math.sin(a)
            if power != 2.0:
                ca = math.copysign(abs(ca) ** (2.0 / power), ca)
                sa = math.copysign(abs(sa) ** (2.0 / power), sa)
            p = add(centre, add(mul(ua, ca * ru), mul(va, sa * rv)))
            st = (u0 + (u1 - u0) * k / sides, v0 + (v1 - v0) * (vrange[0] + (vrange[1] - vrange[0]) * vs[r]))
            add_vert(p, weights, st)
    for r in range(len(rings) - 1):
        for k in range(sides):
            a = first + r * n + k
            b = first + r * n + k + 1
            c = first + (r + 1) * n + k
            d = first + (r + 1) * n + k + 1
            tris.append((a, c, b))
            tris.append((b, c, d))
    mid = ((u0 + u1) / 2, (v0 + v1) / 2)
    if cap_start:
        centre, _, _, _, _, weights = rings[0]
        c = add_vert(centre, weights, mid)
        for k in range(sides):
            tris.append((c, first + k, first + (k + 1) % sides))
    if cap_end:
        centre, _, _, _, _, weights = rings[-1]
        c = add_vert(centre, weights, mid)
        base = first + (len(rings) - 1) * n
        for k in range(sides):
            tris.append((c, base + (k + 1) % sides, base + k))


def refine(rings, per):
    """The rings with more between each two (`per` spans a gap: a number or one per gap): centres and radii on
    Catmull-Rom curves through the rings given (so they keep their places and sizes: the holster plates and the
    flashlight clip are worked out from them), axes and weights blended. Also each ring's place along the texture
    (0 .. 1, the rings given evenly spaced as before)."""
    gaps = len(rings) - 1
    counts = per if isinstance(per, (list, tuple)) else [per] * gaps
    assert len(counts) == gaps
    out, vs = [], []

    def cr(p0, p1, p2, p3, t):
        return tuple(0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (-a + 3 * b - 3 * c + d) * t ** 3)
                     for a, b, c, d in zip(p0, p1, p2, p3))

    for i in range(gaps):
        r0 = rings[max(0, i - 1)]
        r1, r2 = rings[i], rings[i + 1]
        r3 = rings[min(gaps, i + 2)]
        for j in range(counts[i]):
            t = j / counts[i]
            if j == 0:
                out.append(r1)
            else:
                c = cr(r0[0], r1[0], r2[0], r3[0], t)
                ru, rv = cr((r0[3], r0[4]), (r1[3], r1[4]), (r2[3], r2[4]), (r3[3], r3[4]), t)
                ua = norm(add(mul(r1[1], 1 - t), mul(r2[1], t)))
                va = add(mul(r1[2], 1 - t), mul(r2[2], t))
                va = norm(sub(va, mul(ua, dot(va, ua))))
                ws = {}
                for (jn, b) in r1[5]:
                    ws[jn] = ws.get(jn, 0.0) + b * (1 - t)
                for (jn, b) in r2[5]:
                    ws[jn] = ws.get(jn, 0.0) + b * t
                out.append((c, ua, va, ru, rv, sorted(ws.items())))
            vs.append((i + t) / gaps if gaps else 0.0)
    out.append(rings[-1])
    vs.append(1.0)
    return out, vs


def w(*pairs):
    return [(index[n], b) for n, b in pairs]


# Builds (make_vrbody.py writes one model each): muscularity scales the arms' girth (the forearms
# a little more, the wrists much less: bones do not grow), and some of it the chest and thighs.
BUILDS = (("_lean", 0.9), ("", 1.2), ("_brawny", 1.5))


def build_mesh(m):
    verts.clear()
    tris.clear()
    torso = 1.0 + (m - 1.0) * 0.35
    fm = m * 1.08               # forearms
    wm = 1.0 + (m - 1.0) * 0.3  # wrists

    # Torso: vertical rings (u forward, v left).
    X, Y = (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)
    loft([
        ((-0.01, 0.0, 0.86), X, Y, 0.10, 0.15, w(("pelvis", 1.0))),
        ((-0.01, 0.0, 0.95), X, Y, 0.115, 0.17, w(("pelvis", 1.0))),
        ((-0.01, 0.0, 1.05), X, Y, 0.11, 0.155, w(("pelvis", 0.5), ("spine", 0.5))),
        ((-0.01, 0.0, 1.15), X, Y, 0.105 * torso, 0.15 * torso, w(("spine", 1.0))),
        ((-0.01, 0.0, 1.25), X, Y, 0.12 * torso, 0.17 * torso, w(("spine", 0.4), ("chest", 0.6))),
        ((-0.01, 0.0, 1.35), X, Y, 0.13 * torso, 0.19 * torso, w(("chest", 1.0))),
        ((-0.01, 0.0, 1.43), X, Y, 0.11 * torso, 0.17 * torso, w(("chest", 1.0))),
        ((-0.01, 0.0, 1.47), X, Y, 0.06, 0.07, w(("chest", 0.5), ("neck", 0.5))),
    ], "leather", sides=16, per=[1, 1, 2, 2, 2, 2, 1])
    # The belt: a band standing out of the torso where the belt is painted (10-20% up the torso's texture: the
    # rings from 0.925 to 0.99 m), its outside painted from the belt's row.
    def belt_ring(z, out, weights):
        # The torso's ellipse at z (between its rings at 0.86, 0.95 and 1.05 m), `out` further out.
        k = (z - 0.86) / 0.09 if z < 0.95 else (z - 0.95) / 0.10
        rx = 0.10 + 0.015 * k if z < 0.95 else 0.115 - 0.005 * k
        ry = 0.15 + 0.02 * k if z < 0.95 else 0.17 - 0.015 * k
        return ((-0.01, 0.0, z), X, Y, rx + out, ry + out, weights)
    loft([
        belt_ring(0.925, -0.004, w(("pelvis", 1.0))),  # its edges tucked into the torso: no slit under them
        belt_ring(0.93, 0.008, w(("pelvis", 1.0))),
        belt_ring(0.985, 0.008, w(("pelvis", 0.83), ("spine", 0.17))),
        belt_ring(0.99, -0.004, w(("pelvis", 0.8), ("spine", 0.2))),
    ], "leather", cap_start=False, cap_end=False, sides=16, vrange=(0.125, 0.175))
    # Neck and head.
    loft([
        ((-0.01, 0.0, 1.47), X, Y, 0.05 * torso, 0.055 * torso, w(("neck", 1.0))),
        ((-0.01, 0.0, 1.55), X, Y, 0.05 * torso, 0.055 * torso, w(("neck", 0.5), ("head", 0.5))),
        ((0.0, 0.0, 1.60), X, Y, 0.095, 0.085, w(("head", 1.0))),
        ((0.0, 0.0, 1.68), X, Y, 0.10, 0.09, w(("head", 1.0))),
        ((-0.01, 0.0, 1.75), X, Y, 0.08, 0.07, w(("head", 1.0))),
    ], "head", cap_start=False, sides=10, per=2)

    for side, sy in (("l", 1.0), ("r", -1.0)):
        shoulder = joints[index["upperarm_" + side]][2]
        elbow = joints[index["forearm_" + side]][2]
        wrist = joints[index["hand_" + side]][2]
        upper = bases["upperarm_" + side]
        fore = bases["forearm_" + side]
        # Ring axes: u towards the bone's hint (back of the upper arm; the little finger's side
        # of the forearm, the palms facing the thighs), v the other way round. A deltoid over the
        # shoulder, biceps and triceps (front to back), forearms thick below the elbow and flat
        # at the wrist (wider across than through). The wrist ring bends with the hand, and the
        # bracer ends in a cuff over the base of the hand (the hand bone), so that a bent wrist
        # does not open a gap between the arm and the hand.
        ua, va = upper[2], upper[1]
        fa, fv = fore[2], fore[1]
        ud, fd = upper[0], fore[0]
        ua_, cl_ = "upperarm_" + side, "clavicle_" + side
        fo_, ha_ = "forearm_" + side, "hand_" + side
        loft([
            (sub(shoulder, mul(ud, 0.05)), ua, va, 0.055 * m, 0.055 * m, w((cl_, 0.6), (ua_, 0.4))),
            (add(shoulder, mul(ud, 0.00)), ua, va, 0.070 * m, 0.066 * m, w((cl_, 0.25), (ua_, 0.75))),
            (add(shoulder, mul(ud, 0.06)), ua, va, 0.068 * m, 0.062 * m, w((ua_, 1.0))),
            (add(shoulder, mul(ud, 0.13)), ua, va, 0.064 * m, 0.052 * m, w((ua_, 1.0))),
            (add(shoulder, mul(ud, 0.21)), ua, va, 0.054 * m, 0.047 * m, w((ua_, 1.0))),
            (elbow, ua, va, 0.046 * m, 0.046 * m, w((ua_, 0.5), (fo_, 0.5))),
            (add(elbow, mul(fd, 0.05)), fa, fv, 0.052 * fm, 0.050 * fm, w((fo_, 1.0))),
            (add(elbow, mul(fd, 0.12)), fa, fv, 0.046 * fm, 0.040 * fm, w((fo_, 1.0))),
            (add(elbow, mul(fd, 0.19)), fa, fv, 0.037 * wm, 0.029 * wm, w((fo_, 0.8), (ha_, 0.2))),
            (wrist, fa, fv, 0.031 * wm, 0.022 * wm, w((fo_, 0.4), (ha_, 0.6))),
        ], "skin", sides=12, per=[2, 1, 1, 1, 2, 2, 1, 1, 1])
        # A leather bracer over the forearm's lower half (its strap shows how the forearm turns),
        # ending in a cuff over the base of the hand.
        loft([
            (add(elbow, mul(fd, 0.13)), fa, fv, 0.049 * fm, 0.043 * fm, w((fo_, 1.0))),
            (add(elbow, mul(fd, 0.19)), fa, fv, 0.041 * wm, 0.033 * wm, w((fo_, 0.8), (ha_, 0.2))),
            (wrist, fa, fv, 0.036 * wm, 0.027 * wm, w((fo_, 0.4), (ha_, 0.6))),
            (add(wrist, mul(fd, 0.03)), fa, fv, 0.037 * wm, 0.030 * wm, w((ha_, 1.0))),  # flared: the hand's base stays inside
        ], "bracer", sides=12)

        hip = joints[index["thigh_" + side]][2]
        knee = joints[index["calf_" + side]][2]
        ankle = joints[index["foot_" + side]][2]
        thigh = bases["thigh_" + side]
        calf = bases["calf_" + side]
        # Sturdy legs, as Quake's models have (u front to back, v across: the thighs just clear
        # each other).
        loft([
            (add(hip, (0.0, 0.0, 0.03)), thigh[2], thigh[1], 0.100 * torso, 0.088 * torso, w(("thigh_" + side, 1.0))),
            (add(hip, (0.0, 0.0, -0.18)), thigh[2], thigh[1], 0.090 * torso, 0.080 * torso, w(("thigh_" + side, 1.0))),
            (knee, thigh[2], thigh[1], 0.064, 0.062, w(("thigh_" + side, 0.5), ("calf_" + side, 0.5))),
            (add(knee, (0.0, 0.0, -0.15)), calf[2], calf[1], 0.066 * torso, 0.060 * torso, w(("calf_" + side, 1.0))),
            (add(ankle, (0.0, 0.0, 0.04)), calf[2], calf[1], 0.047, 0.046, w(("calf_" + side, 0.5), ("foot_" + side, 0.5))),
        ], "cloth", sides=12, per=[1, 2, 2, 1])
        # The ranger's tall boots: a shaft over the calf, from the ankle to below the knee, flared at
        # its top.
        loft([
            (add(ankle, (0.0, 0.0, 0.01)), calf[2], calf[1], 0.060, 0.057, w(("calf_" + side, 0.4), ("foot_" + side, 0.6))),
            (add(ankle, (0.0, 0.0, 0.12)), calf[2], calf[1], 0.062 * torso, 0.059 * torso, w(("calf_" + side, 1.0))),
            (add(knee, (0.0, 0.0, -0.13)), calf[2], calf[1], 0.075 * torso, 0.069 * torso, w(("calf_" + side, 1.0))),
            (add(knee, (0.0, 0.0, -0.09)), calf[2], calf[1], 0.081 * torso, 0.075 * torso, w(("calf_" + side, 1.0))),
        ], "shaft", cap_start=False, cap_end=False, sides=12)
        # Foot: rings along x (u up, v left), squarer than ellipses (a boot's flat sole and upright sides): the heel,
        # the instep, the ball of the foot, a rounded toe cap.
        Z = (0.0, 0.0, 1.0)
        loft([
            ((-0.075, 0.09 * sy, 0.05), Z, Y, 0.045, 0.042, w(("foot_" + side, 1.0))),
            ((-0.062, 0.09 * sy, 0.055), Z, Y, 0.055, 0.05, w(("foot_" + side, 1.0))),
            ((0.03, 0.09 * sy, 0.05), Z, Y, 0.05, 0.056, w(("foot_" + side, 1.0))),
            ((0.12, 0.09 * sy, 0.036), Z, Y, 0.036, 0.052, w(("foot_" + side, 1.0))),
            ((0.17, 0.09 * sy, 0.03), Z, Y, 0.028, 0.045, w(("foot_" + side, 1.0))),
            ((0.19, 0.09 * sy, 0.027), Z, Y, 0.02, 0.03, w(("foot_" + side, 1.0))),
        ], "boots", sides=10, power=2.6)


# ----------------------------------------------------------------------------
# Output


def fmt(v):
    return " ".join("%.6f" % c for c in v)


def write_md5mesh(path):
    weights = []  # (joint, bias, local position)
    vert_lines = []
    for i, (pos, ws, st) in enumerate(verts):
        start = len(weights)
        total = sum(b for _, b in ws)
        for j, b in ws:
            jp = joints[j][2]
            local = to_local(bases[joints[j][0]], sub(pos, jp))
            weights.append((j, b / total, mul(local, UNITS)))
        vert_lines.append("\tvert %d ( %.6f %.6f ) %d %d" % (i, st[0], st[1], start, len(ws)))

    with open(path, "w", newline="\n") as f:
        f.write("MD5Version 10\ncommandline \"Misc/quakevr/make_vrbody.py\"\n\n")
        f.write("numJoints %d\nnumMeshes 1\n\njoints {\n" % len(joints))
        for name, parent, pos in joints:
            f.write("\t\"%s\"\t%d ( %s ) ( %s )\n" % (name, parent, fmt(mul(pos, UNITS)), fmt(quat_from_basis(bases[name]))))
        f.write("}\n\nmesh {\n\tshader \"vrbody\"\n\n")
        f.write("\tnumverts %d\n%s\n\n" % (len(verts), "\n".join(vert_lines)))
        f.write("\tnumtris %d\n" % len(tris))
        for i, t in enumerate(tris):
            f.write("\ttri %d %d %d %d\n" % (i, t[0], t[1], t[2]))
        f.write("\n\tnumweights %d\n" % len(weights))
        for i, (j, b, p) in enumerate(weights):
            f.write("\tweight %d %d %.6f ( %s )\n" % (i, j, b, fmt(p)))
        f.write("}\n")


def write_md5anim(path):
    # One frame holding the bind pose. Ironwail reads only the animated components, so every
    # component is animated.
    lines = []
    for name, parent, pos in joints:
        if parent < 0:
            lp, lq = mul(pos, UNITS), quat_from_basis(bases[name])
        else:
            pname, _, ppos = joints[parent]
            lp = mul(to_local(bases[pname], sub(pos, ppos)), UNITS)
            lq = quat_from_basis(mat_mul_basis(bases[pname], bases[name]))
        lines.append((lp, lq))

    mins = [min(v[0][k] for v in verts) * UNITS for k in range(3)]
    maxs = [max(v[0][k] for v in verts) * UNITS for k in range(3)]
    with open(path, "w", newline="\n") as f:
        f.write("MD5Version 10\ncommandline \"Misc/quakevr/make_vrbody.py\"\n\n")
        f.write("numFrames 1\nnumJoints %d\nframeRate 24\nnumAnimatedComponents %d\n\n" % (len(joints), 6 * len(joints)))
        f.write("hierarchy {\n")
        for i, (name, parent, _) in enumerate(joints):
            f.write("\t\"%s\"\t%d 63 %d\n" % (name, parent, 6 * i))
        f.write("}\n\nbounds {\n\t( %s ) ( %s )\n}\n\nbaseframe {\n" % (fmt(mins), fmt(maxs)))
        for lp, lq in lines:
            f.write("\t( %s ) ( %s )\n" % (fmt(lp), fmt(lq)))
        f.write("}\n\nframe 0 {\n")
        for lp, lq in lines:
            f.write("\t%s %s\n" % (fmt(lp), fmt(lq)))
        f.write("}\n")


# The skins, painted texel by texel in Quake's palette (every texel one of its colours, never a fullbright one): the
# ranger's clothes (progs/player.mdl's skin: his arms 116-123, the olive vest 19-27, red-brown camouflage trousers
# 97-104, olive thigh plates, dark brown belt, boots and bracers 171-174), the worn armour in the armour's colours
# (progs/armor.mdl's skins), the damage (scratches, blood). Each block is painted as it is wrapped round the body
# (loft(): u round the ring from its first axis, v along the loft), with the light from above painted in as Quake's
# skins have it: folds and quilting lit on their upper side, seams, stitches and the shadows where parts meet.

SKIN_SIZE = 256
PALETTE = [tuple(int(c) for c in p) for p in palette()]

SKIN = [116, 117, 118, 119, 120, 121, 122, 123]             # the arms (and the face), dark to light
HAIR = [0, 16, 17, 18, 19, 97]
LEATHER = [16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27]  # the olive vest
CLOTH = [96, 97, 98, 99, 100, 101, 102, 103, 104]            # the red-brown trousers
BROWN = [0, 175, 174, 173, 114, 172, 171, 170]               # dark leather: belt, boots, bracers
METAL = [2, 4, 6, 8, 10, 12]                                 # buckles, rivets
LACE = [25, 27, 29, 30, 31]

# The armour's colours (progs/armor.mdl's skins 0-2): green, yellow, red, dark to light.
ARMOR_RAMPS = {
    1: [189, 188, 187, 186, 185, 184, 183, 182, 181, 180],
    2: [207, 206, 205, 204, 203, 202, 201, 200, 199, 198],
    3: [67, 69, 70, 71, 72, 73, 74, 76, 77, 79],
}


def h01(x, y, seed=0):
    """A hash of integer (x, y) in 0..1."""
    n = (x * 374761393 + y * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def vnoise(x, y, seed=0):
    """Smooth value noise (a lattice of hashes, smoothly blended)."""
    xi, yi = math.floor(x), math.floor(y)
    fx, fy = x - xi, y - yi
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = h01(xi, yi, seed), h01(xi + 1, yi, seed)
    c, d = h01(xi, yi + 1, seed), h01(xi + 1, yi + 1, seed)
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy


def pick(ramp, x, dither):
    """The ramp's index at x (0 dark .. 1 light), dithered: `dither` (0..1, per texel) nudges it a step."""
    f = max(0.0, min(1.0, x)) * (len(ramp) - 1)
    k = int(f)
    k += 1 if dither < f - k else 0
    return ramp[max(0, min(len(ramp) - 1, k))]


def block_uv(block, s, t):
    u0, v0, u1, v1 = BLOCKS[block]
    return (s - u0 - PAD) / (u1 - u0 - 2 * PAD), (t - v0 - PAD) / (v1 - v0 - 2 * PAD)


def camo(s, t):
    """The trousers' camouflage: -1 (dark) .. 1 (light) blotches, in whole-skin coordinates."""
    n = (math.sin(s * 41.0 + 1.3 * math.sin(t * 29.0)) + math.sin(t * 37.0 + 1.7 * math.sin(s * 23.0)) +
         0.6 * math.sin((s + t) * 67.0))
    return n / 2.6


def around(bu, at):
    """How far round the ring (0 .. 0.5) bu is from `at`."""
    d = abs(bu - at) % 1.0
    return min(d, 1.0 - d)


def arm_texel(bu, bv, d, s, t):
    # u: 0 the back of the upper arm (the elbow's side), 0.5 its front; v: the shoulder (0) to the wrist (1), the elbow
    # at 5/9. Muscles lit where they bulge (the deltoid, biceps and triceps, the forearm's top), shadowed between
    # them and in the elbow's crook.
    x = 0.52 + 0.08 * (vnoise(s * 60, t * 60, 3) - 0.5)
    if bv < 0.12:
        x += 0.12 * (1 - bv / 0.12)                                  # the shoulder's cap
    if bv < 0.5:
        k = math.sin(math.pi * bv / 0.5)
        x += 0.1 * k * (1 - around(bu, 0.5) * 4) if around(bu, 0.5) < 0.25 else 0.0  # biceps
        x += 0.08 * k * (1 - around(bu, 0.0) * 4) if around(bu, 0.0) < 0.25 else 0.0  # triceps
        x -= 0.1 * k if abs(around(bu, 0.25) - 0.0) < 0.03 or around(bu, 0.75) < 0.03 else 0.0
    if 0.5 < bv < 0.62:
        e = 1 - abs(bv - 0.556) / 0.06
        if around(bu, 0.5) < 0.15:
            x -= 0.25 * e                                            # the crook of the elbow
        elif around(bu, 0.0) < 0.1:
            x += 0.12 * e                                            # the elbow's point
    if bv > 0.62:
        x += 0.08 * (1 - around(bu, 0.25) * 4) if around(bu, 0.25) < 0.25 else 0.0  # the forearm's top
    return pick(SKIN, x, d)


def head_texel(bu, bv, d, s, t):
    # u: 0 the face's middle (forward), v: the base of the neck (0) to the crown (1): the jaw at 0.5, the eyes 0.75.
    f = around(bu, 0.0)
    if bv > 0.86 or (bv > 0.62 and f > 0.2) or (bv > 0.8 and f > 0.12):
        return pick(HAIR, 0.45 + 0.35 * vnoise(s * 90, t * 30, 5) - (0.25 if bv < 0.7 else 0.0), d)  # short hair
    x = 0.5 + 0.06 * (vnoise(s * 60, t * 60, 4) - 0.5)
    if bv < 0.4:
        x -= 0.12 * (1 - bv / 0.4) + (0.1 if f < 0.08 and bv > 0.3 else 0.0)  # the neck, the shadow under the jaw
    if f < 0.16:
        if 0.73 < bv < 0.77 and 0.03 < f < 0.062:
            return SKIN[0] if 0.04 < f < 0.055 else SKIN[1]          # the eyes, deep set
        if 0.78 <= bv < 0.81 and 0.015 < f < 0.08:
            return HAIR[1]                                           # the brows
        if 0.6 < bv < 0.72 and f < 0.018:
            x += 0.15                                                # the nose's ridge
        if 0.59 < bv < 0.61 and f < 0.035:
            return SKIN[1]                                           # the mouth
        if 0.46 < bv < 0.6 and f > 0.02:
            x -= 0.1 * vnoise(s * 200, t * 200, 6)                   # stubble on the jaw
    return pick(SKIN, x, d)


def vest_texel(bu, bv, d, s, t):
    """The torso block without armour: the top of the trousers, the belt, the quilted vest, the collar."""
    front = around(bu, 0.0)
    if bv < 0.1:
        return trousers_texel(bu, bv * 0.2, d, s, t, top=True)
    if bv < 0.2:  # the belt, its buckle in front, stitched along both edges
        if front < 0.045:
            if front < 0.03 and 0.125 < bv < 0.175:
                edge = front > 0.022 or bv < 0.133 or bv > 0.167
                return pick(METAL, 0.85 if edge and bv > 0.15 else 0.55 if edge else 0.15, d)  # the buckle's frame
            return pick(BROWN, 0.45, d)
        if bv < 0.112 or bv > 0.188:
            return BROWN[1]
        if (bv < 0.122 or bv > 0.178) and int(bu * 180) % 2 == 0:
            return BROWN[5]                                          # stitches
        if 0.24 < bu < 0.27 or 0.73 < bu < 0.76:
            return pick(BROWN, 0.25, d)                              # belt loops
        return pick(BROWN, 0.4 + 0.2 * (bv - 0.1) / 0.1 + 0.12 * (vnoise(s * 80, t * 40, 7) - 0.5), d)
    if bv > 0.9:  # the collar, rolled: lit along its top
        return pick(BROWN, 0.75 if bv > 0.95 else 0.45 if bv > 0.92 else 0.2, d)
    # Quilted: vertical channels, lit in their middles, stitched between; horizontal stitch rows.
    ch = (bu * 18.0) % 1.0
    x = 0.4 + 0.14 * math.sin(math.pi * ch)
    if ch < 0.05 or ch > 0.95:
        x = 0.22
    row = (bv - 0.2) % 0.1
    if row < 0.012:
        x = min(x, 0.2) if int(bu * 200) % 2 == 0 else x - 0.1
    elif row < 0.03:
        x -= 0.08                                                    # the padding pinched under the stitch row
    x += 0.1 * (vnoise(s * 40, t * 40, 8) - 0.5)
    # Shadows where the vest meets the belt, the collar and the arms.
    if bv < 0.25:
        x -= 0.25 * (0.25 - bv) / 0.05
    if bv > 0.85:
        x -= 0.2 * (bv - 0.85) / 0.05
    if bv > 0.7 and (around(bu, 0.25) < 0.06 or around(bu, 0.75) < 0.06):
        x -= 0.18 * (bv - 0.7) / 0.2
    if front < 0.02:  # the lacing down the front: crossing laces over a dark slit
        k = (bv * 45) % 1.0
        if abs(front / 0.02 - k) < 0.22 or abs(front / 0.02 - (1 - k)) < 0.22:
            return pick(LACE, 0.3 + 0.5 * (1 - k), d)
        return LEATHER[0]
    if around(bu, 0.25) < 0.008 or around(bu, 0.75) < 0.008:  # side seams
        return LEATHER[1]
    if front > 0.3 and abs(bv - 0.8) < 0.008:  # the back's yoke
        return LEATHER[2]
    return pick(LEATHER, x, d)


def armor_texel(bu, bv, armor, d, s, t):
    """The armour's colour at (bu, bv) of the torso block, or None where it leaves the torso bare
    (the belt at the bottom, the collar at the top): five overlapping lames, each lit along its rolled
    lower edge and shadowed under the lame above, riveted at their ends; a strap under each arm."""
    if bv < 0.2 or bv > 0.9:
        return None
    ramp = ARMOR_RAMPS[armor]
    front = around(bu, 0.0)
    if around(bu, 0.25) < 0.018 or around(bu, 0.75) < 0.018:  # the side straps
        if abs(((bv - 0.2) * 5 / 0.7) % 1.0 - 0.5) < 0.18:
            return pick(METAL, 0.6, d) if around(bu, 0.25) < 0.008 or around(bu, 0.75) < 0.008 else pick(BROWN, 0.5, d)
        return pick(BROWN, 0.3, d)
    lame = (bv - 0.2) / 0.7 * 5.0
    within = lame - int(lame)  # 0 the lame's lower edge .. 1 its top
    if front < 0.012:
        return pick(ramp, 0.08, d)  # the closure at the front
    if within < 0.05:
        return pick(ramp, 0.0, d)   # the gap under the edge
    if within < 0.13:
        x = 0.95 if within < 0.09 else 0.75  # the rolled edge, catching the light
    elif within > 0.86:
        x = 0.18 + 0.1 * (within < 0.93)     # in the shadow of the lame above
    else:
        x = 0.62 - 0.25 * (within - 0.13) / 0.73
    # Rivets near each lame's top, either side of the front closure and by the side straps: a highlight above, a
    # shadow below.
    for c in (0.035, 0.2, 0.3, 0.465):
        for side in (c, 1.0 - c):
            du = abs(bu - side) * 128
            dv = (within - 0.72) * 128 * 0.7 / 5
            if du * du + dv * dv < 1.6:
                return pick(ramp, 1.0 if dv > 0.2 else 0.05 if dv < -0.4 else 0.7, d)
    # Scratches and chipped paint.
    if vnoise(s * 30, t * 30, 9) > 0.78 and h01(int(s * SKIN_SIZE), int(t * SKIN_SIZE), 10) > 0.6:
        x += 0.3
    x += 0.12 * (vnoise(s * 50, t * 50, 11) - 0.5)
    return pick(ramp, x, d)


def trousers_texel(bu, bv, d, s, t, top=False):
    c = camo(s, t)
    x = 0.2 if c < -0.35 else 0.75 if c > 0.4 else 0.45
    x += 0.1 * (vnoise(s * 70, t * 70, 12) - 0.5)
    if not top:
        if around(bu, 0.25) < 0.01 or around(bu, 0.75) < 0.01:
            return pick(CLOTH, 0.05, d) if int(bv * 120) % 2 else pick(CLOTH, 0.3, d)  # side seams, stitched
        # Creases: behind the knee (folds), across its front, at the ankle (bunched over the boot).
        if 0.42 < bv < 0.62 and around(bu, 0.5) < 0.2:
            f = math.sin((bv - 0.42) / 0.2 * math.pi * 3 + 3 * around(bu, 0.5))
            x += 0.25 * f
        if 0.44 < bv < 0.56 and around(bu, 0.0) < 0.18:
            x += 0.15 * math.sin((bv - 0.44) / 0.12 * math.pi * 2)
        if bv > 0.85:
            x += 0.2 * math.sin(bu * math.pi * 16) - 0.1
        if bv < 0.05:
            x -= 0.2
    return pick(CLOTH, x, d)


def legs_texel(bu, bv, d, s, t):
    front = around(bu, 0.0)
    if front < 0.13 and 0.06 < bv < 0.42:  # the thigh plates: ridged, lit along each ridge's top
        if front > 0.118 or bv < 0.072 or bv > 0.408:
            return pick(LEATHER, 0.05, d)
        k = ((bv - 0.06) % 0.07) / 0.07
        x = 0.85 if k > 0.86 else 0.2 if k < 0.14 else 0.35 + 0.35 * k
        x -= 0.25 * max(0.0, front - 0.08) / 0.05
        if front < 0.105 and front > 0.095 and abs(k - 0.5) < 0.1:
            return pick(METAL, 0.7, d)                               # a rivet on each ridge
        return pick(LEATHER, x, d)
    return trousers_texel(bu, bv, d, s, t)


def shaft_texel(bu, bv, d, s, t):
    """The boots' shafts, from the ankle (0) to below the knee (1)."""
    if bv > 0.8:  # the turned-down top, its fold lit
        return pick(BROWN, 0.85 if bv > 0.84 and bv < 0.87 else 0.6 if bv > 0.84 else 0.1, d)
    for sb in (0.3, 0.6):  # two straps round the shaft, buckled on the outside
        if abs(bv - sb) < 0.04:
            if around(bu, 0.25) < 0.03 or around(bu, 0.75) < 0.03:
                return pick(METAL, 0.75 if bv > sb else 0.4, d)
            return pick(BROWN, 0.7 if bv > sb + 0.025 else 0.45, d)
        if abs(bv - sb) < 0.055:
            return BROWN[1]
    if around(bu, 0.5) < 0.008:
        return BROWN[4] if int(bv * 100) % 2 else BROWN[1]           # the back seam, stitched
    x = 0.4 + 0.12 * (vnoise(s * 60, t * 30, 13) - 0.5)
    if bv < 0.22:
        x += 0.22 * math.sin(bu * math.pi * 12 + bv * 30) * (1 - bv / 0.22)  # creases over the ankle
    return pick(BROWN, x, d)


def foot_texel(bu, bv, d, s, t):
    """The feet: u 0 the top of the foot, 0.5 the sole; v the heel (0) to the toe (1)."""
    top = around(bu, 0.0)
    if top > 0.36:
        return BROWN[0] if top > 0.4 else pick(BROWN, 0.5, d)         # the sole, the welt round it
    if top < 0.06 and 0.3 < bv < 0.72:  # laces, crossing
        k = (bv * 30) % 1.0
        if abs(top / 0.06 - k) < 0.25 or abs(top / 0.06 - (1 - k)) < 0.25:
            return pick(BROWN, 0.9, d)
        return BROWN[1]
    x = 0.42 + 0.12 * (vnoise(s * 60, t * 60, 14) - 0.5)
    if bv > 0.78:
        x += 0.18 + 0.2 * (h01(int(s * SKIN_SIZE), int(t * SKIN_SIZE), 15) > 0.85)  # the scuffed toe cap
    if bv < 0.15:
        x -= 0.12
    return pick(BROWN, x - 0.2 * top / 0.36, d)


def bracer_texel(bu, bv, d, s, t):
    """The bracers: a dark rim at each end, stitched; two buckled straps round them; the thumb's side's strap."""
    if bv < 0.1 or bv > 0.9:
        return BROWN[1]
    if bv < 0.14 or bv > 0.86:
        return BROWN[5] if int(bu * 96) % 2 else BROWN[2]
    for sb in (0.38, 0.66):
        if abs(bv - sb) < 0.06:
            if around(bu, 0.5) < 0.04:
                return pick(METAL, 0.8 if bv > sb else 0.45, d)
            return pick(BROWN, 0.72 if bv > sb + 0.03 else 0.5, d)
    x = 0.35 + 0.15 * (vnoise(s * 50, t * 50, 16) - 0.5)
    return pick(BROWN, x, d)


def damage_marks(damage):
    """Scratches (short dark lines) and blood (blotches with drips) for a damage level 0..3, in
    (s, t) texture space: the same marks at every level, more of them the worse it is."""
    rng = 777
    marks = []

    def rand():
        nonlocal rng
        rng = (rng * 1103515245 + 12345) & 0x7FFFFFFF
        return (rng >> 8) / float(1 << 23)

    for level in range(1, damage + 1):
        for _ in range(8):  # scratches on the arms (the skin block)
            u0, v0, u1, v1 = BLOCKS["skin"]
            s0, t0 = u0 + 0.06 + rand() * (u1 - u0 - 0.12), v0 + 0.06 + rand() * (v1 - v0 - 0.12)
            angle = rand() * math.pi
            length = 0.02 + rand() * 0.04
            marks.append(("scratch", s0, t0, math.cos(angle) * length, math.sin(angle) * length))
        for _ in range(2 * level):  # blood on the arms, and on the torso and bracers when worse
            block = "skin" if level < 3 or rand() < 0.6 else ("leather" if rand() < 0.6 else "bracer")
            u0, v0, u1, v1 = BLOCKS[block]
            m = 0.035  # not over the block's edge (into the head's)
            marks.append(("blood", u0 + m + rand() * (u1 - u0 - 2 * m), v0 + m + rand() * (v1 - v0 - 2 * m),
                          0.012 + rand() * 0.02, rand()))
    return marks


def nearest(rgb):
    """The palette's (non-fullbright) colour nearest to rgb."""
    return min(range(224), key=lambda i: sum((a - b) ** 2 for a, b in zip(PALETTE[i], rgb)))


BLOOD = [nearest((75, 0, 0)), nearest((91, 7, 7)), nearest((123, 15, 11))]  # clotted, dark, fresh
CUT = nearest((111, 27, 19))


def mark_texel(s, t, marks):
    for kind, a, b, c, d in marks:
        if kind == "scratch":
            # Distance from the segment (a, b) + k (c, d): a dark red cut with a lighter, swollen rim.
            px, py = s - a, t - b
            k = max(0.0, min(1.0, (px * c + py * d) / (c * c + d * d)))
            dist = math.hypot(px - k * c, py - k * d)
            if dist < 0.0045:
                return CUT if dist > 0.0018 else BLOOD[0]
        else:
            dx, dy = s - a, t - b
            r = c * (1.0 + 0.35 * math.sin(7.0 * math.atan2(dy, dx) + d * 6.0))  # ragged edge
            drip = abs(dx) < c * 0.18 and 0 < dy < c * (1.5 + 2.0 * d)  # a drip running down
            if math.hypot(dx, dy) < r or drip:
                return BLOOD[0] if math.hypot(dx, dy) < r * 0.55 else BLOOD[2] if math.hypot(dx, dy) < r * 0.85 else BLOOD[1]
    return None


PAINTERS = {
    "skin": arm_texel, "head": head_texel, "cloth": legs_texel, "boots": foot_texel, "shaft": shaft_texel,
    "bracer": bracer_texel,
}


def skin_indices(armor, size=SKIN_SIZE):
    """The skin without damage, as palette indices (rows top to bottom)."""
    out = bytearray(size * size)
    for row in range(size):
        for col in range(size):
            s, t = (col + 0.5) / size, (row + 0.5) / size
            block = next(b for b, (u0, v0, u1, v1) in BLOCKS.items() if u0 <= s < u1 and v0 <= t < v1)
            bu, bv = block_uv(block, s, t)
            d = h01(col, row, 1)
            if block == "leather":
                i = (armor_texel(bu, bv, armor, d, s, t) if armor else None)
                if i is None:
                    i = vest_texel(bu, bv, d, s, t)
            else:
                i = PAINTERS[block](bu, bv, d, s, t)
            out[row * size + col] = i
    return out


def write_skin(path, base, damage, size=SKIN_SIZE):
    marks = damage_marks(damage)
    pixels = bytearray()
    # TGA rows go bottom-up; t = 0 is the top of the image.
    for row in range(size - 1, -1, -1):
        for col in range(size):
            i = base[row * size + col]
            if marks:
                m = mark_texel((col + 0.5) / size, (row + 0.5) / size, marks)
                if m is not None:
                    i = m
            assert i < 224, "a fullbright colour"
            r, g, b = PALETTE[i]
            pixels += bytes((b, g, r, 255))
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, size, size, 32, 8)
    with open(path, "wb") as f:
        f.write(header + pixels)


def write_placeholder_mdl(path):
    # A single tiny triangle; only its name matters (see the header comment).
    skin_w, skin_h = 8, 8
    data = bytearray(HEADER.pack(b"IDPO", 6, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0,
                                 1.0, 0.0, 0.0, 0.0,
                                 1, skin_w, skin_h, 3, 1, 1, 0, 0,
                                 1.0))
    data += struct.pack("<i", 0) + bytes(skin_w * skin_h)
    for sv in ((0, 0, 0), (0, 4, 0), (0, 0, 4)):
        data += struct.pack("<3i", *sv)
    data += struct.pack("<4i", 1, 0, 1, 2)
    data += struct.pack("<i", 0)
    data += bytes((0, 0, 0, 0)) + bytes((1, 1, 1, 0))
    data += b"vrbody".ljust(16, b"\0")
    for v in ((0, 0, 0), (1, 0, 0), (0, 1, 0)):
        data += bytes((v[0], v[1], v[2], 0))
    with open(path, "wb") as f:
        f.write(data)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    for armor in range(4):
        base = skin_indices(armor)
        for damage in range(4):
            write_skin(os.path.join(out, "vrbody_%02d_00.tga" % (armor * 4 + damage)), base, damage)
    for suffix, muscle in BUILDS:
        build_mesh(muscle)
        name = "vrbody" + suffix
        write_md5mesh(os.path.join(out, name + ".md5mesh"))
        write_md5anim(os.path.join(out, name + ".md5anim"))
        write_placeholder_mdl(os.path.join(out, name + ".mdl"))
        print("%s: %d joints, %d vertices, %d triangles" % (name, len(joints), len(verts), len(tris)))
    print("-> " + os.path.normpath(out))


if __name__ == "__main__":
    main()
