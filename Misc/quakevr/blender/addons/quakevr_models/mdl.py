# mdl.py -- reads and writes Quake alias models (.mdl) byte for byte, and what the Blender add-on needs to keep a
# weapon's or the wrist gadget's anchors: vr_anchor.cpp's strip order, the anchors the engine names (vr_weapons.inc,
# vr_shells.cpp, vr_gadget.cpp) and a report of where they are. Plain Python (no bpy, no numpy).
#
# An MDL vertex is a place per pose (a byte per axis on the header's grid: origin + byte * scale, and a normal index)
# and a skin coordinate (onseam, s, t in texels: a triangle facing back (facesfront 0) reads an onseam vertex's s half
# a skin further right). Anchors are strip-order indices: vr_anchor.cpp orders the vertices as QuakeSpasm's old
# BuildTris walked the triangles into strips and fans, so the order depends on every triangle's vertices and their
# order in the file. Old triangles kept in their places, and new ones after them sharing no vertex with them, keep
# the order of every old strip: every anchor names the same vertex.

import bisect
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
# vr_anchor.cpp's strip order (QuakeSpasm's old BuildTris), exactly: for each triangle not yet used, the longest fan
# or strip from each of its three corners (fans first, the first longest wins), each extended by the first later
# triangle facing the same way that runs along the open edge (a used one ends it).


def strip_order(tris):
    """Strip-order index -> vertex index. tris: [(facesfront, a, b, c)]."""
    n = len(tris)
    used = [0] * n
    edges = {}  # (facesfront, a, b) -> ascending triangles running a -> b
    for j, (ff, a, b, c) in enumerate(tris):
        for x, y in ((a, b), (b, c), (c, a)):
            edges.setdefault((ff, x, y), []).append(j)

    def first_after(ff, m1, m2, start):
        lst = edges.get((ff, m1, m2))
        if not lst:
            return None
        i = bisect.bisect_right(lst, start)
        return lst[i] if i < len(lst) else None

    def walk(start, sv, strip):
        used[start] = 2
        ff, *vi = tris[start]
        verts = [vi[sv % 3], vi[(sv + 1) % 3], vi[(sv + 2) % 3]]
        got = [start]
        if strip:
            m1, m2 = vi[(sv + 2) % 3], vi[(sv + 1) % 3]
        else:
            m1, m2 = vi[sv % 3], vi[(sv + 2) % 3]
        while len(got) < 126:
            j = first_after(ff, m1, m2, start)
            # The C walks j = start + 1 .. and takes the first triangle with the edge m1 -> m2 (in any of its three
            # rotations): that is the first in the edge's list after start.
            if j is None or used[j]:
                break
            cv = tris[j][1:]
            k = next(k for k in range(3) if cv[k] == m1 and cv[(k + 1) % 3] == m2)
            nv = cv[(k + 2) % 3]
            if strip:
                if len(got) & 1:
                    m2 = nv
                else:
                    m1 = nv
            else:
                m2 = nv
            verts.append(nv)
            got.append(j)
            used[j] = 2
        for j in got:
            used[j] = 0 if used[j] == 2 else used[j]
        used[start] = 0
        return verts, got

    order = []
    for i in range(n):
        if used[i]:
            continue
        best = None
        for strip in (False, True):
            for sv in range(3):
                v, t = walk(i, sv, strip)
                if best is None or len(t) > len(best[1]):
                    best = (v, t)
        for j in best[1]:
            used[j] = 1
        order += best[0]
    return order


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


ANCHOR_KEYS = (("HandAnchorVertex", "hand_av", "the hand's"), ("MuzzleAnchorVertex", "muzzle_av", "the muzzle's"),
               ("TwoHHandAnchorVertex", "2h_hand_av", "the off hand's (two-handed)"),
               ("WpnButtonAnchorVertex", "wpnbtn_av", "the ammo button's"),
               ("WpnTextAnchorVertex", "wpntxt_av", "the ammo screen's"))


