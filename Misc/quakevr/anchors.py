#!/usr/bin/env python3
# anchors.py -- the weapon models' anchors (Quake/vr/vr_anchor.hpp) for the scripts: their text, binding a point to a
# model's surface exactly as the engine does, where an anchor is in every pose, and the check of a model edit.
#
# An anchor is text: "x y z" (a point on the model's surface, in its own coordinates as its file's frame 0 has them:
# scale and origin applied), "x y z t" (t a triangle, the tie-break among triangles as near: two parts touching there),
# or a legacy anchor index (one number: a vertex in QuakeSpasm's old strip order, improve_weapons.strip_order). The
# engine binds the point to the nearest triangle of the rest pose (barycentric weights over its three vertices, snapped
# to a corner it is on) and moves it with those weights in every pose: the vertex numbering doesn't matter, so the
# models can be edited freely. The anchors in use: vr_weapons.inc's *_av defaults, vr_view.cpp's magMounts,
# vr_shells.cpp's weaponTable (collect()).
#
# Usage (from the repository root):
#   python Misc/quakevr/anchors.py list                     every anchor in use and where it binds on its model
#   python Misc/quakevr/anchors.py check <model.mdl> [old]  the anchors on that model: within TOLERANCE of the surface
#                                                           in every pose (against `old`: moved at most TOLERANCE)
# The conversion from indices to points (vr_wofs_version 41, round 22), kept for the record:
#   python Misc/quakevr/anchors.py convert-commands         the engine commands converting every legacy anchor
#                                                           (vr_anchor_from_index: run them, keep the console log)
#   python Misc/quakevr/anchors.py apply <log>              writes the log's points into the three sources
#   python Misc/quakevr/anchors.py compare-commands <rev>   vr_anchor_compare for each anchor: the index at git <rev>
#                                                           against the point now
#   python Misc/quakevr/anchors.py table <log>              the comparison's table (engine and this script's own)

import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
INC = os.path.join("Quake", "vr", "vr_weapons.inc")
VIEW = os.path.join("Quake", "vr", "vr_view.cpp")
SHELLS = os.path.join("Quake", "vr", "vr_shells.cpp")
PROGS = os.path.join(ROOT, "quakevr", "progs")

KEYS = ("HandAnchorVertex", "MuzzleAnchorVertex", "TwoHHandAnchorVertex", "WpnButtonAnchorVertex",
        "WpnTextAnchorVertex")

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
    """("legacy", index) or ("point", (x, y, z), hint or -1), as vr_anchor.cpp's parse."""
    nums = []
    for tok in re.split(r"[\s,_]+", text.strip()):
        if not tok:
            continue
        try:
            nums.append(float(tok))
        except ValueError:
            break
        if len(nums) == 4:
            break
    if len(nums) >= 3:
        return ("point", tuple(nums[:3]), int(nums[3]) if len(nums) >= 4 else -1)
    return ("legacy", int(nums[0]) if nums else 0)


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
    def __init__(self, verts, weights, tri=-1, distance=0.0, ties=0, legacy=False):
        self.verts, self.weights, self.tri, self.distance, self.ties, self.legacy = verts, weights, tri, distance, ties, legacy

    def key(self):
        """What it moves with: {vertex: weight}."""
        k = {}
        for v, w in zip(self.verts, self.weights):
            if w > 1e-6:
                k[v] = k.get(v, 0.0) + w
        return k


