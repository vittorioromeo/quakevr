#!/usr/bin/env python3
# make_screen_housing_nail2.py -- the super nailgun's ammo screen housing, modelled on v_nail2.blend (ROUND24.md, "The
# super nailgun's screen housing"; his mockup, kit scratch/mockups/nail2_housing.webp: a riveted bezel on the
# receiver's rear face above the opening the hand grips in, the glass set in it).
#
# Usage: blender -b --factory-startup Misc/quakevr/src_models/blend/v_nail2/v_nail2.blend
#            --python Misc/quakevr/make_screen_housing_nail2.py -- [--save other.blend] [--dry]
# Builds or moves the housing, exports the model as blend_export.py --grow does (quakevr/progs, weapons.txt) and saves
# the .blend (in place, or as --save) as the export left it. Ends with "housing_nail2: ok, ...". --dry: prints the
# sizes it would build (snapped to the model's grid) and stops, nothing written.
#
# One definition: the window's half size and chamfer (`opening`, `radius` < 0, in font pixels) are read from the "nail2"
# entry of Quake/vr/vr_screenstyle.cpp, the table the engine shapes the glass by, and its `size` with weapons.txt's
# wpntxt_scale give a font pixel's size on the model; the glass and the window are one shape. Change them there and run
# this again. It also checks the entry's `offset` puts the glass where the bezel has it (else prints the one that does).
#
# His build spec, in window heights h (the window's height): the window 1.6h x 1h, its corners chamfered 0.25h; the
# bezel 1.8h x 1.2h (a 0.1h frame), 0.1h proud of the gun, no bevel; the glass 0.05h behind the bezel's front; four
# round rivets of radius 0.12h near the bezel's corners; no other parts. The model's grid: Y and Z are fine (0.06), X
# (depth) is 0.25 a step, so the bezel stands 1 step proud (0.09h), its sides and the window's walls go 1 step into
# the gun past the face (the bezel bridges the top of the rear opening: a closed solid, its back seen from inside),
# the rivets' heads 1 step proud of the bezel. The spec's rivets at (+-0.8h, +-0.4h) would cut into the chamfered
# window and overhang the bezel's edge: they sit at (+-0.78h, +-0.45h), in its corners (checked).
#
# Frames: the receiver kicks back up to 2.5 units in the firing frames (the barrels spin too). The housing's vertices
# are given the rear face's own motion in every frame (its rigid fit, checked), so neither the barrels nor the nearest
# vertex the export would pick drag it; the kick puts the rivets past the model's old box, so the export grows it
# (`--grow`: the X grid a little coarser, weapons.txt's offsets keep the gun in place).
# Re-run on a .blend that has the housing (its vertices carry the point attribute `qvr_housing`): with the same shape
# (as many vertices) they are moved and re-mapped in place, so the model's file keeps its vertices; otherwise the old
# housing is deleted and built anew (its old vertices then stay in the .mdl unused: start from the .blend in git).
# Texture, all from the gun's own skin: the bezel on the receiver's brown-green painted metal (texels 424..452 x
# 24..36), the window's walls on a darker patch of it (428..444 x 41..46), the floor behind the glass near black, the
# rivets on the worn grey metal strip (472..490 x 32..39).

import math
import os
import re
import struct
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
STEM = "nail2"
MODEL = "v_nail2"
TAG = "housing_" + STEM

PIXEL_AT_04 = 0.09398  # a font pixel in model units at wpntxt_scale 0.4, style size 1 (vr_debug_screen_where)
# The window's centre on the rear face (model Y, Z: grid points; the opening below it is y -2.44..2.27, the face's top
# z 7.6): the glass where weapons.txt already puts it, the bezel's top 0.18 under the face's top edge.
CENTRE_Y = -0.0525
CENTRE_Z = 5.7727
# Where weapons.txt's wpntxt offsets put the glass with the table's offset 0 (vr_debug_screen_where), and model units
# a world unit: to check the table's offset.
GLASS_AT_ZERO = (-2.6175, -0.0524, 5.7780)
MODEL_PER_WORLD = 2.346  # (measured: moving the offset 0.0347 out moved the glass 0.0814)
# The spec (window heights).
FRAME = 0.1
GLASS_BACK = 0.05
RIVET_R = 0.12
RIVET_AT = (0.78, 0.45)
RIVET_SIDES = 10
RIVET_TOP = 0.7  # the head's flat top, of its radius
# Skin places (texels: x, y from the top).
PAINT = (424.0, 452.0, 24.0, 36.0)   # the bezel's front and sides
WALL = (428.0, 444.0, 41.0, 46.0)    # the window's walls (darker)
FLOOR = (428.0, 51.0)                # behind the glass, the back
RIVET_X = 481.0
RIVET_ROWS = (38.0, 33.5, 33.0)      # base ring, top ring, top centre


