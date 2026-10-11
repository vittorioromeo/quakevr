#!/usr/bin/env python3
# anchors.py -- the weapon models' anchors (Quake/vr/vr_anchor.hpp) for the scripts: their text, binding a point to a
# model's surface exactly as the engine does, where an anchor is in every pose, and the check of a model edit.
#
# An anchor is text, "x y z": a point on the model's surface, in its own coordinates as its file's frame 0 has them
# (scale and origin applied). The engine binds it to the nearest triangle of the rest pose (barycentric weights over its
# three vertices, snapped to a corner it is on; among triangles as near, those with the point on a corner, the first)
# and moves it with those weights in every pose: the vertex numbering doesn't matter, so the models can be edited
# freely. The anchors in use: vr_weapons.inc's *_av defaults, vr_view.cpp's magMounts, vr_shells.cpp's weaponTable,
# vr_weaponfx.cpp's monster guns (collect()).
#
# Usage (from the repository root):
#   python Misc/quakevr/anchors.py list                     every anchor in use and where it binds on its model
#   python Misc/quakevr/anchors.py selftest                 every anchor bound on its model renumbered (vertices and
#                                                           triangles reversed): it must stay where it is in every pose
#   python Misc/quakevr/anchors.py check <model.mdl> [old]  the anchors on that model: within TOLERANCE of the surface
#                                                           in every pose (against `old`: moved at most TOLERANCE)
#   python Misc/quakevr/anchors.py point <model.mdl> <vertex>  the anchor at a vertex (the file's numbering), to paste

import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
INC = os.path.join("Quake", "vr", "vr_weapons.inc")
VIEW = os.path.join("Quake", "vr", "vr_view.cpp")
SHELLS = os.path.join("Quake", "vr", "vr_shells.cpp")
WEAPONFX = os.path.join("Quake", "vr", "vr_weaponfx.cpp")
PROGS = os.path.join(ROOT, "quakevr", "progs")

KEYS = ("HandAnchor", "MuzzleAnchor", "TwoHHandAnchor", "WpnButtonAnchor", "WpnTextAnchor")

# As vr_anchor.cpp: a point this near a triangle's corner is on it; triangles this much nearer than the nearest tie.
SNAP = 0.001
TIE = 0.001
# A model edit's check: an anchor this near the surface (and moved at most this far) in every pose, in model units
# (a Quake VR weapon's model unit is about 3 mm drawn).
TOLERANCE = 0.1

SHELL_MODELS = {"VShot": "progs/v_shot.mdl", "VShot2": "progs/v_shot2.mdl"}


# ----------------------------------------------------------------------------
# Text

def parse(text):
    """The point (x, y, z), as vr_anchor.cpp's parse; None if the text is not three numbers."""
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


# ----------------------------------------------------------------------------
# Binding (vr_anchor.cpp's closestOnTriangle, bindTriangle, bind)

def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _dist(a, b):
    d = _sub(a, b)
    return _dot(d, d) ** 0.5


def _at(a, ab, ac, v, w):
    return (a[0] + ab[0] * v + ac[0] * w, a[1] + ab[1] * v + ac[1] * w, a[2] + ab[2] * v + ac[2] * w)


def closest_on_triangle(p, a, b, c):
    """(point, (wa, wb, wc)): the point of triangle abc nearest p (Ericson 5.1.5)."""
    ab, ac, ap = _sub(b, a), _sub(c, a), _sub(p, a)
    d1, d2 = _dot(ab, ap), _dot(ac, ap)
    if d1 <= 0 and d2 <= 0:
        return a, (1.0, 0.0, 0.0)
    bp = _sub(p, b)
    d3, d4 = _dot(ab, bp), _dot(ac, bp)
    if d3 >= 0 and d4 <= d3:
        return b, (0.0, 1.0, 0.0)
    vc = d1 * d4 - d3 * d2
    if vc <= 0 and d1 >= 0 and d3 <= 0 and d1 - d3 > 0:
        v = d1 / (d1 - d3)
        return _at(a, ab, ac, v, 0.0), (1 - v, v, 0.0)
    cp = _sub(p, c)
    d5, d6 = _dot(ab, cp), _dot(ac, cp)
    if d6 >= 0 and d5 <= d6:
        return c, (0.0, 0.0, 1.0)
    vb = d5 * d2 - d1 * d6
    if vb <= 0 and d2 >= 0 and d6 <= 0 and d2 - d6 > 0:
        w = d2 / (d2 - d6)
        return _at(a, ab, ac, 0.0, w), (1 - w, 0.0, w)
    va = d3 * d6 - d5 * d4
    if va <= 0 and d4 - d3 >= 0 and d5 - d6 >= 0 and (d4 - d3) + (d5 - d6) > 0:
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        bc = _sub(c, b)
        return (b[0] + bc[0] * w, b[1] + bc[1] * w, b[2] + bc[2] * w), (0.0, 1 - w, w)
    s = va + vb + vc
    if s <= 1e-18:
        ds = [_dist(p, a), _dist(p, b), _dist(p, c)]
        k = ds.index(min(ds))
        return (a, b, c)[k], tuple(1.0 if i == k else 0.0 for i in range(3))
    v, w = vb / s, vc / s
    return _at(a, ab, ac, v, w), (1 - v - w, v, w)


