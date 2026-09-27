# md5hand.py -- reads and writes Quake VR's jointed hand (quakevr/progs/hand_rig.md5mesh, .md5anim and the
# hand_rig_NN_00.lmp skins). Plain Python (no bpy, no numpy): the Blender add-on (quakevr_hand) and the generator
# (Misc/quakevr/make_hand_rig.py) both write the files through it, so they come out the same.
#
# Exact numbers: the engine (Quake/vr/vr_handrig.cpp) reads each vertex's rest place as the weighted mean of its
# weights' joint + offset, in double (the offsets read as doubles, the joints and weights as floats), rounded to float.
# The writer gives each offset as the exact difference in double, so this gives back exactly the float it was asked
# for; every float is written with the fewest digits that read back as the same float. So a hand read and written
# again without edits is the same, bit for bit, in the engine and in Blender.

import math
import struct

# The MD5's joints, in the engine's order (vr_handrig_data.inc's joints[]): the palm; each finger's three segments
# (thumb, index, middle, ring, pinky); a helper per joint turned by a share of it (the ring of vertices there); two
# more at the thumb's base (the ball of the thumb). Renaming or deleting one breaks the hand: the engine refuses it.
FINGERS = ("thumb", "index", "middle", "ring", "pinky")
PALM, SEGMENT, PART = 0, 1, 2


def joint_table():
    """(name, kind, finger, index, share): SEGMENT index = segment 1..3; PART index = its joint 0..2."""
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
JOINT_NAMES = [j[0] for j in JOINTS]
JOINT_INDEX = {n: i for i, n in enumerate(JOINT_NAMES)}


def segment_joint(finger, segment):
    return 1 + finger * 3 + (segment - 1)


def pivot_joint(j):
    """The segment joint whose place is joint j's pivot (a helper sits at the pivot of the joint it shares): None
    for the palm."""
    name, kind, fi, idx, _ = JOINTS[j]
    if kind == PALM:
        return None
    return segment_joint(fi, idx if kind == SEGMENT else idx + 1)


# ----------------------------------------------------------------------------
# Floats


def f32(x):
    """x rounded to a 32-bit float (as C's (float)double does)."""
    return struct.unpack("<f", struct.pack("<f", x))[0]


def fstr(x):
    """The shortest plain decimal (no exponent) that reads back (strtod, then to float) as the float32 of x."""
    x = f32(x)
    if x == 0.0:
        return "0"
    for p in range(0, 60):
        s = "%.*f" % (p, x)
        if f32(float(s)) == x:
            if "." in s:
                s = s.rstrip("0").rstrip(".")
            return "0" if s in ("-0", "") else s
    return repr(x)


def dstr(x):
    """The shortest plain decimal (no exponent) that reads back (strtod) as the double x."""
    s = repr(float(x))
    if "e" in s or "E" in s:
        for p in range(1, 400):
            s = "%.*f" % (p, x)
            if float(s) == x:
                break
    if s.endswith(".0"):
        s = s[:-2]
    return "0" if s in ("-0", "") else s


def next_f32(x, n):
    """The float32 n steps (ulps) from float32 x."""
    i = struct.unpack("<i", struct.pack("<f", x))[0]
    if i < 0:
        i = -(i & 0x7FFFFFFF)
    i += n
    if i < 0:
        i = (-i) | -0x80000000
    return struct.unpack("<f", struct.pack("<i", i))[0]


def quat_w(x, y, z):
    """MD5 stores a unit quaternion's x, y, z; w is the negative root (Doom 3's and the engine's convention)."""
    t = 1.0 - (x * x + y * y + z * z)
    return -math.sqrt(t) if t > 0.0 else 0.0


def rotate(q, v):
    """v turned by the unit quaternion q = (x, y, z) (w from quat_w), in double, as vr_handrig.cpp does it."""
    x, y, z = q
    if x == 0.0 and y == 0.0 and z == 0.0:
        return (v[0], v[1], v[2])
    w = quat_w(x, y, z)
    tx = 2.0 * (y * v[2] - z * v[1])
    ty = 2.0 * (z * v[0] - x * v[2])
    tz = 2.0 * (x * v[1] - y * v[0])
    return (v[0] + w * tx + (y * tz - z * ty), v[1] + w * ty + (z * tx - x * tz), v[2] + w * tz + (x * ty - y * tx))


def rest_position(joints, weights):
    """A vertex's rest place: its weights' joint + turned offset, their weighted mean summed in double, as a float32
    triple. joints: [(name, parent, pos, quat)]; weights: [(joint, bias, offset)]."""
    acc = [0.0, 0.0, 0.0]
    total = 0.0
    for j, b, o in weights:
        pos, q = joints[j][2], joints[j][3]
        p = rotate(q, o)
        for k in range(3):
            acc[k] += b * (pos[k] + p[k])
        total += b
    return tuple(f32(a / total) for a in acc)


