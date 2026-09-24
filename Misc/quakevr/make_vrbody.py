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

UNITS = 1.0 / 0.0381  # Quake units per metre at vr_world_scale 1

# ----------------------------------------------------------------------------
# Small vector helpers


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def norm(a): return mul(a, 1.0 / length(a))


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

# Skin blocks (u0, v0, u1, v1): skin, leather, cloth, boots, bracers.
BLOCKS = {
    "skin": (0.0, 0.0, 0.5, 0.5),
    "leather": (0.5, 0.0, 1.0, 0.5),
    "cloth": (0.0, 0.5, 0.5, 1.0),
    "boots": (0.5, 0.5, 0.75, 1.0),
    "bracer": (0.75, 0.5, 1.0, 1.0),
}

verts = []  # (position, [(joint index, bias)], (s, t))
tris = []


def add_vert(pos, weights, st):
    verts.append((pos, weights, st))
    return len(verts) - 1


def loft(rings, block, cap_start=True, cap_end=True, sides=SIDES):
    """rings: list of (centre, u axis, v axis, radius along u, radius along v, weights)."""
    u0, v0, u1, v1 = BLOCKS[block]
    # Keep away from the block edges so filtering does not bleed into the next block.
    pad = 0.04
    u0, v0, u1, v1 = u0 + pad, v0 + pad, u1 - pad, v1 - pad
    # Front faces are clockwise seen from outside (Ironwail: glFrontFace(GL_CW)), which the
    # triangles below are when the loft runs along u x v (the direction the rings go around);
    # otherwise go around the other way.
    along = sub(rings[-1][0], rings[0][0])
    if dot(cross(rings[0][1], rings[0][2]), along) < 0:
        rings = [(c, ua, mul(va, -1.0), ru, rv, ws) for c, ua, va, ru, rv, ws in rings]
    first = len(verts)
    for r, (centre, ua, va, ru, rv, weights) in enumerate(rings):
        for k in range(sides):
            a = 2 * math.pi * k / sides
            p = add(centre, add(mul(ua, math.cos(a) * ru), mul(va, math.sin(a) * rv)))
            st = (u0 + (u1 - u0) * k / (sides - 1), v0 + (v1 - v0) * r / max(1, len(rings) - 1))
            add_vert(p, weights, st)
    for r in range(len(rings) - 1):
        for k in range(sides):
            a = first + r * sides + k
            b = first + r * sides + (k + 1) % sides
            c = first + (r + 1) * sides + k
            d = first + (r + 1) * sides + (k + 1) % sides
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
        base = first + (len(rings) - 1) * sides
        for k in range(sides):
            tris.append((c, base + (k + 1) % sides, base + k))


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
    ], "leather")
    # Neck and head.
    loft([
        ((-0.01, 0.0, 1.47), X, Y, 0.05 * torso, 0.055 * torso, w(("neck", 1.0))),
        ((-0.01, 0.0, 1.55), X, Y, 0.05 * torso, 0.055 * torso, w(("neck", 0.5), ("head", 0.5))),
        ((0.0, 0.0, 1.60), X, Y, 0.095, 0.085, w(("head", 1.0))),
        ((0.0, 0.0, 1.68), X, Y, 0.10, 0.09, w(("head", 1.0))),
        ((-0.01, 0.0, 1.75), X, Y, 0.08, 0.07, w(("head", 1.0))),
    ], "skin", cap_start=False)

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
        ], "skin", sides=10)
        # A leather bracer over the forearm's lower half (its strap shows how the forearm turns),
        # ending in a cuff over the base of the hand.
        loft([
            (add(elbow, mul(fd, 0.13)), fa, fv, 0.049 * fm, 0.043 * fm, w((fo_, 1.0))),
            (add(elbow, mul(fd, 0.19)), fa, fv, 0.041 * wm, 0.033 * wm, w((fo_, 0.8), (ha_, 0.2))),
            (wrist, fa, fv, 0.036 * wm, 0.027 * wm, w((fo_, 0.4), (ha_, 0.6))),
            (add(wrist, mul(fd, 0.03)), fa, fv, 0.037 * wm, 0.030 * wm, w((ha_, 1.0))),  # flared: the hand's base stays inside
        ], "bracer", sides=10)

        hip = joints[index["thigh_" + side]][2]
        knee = joints[index["calf_" + side]][2]
        ankle = joints[index["foot_" + side]][2]
        thigh = bases["thigh_" + side]
        calf = bases["calf_" + side]
        loft([
            (add(hip, (0.0, 0.0, 0.03)), thigh[2], thigh[1], 0.085 * torso, 0.08 * torso, w(("thigh_" + side, 1.0))),
            (add(hip, (0.0, 0.0, -0.18)), thigh[2], thigh[1], 0.075 * torso, 0.07 * torso, w(("thigh_" + side, 1.0))),
            (knee, thigh[2], thigh[1], 0.055, 0.055, w(("thigh_" + side, 0.5), ("calf_" + side, 0.5))),
            (add(knee, (0.0, 0.0, -0.15)), calf[2], calf[1], 0.055 * torso, 0.05 * torso, w(("calf_" + side, 1.0))),
            (add(ankle, (0.0, 0.0, 0.04)), calf[2], calf[1], 0.04, 0.04, w(("calf_" + side, 0.5), ("foot_" + side, 0.5))),
        ], "cloth")
        # Foot: rings along x (u up, v left).
        Z = (0.0, 0.0, 1.0)
        loft([
            ((-0.06, 0.09 * sy, 0.05), Z, Y, 0.05, 0.045, w(("foot_" + side, 1.0))),
            ((0.03, 0.09 * sy, 0.045), Z, Y, 0.045, 0.05, w(("foot_" + side, 1.0))),
            ((0.16, 0.09 * sy, 0.03), Z, Y, 0.025, 0.04, w(("foot_" + side, 1.0))),
        ], "boots")


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


