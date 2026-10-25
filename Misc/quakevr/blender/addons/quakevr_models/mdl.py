# mdl.py -- reads and writes Quake alias models (.mdl) byte for byte, and what the Blender add-on needs to keep a
# weapon's or the wrist gadget's anchors: the anchors the engine names (vr_weapons.inc, vr_view.cpp's magazine mounts,
# vr_shells.cpp, vr_weaponfx.cpp, vr_gadget.cpp), bound as the engine binds them (points on the surface) and a report
# of where they are. Plain Python (no bpy, no numpy).
#
# An MDL vertex is a place per pose (a byte per axis on the header's grid: origin + byte * scale, and a normal index)
# and a skin coordinate (onseam, s, t in texels: a triangle facing back (facesfront 0) reads an onseam vertex's s half
# a skin further right). Anchors are points on the surface (vr_anchor.hpp): an edit may renumber the vertices and
# triangles freely; what matters is that the surface stays where the anchors are (the report gives how far each moved
# in every frame).

import math
import os
import re
import struct

HEADER = struct.Struct("<4si3f3ff3f8if")
IDENT = b"IDPO"


class MdlError(Exception):
    pass


class Model:
    """An MDL as its parts: header fields, skins, skin vertices, triangles, frames (the bytes kept as read)."""

    def __init__(self, data, name="model.mdl"):
        self.name = name
        if len(data) < HEADER.size:
            raise MdlError("%s: too short for a Quake model" % name)
        h = list(HEADER.unpack_from(data, 0))
        if h[0] != IDENT or h[1] != 6:
            raise MdlError("%s: not a Quake model (IDPO version 6)" % name)
        self.h = h
        ns, sw, sh, nv, nt, nf = h[12:18]
        off = HEADER.size
        self.skins = []  # [group, intervals bytes, [image bytes]]
        try:
            for _ in range(ns):
                g, = struct.unpack_from("<i", data, off)
                off += 4
                if g == 0:
                    self.skins.append([0, b"", [bytes(data[off:off + sw * sh])]])
                    off += sw * sh
                else:
                    n, = struct.unpack_from("<i", data, off)
                    iv = bytes(data[off + 4:off + 4 + 4 * n])
                    off += 4 + 4 * n
                    ims = []
                    for _ in range(n):
                        ims.append(bytes(data[off:off + sw * sh]))
                        off += sw * sh
                    self.skins.append([1, iv, ims])
            self.st = [list(struct.unpack_from("<3i", data, off + 12 * i)) for i in range(nv)]
            off += 12 * nv
            self.tris = [tuple(struct.unpack_from("<4i", data, off + 16 * i)) for i in range(nt)]
            off += 16 * nt
            self.frames = []  # ["simple", header 24, verts] or ["group", bbox 8, intervals, [[header 24, verts]]]
            for _ in range(nf):
                t, = struct.unpack_from("<i", data, off)
                off += 4
                if t == 0:
                    self.frames.append(["simple", bytes(data[off:off + 24]), bytes(data[off + 24:off + 24 + 4 * nv])])
                    off += 24 + 4 * nv
                else:
                    n, = struct.unpack_from("<i", data, off)
                    bbox = bytes(data[off + 4:off + 12])
                    iv = bytes(data[off + 12:off + 12 + 4 * n])
                    off += 12 + 4 * n
                    subs = []
                    for _ in range(n):
                        subs.append([bytes(data[off:off + 24]), bytes(data[off + 24:off + 24 + 4 * nv])])
                        off += 24 + 4 * nv
                    self.frames.append(["group", bbox, iv, subs])
        except struct.error:
            raise MdlError("%s: the file ends early" % name)
        self.trailing = bytes(data[off:])  # some id models carry junk after their frames: kept as it is
        for t in self.tris:
            if min(t[1:]) < 0 or max(t[1:]) >= nv:
                raise MdlError("%s: a triangle names a vertex the model doesn't have" % name)

    @classmethod
    def read(cls, path):
        with open(path, "rb") as f:
            return cls(f.read(), os.path.basename(path))

    # Header fields.
    @property
    def scale(self):
        return tuple(self.h[2:5])

    @property
    def origin(self):
        return tuple(self.h[5:8])

    @property
    def skin_size(self):
        return self.h[13], self.h[14]

    @property
    def num_verts(self):
        return len(self.st)

    # Poses: every simple frame, and every frame of each group, in file order: [header 24, verts].
    def poses(self):
        out = []
        for fr in self.frames:
            if fr[0] == "simple":
                out.append((fr, None))
            else:
                out.extend((fr, k) for k in range(len(fr[3])))
        return out

    def pose_header(self, fr_k):
        fr, k = fr_k
        return fr[1] if k is None else fr[3][k][0]

    def pose_verts(self, fr_k):
        fr, k = fr_k
        return fr[2] if k is None else fr[3][k][1]

    def pose_names(self):
        """A name per pose: the frame's; a group's frames as name.k (0-based)."""
        out = []
        for fr in self.frames:
            if fr[0] == "simple":
                out.append(frame_name(fr[1]))
            else:
                for k, (hdr, _) in enumerate(fr[3]):
                    out.append("%s.%d" % (frame_name(hdr), k))
        return out

    def pose_bytes(self):
        """Per pose, the vertices' (x, y, z, normal) byte tuples."""
        out = []
        for p in self.poses():
            vb = self.pose_verts(p)
            out.append([tuple(vb[4 * i:4 * i + 4]) for i in range(self.num_verts)])
        return out

    def place(self, b):
        """A byte triple's place in model space."""
        s, o = self.scale, self.origin
        return (o[0] + b[0] * s[0], o[1] + b[1] * s[1], o[2] + b[2] * s[2])

    def to_bytes(self):
        h = list(self.h)
        h[12] = len(self.skins)
        h[15], h[16] = len(self.st), len(self.tris)
        h[17] = len(self.frames)
        out = bytearray(HEADER.pack(*h))
        for g, iv, ims in self.skins:
            if g == 0:
                out += struct.pack("<i", 0) + ims[0]
            else:
                out += struct.pack("<ii", 1, len(ims)) + iv
                for im in ims:
                    out += im
        for st in self.st:
            out += struct.pack("<3i", *st)
        for t in self.tris:
            out += struct.pack("<4i", *t)
        for fr in self.frames:
            if fr[0] == "simple":
                out += struct.pack("<i", 0) + fr[1] + fr[2]
            else:
                out += struct.pack("<ii", 1, len(fr[3])) + fr[1] + fr[2]
                for hdr, vb in fr[3]:
                    out += hdr + vb
        return bytes(out + self.trailing)