# ----------------------------------------------------------------------------
# Reading


class Md5Error(Exception):
    pass


def tokens(text):
    """(token, line) pairs: quoted strings (without the quotes), ( ) { } alone, // comments skipped."""
    out = []
    i, n, line = 0, len(text), 1
    while i < n:
        c = text[i]
        if c == "\n":
            line += 1
            i += 1
        elif c.isspace():
            i += 1
        elif c == "/" and text.startswith("//", i):
            while i < n and text[i] != "\n":
                i += 1
        elif c == '"':
            j = text.find('"', i + 1)
            if j < 0:
                raise Md5Error("line %d: a name's closing quote is missing" % line)
            out.append((text[i + 1:j], line))
            line += text.count("\n", i, j)
            i = j + 1
        elif c in "(){}":
            out.append((c, line))
            i += 1
        else:
            j = i
            while j < n and not text[j].isspace() and text[j] not in '(){}"':
                j += 1
            out.append((text[i:j], line))
            i = j
    return out


class Reader:
    def __init__(self, text, what):
        self.t = tokens(text)
        self.i = 0
        self.what = what

    def line(self):
        return self.t[min(self.i, len(self.t) - 1)][1] if self.t else 0

    def next(self):
        if self.i >= len(self.t):
            raise Md5Error("%s: the file ends early" % self.what)
        tok = self.t[self.i][0]
        self.i += 1
        return tok

    def expect(self, s):
        line = self.line()
        tok = self.next()
        if tok != s:
            raise Md5Error("%s, line %d: expected \"%s\", found \"%s\"" % (self.what, line, s, tok))

    def peek(self):
        return self.t[self.i][0] if self.i < len(self.t) else None

    def int(self):
        line = self.line()
        tok = self.next()
        try:
            return int(tok)
        except ValueError:
            raise Md5Error("%s, line %d: expected a whole number, found \"%s\"" % (self.what, line, tok))

    def float(self):
        line = self.line()
        tok = self.next()
        try:
            v = float(tok)
        except ValueError:
            raise Md5Error("%s, line %d: expected a number, found \"%s\"" % (self.what, line, tok))
        if not math.isfinite(v):
            raise Md5Error("%s, line %d: %s is not a finite number" % (self.what, line, tok))
        return v

    def vec(self, n, single=True):
        self.expect("(")
        v = tuple(f32(self.float()) if single else self.float() for _ in range(n))
        self.expect(")")
        return v


def parse_md5mesh(text, what="hand_rig.md5mesh"):
    """{'joints': [(name, parent, pos, quat)], 'shader', 'verts': [(s, t, first, count)], 'tris', 'weights':
    [(joint, bias, offset)]} (floats as float32 values). Only the one-mesh files this hand uses."""
    r = Reader(text, what)
    r.expect("MD5Version")
    if r.int() != 10:
        raise Md5Error("%s: not MD5Version 10" % what)
    if r.peek() == "commandline":
        r.next()
        r.next()
    r.expect("numJoints")
    nj = r.int()
    r.expect("numMeshes")
    nm = r.int()
    if nm != 1:
        raise Md5Error("%s: %d meshes; the hand is one mesh (join its objects into one)" % (what, nm))
    r.expect("joints")
    r.expect("{")
    joints = []
    for _ in range(nj):
        name = r.next()
        parent = r.int()
        pos = r.vec(3)
        quat = r.vec(3)
        joints.append((name, parent, pos, quat))
    r.expect("}")
    r.expect("mesh")
    r.expect("{")
    shader = ""
    if r.peek() == "shader":
        r.next()
        shader = r.next()
    r.expect("numverts")
    nv = r.int()
    verts = []
    for i in range(nv):
        r.expect("vert")
        r.int()
        st = r.vec(2)
        verts.append((st[0], st[1], r.int(), r.int()))
    r.expect("numtris")
    nt = r.int()
    tris = []
    for i in range(nt):
        r.expect("tri")
        r.int()
        tris.append((r.int(), r.int(), r.int()))
    r.expect("numweights")
    nw = r.int()
    weights = []
    for i in range(nw):
        r.expect("weight")
        r.int()
        j = r.int()
        b = f32(r.float())
        weights.append((j, b, r.vec(3, single=False)))  # offsets in double: see the top
    r.expect("}")
    return {"joints": joints, "shader": shader, "verts": verts, "tris": tris, "weights": weights}


