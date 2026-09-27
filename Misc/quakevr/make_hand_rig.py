#!/usr/bin/env python3
# make_hand_rig.py -- generates the jointed hand (round 21, "fitted hands"; Quake/vr/vr_handrig.cpp) from the
# six hand models drawn so far (progs/hand_base.mdl and the five progs/finger_*.mdl, which keep being the
# fallback):
#   quakevr/progs/hand_rig.md5mesh, .md5anim   the hand as one skinned mesh
#   quakevr/progs/hand_rig.mdl                 a placeholder: Ironwail loads MD5 only as the "enhanced"
#                                              replacement of an existing .mdl
#   quakevr/progs/hand_rig_NN_00.lmp           its skins (the models' own 8-bit skin and its three bloody
#                                              ones, make_bloody_hands.py), NN = the damage level
#   Quake/vr/vr_handrig_data.inc               the rig for the engine: joints, their frames, the vertices
#
# Usage: python Misc/quakevr/make_hand_rig.py [progs folder [include file]]
# Run it again after changing a hand model (taper_hand.py, make_bloody_hands.py): it reads them.
#
# The rig. Each finger is three rigid segments (phalanges) on three joints, the thumb too (its first
# segment, the metacarpal, turns at the wrist's end of the palm: the opposable thumb). The finger models
# are low-poly tubes of rings of vertices: a finger's base ring is glued to the palm, then one ring per
# joint and the tip; the thumb has a base ring (on the palm's thumb socket), a middle ring and a tip. Each
# ring rides the segment that it ends (its bone), the tube between two rings bends as they turn.
#
# The finger models' frames (0 open .. 4 the tightest fist, 5 = 3: the curls vr_view.cpp draws) are
# hand-animated: the segments stretch and the rings deform, which no rigid joints can do. So the rig
# keeps them exactly: per frame, each joint's turn (the best rigid fit of its ring relative to the ring
# before it) turns about a fixed pivot (the least-squares centre of those turns), and each vertex keeps,
# per frame, its place in its bone's frame (`local`): posed at a frame's joint turns it is exactly where
# that frame has it. Between frames, and in the grasp poses (each joint at its own curl), the joints'
# turns and the vertices' places are interpolated by the joint's curl. The engine skins each finger
# vertex with a matrix of its own (a "vertex joint" in the MD5), computed on the CPU from the rig: the
# GPU path is the body's (VR_AliasBonePoses), one draw for the whole hand instead of six.
#
# Space: hand_base.mdl's model space (+x towards the fingers, +y the palm's side, +z the thumb's and index
# finger's side). The finger models are placed in it as vr_view.cpp draws them at the default settings
# (vr_fingers_*, vr_finger_<name>_*, the fist slot's Scale 0.34 about each model's own scale origin);
# the engine moves each finger by the difference its current settings make (vr_handrig.cpp).
#
# The palm's thumb socket (its five vertices on the thumb's base ring) and the thenar round it ride the
# metacarpal (thumb_1) with weights falling off towards the wrist and the palm's middle.

import math
import os
import struct
import sys

import numpy as np

from make_bloody_hands import coverage
from mdlgen import HEADER, anorms, read_skins

HERE = os.path.dirname(os.path.abspath(__file__))
PARTS = ["hand_base", "finger_thumb", "finger_index", "finger_middle", "finger_ring", "finger_pinky"]
FINGERS = ["thumb", "index", "middle", "ring", "pinky"]  # vr_handrig.cpp's order
FRAMES = 6  # curl frames 0..5 (vr_view.cpp: 0 open .. 5; the models' frames 5..8 go back to 0)

# Default placement (vr_cvars.inc): vr_fingers_*, vr_finger_<name>_*, vr_finger_base_* (hand model units;
# y is to the hand's right, the model's -y), and the fist slot's Scale (vr_weapons.inc, slot 16). The
# offsets are drawn at weapons::offsetScale(), the models at ModelTransform::k: their ratio at any
# world and gun model scale is 0.75 / (1.25 * 0.7).
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