class Mesh:
    """A model file's triangles and poses in its own coordinates (pose 0: frame 0, a group's first: the rest)."""

    def __init__(self, path):
        import mdlpolish as mp
        self.path = path
        self.m = mp.Model(path)
        self.tris = [t[1:] for t in self.m.tris]
        self.poses = [[tuple(float(x) for x in v) for v in self.m.positions(p)] for p in range(self.m.num_poses())]
        self.rest = self.poses[0]
        self._order = None

    def order(self):
        if self._order is None:
            from improve_weapons import strip_order
            self._order = strip_order(self.m.tris)
        return self._order

    def bind_triangle(self, t, p):
        tri = self.tris[t]
        q, w = closest_on_triangle(p, *(self.rest[v] for v in tri))
        d = _dist(p, q)
        best, bd = -1, SNAP
        for k in range(3):
            dk = _dist(p, self.rest[tri[k]])
            if dk < bd:
                best, bd = k, dk
        if best >= 0:
            w, d = tuple(1.0 if k == best else 0.0 for k in range(3)), bd
        return Binding(tuple(tri), w, t, d)

    def bind(self, text):
        a = parse(text)
        if a[0] == "legacy":
            order = self.order()
            v = order[max(0, min(a[1], len(order) - 1))]
            return Binding((v, v, v), (1.0, 0.0, 0.0), legacy=True)
        p, hint = a[1], a[2]
        all_ = [self.bind_triangle(t, p) for t in range(len(self.tris))]
        nearest = min(b.distance for b in all_)
        tied = [b for b in all_ if b.distance <= nearest + TIE]
        out = next((b for b in tied if b.tri == hint), tied[0])
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


def text_of(point, hint=-1):
    """An anchor's text as the engine writes it (4 decimals, no trailing zeros)."""
    def num(f):
        s = ("%.4f" % f).rstrip("0").rstrip(".")
        return "0" if s in ("-0", "") else s
    return " ".join(num(f) for f in point) + ("" if hint < 0 else " %d" % hint)


# ----------------------------------------------------------------------------
# The anchors in use

def _read(rel, rev=None):
    if rev:
        return subprocess.run(["git", "-C", ROOT, "show", "%s:%s" % (rev, rel.replace(os.sep, "/"))],
                              capture_output=True, text=True, check=True).stdout
    return open(os.path.join(ROOT, rel), encoding="utf-8").read()


INC_RE = re.compile(r'QVR_WEAPON_DEFAULT\((\d+), (\w+), "([^"]*)"\)')
# (the anchor quoted, or a bare number: an index before round 22)
MAG_RE = re.compile(r'"progs/vr_mag_on_(v_\w+\.mdl)", "?([^",]*?)"?,')
SHELL_RE = re.compile(r'\{modelmeta::Id::(\w+), .*?,\s+"?([^",{}\n]*?)"?\},[ \t]*$', re.S | re.M)


def collect(rev=None):
    """[(where, model, text)] in source order: vr_weapons.inc's anchor defaults (by its slot's ID), the magazines'
    mounts, the shells' ports."""
    inc = _read(INC, rev)
    ids = {}
    for s, k, v in INC_RE.findall(inc):
        if k == "ID":
            ids[int(s)] = v
    out = []
    for s, k, v in INC_RE.findall(inc):
        if k in KEYS:
            out.append(("vr_weapons.inc slot %s %s" % (s, k), ids.get(int(s), ""), v))
    for gun, a in MAG_RE.findall(_read(VIEW, rev)):
        out.append(("vr_view.cpp magMounts %s" % gun, "progs/" + gun, a))
    shells = _read(SHELLS, rev)
    table = shells[shells.find("constexpr Weapon weaponTable[]"):]
    table = table[:table.find("};")]
    for ident, a in SHELL_RE.findall(table):
        out.append(("vr_shells.cpp weaponTable %s" % ident, SHELL_MODELS.get(ident, ""), a))
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
# The conversion (round 22)

def convert_commands():
    seen, cmds = set(), []
    for where, model, text in collect():
        a = parse(text)
        if a[0] == "legacy" and model and model != "-1" and (model, a[1]) not in seen:
            seen.add((model, a[1]))
            cmds.append("vr_anchor_from_index %s %d" % (model, a[1]))
    return cmds