def vertex_weights(mesh, v):
    s, t, first, count = mesh["verts"][v]
    return mesh["weights"][first:first + count]


# ----------------------------------------------------------------------------
# Writing


def exact_offsets(target, weights, binds):
    """Offsets, one per weight [(joint, bias)], that rest_position turns back into target exactly (identity
    orientations): each the difference in double (exact for two floats), written with all its digits (dstr)."""
    offs = [tuple(target[k] - binds[j][k] for k in range(3)) for j, _ in weights]
    joints = [(None, 0, b, (0.0, 0.0, 0.0)) for b in binds]
    got = rest_position(joints, [(j, b, o) for (j, b), o in zip(weights, offs)])
    if got != tuple(target):
        raise ValueError("offsets give back %r, not %r" % (got, target))
    return offs


def write_md5mesh(path, joints, verts, tris, shader="hand_rig", commandline=""):
    """joints: [(name, parent, pos)] (orientations are the identity); verts: [((s, t), [(joint, bias)], rest)];
    tris: [(a, b, c)] clockwise seen from outside (as Quake draws). Floats are taken as float32."""
    binds = [tuple(f32(c) for c in p) for _, _, p in joints]
    L = ["MD5Version 10", "commandline \"%s\"" % commandline, "", "numJoints %d" % len(joints), "numMeshes 1", "",
         "joints {"]
    for (name, parent, _), p in zip(joints, binds):
        L.append("\t\"%s\"\t%d ( %s ) ( 0 0 0 )" % (name, parent, " ".join(fstr(c) for c in p)))
    L += ["}", "", "mesh {", "\tshader \"%s\"" % shader, "", "\tnumverts %d" % len(verts)]
    wl = []
    for i, (st, infl, rest) in enumerate(verts):
        rest = tuple(f32(c) for c in rest)
        infl = [(j, f32(b)) for j, b in infl]
        offs = exact_offsets(rest, infl, binds)
        L.append("\tvert %d ( %s %s ) %d %d" % (i, fstr(st[0]), fstr(st[1]), len(wl), len(infl)))
        for (j, b), o in zip(infl, offs):
            wl.append((j, b, o))
    L += ["", "\tnumtris %d" % len(tris)]
    for i, t in enumerate(tris):
        L.append("\ttri %d %d %d %d" % (i, t[0], t[1], t[2]))
    L += ["", "\tnumweights %d" % len(wl)]
    for i, (j, b, o) in enumerate(wl):
        L.append("\tweight %d %d %s ( %s )" % (i, j, fstr(b), " ".join(dstr(c) for c in o)))
    L += ["}", ""]
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(L))


def write_md5anim(path, joints, rests, commandline=""):
    """One frame, the bind pose (the engine poses the hand itself). joints: [(name, parent, pos)] with the palm first
    at its parent; rests: the vertices' places (for the bounds)."""
    binds = [tuple(f32(c) for c in p) for _, _, p in joints]
    lo = [min(r[k] for r in rests) for k in range(3)]
    hi = [max(r[k] for r in rests) for k in range(3)]
    local = []
    for (name, parent, _), p in zip(joints, binds):
        if parent < 0:
            local.append(p)
        else:
            q = binds[parent]
            local.append(tuple(f32(p[k] - q[k]) for k in range(3)))
    L = ["MD5Version 10", "commandline \"%s\"" % commandline, "", "numFrames 1", "numJoints %d" % len(joints),
         "frameRate 24", "numAnimatedComponents %d" % (6 * len(joints)), "", "hierarchy {"]
    for i, (name, parent, _) in enumerate(joints):
        L.append("\t\"%s\"\t%d 63 %d" % (name, parent, 6 * i))
    L += ["}", "", "bounds {", "\t( %s ) ( %s )" % (" ".join(fstr(c) for c in lo), " ".join(fstr(c) for c in hi)), "}",
          "", "baseframe {"]
    for p in local:
        L.append("\t( %s ) ( 0 0 0 )" % " ".join(fstr(c) for c in p))
    L += ["}", "", "frame 0 {"]
    for p in local:
        L.append("\t%s 0 0 0" % " ".join(fstr(c) for c in p))
    L += ["}", ""]
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(L))


# ----------------------------------------------------------------------------
# Skins: Quake's palette and .lmp images (width, height, then a byte per texel, rows top to bottom)