# The thumb's metacarpal (CMC joint): at the wrist's end of the thenar, below the thumb's socket; the
# socket is its far end. Hand model units, hand_base.mdl's space.
CMC_PIVOT = (-3.4, 1.2, 2.2)
THENAR_REACH = 4.5  # how far from the metacarpal's line palm vertices follow it (falling off)


# ----------------------------------------------------------------------------
# Models


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
    frames, normals = [], []
    table = np.array(anorms())
    for _ in range(nf):
        (kind,) = struct.unpack_from("<i", d, o)
        assert kind == 0, "%s: frame groups not supported" % path
        o += 4 + 8 + 16
        v = np.frombuffer(d, np.uint8, nv * 4, o).reshape(nv, 4)
        o += nv * 4
        frames.append(v[:, :3] * scale + origin)
        # The renderer turns an MDL's normals with the model's matrix, its per-axis scale (the header's) included
        # (r_alias.c's out_nor): the drawn models are lit by these, which the rig reproduces.
        n = table[v[:, 3]] * scale
        normals.append(n / np.linalg.norm(n, axis=1, keepdims=True))
    for s in skins:
        assert struct.unpack_from("<i", s, 0)[0] == 0, "%s: skin groups not supported" % path
    return dict(origin=origin, skins=[np.frombuffer(s[4:], np.uint8).reshape(sh, sw) for s in skins], sw=sw, sh=sh,
                st=st, tris=tris, frames=frames, normals=normals)


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
# Geometry


def unique_positions(frame0):
    """Groups of vertex indices sharing a position (the skin's seams)."""
    groups = {}
    for i, v in enumerate(frame0):
        groups.setdefault(tuple(np.round(v, 4)), []).append(i)
    return list(groups.values())


def kabsch(a, b):
    ca, cb = a.mean(0), b.mean(0)
    u, _, vt = np.linalg.svd((a - ca).T @ (b - cb))
    d = np.sign(np.linalg.det(vt.T @ u.T))
    r = vt.T @ np.diag([1.0, 1.0, d]) @ u.T
    return r, cb - r @ ca


def rings_of(finger, P):
    """Each unique vertex's ring: 0 the base (glued to the palm), 1, 2 the joints' rings, 3 the tip. The
    fingers are tubes of two vertices on the palm's side and three on the back per ring (the pinky has a
    few more round its base); the thumb has three rings (base, middle, tip) -> 1, 2, 3."""
    moved = np.linalg.norm(P - P[0], axis=2).max(0)
    lab = np.zeros(P.shape[1], int)
    if finger == "thumb":
        tip = np.linalg.norm(P[4] - P[0], axis=1)
        lab[:] = np.where(moved < 0.15, 1, np.where(tip < 3.5, 2, 3))
        assert [int((lab == k).sum()) for k in (1, 2, 3)] == [5, 4, 4], lab
        return lab
    y = P[0][:, 1]
    ys = np.sort(y)
    gap = int(np.argmax(np.diff(ys)))  # the palm's side and the back: the widest gap between the vertices' heights
    palm_side = y > (ys[gap] + ys[gap + 1]) / 2
    back = sorted(np.where(~palm_side)[0], key=lambda i: P[0][i][0])
    front = sorted(np.where(palm_side)[0], key=lambda i: P[0][i][0])
    assert len(back) == 12, (finger, len(back))
    for k in range(4):
        lab[back[3 * k:3 * k + 3]] = k
    for k in range(3):
        lab[front[len(front) - 6 + 2 * k:len(front) - 4 + 2 * k]] = k + 1
    lab[front[:len(front) - 6]] = 0
    assert all(moved[i] < 0.15 for i in back[:3]), finger
    return lab


def rot_about(pivot, r):
    return r, pivot - r @ pivot


def compose(a, b):
    return a[0] @ b[0], a[0] @ b[1] + a[1]


def inverse(a):
    return a[0].T, -a[0].T @ a[1]


def apply(a, p):
    return a[0] @ p + a[1]


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