def apply(log):
    conv = {}
    for m, i, t in re.findall(r'anchor_from_index (\S+) (-?\d+) "([^"]*)"', open(log, encoding="utf-8", errors="replace").read()):
        conv[(m, int(i))] = t
    counts = {}

    def fix(m, group, model):
        """The match with its group `group` (an anchor's text on `model`) converted, if the log has it."""
        a = parse(m.group(group))
        if a[0] != "legacy" or (model, a[1]) not in conv:
            return m.group(0)
        counts[model] = counts.get(model, 0) + 1
        s, e = m.start(group) - m.start(0), m.end(group) - m.start(0)
        return m.group(0)[:s] + conv[(model, a[1])] + m.group(0)[e:]

    inc = _read(INC)
    ids = {int(s): v for s, k, v in INC_RE.findall(inc) if k == "ID"}
    inc = INC_RE.sub(lambda m: fix(m, 3, ids.get(int(m.group(1)), "")) if m.group(2) in KEYS else m.group(0), inc)
    open(os.path.join(ROOT, INC), "w", encoding="utf-8", newline=chr(10)).write(inc)
    view = MAG_RE.sub(lambda m: fix(m, 2, "progs/" + m.group(1)), _read(VIEW))
    open(os.path.join(ROOT, VIEW), "w", encoding="utf-8", newline=chr(10)).write(view)
    shells = _read(SHELLS)
    start = shells.find("constexpr Weapon weaponTable[]")
    end = shells.find("};", start)
    table = SHELL_RE.sub(lambda m: fix(m, 2, SHELL_MODELS.get(m.group(1), "")), shells[start:end])
    open(os.path.join(ROOT, SHELLS), "w", encoding="utf-8", newline=chr(10)).write(shells[:start] + table + shells[end:])
    print("converted: %s" % ", ".join("%s %d" % kv for kv in sorted(counts.items())))


def compare_pairs(rev):
    old, new = collect(rev), collect()
    if [w for w, m, t in old] != [w for w, m, t in new]:
        sys.exit("the anchors at %s and now differ in number or order" % rev)
    return [(w, m, ot, nt) for (w, m, ot), (_, _, nt) in zip(old, new)]


def compare_commands(rev):
    cmds = []
    for where, model, old, new in compare_pairs(rev):
        a = parse(old)
        if a[0] == "legacy" and parse(new)[0] == "point" and model:
            cmds.append("vr_anchor_compare %s %d %s" % (model, a[1], new))
    return cmds


def table(log, rev):
    text = open(log, encoding="utf-8", errors="replace").read().replace("\n", " ")
    engine = {}
    for m, i, t, poses, worst, rest in re.findall(
            r'anchor_compare (\S+) (-?\d+) "([^"]*)": (\d+) poses, max ([\d.]+) \(pose \d+\); (.*?) ties', text):
        engine[(m, int(i), t)] = (int(poses), float(worst), rest + " ties")
    meshes = {}
    worst_all = 0.0
    rows = 0
    print("%-48s %-22s %6s %-30s %5s %9s %9s" % ("where", "model", "index", "point", "poses", "engine", "script"))
    for where, model, old, new in compare_pairs(rev):
        a = parse(old)
        if a[0] != "legacy" or parse(new)[0] != "point" or not model:
            continue
        e = engine.get((model, a[1], new))
        path = model_file(model)
        script = "-"
        if path:
            mesh = meshes.setdefault(path, Mesh(path))
            ob, nb = mesh.bind(old), mesh.bind(new)
            script = "%.5f" % max(_dist(mesh.position(ob, p), mesh.position(nb, p)) for p in range(len(mesh.poses)))
            worst_all = max(worst_all, float(script))
        if e:
            worst_all = max(worst_all, e[1])
        rows += 1
        print("%-48s %-22s %6d %-30s %5s %9s %9s" % (where, model, a[1], new, e[0] if e else "-",
                                                    "%.5f" % e[1] if e else "MISSING", script))
    print("%d anchors, worst %.5f model units" % (rows, worst_all))


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
            print("%-48s %-28s %-30s %s" % (where, model, text, "legacy vertex %d" % b.verts[0] if b.legacy else
                                            "triangle %d, %.4f off, %d ties" % (b.tri, b.distance, b.ties)))
    elif args[0] == "check":
        bad = 0
        for where, text, worst, line in check(args[1], args[2] if len(args) > 2 else None):
            bad += worst > TOLERANCE
            print("%-48s %-30s %s" % (where, text, line))
        sys.exit(1 if bad else 0)
    elif args[0] == "convert-commands":
        print(";".join(convert_commands()))
    elif args[0] == "apply":
        apply(args[1])
    elif args[0] == "compare-commands":
        print(";".join(compare_commands(args[1])))
    elif args[0] == "table":
        table(args[1], args[2])
    else:
        sys.exit(__doc__ or "see the header")


if __name__ == "__main__":
    main()
