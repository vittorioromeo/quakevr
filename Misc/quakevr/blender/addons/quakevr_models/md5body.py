# md5body.py -- reads and writes the body's MD5 files (quakevr/progs/vrbody*.md5mesh and .md5anim, from
# Misc/quakevr/make_vrbody.py) keeping every number the body wasn't edited at as it was written: a vertex that
# wasn't moved, reweighted or given other texture coordinates is written with its own lines' numbers. Plain Python.
#
# The engine reads a vertex's place as its weights' sum of bias * (joint + joint's rotation * offset): the offsets
# are in each joint's own frame (make_vrbody.py: +x along the bone, +z its hint). The joints are the engine's bind
# pose (vr_avatar.cpp's bind() and jointNames): their names, places and orientations are kept as they are.

import math
import re
import struct


class Md5Error(Exception):
    pass


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def quat_w(x, y, z):
    t = 1.0 - (x * x + y * y + z * z)
    return -math.sqrt(t) if t > 0.0 else 0.0


def quat_matrix(q):
    """The rotation (rows) of the MD5 quaternion (x, y, z) with w = -sqrt(1 - |xyz|^2)."""
    x, y, z = q
    w = quat_w(x, y, z)
    return ((1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)),
            (2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)),
            (2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)))


def mat_vec(m, v):
    return tuple(m[r][0] * v[0] + m[r][1] * v[1] + m[r][2] * v[2] for r in range(3))


def mat_t_vec(m, v):
    return tuple(m[0][c] * v[0] + m[1][c] * v[1] + m[2][c] * v[2] for c in range(3))


NUM = r"(-?[0-9.eE+-]+)"
RE_JOINT = re.compile(r'^\s*"([^"]*)"\s+(-?\d+)\s*\(\s*%s\s+%s\s+%s\s*\)\s*\(\s*%s\s+%s\s+%s\s*\)' % ((NUM,) * 6))
RE_VERT = re.compile(r"^\s*vert\s+(\d+)\s*\(\s*%s\s+%s\s*\)\s*(\d+)\s+(\d+)" % (NUM, NUM))
RE_TRI = re.compile(r"^\s*tri\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)")
RE_WEIGHT = re.compile(r"^\s*weight\s+(\d+)\s+(\d+)\s+%s\s*\(\s*%s\s+%s\s+%s\s*\)" % ((NUM,) * 4))