def frame_name(hdr24):
    return hdr24[8:24].split(b"\0", 1)[0].decode("latin-1")


# ----------------------------------------------------------------------------
# Quake's 162 normals (anorms.h)

_ANORMS = None


def anorms():
    global _ANORMS
    if _ANORMS is None:
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "anorms.txt")
        with open(path) as f:
            _ANORMS = [tuple(float(x) for x in line.split()) for line in f if line.strip()]
        assert len(_ANORMS) == 162
    return _ANORMS


def nearest_normal(n):
    best, bi = -2.0, 0
    for i, a in enumerate(anorms()):
        d = a[0] * n[0] + a[1] * n[1] + a[2] * n[2]
        if d > best:
            best, bi = d, i
    return bi


# ----------------------------------------------------------------------------
# Welding: the vertices at the same place in every pose (a model's vertices are split along its skin's seams and
# where its shading is flat) are one vertex in Blender.


def weld(model, poses=None):
    """(group of each vertex, [vertices of each group]), groups in the order of their first vertex."""
    poses = poses if poses is not None else model.pose_bytes()
    key_group, group_of, groups = {}, [], []
    for v in range(model.num_verts):
        key = tuple(p[v][:3] for p in poses)
        g = key_group.get(key)
        if g is None:
            g = key_group[key] = len(groups)
            groups.append([])
        groups[g].append(v)
        group_of.append(g)
    return group_of, groups


# ----------------------------------------------------------------------------
# The anchors the engine names