class Binding:
    def __init__(self, verts, weights, tri=-1, distance=0.0):
        self.verts, self.weights, self.tri, self.distance = verts, weights, tri, distance
        self.corner, self.ties = False, 0

    def key(self):
        """What it moves with: {vertex: weight}."""
        k = {}
        for v, w in zip(self.verts, self.weights):
            if w > 1e-6:
                k[v] = k.get(v, 0.0) + w
        return k


class Mesh:
    """A model file's triangles and poses in its own coordinates (pose 0: frame 0, a group's first: the rest)."""

    def __init__(self, path, model=None):
        import mdlpolish as mp
        self.path = path
        self.m = model if model is not None else mp.Model(path)
        self.tris = [t[1:] for t in self.m.tris]
        self.poses = [[tuple(float(x) for x in v) for v in self.m.positions(p)] for p in range(self.m.num_poses())]
        self.rest = self.poses[0]

    def bind_triangle(self, t, p):
        tri = self.tris[t]
        q, w = closest_on_triangle(p, *(self.rest[v] for v in tri))
        d = _dist(p, q)
        best, bd = -1, SNAP
        for k in range(3):
            dk = _dist(p, self.rest[tri[k]])
            if dk < bd:
                best, bd = k, dk
        b = Binding(tuple(tri), w, t, d)
        if best >= 0:
            b.weights, b.distance, b.corner = tuple(1.0 if k == best else 0.0 for k in range(3)), bd, True
        return b

    def bind(self, text):
        """As vr_anchor.cpp's bind: the nearest triangles (within TIE), of them those with the point on a corner if
        any, the first; ties: the others that bind elsewhere. Text that is not a point: the first vertex."""
        p = parse(text)
        if p is None:
            return Binding((0, 0, 0), (1.0, 0.0, 0.0))
        all_ = [self.bind_triangle(t, p) for t in range(len(self.tris))]
        nearest = min(b.distance for b in all_)
        tied = [b for b in all_ if b.distance <= nearest + TIE]
        any_corner = any(b.corner for b in tied)
        tied = [b for b in tied if b.corner or not any_corner]
        out = tied[0]
        out.ties = sum(1 for b in tied if not _same(out, b))
        return out

    def position(self, b, pose):
        P = self.poses[pose]
        x = [0.0, 0.0, 0.0]
        for v, w in zip(b.verts, b.weights):
            if w:
                for i in range(3):
                    x[i] += P[v][i] * w
        return tuple(x)


def _same(a, b):
    ka, kb = a.key(), b.key()
    return all(abs(kb.get(v, 0.0) - w) <= 1e-4 for v, w in ka.items())


def anchor_vertices(m, texts):
    """The vertices of mdlpolish model `m` that the anchors are on (a point bound to a triangle's corner): for the
    generators that keep those vertices where they are (refine_light.py)."""
    mesh = Mesh(getattr(m, "path", ""), m)
    out = set()
    for text in texts:
        b = mesh.bind(text)
        if b.corner:
            out.add(b.verts[b.weights.index(max(b.weights))])
    return out


def vertex_text(mesh, v):
    """The anchor at vertex `v` of `mesh` (a Mesh, or a model file's path): its rest place, checked to be where the
    vertex is in every pose (else an AssertionError: another vertex there that moves elsewhere; pick a point off it).
    What the generators print for vr_weapons.inc."""
    mesh = mesh if isinstance(mesh, Mesh) else Mesh(mesh)
    text = text_of(mesh.rest[v])
    b = mesh.bind(text)
    worst = max(_dist(mesh.position(b, p), mesh.poses[p][v]) for p in range(len(mesh.poses)))
    assert worst < 1e-3, "vertex %d: its point binds elsewhere (%.3f off in some pose)" % (v, worst)
    return text


def text_of(point):
    """An anchor's text as the engine writes it (4 decimals, no trailing zeros)."""
    def num(f):
        s = ("%.4f" % f).rstrip("0").rstrip(".")
        return "0" if s in ("-0", "") else s
    return " ".join(num(f) for f in point)


# ----------------------------------------------------------------------------
# The anchors in use

def _read(rel):
    return open(os.path.join(ROOT, rel), encoding="utf-8").read()


INC_RE = re.compile(r'QVR_WEAPON_DEFAULT\((\d+), (\w+), "([^"]*)"\)')
MAG_RE = re.compile(r'"progs/vr_mag_on_(v_\w+\.mdl)", "([^"]*)",')
SHELL_RE = re.compile(r'\{modelmeta::Id::(\w+), .*?,\s+"([^"]*)"\},[ 	]*$', re.S | re.M)
MONSTER_RE = re.compile(r'constexpr const char\* (soldier|enforcer)(Muzzle|Rear)\[\] = \{(.*?)\};', re.S)