PALETTE_HEX = (
    "0000000f0f0f1f1f1f2f2f2f3f3f3f4b4b4b5b5b5b6b6b6b7b7b7b8b8b8b9b9b9babababbbbbbbcbcbcbdbdbdbebebeb"
    "0f0b07170f0b1f170b271b0f2f2313372b173f2f174b371b533b1b5b431f634b1f6b531f73571f7b5f238367238f6f23"
    "0b0b0f13131b1b1b272727332f2f3f37374b3f3f574747674f4f735b5b7f63638b6b6b977373a37b7baf8383bb8b8bcb"
    "0000000707000b0b001313001b1b002323002b2b072f2f073737073f3f074747074b4b0b53530b5b5b0b63630b6b6b0f"
    "0700000f00001700001f00002700002f00003700003f00004700004f00005700005f00006700006f00007700007f0000"
    "1313001b1b002323002f2b00372f004337004b3b075743075f47076b4b0b77530f8357138b5b13975f1ba3631faf6723"
    "2313072f170b3b1f0f4b2313572b17632f1f7337237f3b2b8f43339f4f33af632fbf772fcf8f2bdfab27efcb1ffff31b"
    "0b07001b13002b230f372b1347331b533723633f2b6f47337f533f8b5f479b6b53a77b5fb7876bc3937bd3a38be3b397"
    "ab8ba39f7f979373878b677b7f5b6f7753636b4b575f3f4b5737434b2f3743272f371f232b171b231313170b0b0f0707"
    "bb739faf6b8fa35f839757778b4f6b7f4b5f7343536b3b4b5f333f532b3747232b3b1f232f171b231313170b0b0f0707"
    "dbc3bbcbb3a7bfa39baf978ba3877b977b6f876f5f7b63536b57475f4b3b533f33433327372b1f271f171b130f0f0b07"
    "6f837b677b6f5f7367576b5f4f6357475b4f3f5347374b3f2f43372b3b2f2333271f2b1f1723170f1b130b130b070b07"
    "fff31befdf17dbcb13cbb70fbba70fab970b9b83078b73077b63076b53005b47004b37003b2b002b1f001b0f000b0700"
    "0000ff0b0bef1313df1b1bcf2323bf2b2baf2f2f9f2f2f8f2f2f7f2f2f6f2f2f5f2b2b4f23233f1b1b2f13131f0b0b0f"
    "2b00003b00004b07005f07006f0f007f1707931f07a3270bb7330fc34b1bcf632bdb7f3be3974fe7ab5fefbf77f7d38b"
    "a77b3bb79b37c7c337e7e3577fbfffabe7ffd7ffff6700008b0000b30000d70000ff0000fff393fff7c7ffffff9f5b53"
)
FULLBRIGHT = 224  # indices from here on glow in the dark: never painted onto the hand


def palette():
    b = bytes.fromhex("".join(PALETTE_HEX))
    return [tuple(b[3 * i:3 * i + 3]) for i in range(256)]


def read_lmp(path):
    """(width, height, bytes of indices, rows top to bottom)."""
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 8:
        raise Md5Error("%s: too short for a skin" % path)
    w, h = struct.unpack("<ii", data[:8])
    if w <= 0 or h <= 0 or len(data) != 8 + w * h:
        raise Md5Error("%s: not a %dx%d skin" % (path, w, h))
    return w, h, data[8:]


def write_lmp(path, w, h, pixels):
    with open(path, "wb") as f:
        f.write(struct.pack("<ii", w, h) + bytes(pixels))


class Quantizer:
    """Colours to palette indices: a texel whose colour is still its old index's keeps that index (so an unedited
    skin comes back byte for byte); any other colour becomes the nearest non-fullbright entry."""

    def __init__(self):
        self.pal = palette()
        self.cache = {}

    def nearest(self, rgb):
        c = self.cache.get(rgb)
        if c is None:
            r, g, b = rgb
            best, c = 1 << 30, 0
            for i in range(FULLBRIGHT):
                p = self.pal[i]
                d = (p[0] - r) ** 2 + (p[1] - g) ** 2 + (p[2] - b) ** 2
                if d < best:
                    best, c = d, i
            self.cache[rgb] = c
        return c

    def indices(self, rgb_rows, old=None):
        """rgb_rows: a flat list of (r, g, b) 0..255, rows top to bottom; old: the indices they came from."""
        out = bytearray(len(rgb_rows))
        for i, rgb in enumerate(rgb_rows):
            if old is not None and self.pal[old[i]] == rgb:
                out[i] = old[i]
            else:
                out[i] = self.nearest(rgb)
        return out


def carry_damage(old_clean, old_damaged, new_clean):
    """A damage skin with the clean skin's edits: the texels its blood covers (where it differs from the old clean
    skin) stay as they were, every other texel is the new clean skin's."""
    return bytes(d if d != c else n for c, d, n in zip(old_clean, old_damaged, new_clean))