def repo_root(path):
    """The Quake VR repository a model file is in (<root>/quakevr/progs/x.mdl), or None."""
    d = os.path.dirname(os.path.abspath(path))
    for _ in range(4):
        if os.path.exists(os.path.join(d, "Quake", "vr", "vr_weapons.inc")):
            return d
        d = os.path.dirname(d)
    return None


ANCHOR_KEYS = (("HandAnchor", "hand_av", "the hand's"), ("MuzzleAnchor", "muzzle_av", "the muzzle's"),
               ("TwoHHandAnchor", "2h_hand_av", "the off hand's (two-handed)"),
               ("WpnButtonAnchor", "wpnbtn_av", "the ammo button's"),
               ("WpnTextAnchor", "wpntxt_av", "the ammo screen's"))


# The tables in the engine's sources that hold anchors besides vr_weapons.inc: the magazines' mounts (vr_view.cpp
# magMounts, on the gun's body), the shells' ejection ports (vr_shells.cpp weaponTable) and the monsters' guns
# (vr_weaponfx.cpp: the middle of a muzzle's and of a rear's anchors).
MAG_RE = re.compile(r'"progs/vr_mag_on_(v_\w+\.mdl)", "([^"]*)",')
SHELL_RE = re.compile(r'\{modelmeta::Id::(\w+), .*?,\s+"([^"]*)"\},[ \t]*$', re.S | re.M)
SHELL_MODELS = {"VShot": "v_shot.mdl", "VShot2": "v_shot2.mdl"}
MONSTER_RE = re.compile(r'constexpr const char\* (soldier|enforcer)(Muzzle|Rear)\[\] = \{(.*?)\};', re.S)


def weapon_anchors(root, model_name):
    """[(what, anchor text)] for progs/<model_name>: vr_weapons.inc's defaults for every slot that uses the model, the
    magazine mounts (vr_view.cpp), the shell ejection ports (vr_shells.cpp) and the monster guns (vr_weaponfx.cpp)."""
    out = []
    if root is None:
        return out
    inc = os.path.join(root, "Quake", "vr", "vr_weapons.inc")
    slots = {}
    with open(inc) as f:
        for s, k, v in re.findall(r'QVR_WEAPON_(?:DEFAULT|UNINHERITED)\((\d+), (\w+), "([^"]*)"\)', f.read()):
            slots.setdefault(int(s), {})[k] = v
    for slot in sorted(slots):
        keys = slots[slot]
        if keys.get("ID", "").replace("\\", "/") != "progs/" + model_name:
            continue
        for enum, key, what in ANCHOR_KEYS:
            if enum in keys:
                out.append(("slot %d %s (vr_wofs_%s_%02d)" % (slot, what, key, slot + 1), keys[enum]))
    view = os.path.join(root, "Quake", "vr", "vr_view.cpp")
    if os.path.exists(view):
        with open(view) as f:
            for gun, text in MAG_RE.findall(f.read()):
                if gun == model_name:
                    out.append(("the magazine's mount (vr_view.cpp magMounts)", text))
    shells = os.path.join(root, "Quake", "vr", "vr_shells.cpp")
    if os.path.exists(shells):
        with open(shells) as f:
            text = f.read()
        table = text[text.find("constexpr Weapon weaponTable[]"):]
        for ident, a in SHELL_RE.findall(table[:table.find("};")]):
            if SHELL_MODELS.get(ident) == model_name:
                out.append(("the shell ejection port (vr_shells.cpp)", a))
    fx = os.path.join(root, "Quake", "vr", "vr_weaponfx.cpp")
    if os.path.exists(fx):
        with open(fx) as f:
            for monster, part, body in MONSTER_RE.findall(f.read()):
                if monster + ".mdl" == model_name.lower():
                    for i, a in enumerate(re.findall(r'"([^"]*)"', body)):
                        out.append(("the gun's %s %d (vr_weaponfx.cpp %s%s)" % (part.lower(), i, monster, part), a))
    return out