# Quake palette colours (index ramps) per block, dithered for a Quake-like grain.
PALETTE_RAMPS = {
    "skin": [(99, 63, 43), (111, 71, 51), (127, 83, 63), (139, 95, 71), (155, 107, 83)],
    "leather": [(27, 19, 15), (39, 31, 23), (55, 43, 31), (67, 51, 39)],
    "cloth": [(35, 27, 19), (43, 35, 23), (55, 43, 27), (63, 51, 31)],
    "boots": [(15, 11, 7), (23, 15, 11), (31, 23, 15), (39, 27, 15)],
    "bracer": [(35, 23, 15), (47, 31, 19), (59, 39, 23), (71, 47, 27)],
}


# The player's armour on the torso: plates in the armour's colour (Quake's green, yellow and red
# armours), dark seams between them and rivets along the seams.
ARMOR_RAMPS = {
    1: [(19, 43, 19), (27, 59, 27), (35, 71, 31), (47, 87, 39)],
    2: [(79, 59, 15), (99, 75, 19), (115, 87, 23), (131, 99, 27)],
    3: [(75, 11, 7), (95, 19, 11), (115, 27, 15), (135, 35, 19)],
}


def armor_texel(bu, bv, armor, rng):
    """The armour's colour at (bu, bv) of the torso block, or None where it leaves the torso bare
    (the belt at the bottom, the collar at the top)."""
    if bv < 0.16 or bv > 0.9:
        return None
    ramp = ARMOR_RAMPS[armor]
    lame = (bv - 0.16) / 0.74 * 5.0  # five overlapping lames, top to bottom
    within = lame - int(lame)
    if within < 0.08:
        return (11, 11, 11) if armor != 2 else (39, 27, 7)  # seam
    if within < 0.2 and int(bu * 24) % 3 == 0 and int(within * 40) % 2 == 0:
        return (171, 171, 171)  # rivet
    k = (rng >> 16) % 10
    i = 2 - (1 if within > 0.8 else 0) + (1 if k > 7 else -1 if k < 2 else 0)  # lower edge darker
    return ramp[max(0, min(len(ramp) - 1, i))]


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
            s0, t0 = rand() * 0.5, rand() * 0.5
            angle = rand() * math.pi
            length = 0.02 + rand() * 0.04
            marks.append(("scratch", s0, t0, math.cos(angle) * length, math.sin(angle) * length))
        for _ in range(2 * level):  # blood on the arms, and on the torso and bracers when worse
            block = "skin" if level < 3 or rand() < 0.6 else ("leather" if rand() < 0.6 else "bracer")
            u0, v0, u1, v1 = BLOCKS[block]
            marks.append(("blood", u0 + rand() * (u1 - u0), v0 + rand() * (v1 - v0), 0.012 + rand() * 0.02, rand()))
    return marks