def weapon_anchors(root, model_name):
    """[(what, slot, key, strip index)] for progs/<model_name>: vr_weapons.inc's defaults for every slot that uses
    the model, and vr_shells.cpp's ejection port anchors."""
    out = []
    if root is None:
        return out
    inc = os.path.join(root, "Quake", "vr", "vr_weapons.inc")
    slots = {}
    with open(inc) as f:
        for s, k, v in re.findall(r'QVR_WEAPON_DEFAULT\((\d+), (\w+), "([^"]*)"\)', f.read()):
            slots.setdefault(int(s), {})[k] = v
    for slot in sorted(slots):
        keys = slots[slot]
        if keys.get("ID", "").replace("\\", "/") != "progs/" + model_name:
            continue
        for enum, key, what in ANCHOR_KEYS:
            if enum in keys:
                try:
                    idx = int(float(keys[enum]))
                except ValueError:
                    continue
                if idx >= 0:
                    out.append(("slot %d %s (vr_wofs_%s_%02d)" % (slot, what, key, slot + 1), idx))
    shells = os.path.join(root, "Quake", "vr", "vr_shells.cpp")
    if os.path.exists(shells):
        with open(shells) as f:
            text = f.read()
        for m in re.finditer(r'\{"progs/([^"]+)",[^;]*?,\s*(-?\d+)\s*\}', text):
            if m.group(1) == model_name and int(m.group(2)) >= 0:
                out.append(("the shell ejection port (vr_shells.cpp)", int(m.group(2))))
    return out


def weapon_slots(root, model_name):
    """[(slot, Scale)] for every slot of vr_weapons.inc's defaults that uses progs/<model_name>."""
    if root is None:
        return []
    slots = {}
    with open(os.path.join(root, "Quake", "vr", "vr_weapons.inc")) as f:
        for s, k, v in re.findall(r'QVR_WEAPON_DEFAULT\((\d+), (\w+), "([^"]*)"\)', f.read()):
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


def anchor_report(old, new, anchors, old_poses=None, new_poses=None):
    """(lines, moved, renamed): per anchor, the vertex it names and where it is in every pose, before and after.
    moved: [(what, index, distance)] for anchors whose place changed; renamed: [(what, index, old vertex, new vertex,
    the index naming the old vertex now or None)] for anchors now naming another vertex (the strip order changed)."""
    old_poses = old_poses or old.pose_bytes()
    new_poses = new_poses or new.pose_bytes()
    oo = strip_order(old.tris)
    no = strip_order(new.tris)
    lines, moved, renamed = [], [], []
    if no[:len(oo)] == oo:
        lines.append("strip order: unchanged (every anchor index names the same vertex as before)")
    else:
        first = next((i for i in range(min(len(oo), len(no))) if oo[i] != no[i]), min(len(oo), len(no)))
        hit = [a for a in anchors if a[1] >= first]
        lines.append("strip order: changed from index %d on (old triangles deleted, or changed so they left their "
                     "places): an anchor index from %d on names another vertex now; %s" % (
                         first, first, "anchors in use there: %s" % ", ".join(str(a[1]) for a in hit) if hit else
                         "the anchors in vr_weapons.inc are all below it (your own vr_wofs_*_av settings may not be)"))
    for what, idx in anchors:
        ov = oo[max(0, min(idx, len(oo) - 1))]
        nv = no[max(0, min(idx, len(no) - 1))]
        op = [old.place(p[ov]) for p in old_poses]
        np_ = [new.place(p[ov]) for p in new_poses]
        dist = max(math.dist(a, b) for a, b in zip(op, np_))
        tag = "ok    "
        extra = ""
        if ov != nv:
            now = next((i for i, v in enumerate(no) if v == ov), None)
            renamed.append((what, idx, ov, nv, now))
            tag = "RENAMED"
            extra = " -- index %d now names vertex %d; vertex %d is %s" % (
                idx, nv, ov, "index %d now" % now if now is not None else "in no triangle now")
        if dist > 1e-6:
            moved.append((what, idx, dist))
            tag = "MOVED " if ov == nv else tag
            extra += " -> (%.2f, %.2f, %.2f): moved %.3f units (%.2f cm at the default size) at most over the frames" % (
                np_[0][0], np_[0][1], np_[0][2], dist, dist * 2.54)
        lines.append("%s %s: index %d = vertex %d at (%.2f, %.2f, %.2f)%s" % (
            tag, what, idx, ov, op[0][0], op[0][1], op[0][2], extra))
    return lines, moved, renamed


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
        worst = max([math.dist(old_p[v], new_p[v]) for v in near if v < len(new_p)] + [0.0])
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