def weapon_slots(root, model_name):
    """[(slot, Scale)] for every slot of vr_weapons.inc's defaults that uses progs/<model_name>."""
    if root is None:
        return []
    slots = {}
    with open(os.path.join(root, "Quake", "vr", "vr_weapons.inc")) as f:
        for s, k, v in re.findall(r'QVR_WEAPON_(?:DEFAULT|UNINHERITED)\((\d+), (\w+), "([^"]*)"\)', f.read()):
            slots.setdefault(int(s), {})[k] = v
    out = []
    for slot in sorted(slots):
        if slots[slot].get("ID", "").replace("\\", "/") == "progs/" + model_name:
            try:
                out.append((slot, float(slots[slot].get("Scale", "1"))))
            except ValueError:
                out.append((slot, 1.0))
    return out


# The wrist gadget is placed by points in its model space, not by vertices (vr_gadget.cpp, vr_view.cpp): the screen
# spans x -1.5..1.5, y -0.93..0.93 at z 0.43 (just over its face at 0.41) and the HUD is drawn there; the hologram and
# the log rise from its centre; gadgetTop() takes the casing as 3.8 x 2.6 x 0.7 round the origin; the straps
# (vrgadget_strap.mdl) are drawn under the lugs at x -+1.25, z -0.47. make_gadget.py's hologram emitter (a slot along
# the top edge, y 1.21) is its look.
GADGET_SCREEN = ((-1.5, -0.93), (1.5, 0.93), 0.43)
GADGET_POINTS = (("the screen's centre (the HUD, the hologram and the log rise from it)", (0.0, 0.0, 0.43), 0.3),
                 ("the screen's corner -x -y", (-1.5, -0.93, 0.43), 0.25),
                 ("the screen's corner +x -y", (1.5, -0.93, 0.43), 0.25),
                 ("the screen's corner -x +y", (-1.5, 0.93, 0.43), 0.25),
                 ("the screen's corner +x +y", (1.5, 0.93, 0.43), 0.25),
                 ("the hologram's emitter (make_gadget.py)", (0.0, 1.21, 0.37), 0.2),
                 ("the lug of the elbow's strap (vr_view.cpp)", (-1.25, 0.0, -0.47), 0.35),
                 ("the lug of the wrist's strap (vr_view.cpp)", (1.25, 0.0, -0.47), 0.35))


def is_gadget(model_name):
    return model_name.lower() in ("vrgadget.mdl", "vrgadget_strap.mdl")


# ----------------------------------------------------------------------------
# The report after an export: every anchor before and after.


# Anchors (Quake/vr/vr_anchor.hpp): "x y z", a point on the surface in the model's own coordinates (frame 0). The
# engine binds it to the nearest triangle of the rest pose (barycentric weights, snapped to a corner it is on; among
# triangles as near, those with the point on a corner, the first) and moves it with those weights in every pose; the
# same here (Misc/quakevr/anchors.py has it too, for the scripts).
ANCHOR_SNAP = 0.001  # a point this near a triangle's corner is on it
ANCHOR_TIE = 0.001   # triangles this much nearer than the nearest are as near
ANCHOR_MOVED = 0.01  # an anchor that moved more than this (model units) in any pose is reported MOVED


def parse_anchor(text):
    """The point (x, y, z), or None: not three numbers."""
    nums = []
    for tok in re.split(r"[\s,_]+", text.strip()):
        if not tok:
            continue
        try:
            nums.append(float(tok))
        except ValueError:
            break
        if len(nums) == 3:
            return tuple(nums)
    return None