def fit_finger(finger, model, shift):
    """The finger's rig in hand_base's space: pivots[3], rotations[FRAMES][3] (relative to the parent
    segment), and per unique vertex: its MDL vertices, bone (0 root .. 3 tip), and local place per frame."""
    F = [model["frames"][f] for f in range(FRAMES)]
    groups = unique_positions(F[0])
    reps = [g[0] for g in groups]
    P = np.array([[F[f][i] for i in reps] for f in range(FRAMES)])
    lab = rings_of(finger, P)

    # Each ring's rigid motion per frame (the thumb's base ring and the fingers' base rings stay).
    W = {}
    for k in range(4):
        sel = np.where(lab == k)[0]
        for f in range(FRAMES):
            W[(k, f)] = (np.eye(3), np.zeros(3)) if k == 0 or (finger == "thumb" and k == 1) or len(sel) == 0 \
                else kabsch(P[0][sel], P[f][sel])

    # Joint k turns ring k relative to ring k - 1: its turn per frame, and the pivot that best explains
    # the rings' moves (least squares; a little pull towards the proximal ring's middle keeps it defined
    # where the frames barely turn).
    pivots, rots = [], np.zeros((FRAMES, 3, 3, 3))
    for k in range(1, 4):
        a, b = [], []
        for f in range(FRAMES):
            qp, tp = W[(k - 1, f)]
            qk, tk = W[(k, f)]
            rel = qp.T @ qk
            rots[f, k - 1] = rel
            a.append(np.eye(3) - rel)
            b.append(qp.T @ (tk - tp))
        a, b = np.concatenate(a), np.concatenate(b)
        prox = P[0][lab == k - 1].mean(0) if (lab == k - 1).any() else P[0][lab == k].mean(0)
        lam = 0.05
        pivots.append(np.linalg.solve(a.T @ a + lam * np.eye(3), a.T @ b + lam * prox))
    if finger == "thumb":
        pivots[0] = np.array(CMC_PIVOT) - shift  # the metacarpal: no turn in the frames

    def chain(f):
        t = [(np.eye(3), np.zeros(3))]
        for k in range(3):
            t.append(compose(t[-1], rot_about(pivots[k], rots[f, k])))
        return t

    chains = [chain(f) for f in range(FRAMES)]
    local = np.zeros((len(reps), FRAMES, 3))
    normal = np.zeros((len(reps), FRAMES, 3))  # the model's normal in the bone's frame
    for u in range(len(reps)):
        for f in range(FRAMES):
            local[u, f] = apply(inverse(chains[f][lab[u]]), P[f][u])
            normal[u, f] = chains[f][lab[u]][0].T @ model["normals"][f][reps[u]]
    # Exact at every frame by construction; check.
    for f in range(FRAMES):
        err = max(np.linalg.norm(apply(chains[f][lab[u]], local[u, f]) - P[f][u]) for u in range(len(reps)))
        assert err < 1e-4, (finger, f, err)
    stretch = max(np.linalg.norm(local[u, f] - local[u, 0]) for u in range(len(reps)) for f in range(FRAMES))
    angles = [[round(math.degrees(math.acos(np.clip((np.trace(rots[f, k]) - 1) / 2, -1, 1))), 1) for k in range(3)]
              for f in range(FRAMES)]
    print("  %-6s %2d unique vertices, rings %s, most a vertex moves in its bone: %.2f; joint angles per frame %s"
          % (finger, len(reps), [int((lab == k).sum()) for k in range(4)], stretch, angles))
    return dict(groups=groups, bone=lab, pivots=[p + shift for p in pivots], rots=rots, local=local + shift, normal=normal)


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
    s = "%.6g" % x
    if "e" not in s and "." not in s:
        s += ".0"
    return s + "f"


def cvec(v):
    return "{" + ", ".join(cfloat(c) for c in v) + "}"