def mark_texel(s, t, marks):
    for kind, a, b, c, d in marks:
        if kind == "scratch":
            # Distance from the segment (a, b) + k (c, d).
            px, py = s - a, t - b
            k = max(0.0, min(1.0, (px * c + py * d) / (c * c + d * d)))
            if math.hypot(px - k * c, py - k * d) < 0.0045:
                return (111, 27, 19)
        else:
            dx, dy = s - a, t - b
            r = c * (1.0 + 0.35 * math.sin(7.0 * math.atan2(dy, dx) + d * 6.0))  # ragged edge
            drip = abs(dx) < c * 0.18 and 0 < dy < c * (1.5 + 2.0 * d)  # a drip running down
            if math.hypot(dx, dy) < r or drip:
                return (91, 7, 7) if math.hypot(dx, dy) < r * 0.6 else (123, 15, 11)
    return None


def write_skin(path, size=128, damage=0, armor=0):
    rng = 12345
    marks = damage_marks(damage)
    pixels = bytearray()
    # TGA rows go bottom-up; t = 0 is the top of the image.
    for row in range(size - 1, -1, -1):
        for col in range(size):
            s, t = (col + 0.5) / size, (row + 0.5) / size
            block = next(b for b, (u0, v0, u1, v1) in BLOCKS.items() if u0 <= s < u1 and v0 <= t < v1)
            ramp = PALETTE_RAMPS[block]
            if block == "bracer":
                # A lighter strap along the thumb's side and a dark rim at each end. The strap
                # is where the ring crosses its u axis on the far side (vertex 5 of 10, u = 5/9), so
                # that it is on the same side of both arms, whichever way their rings go around.
                u0, v0, u1, v1 = BLOCKS[block]
                bu, bv = (s - u0) / (u1 - u0), (t - v0) / (v1 - v0)
                pad = 0.04 / (u1 - u0)
                if abs(bu - (pad + (1 - 2 * pad) * 5 / 9)) < 0.05:
                    pixels += bytes((39, 63, 99, 255))
                    continue
                if bv < 0.12 or bv > 0.88:
                    pixels += bytes((11, 15, 23, 255))
                    continue
            rng = (rng * 1103515245 + 12345) & 0x7FFFFFFF
            mark = mark_texel(s, t, marks) if marks else None
            if mark:
                r, g, b = mark
                pixels += bytes((b, g, r, 255))
                continue
            if armor and block == "leather":
                u0, v0, u1, v1 = BLOCKS[block]
                plate = armor_texel((s - u0) / (u1 - u0), (t - v0) / (v1 - v0), armor, rng)
                if plate:
                    r, g, b = plate
                    pixels += bytes((b, g, r, 255))
                    continue
            # Mostly the middle of the ramp, some lighter and darker specks.
            k = (rng >> 16) % 10
            i = len(ramp) // 2 + (-1 if k < 3 else 1 if k > 7 else 0)
            r, g, b = ramp[max(0, min(len(ramp) - 1, i))]
            pixels += bytes((b, g, r, 255))
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, size, size, 32, 8)
    with open(path, "wb") as f:
        f.write(header + pixels)


def write_placeholder_mdl(path):
    # A single tiny triangle; only its name matters (see the header comment).
    skin_w, skin_h = 8, 8
    header = struct.pack("<4si3f3f f3f 8i f",
                         b"IDPO", 6, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0,
                         1.0, 0.0, 0.0, 0.0,
                         1, skin_w, skin_h, 3, 1, 1, 0, 0,
                         1.0)
    data = bytearray(header)
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
        for damage in range(4):
            write_skin(os.path.join(out, "vrbody_%02d_00.tga" % (armor * 4 + damage)), damage=damage, armor=armor)
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