def closest_on_triangle(p, a, b, c):
    """(point, (wa, wb, wc)): the point of triangle abc nearest p (Ericson 5.1.5; as vr_anchor.cpp)."""
    def sub(u, v):
        return (u[0] - v[0], u[1] - v[1], u[2] - v[2])

    def dot(u, v):
        return u[0] * v[0] + u[1] * v[1] + u[2] * v[2]

    def at(o, u, v, x, y):
        return (o[0] + u[0] * x + v[0] * y, o[1] + u[1] * x + v[1] * y, o[2] + u[2] * x + v[2] * y)

    ab, ac, ap = sub(b, a), sub(c, a), sub(p, a)
    d1, d2 = dot(ab, ap), dot(ac, ap)
    if d1 <= 0 and d2 <= 0:
        return a, (1.0, 0.0, 0.0)
    bp = sub(p, b)
    d3, d4 = dot(ab, bp), dot(ac, bp)
    if d3 >= 0 and d4 <= d3:
        return b, (0.0, 1.0, 0.0)
    vc = d1 * d4 - d3 * d2
    if vc <= 0 and d1 >= 0 and d3 <= 0 and d1 - d3 > 0:
        v = d1 / (d1 - d3)
        return at(a, ab, ac, v, 0.0), (1 - v, v, 0.0)
    cp = sub(p, c)
    d5, d6 = dot(ab, cp), dot(ac, cp)
    if d6 >= 0 and d5 <= d6:
        return c, (0.0, 0.0, 1.0)
    vb = d5 * d2 - d1 * d6
    if vb <= 0 and d2 >= 0 and d6 <= 0 and d2 - d6 > 0:
        w = d2 / (d2 - d6)
        return at(a, ab, ac, 0.0, w), (1 - w, 0.0, w)
    va = d3 * d6 - d5 * d4
    if va <= 0 and d4 - d3 >= 0 and d5 - d6 >= 0 and (d4 - d3) + (d5 - d6) > 0:
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        return at(b, sub(c, b), (0.0, 0.0, 0.0), w, 0.0), (0.0, 1 - w, w)
    s = va + vb + vc
    if s <= 1e-18:
        ds = [math.dist(p, a), math.dist(p, b), math.dist(p, c)]
        k = ds.index(min(ds))
        return (a, b, c)[k], tuple(1.0 if i == k else 0.0 for i in range(3))
    v, w = vb / s, vc / s
    return at(a, ab, ac, v, w), (1 - v - w, v, w)


def bind_anchor(model, text, rest=None):
    """{vertex: weight}, the triangle bound to and the point's distance from the surface ({}: not an anchor)."""
    p = parse_anchor(text)
    if p is None:
        return {}, -1, 0.0
    rest = rest or [model.place(b) for b in model.pose_bytes()[0]]
    found = []
    for t, tri in enumerate(model.tris):
        vs = tri[1:4]
        q, w = closest_on_triangle(p, *(rest[v] for v in vs))
        d = math.dist(p, q)
        best, bd = -1, ANCHOR_SNAP
        for k in range(3):
            dk = math.dist(p, rest[vs[k]])
            if dk < bd:
                best, bd = k, dk
        if best >= 0:
            w, d = tuple(1.0 if k == best else 0.0 for k in range(3)), bd
        found.append((d, t, vs, w, best >= 0))
    if not found:
        return {}, -1, 0.0
    nearest = min(f[0] for f in found)
    tied = [f for f in found if f[0] <= nearest + ANCHOR_TIE]
    if any(f[4] for f in tied):  # (as vr_anchor.cpp: those on a corner, the first)
        tied = [f for f in tied if f[4]]
    d, t, vs, w, _ = tied[0]
    weights = {}
    for v, x in zip(vs, w):
        if x > 1e-6:
            weights[v] = weights.get(v, 0.0) + x
    return weights, t, d


def anchor_place(model, weights, pose):
    """Where a binding is in one pose's bytes (model space)."""
    x = [0.0, 0.0, 0.0]
    for v, w in weights.items():
        q = model.place(pose[v])
        for i in range(3):
            x[i] += q[i] * w
    return tuple(x)