def collect():
    """[(where, model, text)] in source order: vr_weapons.inc's anchor defaults (by its slot's ID), the magazines'
    mounts, the shells' ports, the monster guns' muzzles and rears."""
    inc = _read(INC)
    ids = {}
    for s, k, v in INC_RE.findall(inc):
        if k == "ID":
            ids[int(s)] = v
    out = []
    for s, k, v in INC_RE.findall(inc):
        if k in KEYS:
            out.append(("vr_weapons.inc slot %s %s" % (s, k), ids.get(int(s), ""), v))
    for gun, a in MAG_RE.findall(_read(VIEW)):
        out.append(("vr_view.cpp magMounts %s" % gun, "progs/" + gun, a))
    shells = _read(SHELLS)
    table = shells[shells.find("constexpr Weapon weaponTable[]"):]
    table = table[:table.find("};")]
    for ident, a in SHELL_RE.findall(table):
        out.append(("vr_shells.cpp weaponTable %s" % ident, SHELL_MODELS.get(ident, ""), a))
    for monster, part, body in MONSTER_RE.findall(_read(WEAPONFX)):
        for i, a in enumerate(re.findall(r'"([^"]*)"', body)):
            out.append(("vr_weaponfx.cpp %s%s %d" % (monster, part, i), "progs/%s.mdl" % monster, a))
    return out


def model_file(model):
    """The repository's file for a model path (progs/x.mdl), or None (an owned game's, a placeholder's)."""
    if not model.startswith("progs/"):
        return None
    path = os.path.join(PROGS, model[len("progs/"):])
    return path if os.path.exists(path) else None


# ----------------------------------------------------------------------------
# Checks

def check(path, old_path=None, anchors=None, tolerance=TOLERANCE):
    """[(where, text, worst, line)]: each anchor on the model at `path` -- its distance from the surface at rest, and
    against `old_path` how far it moved in any pose (each bound on its own model) -- and the worst of them."""
    name = "progs/" + os.path.basename(path)
    anchors = anchors if anchors is not None else [(w, t) for w, m, t in collect() if m == name]
    new = Mesh(path)
    old = Mesh(old_path) if old_path else None
    out = []
    for where, text in anchors:
        b = new.bind(text)
        worst = b.distance
        line = "%.4f off the surface" % b.distance
        if old:
            ob = old.bind(text)
            n = min(len(old.poses), len(new.poses))
            moved = max(_dist(old.position(ob, p), new.position(b, p)) for p in range(n))
            worst = max(worst, moved)
            line += ", moved %.4f at most over %d poses" % (moved, n)
        out.append((where, text, worst, line + ("" if worst <= tolerance else "  OVER %.3f" % tolerance)))
    return out


# ----------------------------------------------------------------------------
# The self-test

def renumbered(mesh):
    """A copy of `mesh` with its vertices and triangles in reverse order and each triangle's corners turned: the same
    surface in every pose, numbered otherwise (what an edit in Blender may do)."""
    import copy
    n = len(mesh.rest)
    out = copy.copy(mesh)
    out.poses = [list(reversed(P)) for P in mesh.poses]
    out.rest = out.poses[0]
    out.tris = [(n - 1 - c, n - 1 - a, n - 1 - b) for a, b, c in reversed(mesh.tris)]
    return out


def selftest():
    """Every anchor in use, bound on its model and on the model renumbered: where it is in every pose, both ways."""
    worst, moved, n = 0.0, [], 0
    meshes = {}
    for where, model, text in collect():
        path = model_file(model)
        if not path or parse(text) is None:
            continue
        mesh = meshes.setdefault(path, Mesh(path))
        other = meshes.setdefault(path + "#renumbered", renumbered(mesh))
        b, c = mesh.bind(text), other.bind(text)
        d = max(_dist(mesh.position(b, p), other.position(c, p)) for p in range(len(mesh.poses)))
        n += 1
        if d > 1e-4:
            moved.append((where, model, text, d))
        worst = max(worst, d)
    for m in moved:
        print("moved when renumbered: %s %s \"%s\": %.3f" % m)
    print("%d anchors bound on their models renumbered: %d moved, %.6f at most" % (n, len(moved), worst))
    return not moved


def main():
    args = sys.argv[1:]
    sys.path.insert(0, HERE)
    if not args or args[0] == "list":
        meshes = {}
        for where, model, text in collect():
            path = model_file(model)
            if not path:
                print("%-48s %-28s %-30s (no file in quakevr/progs)" % (where, model, text))
                continue
            mesh = meshes.setdefault(path, Mesh(path))
            b = mesh.bind(text)
            print("%-48s %-28s %-30s triangle %d, %.4f off, %d ties" % (where, model, text, b.tri, b.distance, b.ties))
    elif args[0] == "selftest":
        sys.exit(0 if selftest() else 1)
    elif args[0] == "check":
        bad = 0
        for where, text, worst, line in check(args[1], args[2] if len(args) > 2 else None):
            bad += worst > TOLERANCE
            print("%-48s %-30s %s" % (where, text, line))
        sys.exit(1 if bad else 0)
    elif args[0] == "point":
        print(vertex_text(args[1], int(args[2])))
    else:
        sys.exit(__doc__ or "see the header")


if __name__ == "__main__":
    main()