class Mesh:
    """joints: [(name, parent, pos, quat)] (doubles as read); joint_lines: the joints block's lines as written;
    verts: [((s, t), (s text, t text), first, count)]; tris: [(a, b, c)]; weights: [(joint, bias, offset,
    (bias text, x text, y text, z text))]; commandline, shader."""

    def __init__(self, text, what="vrbody.md5mesh"):
        self.what = what
        lines = text.split("\n")
        self.commandline = ""
        self.shader = ""
        self.joints, self.joint_lines, self.verts, self.tris, self.weights = [], [], [], [], []
        section = None
        counts = {}
        for n, line in enumerate(lines, 1):
            s = line.strip()
            if not s or s.startswith("//"):
                continue
            if s.startswith("MD5Version"):
                if s.split()[1] != "10":
                    raise Md5Error("%s: not MD5Version 10" % what)
            elif s.startswith("commandline"):
                self.commandline = s[len("commandline"):].strip().strip('"')
            elif s.startswith(("numJoints", "numMeshes", "numverts", "numtris", "numweights")):
                k, v = s.split()
                counts[k] = int(v)
                if k == "numMeshes" and int(v) != 1:
                    raise Md5Error("%s: %s meshes; the body is one mesh" % (what, v))
            elif s == "joints {":
                section = "joints"
            elif s == "mesh {":
                section = "mesh"
            elif s == "}":
                section = None
            elif section == "joints":
                m = RE_JOINT.match(line)
                if not m:
                    raise Md5Error("%s, line %d: not a joint: %s" % (what, n, s))
                g = m.groups()
                self.joints.append((g[0], int(g[1]), tuple(float(x) for x in g[2:5]), tuple(float(x) for x in g[5:8])))
                self.joint_lines.append(line)
            elif section == "mesh":
                if s.startswith("shader"):
                    self.shader = s[len("shader"):].strip().strip('"')
                elif s.startswith("vert"):
                    m = RE_VERT.match(line)
                    if not m:
                        raise Md5Error("%s, line %d: not a vertex: %s" % (what, n, s))
                    g = m.groups()
                    self.verts.append(((float(g[1]), float(g[2])), (g[1], g[2]), int(g[3]), int(g[4])))
                elif s.startswith("tri"):
                    m = RE_TRI.match(line)
                    if not m:
                        raise Md5Error("%s, line %d: not a triangle: %s" % (what, n, s))
                    self.tris.append(tuple(int(x) for x in m.groups()[1:]))
                elif s.startswith("weight"):
                    m = RE_WEIGHT.match(line)
                    if not m:
                        raise Md5Error("%s, line %d: not a weight: %s" % (what, n, s))
                    g = m.groups()
                    self.weights.append((int(g[1]), float(g[2]), tuple(float(x) for x in g[3:6]), g[2:6]))
                else:
                    raise Md5Error("%s, line %d: unexpected: %s" % (what, n, s))
        for k, got in (("numJoints", self.joints), ("numverts", self.verts), ("numtris", self.tris),
                       ("numweights", self.weights)):
            if counts.get(k) != len(got):
                raise Md5Error("%s: %s says %s, the file has %d" % (what, k, counts.get(k), len(got)))
        nj, nv, nw = len(self.joints), len(self.verts), len(self.weights)
        for i, (_, _, first, count) in enumerate(self.verts):
            if count < 1 or first + count > nw:
                raise Md5Error("%s: vertex %d's weights run past the file's" % (what, i))
        for w in self.weights:
            if not 0 <= w[0] < nj:
                raise Md5Error("%s: a weight names joint %d; the file has %d" % (what, w[0], nj))
        for t in self.tris:
            if max(t) >= nv:
                raise Md5Error("%s: a triangle names a vertex the file doesn't have" % what)
        self.rot = [quat_matrix(q) for _, _, _, q in self.joints]

    def vertex_weights(self, v):
        _, _, first, count = self.verts[v]
        return self.weights[first:first + count]

    def rest(self, v):
        """Vertex v's place (as the engine reads it; in double, then float32)."""
        acc = [0.0, 0.0, 0.0]
        for j, b, o, _ in self.vertex_weights(v):
            p = mat_vec(self.rot[j], o)
            pos = self.joints[j][2]
            for k in range(3):
                acc[k] += b * (pos[k] + p[k])
        return tuple(f32(a) for a in acc)

    def offset_for(self, j, place):
        """The offset in joint j's frame that puts a vertex weighted to it at `place`."""
        pos = self.joints[j][2]
        return mat_t_vec(self.rot[j], tuple(place[k] - pos[k] for k in range(3)))


def fmt(v):
    return " ".join("%.6f" % c for c in v)


def write_md5mesh(path, src, verts, tris, commandline):
    """src: the Mesh read (its joints are written as they were). verts: [(st, st texts or None, weights)] with
    weights [(joint, bias, offset, texts or None)] (texts: the original's, written as they were); tris: [(a, b, c)]."""
    L = ["MD5Version 10", 'commandline "%s"' % commandline, "", "numJoints %d" % len(src.joints), "numMeshes 1", "",
         "joints {"]
    L += src.joint_lines
    L += ["}", "", "mesh {", '\tshader "%s"' % src.shader, "", "\tnumverts %d" % len(verts)]
    wl = []
    for i, (st, st_text, weights) in enumerate(verts):
        s, t = st_text if st_text is not None else ("%.6f" % st[0], "%.6f" % st[1])
        L.append("\tvert %d ( %s %s ) %d %d" % (i, s, t, len(wl), len(weights)))
        wl += weights
    L += ["", "\tnumtris %d" % len(tris)]
    for i, t in enumerate(tris):
        L.append("\ttri %d %d %d %d" % (i, t[0], t[1], t[2]))
    L += ["", "\tnumweights %d" % len(wl)]
    for i, (j, b, o, texts) in enumerate(wl):
        if texts is not None:
            L.append("\tweight %d %d %s ( %s %s %s )" % ((i, j) + tuple(texts)))
        else:
            L.append("\tweight %d %d %.6f ( %s )" % (i, j, b, fmt(o)))
    L += ["}", ""]
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(L))


RE_BOUNDS = re.compile(r"(bounds \{\n)(.*?)(\n\})", re.S)


def write_md5anim(path, src_text, bounds, commandline):
    """The .md5anim as it was (the joints' bind pose), with the bounds of the mesh (None: kept) and commandline."""
    text = src_text
    if bounds is not None:
        lo, hi = bounds
        text = RE_BOUNDS.sub(lambda m: m.group(1) + "\t( %s ) ( %s )" % (fmt(lo), fmt(hi)) + m.group(3), text, count=1)
    text = re.sub(r'commandline "[^"]*"', lambda m: 'commandline "%s"' % commandline, text, count=1)
    with open(path, "w", newline="\n") as f:
        f.write(text)