def anchor_report(old, new, anchors, old_poses=None, new_poses=None):
    """(lines, moved): per anchor, where it binds and where it is in every pose, before and after (each bound on its own
    model, as the engine will). moved: [(what, text, distance)] for anchors that moved more than ANCHOR_MOVED in any
    pose."""
    old_poses = old_poses or old.pose_bytes()
    new_poses = new_poses or new.pose_bytes()
    old_rest = [old.place(b) for b in old_poses[0]]
    new_rest = [new.place(b) for b in new_poses[0]]
    lines, moved = [], []
    for what, text in anchors:
        ow, ot, od = bind_anchor(old, text, old_rest)
        nw, nt, nd = bind_anchor(new, text, new_rest)
        if not ow or not nw:
            lines.append("?      %s: \"%s\" binds to nothing (not x y z, or no triangles)" % (what, text))
            continue
        op = [anchor_place(old, ow, p) for p in old_poses]
        np_ = [anchor_place(new, nw, p) for p in new_poses]
        dist = max(math.dist(a, b) for a, b in zip(op, np_))
        tag, extra = "ok    ", ""
        if dist > ANCHOR_MOVED:
            moved.append((what, text, dist))
            tag = "MOVED "
            extra += " -> (%.2f, %.2f, %.2f): moved %.3f units (%.2f cm at the default size) at most over the frames" % (
                np_[0][0], np_[0][1], np_[0][2], dist, dist * 2.54)
        elif dist > 1e-6:
            extra += " (moved %.4f at most: within %.2f)" % (dist, ANCHOR_MOVED)
        if nd > ANCHOR_MOVED:
            extra += " -- %.3f off the new surface: pick it again (vr_anchor_nearest)" % nd
        lines.append("%s %s: \"%s\" at (%.2f, %.2f, %.2f), triangle %d, %.4f off the surface%s" % (
            tag, what, text, op[0][0], op[0][1], op[0][2], nt, nd, extra))
    return lines, moved


def gadget_report(old, new, old_poses=None, new_poses=None):
    """(lines, moved): the vertices round each point the engine places the gadget by (before and after), and anything
    now standing over the screen; for the strap, its inner radius (the bracer it is sized to)."""
    old_poses = old_poses or old.pose_bytes()
    new_poses = new_poses or new.pose_bytes()
    lines, moved = [], []
    if old.name.lower() == "vrgadget_strap.mdl":
        for p, (op, np_) in enumerate(zip(old_poses, new_poses)):
            def inner(model, pose):
                pts = [model.place(b) for b in pose]
                return min(math.hypot(q[1], q[2]) for q in pts), min(q[0] for q in pts), max(q[0] for q in pts)
            a, b = inner(old, op), inner(new, np_)
            bad = abs(a[0] - b[0]) > 1e-3 or abs(a[1] - b[1]) > 1e-3 or abs(a[2] - b[2]) > 1e-3
            if bad:
                moved.append(("the strap's ring, frame %d" % p, max(abs(x - y) for x, y in zip(a, b))))
            lines.append("%s frame %d: inner radius %.3f, x %.2f..%.2f%s (vr_view.cpp sizes the ring to the bracer: "
                         "radius 1 is its surface, x -1..1 the band's width)" % (
                             "MOVED " if bad else "ok    ", p, a[0], a[1], a[2],
                             " -> %.3f, x %.2f..%.2f" % b if bad else ""))
        return lines, moved
    (x0, y0), (x1, y1), z = GADGET_SCREEN
    old_p = [old.place(b) for b in old_poses[0]]
    new_p = [new.place(b) for b in new_poses[0]]
    for what, pt, r in GADGET_POINTS:
        near = [v for v, p in enumerate(old_p) if math.dist(p, pt) < r]
        worst = max([min(math.dist(old_p[v], q) for q in new_p) for v in near] + [0.0])
        if worst > 1e-6:
            moved.append((what, worst))
        lines.append("%s %s (%.2f, %.2f, %.2f): %d vertices within %.2f%s" % (
            "MOVED " if worst > 1e-6 else "ok    ", what, pt[0], pt[1], pt[2], len(near), r,
            ", moved %.3f units at most" % worst if worst > 1e-6 else ", unchanged"))
    over = [v for v, p in enumerate(new_p) if x0 < p[0] < x1 and y0 < p[1] < y1 and p[2] > z + 1e-3]
    if over:
        moved.append(("over the screen", float(len(over))))
        lines.append("OVER   %d vertices stand over the screen (z > %.2f inside x %.2f..%.2f, y %.2f..%.2f): the HUD is "
                     "drawn at z %.2f and they would cover it" % (len(over), z, x0, x1, y0, y1, z))
    lines.append("       the screen, the hologram, the log and the straps are placed at these points by the engine, "
                 "not from the model: if you move them, change vr_gadget.cpp and vr_view.cpp too")
    return lines, moved