def fail(msg):
    raise SystemExit("%s: %s" % (TAG, msg))


def read_style():
    """The entry's opening (half width, half height), radius, size and offset."""
    src = open(os.path.join(ROOT, "Quake", "vr", "vr_screenstyle.cpp"), encoding="utf-8").read()
    m = re.search(r'\{"%s",(.*?)\n\s*(?:\{"|\};)' % STEM, src, re.S)  # (up to the next entry or the table's end)
    if not m:
        fail("no \"%s\" entry in vr_screenstyle.cpp" % STEM)
    body = m.group(1)
    num = r"([-0-9.]+)f?"
    op = re.search(r"\.opening\s*=\s*\{\s*%s\s*,\s*%s\s*\}" % (num, num), body)
    rad = re.search(r"\.radius\s*=\s*" + num, body)
    size = re.search(r"\.size\s*=\s*" + num, body)
    off = re.search(r"\.offset\s*=\s*\{\s*%s\s*,\s*%s\s*,\s*%s\s*\}" % (num, num, num), body)
    if not (op and rad and size and off):
        fail("the \"%s\" entry needs .radius, .size, .offset and .opening" % STEM)
    return ((float(op.group(1)), float(op.group(2))), float(rad.group(1)), float(size.group(1)),
            tuple(float(off.group(i)) for i in (1, 2, 3)))


def wpntxt_scale():
    src = open(os.path.join(ROOT, "quakevr", "data", "weapons.txt"), encoding="utf-8").read()
    m = re.search(r'\{[^{}]*id "progs/%s\.mdl"[^{}]*\}' % MODEL, src)
    s = re.search(r'wpntxt_scale "([0-9.]+)"', m.group(0)) if m else None
    if not s:
        fail("no wpntxt_scale for progs/%s.mdl in weapons.txt" % MODEL)
    return float(s.group(1))


def grid():
    """The model file's grid: origin and scale (x, y, z)."""
    d = open(os.path.join(ROOT, "quakevr", "progs", MODEL + ".mdl"), "rb").read()
    return np.array(struct.unpack_from("<3f", d, 20)), np.array(struct.unpack_from("<3f", d, 8))


def chamfered(hw, hh, c):
    """A rectangle with its corners cut by c: 8 points counter-clockwise from the top right corner's right end."""
    pts = []
    for sx, sy, a0 in ((1, 1, 0), (-1, 1, 90), (-1, -1, 180), (1, -1, 270)):
        a = (sx * hw, sy * (hh - c)) if a0 in (0, 180) else (sx * (hw - c), sy * hh)
        b = (sx * (hw - c), sy * hh) if a0 in (0, 180) else (sx * hw, sy * (hh - c))
        pts += [a, b]
    return pts