def main():
    progs = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "quakevr", "progs")
    inc = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "Quake", "vr", "vr_handrig_data.inc")
    models = {n: read_mdl(os.path.join(progs, n + ".mdl")) for n in PARTS}
    for n in PARTS:
        models[n]["name"] = n
    sw, sh = models["hand_base"]["sw"], models["hand_base"]["sh"]
    for n in PARTS:
        assert (models[n]["sw"], models[n]["sh"]) == (sw, sh) and len(models[n]["skins"]) == 4, n
        assert np.array_equal(models[n]["skins"][0], models["hand_base"]["skins"][0]), "%s: another skin" % n
        assert len(models[n]["frames"]) >= FRAMES or n == "hand_base", n
        assert all(not onseam for onseam, _, _ in models[n]["st"]), "%s: seam vertices not supported" % n

    print("fitting the rig:")
    shifts = {n: placement(models, n) for n in PARTS}
    rigs = {f: fit_finger(f, models["finger_" + f], shifts["finger_" + f]) for f in FINGERS}

    # The palm: its thumb socket and thenar follow the metacarpal (by weight, falling off towards the wrist and the
    # palm's middle).
    base = models["hand_base"]
    bp = base["frames"][0]
    thumb = rigs["thumb"]
    socket = np.array([thumb["local"][u, 0] for u in range(len(thumb["groups"])) if thumb["bone"][u] == 1])
    cmc = np.array(CMC_PIVOT)
    tip = socket.mean(0)
    axis = (tip - cmc) / np.linalg.norm(tip - cmc)
    pgroups = unique_positions(bp)
    palm_weight = []
    for g in pgroups:
        p = bp[g[0]]
        if np.min(np.linalg.norm(socket - p, axis=1)) < 0.35:
            w = 1.0  # on the socket
        else:
            along = np.clip((p - cmc) @ axis / np.linalg.norm(tip - cmc), 0.0, 1.0)
            off = np.linalg.norm((p - cmc) - ((p - cmc) @ axis) * axis)
            w = along * along * max(0.0, 1.0 - off / THENAR_REACH) ** 2
            w = 0.0 if w < 0.02 else min(0.9, w)
        palm_weight.append(w)
    print("  palm: %d unique vertices, %d on the thumb's socket, %d more on the thenar"
          % (len(pgroups), sum(1 for w in palm_weight if w == 1.0), sum(1 for w in palm_weight if 0 < w < 1)))

    # MD5 joints: 0 the palm, 1 the metacarpal, then one per palm vertex and one per finger vertex (unique
    # positions), each vertex at its joint: the engine computes their matrices (their places, and turns that give
    # the vertices the models' own normals).
    joints = [("palm", -1, np.zeros(3)), ("thumb_1", 0, np.array(CMC_PIVOT))]
    palm_joint = []
    for u, g in enumerate(pgroups):
        palm_joint.append(len(joints))
        joints.append(("palm_v%02d" % u, 0, bp[g[0]]))
    vertex_joint = {}
    for f in FINGERS:
        r = rigs[f]
        for u in range(len(r["groups"])):
            vertex_joint[(f, u)] = len(joints)
            joints.append(("%s_v%02d" % (f, u), 0, r["local"][u, 0]))

    # Mesh vertices (MDL vertices, with their skin coordinates), one weight each.
    verts, tris, weights = [], [], []

    # The skin: the models share skin 0 and its layout, but their bloody skins mark the texels each uses, and
    # the fingers use the same texels. So the skin is a stack of bands of the models' skin (a power of two of
    # them), each model on a band where no model it shares marked texels with is.
    cover = {}
    for n in PARTS:
        m = models[n]
        cover[n] = np.frombuffer(coverage(m["st"], [t for t in m["tris"]], sw, sh), np.uint8).reshape(sh, sw).astype(bool)
    band = {}
    for n in PARTS:
        taken = set()
        for o, b in band.items():
            shared = cover[n] & cover[o]
            if any(((models[n]["skins"][lv] != models[o]["skins"][lv]) & shared).any() for lv in (1, 2, 3)):
                taken.add(b)
        band[n] = min(set(range(len(PARTS))) - taken)
    bands = 1
    while bands < max(band.values()) + 1:
        bands *= 2
    print("  skin: %d bands of %dx%d (%s)" % (bands, sw, sh, ", ".join("%s %d" % (n, band[n]) for n in PARTS)))

    def st_of(m, i):
        _, s, t = m["st"][i]
        return ((s + 0.5) / sw, (t + 0.5 + sh * band[m["name"]]) / (sh * bands))

    vjoint = []  # each mesh vertex's joint
    pof = {}
    for u, g in enumerate(pgroups):
        for i in g:
            pof[i] = u
    for i in range(len(bp)):
        weights.append((palm_joint[pof[i]], 1.0, np.zeros(3)))
        verts.append((st_of(base, i), len(weights) - 1, 1))
        vjoint.append(palm_joint[pof[i]])
    for _, a, b, c in base["tris"]:
        tris.append((a, b, c))
    for f in FINGERS:
        m = models["finger_" + f]
        r = rigs[f]
        of = {}
        for u, g in enumerate(r["groups"]):
            for i in g:
                of[i] = u
        start_vertex = len(verts)
        for i in range(len(m["st"])):
            weights.append((vertex_joint[(f, of[i])], 1.0, np.zeros(3)))
            verts.append((st_of(m, i), len(weights) - 1, 1))
            vjoint.append(vertex_joint[(f, of[i])])
        for _, a, b, c in m["tris"]:
            tris.append((start_vertex + a, start_vertex + b, start_vertex + c))

    # The normals the engine gives the mesh (Mod_LoadMD5MeshModel: MD5_ComputeNormals, welded by position,
    # stored as bytes), per joint: the vertices' matrices turn them to the models' normals.
    pos = np.array([joints[j][2] for j in vjoint])
    acc = {}
    key = [tuple(np.round(pv, 6)) for pv in pos]
    for a, b, c in tris:
        n = np.cross(pos[c] - pos[a], pos[b] - pos[a])
        for v in (a, b, c):
            acc[key[v]] = acc.get(key[v], 0) + n
    bind_normal = {}
    for v, j in enumerate(vjoint):
        n = acc[key[v]]
        n = n / (np.linalg.norm(n) or 1.0)
        q = np.trunc(n * 127.0) / 127.0
        bind_normal[j] = q / (np.linalg.norm(q) or 1.0)

    skins = []
    for level in range(4):
        stack = np.concatenate([models["hand_base"]["skins"][0].copy() for _ in range(bands)])
        for n in PARTS:
            b = stack[band[n] * sh:(band[n] + 1) * sh]
            b[cover[n]] = models[n]["skins"][level][cover[n]]
        skins.append(stack)
    for level, s in enumerate(skins):
        lmp(os.path.join(progs, "hand_rig_%02d_00.lmp" % level), s)

    with open(os.path.join(progs, "hand_rig.md5mesh"), "w", newline="\n") as fo:
        fo.write("MD5Version 10\ncommandline \"Misc/quakevr/make_hand_rig.py\"\n\n")
        fo.write("numJoints %d\nnumMeshes 1\n\njoints {\n" % len(joints))
        for name, parent, pos in joints:
            fo.write("\t\"%s\"\t%d ( %s ) ( 0.000000 0.000000 0.000000 )\n" % (name, parent, fmt(pos)))
        fo.write("}\n\nmesh {\n\tshader \"hand_rig\"\n\n")
        fo.write("\tnumverts %d\n" % len(verts))
        for i, (st, start, count) in enumerate(verts):
            fo.write("\tvert %d ( %.6f %.6f ) %d %d\n" % (i, st[0], st[1], start, count))
        fo.write("\n\tnumtris %d\n" % len(tris))
        for i, t in enumerate(tris):
            fo.write("\ttri %d %d %d %d\n" % (i, t[0], t[1], t[2]))
        fo.write("\n\tnumweights %d\n" % len(weights))
        for i, (j, w, p) in enumerate(weights):
            fo.write("\tweight %d %d %.6f ( %s )\n" % (i, j, w, fmt(p)))
        fo.write("}\n")

    allpos = np.array([p for _, _, p in joints[2:]])
    with open(os.path.join(progs, "hand_rig.md5anim"), "w", newline="\n") as fo:
        # One frame holding the bind pose (the engine poses the hand: VR_AliasBonePoses).
        fo.write("MD5Version 10\ncommandline \"Misc/quakevr/make_hand_rig.py\"\n\n")
        fo.write("numFrames 1\nnumJoints %d\nframeRate 24\nnumAnimatedComponents %d\n\n" % (len(joints), 6 * len(joints)))
        fo.write("hierarchy {\n")
        for i, (name, parent, _) in enumerate(joints):
            fo.write("\t\"%s\"\t%d 63 %d\n" % (name, parent, 6 * i))
        fo.write("}\n\nbounds {\n\t( %s ) ( %s )\n}\n\nbaseframe {\n" % (fmt(allpos.min(0)), fmt(allpos.max(0))))
        local = [(pos if parent < 0 else pos - joints[parent][2]) for _, parent, pos in joints]
        for lp in local:
            fo.write("\t( %s ) ( 0.000000 0.000000 0.000000 )\n" % fmt(lp))
        fo.write("}\n\nframe 0 {\n")
        for lp in local:
            fo.write("\t%s 0.000000 0.000000 0.000000\n" % fmt(lp))
        fo.write("}\n")
    write_placeholder_mdl(os.path.join(progs, "hand_rig.mdl"))

    # The engine's tables.
    L = []
    L.append("// vr_handrig_data.inc -- generated by Misc/quakevr/make_hand_rig.py from the hand models; do not edit.")
    L.append("// The jointed hand (vr_handrig.cpp): hand_base.mdl's model space, hand model units.")
    L.append("")
    L.append("inline constexpr int numJoints = %d; // in progs/hand_rig.md5mesh: 0 the palm, 1 the metacarpal, then one per palm and finger vertex" % len(joints))
    L.append("inline constexpr int numVertices = %d; // unique finger vertices" % sum(len(rigs[f]["groups"]) for f in FINGERS))
    L.append("inline constexpr int numFrames = %d;" % FRAMES)
    L.append("inline constexpr float fistScale = %s; // the fist slot's Scale the rig was placed at" % cfloat(FIST_SCALE))
    L.append("inline constexpr float offsetPerModel = %s; // weapons::offsetScale() / ModelTransform::k" % cfloat(OFFSET_PER_MODEL))
    L.append("inline constexpr float baseScaleOrigin[3] = %s; // hand_base.mdl's" % cvec(models["hand_base"]["origin"]))
    L.append("")
    L.append("// Per finger (thumb, index, middle, ring, pinky): its model's scale origin, and where it sits relative to")
    L.append("// hand_base.mdl at the default settings (its bind place).")
    L.append("inline constexpr float fingerScaleOrigin[5][3] = {%s};" % ", ".join(cvec(models["finger_" + f]["origin"]) for f in FINGERS))
    L.append("inline constexpr float fingerBindShift[5][3] = {%s};" % ", ".join(cvec(shifts["finger_" + f]) for f in FINGERS))
    L.append("inline constexpr float fingerDefaultOffset[5][3] = {%s}; // vr_fingers_* + vr_finger_<name>_*" %
             ", ".join(cvec(np.add(FINGERS_ALL, FINGER_OFFSETS[f])) for f in FINGERS))
    L.append("")
    L.append("// Joints per finger: 1 the knuckle (the thumb's metacarpal at the wrist), 2, 3; their pivots, and their turns")
    L.append("// relative to the segment before, per curl frame (x, y, z, w).")
    L.append("inline constexpr float pivots[5][3][3] = {%s};" % ", ".join(
        "{" + ", ".join(cvec(p) for p in rigs[f]["pivots"]) + "}" for f in FINGERS))
    L.append("inline constexpr float turns[5][numFrames][3][4] = {")
    for f in FINGERS:
        L.append("    {" + ", ".join("{" + ", ".join(cvec(quat(rigs[f]["rots"][fr, k])) for k in range(3)) + "}"
                               for fr in range(FRAMES)) + "},")
    L.append("};")
    L.append("")
    L.append("// The finger vertices: finger, bone (0 the finger's root, glued to the palm; 1..3 the segments), its joint in the")
    L.append("// MD5, and its place in its bone's frame per curl frame (frame 0: its bind place).")
    L.append("struct Vertex")
    L.append("{")
    L.append("    unsigned char finger, bone;")
    L.append("    short joint;")
    L.append("    float local[numFrames][3];")
    L.append("    float normal[numFrames][3]; // the model's, in the bone's frame")
    L.append("    float bindNormal[3];        // the MD5's (as the engine computes it)")
    L.append("};")
    L.append("inline constexpr Vertex vertices[numVertices] = {")
    firsts = []
    for fi, f in enumerate(FINGERS):
        r = rigs[f]
        firsts.append(sum(len(rigs[g]["groups"]) for g in FINGERS[:fi]))
        for u in range(len(r["groups"])):
            L.append("    {%d, %d, %d, {%s}, {%s}, %s}," % (fi, r["bone"][u], vertex_joint[(f, u)],
                                                  ", ".join(cvec(r["local"][u, fr]) for fr in range(FRAMES)),
                                                  ", ".join(cvec(r["normal"][u, fr]) for fr in range(FRAMES)),
                                                  cvec(bind_normal[vertex_joint[(f, u)]])))
    L.append("};")
    L.append("inline constexpr int firstVertex[6] = {%s};" % ", ".join(str(x) for x in firsts + [sum(len(rigs[g]["groups"]) for g in FINGERS)]))
    L.append("")
    L.append("// The fingers' triangles (as drawn: clockwise seen from outside), by vertex (vertices[]), and the palm's")
    L.append("// (by palm vertex, with the share of the metacarpal in it).")
    ftris = []
    for fi, f in enumerate(FINGERS):
        m = models["finger_" + f]
        of = {}
        for u, g in enumerate(rigs[f]["groups"]):
            for i in g:
                of[i] = firsts[fi] + u
        for _, a, b, c in m["tris"]:
            t = (of[a], of[b], of[c])
            if len(set(t)) == 3:
                ftris.append(t)
    L.append("inline constexpr int numFingerTriangles = %d;" % len(ftris))
    L.append("inline constexpr unsigned short fingerTriangles[numFingerTriangles][3] = {%s};" %
             ", ".join("{%d, %d, %d}" % t for t in ftris))
    ptris = [(pof[a], pof[b], pof[c]) for _, a, b, c in base["tris"] if len({pof[a], pof[b], pof[c]}) == 3]
    L.append("inline constexpr int numPalmVertices = %d;" % len(pgroups))
    L.append("struct PalmVertex")
    L.append("{")
    L.append("    float pos[3];")
    L.append("    float thumb; // the metacarpal's weight")
    L.append("    short joint;")
    L.append("    float normal[3];     // the model's")
    L.append("    float bindNormal[3]; // the MD5's")
    L.append("};")
    L.append("inline constexpr PalmVertex palmVertices[numPalmVertices] = {%s};" % ", ".join(
        "{%s, %s, %d, %s, %s}" % (cvec(bp[g[0]]), cfloat(palm_weight[u]), palm_joint[u], cvec(base["normals"][0][g[0]]),
                                  cvec(bind_normal[palm_joint[u]])) for u, g in enumerate(pgroups)))
    L.append("inline constexpr int numPalmTriangles = %d;" % len(ptris))
    L.append("inline constexpr unsigned short palmTriangles[numPalmTriangles][3] = {%s};" % ", ".join("{%d, %d, %d}" % t for t in ptris))
    with open(inc, "w", newline="\n") as fo:
        fo.write("\n".join(L) + "\n")

    print("hand_rig: %d joints, %d vertices, %d triangles, %d weights" % (len(joints), len(verts), len(tris), len(weights)))
    print("-> %s, %s" % (os.path.normpath(progs), os.path.normpath(inc)))


if __name__ == "__main__":
    main()