class Design:
    """The housing on a grid: its vertices (model space), triangles and what was rounded."""

    def __init__(self, opening, radius, pix, org, sc, x_face):
        self.org, self.sc = org, sc
        step = sc[0]
        k_face = round((x_face - org[0]) / step)
        self.x_face = org[0] + k_face * step
        self.x_front = self.x_face - step     # the bezel's front: 1 step proud
        self.x_back = self.x_face + step      # its sides and the window's walls end 1 step inside
        self.x_head = self.x_front - step     # the rivets' heads
        self.cy = self.snap(1, CENTRE_Y)
        self.cz = self.snap(2, CENTRE_Z)
        hw, hh, c = opening[0] * pix, opening[1] * pix, -radius * pix
        if radius >= 0:
            fail("the entry's radius is %.2f: the spec's window is chamfered (radius < 0)" % radius)
        self.h = 2.0 * hh
        h = self.h
        self.glass_x = self.x_front + GLASS_BACK * h
        self.verts = []   # Vector
        self.tris = []    # (i, j, k, wanted normal, uv mode)
        self.uvinfo = {}  # vertex -> (mode data)

        # The window's walls: the glass's own outline, each point snapped toward the centre (inside the glass: it
        # tucks behind them); the bezel's outside: the spec's rectangle, snapped to the nearest grid point.
        win = [self.uv_snap(u, v, inward=True) for u, v in chamfered(hw, hh, c)]
        bw, bh = hw + FRAME * h, hh + FRAME * h
        out = [self.uv_snap(u, v) for u, v in ((bw, bh), (-bw, bh), (-bw, -bh), (bw, -bh))]
        self.win, self.out = win, out
        wf = [self.add(u, v, self.x_front) for u, v in win]
        wb = [self.add(u, v, self.x_back) for u, v in win]
        of = [self.add(u, v, self.x_front) for u, v in out]
        ob = [self.add(u, v, self.x_back) for u, v in out]
        back = Vector((-1, 0, 0))
        for k in range(4):  # the front: a triangle at each chamfer, a quad along each side
            self.tri(wf[2 * k], wf[2 * k + 1], of[k], back, "paint")
            n = (k + 1) % 4
            self.quad(wf[2 * k + 1], wf[(2 * k + 2) % 8], of[n], of[k], back, "paint")
        for k in range(4):  # the bezel's sides, into the gun
            n = (k + 1) % 4
            self.quad(of[k], of[n], ob[n], ob[k], "out", "side")
        self.quad(ob[0], ob[1], ob[2], ob[3], Vector((1, 0, 0)), "floor")  # its back (inside the opening)
        for k in range(8):  # the window's walls
            n = (k + 1) % 8
            self.quad(wf[k], wf[n], wb[n], wb[k], "in", "wall")
        for k in range(1, 7):  # the window's back, behind the glass
            self.tri(wb[0], wb[k], wb[k + 1], back, "floor")
        self.wall_ring = {vi: (k, 0) for k, vi in enumerate(wf)}
        self.wall_ring.update({vi: (k, 1) for k, vi in enumerate(wb)})
        self.n_bezel = len(self.tris)
        # The rivets: a ring on the bezel's front, a flat head a step proud.
        self.rivets = []
        for su, sv in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
            ru, rv = self.uv_snap(su * RIVET_AT[0] * h, sv * RIVET_AT[1] * h)
            r = RIVET_R * h
            base = [self.add(*self.uv_snap(ru + r * math.cos(2 * math.pi * i / RIVET_SIDES),
                                           rv + r * math.sin(2 * math.pi * i / RIVET_SIDES)), self.x_front)
                    for i in range(RIVET_SIDES)]
            top = [self.add(*self.uv_snap(ru + RIVET_TOP * r * math.cos(2 * math.pi * i / RIVET_SIDES),
                                          rv + RIVET_TOP * r * math.sin(2 * math.pi * i / RIVET_SIDES)), self.x_head)
                   for i in range(RIVET_SIDES)]
            tip = self.add(ru, rv, self.x_head)
            self.rivets.append((ru, rv))
            for i in range(RIVET_SIDES):
                j = (i + 1) % RIVET_SIDES
                self.quad(base[i], base[j], top[j], top[i], ("from", ru, rv), "rivet")
                self.tri(top[i], top[j], tip, back, "rivet")
            for vi in base:
                self.uvinfo[vi] = ("rivet", ru, rv, 0)
            for vi in top:
                self.uvinfo[vi] = ("rivet", ru, rv, 1)
            self.uvinfo[tip] = ("rivet", ru, rv, 2)
        self.bw, self.bh = out[0]

    def snap(self, axis, x):
        return self.org[axis] + round((x - self.org[axis]) / self.sc[axis]) * self.sc[axis]

    def uv_snap(self, u, v, inward=False):
        """(u right, v up) snapped to the grid's Y and Z points about the snapped centre (toward it if inward)."""
        sy, sz = self.sc[1], self.sc[2]
        if inward:
            return (math.copysign(math.floor(abs(u) / sy + 1e-6) * sy, u),
                    math.copysign(math.floor(abs(v) / sz + 1e-6) * sz, v))
        return round(u / sy) * sy, round(v / sz) * sz

    def place(self, u, v, x):
        return Vector((x, self.cy - u, self.cz + v))

    def add(self, u, v, x):
        self.verts.append(self.place(u, v, x))
        return len(self.verts) - 1

    def tri(self, i, j, k, want, mode):
        self.tris.append((i, j, k, want, mode))

    def quad(self, a, b, c, d, want, mode):
        self.tri(a, b, c, want, mode)
        self.tri(a, c, d, want, mode)

    def oriented(self, t):
        i, j, k, want, _ = t
        p0, p1, p2 = self.verts[i], self.verts[j], self.verts[k]
        nrm = (p1 - p0).cross(p2 - p0)
        mid = (p0 + p1 + p2) / 3.0
        if isinstance(want, tuple):  # (radial from a rivet's axis)
            want = Vector((0.0, mid.y - (self.cy - want[1]), mid.z - (self.cz + want[2])))
        elif want in ("out", "in"):
            w = Vector((0.0, mid.y - self.cy, mid.z - self.cz))
            want = w if want == "out" else -w
        return (i, j, k) if nrm.dot(want) >= 0.0 else (i, k, j)

    def texel(self, vi, mode, tri_ids):
        co = self.verts[vi]
        u, v = self.cy - co.y, co.z - self.cz
        depth = (co.x - self.x_front) / (self.x_back - self.x_front)  # 0 front .. 1 back
        if mode == "paint":
            return (PAINT[0] + (PAINT[1] - PAINT[0]) * (u + self.bw) / (2 * self.bw),
                    PAINT[2] + (PAINT[3] - PAINT[2]) * (self.bh - v) / (2 * self.bh))
        if mode == "side":
            # along the side (u for the top and bottom, v for the left and right), its depth over a few rows
            ids = [self.verts[t] for t in tri_ids]
            horiz = max(abs(p.z - ids[0].z) for p in ids) < 1e-4
            a = (u + self.bw) / (2 * self.bw) if horiz else (self.bh - v) / (2 * self.bh)
            return PAINT[0] + (PAINT[1] - PAINT[0]) * a, PAINT[2] + 4.0 + 4.0 * depth
        if mode == "wall":
            k, b = self.wall_ring[vi]
            if k == 0 and any(self.wall_ring.get(t, (0, 0))[0] == 7 for t in tri_ids):
                k = 8  # (the face across the ring's seam)
            return WALL[0] + (WALL[1] - WALL[0]) * k / 8.0, WALL[2] + (WALL[3] - WALL[2]) * b
        if mode == "floor":
            return FLOOR[0] + u * 2.0, FLOOR[1]
        _, ru, rv, ring = self.uvinfo[vi]
        if ring == 2:
            return RIVET_X, RIVET_ROWS[2]
        du, dv = u - ru, v - rv
        rr = max(math.hypot(du, dv), 1e-6)
        return RIVET_X + (4.0 if ring == 0 else 3.0) * du / rr, RIVET_ROWS[ring]

    def report(self):
        h = self.h
        wu = max(u for u, _ in self.win)
        wv = max(v for _, v in self.win)
        cut = wu - max(u for u, v in self.win if v > wv - 1e-6)
        ru, rv = abs(self.rivets[0][0]), abs(self.rivets[0][1])
        # The rivets' rings: on the bezel (inside its edge) and clear of the window (outside its chamfered outline).
        edge, clear = 1e9, 1e9
        for i in range(RIVET_SIDES):
            a = 2 * math.pi * i / RIVET_SIDES
            u, v = abs(ru + RIVET_R * h * math.cos(a)), abs(rv + RIVET_R * h * math.sin(a))
            edge = min(edge, self.bw - u, self.bh - v)
            clear = min(clear, max(u - wu, v - wv, (u + v - (wu + wv - cut)) * 0.7071))
        if edge < -0.01 or clear < 0.02:
            fail("the rivets overhang the bezel (%.3f) or cut into the window (%.3f clear)" % (-edge, clear))
        self.rivet_clear = (edge, clear)
        return ("window %.3f x %.3f (h %.3f; 1.6h x 1h: %.3fh x %.3fh, chamfer %.3fh), bezel %.3f x %.3f "
                "(%.3fh x %.3fh), %.3f proud (%.3fh), walls %.3f deep, glass %.3f behind the front (%.2fh), "
                "rivets r %.3f (%.2fh) at +-%.3f +-%.3f (%.2fh, %.2fh), heads %.3f proud, %.3f inside the bezel's "
                "edge and %.3f clear of the window"
                % (2 * wu, 2 * wv, h, 2 * wu / h, 2 * wv / h, cut / h, 2 * self.bw, 2 * self.bh, 2 * self.bw / h,
                   2 * self.bh / h, self.x_face - self.x_front, (self.x_face - self.x_front) / h,
                   self.x_back - self.x_front, self.glass_x - self.x_front, (self.glass_x - self.x_front) / h,
                   RIVET_R * h, RIVET_R, ru, rv, ru / h, rv / h, self.x_front - self.x_head, edge, clear))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    save = argv[argv.index("--save") + 1] if "--save" in argv else bpy.data.filepath
    dry = "--dry" in argv

    opening, radius, size, offset = read_style()
    pix = PIXEL_AT_04 * wpntxt_scale() / 0.4 * size
    ob = bpy.data.objects[MODEL]
    me = ob.data
    img = bpy.data.images.get(MODEL + "_skin0")
    sw, sh = img.size
    keys = me.shape_keys.key_blocks

    def uv_at(tx, ty):
        return (tx / sw, 1.0 - ty / sh)

    def housing_flags():
        a = me.attributes.get("qvr_housing")
        flags = np.zeros(len(me.vertices), np.int32)
        if a is not None:
            a.data.foreach_get("value", flags)
        return flags

    def rear_face():
        """The receiver's rear face: the gun's (not the housing's) rearmost vertices in frame 0, and its x."""
        flags = housing_flags()
        co = np.array([v.co[:] for v in me.vertices])
        xs = np.where(flags == 0, co[:, 0], np.inf)
        x0 = xs.min()
        ids = np.nonzero(np.abs(xs - x0) < 0.02)[0]
        if len(ids) < 8:
            fail("the rear face has %d vertices at x %.3f (expected 16)" % (len(ids), x0))
        return ids, x0

    def design():
        org, sc = grid()
        _, x0 = rear_face()
        return Design(opening, radius, pix, org, sc, x0)

    d = design()
    print("%s: %s" % (TAG, d.report()))
    glass = (d.glass_x, d.cy, d.cz)
    want_off = ((GLASS_AT_ZERO[1] - glass[1]) / MODEL_PER_WORLD,  # right = -Y
                (glass[2] - GLASS_AT_ZERO[2]) / MODEL_PER_WORLD,
                (GLASS_AT_ZERO[0] - glass[0]) / MODEL_PER_WORLD)  # out = -X
    if dry:
        print("%s: dry run, the glass at %.4f %.4f %.4f (the table's offset for it: {%.4f, %.4f, %.4f}); nothing "
              "written" % (TAG, glass[0], glass[1], glass[2], *want_off))
        return

    def apply(d):
        """Adds the housing or, when the mesh has it with this shape, moves it into place and maps it again. Returns
        whether it moved an existing one."""
        bm = bmesh.new()
        bm.from_mesh(me)
        lay_house = bm.verts.layers.int.get("qvr_housing") or bm.verts.layers.int.new("qvr_housing")
        lay_vert = bm.verts.layers.int.get("qvr_vert")
        lay_tri = bm.faces.layers.int.get("qvr_tri")
        lay_vid = bm.loops.layers.int.get("qvr_vid")
        lay_uv = bm.loops.layers.uv.active
        shapes = list(bm.verts.layers.shape.values())
        old = [v for v in bm.verts if v[lay_house] == 1]
        reuse = len(old) == len(d.verts)
        if old and not reuse:
            bmesh.ops.delete(bm, geom=old, context='VERTS')
            old = []
        if reuse:
            old.sort(key=lambda v: v.index)
            for v, co in zip(old, d.verts):
                v.co = co
                for sl in shapes:
                    v[sl] = co  # (the frames: set from the receiver's motion below)
            oldset = set(old)
            faces = [f for f in bm.faces if all(v in oldset for v in f.verts)]
            if len(faces) != len(d.tris):
                fail("the old housing has %d faces, not %d" % (len(faces), len(d.tris)))
            lookup = {v: i for i, v in enumerate(old)}
            mode_of = {frozenset(t[:3]): t[4] for t in d.tris}
            for f in faces:
                ids = [lookup[v] for v in f.verts]
                mode = mode_of.get(frozenset(ids))
                if mode is None:
                    fail("the old housing's faces don't match this shape")
                for loop in f.loops:
                    loop[lay_uv].uv = uv_at(*d.texel(lookup[loop.vert], mode, ids))
        else:
            bv = []
            for co in d.verts:
                v = bm.verts.new(co)
                v[lay_house] = 1
                if lay_vert is not None:
                    v[lay_vert] = -1
                for sl in shapes:
                    v[sl] = co
                bv.append(v)
            for t in d.tris:
                i, j, k = d.oriented(t)
                f = bm.faces.new((bv[i], bv[j], bv[k]))
                f.material_index = 0
                f.smooth = True
                if lay_tri is not None:
                    f[lay_tri] = -1
                for loop in f.loops:
                    if lay_vid is not None:
                        loop[lay_vid] = -1
                    loop[lay_uv].uv = uv_at(*d.texel(bv.index(loop.vert), t[4], (i, j, k)))
        bm.to_mesh(me)
        me.update()
        bm.free()
        return reuse

    def receiver_motion():
        """Per frame, the rear face's rigid motion (R, its centre in frame 0, in that frame) and the fit's residual."""
        ids, _ = rear_face()
        n = len(me.vertices)
        buf = np.empty(n * 3, np.float32)
        out = []
        worst = 0.0
        keys[0].data.foreach_get("co", buf)
        A = buf.reshape(n, 3)[ids].astype(np.float64)
        for kb in keys:
            kb.data.foreach_get("co", buf)
            B = buf.reshape(n, 3)[ids].astype(np.float64)
            R = mdl_blender.kabsch(A, B)
            res = np.abs((A - A.mean(0)) @ R.T + B.mean(0) - B).max()
            worst = max(worst, res)
            out.append((R, A.mean(0), B.mean(0)))
        if worst > 0.02:
            fail("the rear face doesn't move as one piece (fit off by %.3f)" % worst)
        return out, worst

    def set_frames():
        """The housing's place in every frame: frame 0's moved with the rear face."""
        motion, worst = receiver_motion()
        flags = housing_flags()
        hid = np.nonzero(flags)[0]
        n = len(me.vertices)
        buf = np.empty(n * 3, np.float32)
        keys[0].data.foreach_get("co", buf)
        P0 = buf.reshape(n, 3)[hid].astype(np.float64)
        for kb, (R, c0, c1) in zip(keys, motion):
            kb.data.foreach_get("co", buf)
            co = buf.reshape(n, 3)
            co[hid] = (P0 - c0) @ R.T + c1
            kb.data.foreach_set("co", co.ravel())
        me.update()
        return motion, worst

    sys.path.insert(0, os.path.join(HERE, "blender", "addons"))
    from quakevr_models import mdl_blender, source

    def export():
        try:
            written, _ = source.export(bpy.context, ROOT, None, grow=True)
        except source.SourceError as e:
            fail("the export REFUSED:\n  " + str(e).replace("\n", "\n  "))
        return len(written)

    passes = 0
    while True:
        moved = apply(d)
        set_frames()
        files = export()
        passes += 1
        d2 = design()  # (the grid the export left: grown the first time)
        same = all((a - b).length < 1e-5 for a, b in zip(d.verts, d2.verts))
        d = d2
        if moved and same:
            break
        if passes >= 4:
            fail("the housing's grid didn't settle in %d exports" % passes)

    # Check: what the export wrote for the housing in every frame is the rear face's motion of frame 0's housing.
    mdl_blender.carry_frames(ob)
    motion, worst = receiver_motion()
    hid = np.nonzero(housing_flags())[0]
    n = len(me.vertices)
    buf = np.empty(n * 3, np.float32)
    keys[0].data.foreach_get("co", buf)
    P0 = buf.reshape(n, 3)[hid].astype(np.float64)
    off = 0.0
    for kb, (R, c0, c1) in zip(keys, motion):
        kb.data.foreach_get("co", buf)
        off = max(off, np.abs(buf.reshape(n, 3)[hid] - ((P0 - c0) @ R.T + c1)).max())
    if off > 1e-3:
        fail("the exported frames don't carry the housing with the rear face (off by %.4f)" % off)

    glass = (d.glass_x, d.cy, d.cz)
    want_off = ((GLASS_AT_ZERO[1] - glass[1]) / MODEL_PER_WORLD, (glass[2] - GLASS_AT_ZERO[2]) / MODEL_PER_WORLD,
                (GLASS_AT_ZERO[0] - glass[0]) / MODEL_PER_WORLD)
    if max(abs(a - b) for a, b in zip(want_off, offset)) > 0.002:
        print("%s: WARNING the table's offset {%.4f, %.4f, %.4f} doesn't put the glass at %.4f %.4f %.4f: use "
              "{%.4ff, %.4ff, %.4ff}" % (TAG, *offset, *glass, *want_off))
    bpy.context.preferences.filepaths.save_version = 0  # (no .blend1 beside it)
    bpy.ops.wm.save_as_mainfile(filepath=save, compress=True)
    print("%s: ok, %d vertices, %d triangles (bezel %d, rivets %d); %s; the glass at %.4f %.4f %.4f; frames carried "
          "with the rear face (its fit %.4f, the export %.4f off); %d exports, %d files; saved %s"
          % (TAG, len(d.verts), len(d.tris), d.n_bezel, len(d.tris) - d.n_bezel, d.report(), *glass, worst, off,
             passes, files, save))


if __name__ == "__main__":
    main()
