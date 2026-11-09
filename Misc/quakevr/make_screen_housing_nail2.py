#!/usr/bin/env python3
# make_screen_housing_nail2.py -- the super nailgun's ammo screen housing, modelled on v_nail2.blend (ROUND24.md, "The
# super nailgun's screen housing on top"; his mockup, kit scratch/mockups/nail2_top.webp: a raised block on the
# receiver's top at its back end, its rear face sloping up and forward with the screen in it; the back of the gun round
# the handle left free).
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
# this again. It also checks that weapons.txt's wpntxt_pitch tilts the glass with the sloped face and that the entry's
# `offset` puts the glass where the window has it (else prints the one that does).
#
# The shape, in window heights h (the window's height), on the receiver's rear top band (x -2.58..0.70, its top z 7.6,
# 5.85 wide; the receiver's sides below it slope out 21.6 degrees):
# - the block: its rear face slopes up and forward at SLOPE degrees from the band's rear top edge (the edge of the gun's
#   rear face: nothing on the rear face itself), MARGIN round the bezel, then a flat top to the band's front (x 0.70).
#   Its sides lean in SIDE degrees (a trapezoid seen from behind, narrower at the top), standing OVERHANG past the
#   band's edges at its top so the bezel's top corners keep EDGE inside them, and come down LEDGE below the top, where
#   a ledge underside meets the receiver's sloped sides; small closing faces in the rear face's plane fill the corners
#   outside the gun's rear outline.
# - in the sloped face: the bezel 1.8h x 1.2h (a 0.1h frame) standing 0.1h proud, the window 1.6h x 1h with its corners
#   chamfered (the entry's radius), its walls going 0.1h into the block to a near-black floor, the glass 0.05h behind
#   the bezel's front; four round rivets at (+-0.78h, +-0.45h) on the bezel, radius 0.11h, heads 0.08h proud.
# The model's grid is coarse in X (0.25) and fine in Y and Z: points on the sloped face snap to the grid point nearest
# the face's plane (the window's outline toward its centre: the glass tucks behind its walls).
#
# Frames: the receiver kicks back up to 2.5 units in the firing frames (the barrels spin too). The housing's vertices
# are given the rear face's own motion in every frame (its rigid fit, checked), so neither the barrels nor the nearest
# vertex the export would pick drag it; the block's top is past the model's old box, so the export grows it (`--grow`:
# the Z grid a little coarser, weapons.txt's offsets keep the gun in place). The barrels (with the rear face's motion
# taken out) stay below the block's lowest point in every frame (checked).
# Re-run on a .blend that has the housing (its vertices carry the point attribute `qvr_housing`): with the same shape
# (as many vertices) they are moved and re-mapped in place, so the model's file keeps its vertices; otherwise the old
# housing is deleted and built anew (its old vertices then stay in the .mdl unused: start from the .blend in git from
# before any housing, 5c63e5cdd..aa0ba1aa4's).
# Texture, all from the gun's own skin: the block on the upper housing's painted metal (its top band 432..464 x 87..105,
# its sloped sides 420..430 x 87..105), the bezel on the receiver's brown-green paint (424..452 x 24..36), the window's
# walls on a darker patch of it (428..444 x 41..46), the floor near black, the rivets on the worn grey metal strip
# (472..490 x 32..39).

import math
import os
import re
import sys

import bpy
import numpy as np
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import housing_common as hc  # noqa: E402
STEM = "nail2"
MODEL = "v_nail2"
TAG = "housing_" + STEM

PIXEL_AT_04 = 0.09398  # a font pixel in model units at wpntxt_scale 0.4, style size 1 (vr_debug_screen_where)
MODEL_PER_WORLD = PIXEL_AT_04 / 0.04  # (a font pixel is 0.1 * wpntxt_scale world units)
# The receiver's rear top band (model units: its rear face's x, its top's z, its front's x; read and checked).
BAND_FRONT = 0.703
SIDE_SLOPE = 0.396  # the receiver's sides below the top: out this much a unit down
# Where weapons.txt's wpntxt values put the glass with the table's offset 0 (vr_debug_screen_where on the exported
# model), and the glass's up there: to check the table's offset and wpntxt_pitch.
GLASS_AT_ZERO = (-1.4707, -0.0522, 9.1166)
GLASS_UP = (0.6428, 0.0, 0.7660)
# The shape.
SLOPE = 50.0      # the screen's face, degrees from level (its normal 40 degrees above straight back)
SIDE = 15.0       # the block's sides, degrees from upright
MARGIN = 0.1      # h: the sloped face round the bezel, below and above it
EDGE = 0.05       # h: the bezel's top corners at least this far inside the block's sides
LEDGE = 0.25      # model units: the block's sides come down this far below the band's top
SINK = 0.1        # model units: the ledge's underside goes this far into the receiver's side
FRAME = 0.1       # h: the bezel's frame round the window
PROUD = 0.1       # h: the bezel's front off the sloped face
GLASS_BACK = 0.05  # h: the glass behind the bezel's front
WALL_DEEP = 0.1   # h: the window's walls into the block (past the face)
RIVET_R = 0.11  # (the spec 0.12: on the exact chamfer the ring would touch the window)
RIVET_AT = (0.78, 0.45)
RIVET_SIDES = 10
RIVET_TOP = 0.7   # the head's flat top, of its radius
RIVET_PROUD = 0.08
# Skin places (texels: x, y from the top).
TOP = (432.0, 464.0, 87.0, 105.0)    # the block's top and sloped face (the band's own top)
SIDES = (420.0, 430.0, 87.0, 105.0)  # its sides, front and corners (the band's sloped side)
PAINT = (424.0, 452.0, 24.0, 36.0)   # the bezel's front and sides
WALL = (428.0, 444.0, 41.0, 46.0)    # the window's walls (darker)
FLOOR = (428.0, 51.0)                # behind the glass
RIVET_X = 481.0
RIVET_ROWS = (38.0, 33.5, 33.0)      # base ring, top ring, top centre


RUN = []  # (the housing_common.Run of this run: its refusals)


def fail(msg):
    RUN[0].fail(msg)


def weapon_values():
    src = open(os.path.join(ROOT, "quakevr", "data", "weapons.txt"), encoding="utf-8").read()
    m = re.search(r'\{[^{}]*id "progs/%s\.mdl"[^{}]*\}' % MODEL, src)
    if not m:
        fail("no progs/%s.mdl in weapons.txt" % MODEL)
    out = {}
    for k in ("wpntxt_scale", "wpntxt_pitch", "hs1_x", "hs1_y", "hs1_z"):
        s = re.search(r'\b%s "(-?[0-9.]+)"' % k, m.group(0))
        if not s:
            fail("no %s for progs/%s.mdl in weapons.txt" % (k, MODEL))
        out[k] = float(s.group(1))
    return out


def grid():
    """The model file's grid: origin and scale (x, y, z)."""
    sc, org = hc.mdl_grid(MODEL)
    return np.array(org), np.array(sc)


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

    def __init__(self, opening, radius, pix, org, sc, band):
        self.org, self.sc = [float(a) for a in org], [float(a) for a in sc]
        x_r, z_t, y_l, y_r = band  # the band's rear x, top z, top's left (+Y) and right (-Y) edges
        self.cy = self.snap_axis(1, 0.5 * (y_l + y_r))
        self.hw_top = 0.5 * (y_l - y_r)
        self.x_r, self.z_t = x_r, z_t
        hw, hh, c = opening[0] * pix, opening[1] * pix, -radius * pix
        if radius >= 0:
            fail("the entry's radius is %.2f: the window is chamfered (radius < 0)" % radius)
        h = self.h = 2.0 * hh
        phi = math.radians(SLOPE)
        self.R = Vector((0.0, -1.0, 0.0))
        self.D = Vector((math.cos(phi), 0.0, math.sin(phi)))
        self.N = self.R.cross(self.D)  # (out: back and up)
        self.O = Vector((x_r, self.cy, z_t))
        bw, bh = hw + FRAME * h, hh + FRAME * h
        self.bw, self.bh = bw, bh
        self.vc = MARGIN * h + bh                       # the bezel's (and window's) centre up the face
        self.lf = 2.0 * (MARGIN * h + bh)               # the face's length
        self.z_top = z_t + self.lf * self.D.z
        self.x_ts = x_r + self.lf * self.D.x            # where the face meets the top
        self.x_f = BAND_FRONT
        if self.x_f - self.x_ts < 0.3:
            fail("the sloped face (%.2f long) leaves %.2f of flat top on the band" % (self.lf, self.x_f - self.x_ts))
        tan_side = math.tan(math.radians(SIDE))
        z_bt = z_t + (MARGIN * h + 2.0 * bh) * self.D.z  # the bezel's top corners' height
        self.over = max(0.1, bw + EDGE * h - self.hw_top + tan_side * (z_bt - z_t))
        self.z_l = z_t - LEDGE

        def half(z):  # the block's half width at z
            return self.hw_top + self.over - tan_side * (z - z_t)

        def rhalf(z):  # the receiver's below its top
            return self.hw_top + SIDE_SLOPE * (z_t - z)

        self.half, self.rhalf = half, rhalf
        self.glass_w = PROUD * h - GLASS_BACK * h
        self.verts = []   # Vector
        self.tris = []    # (i, j, k, wanted normal, uv mode)
        self.uvinfo = {}  # vertex -> (mode data)
        self.face_pts = []  # (vertex, wanted face coordinates): the sloped face's points, to report the rounding

        # The block. Face corners: bottom (on the band's rear top edge) and top; the top's front; the sides' bottoms.
        fbl = self.add_face(-half(z_t), 0.0, 0.0)
        fbr = self.add_face(half(z_t), 0.0, 0.0)
        ftl = self.add_face(-half(self.z_top), self.lf, 0.0)
        ftr = self.add_face(half(self.z_top), self.lf, 0.0)
        tfl = self.add_xyz(self.x_f, -half(self.z_top), self.z_top)
        tfr = self.add_xyz(self.x_f, half(self.z_top), self.z_top)
        sides = {}
        for s in (-1, 1):
            # rear bottom, front bottom (outside), the ledge's inner ends (rear, front: into the receiver), and the
            # receiver's outline at the ledge's height in the rear face's plane
            sides[s] = dict(rb=self.add_xyz(x_r, s * half(self.z_l), self.z_l),
                            fb=self.add_xyz(self.x_f, s * half(self.z_l), self.z_l),
                            ri=self.add_xyz(x_r, s * (rhalf(self.z_l) - SINK), self.z_l),
                            fi=self.add_xyz(self.x_f, s * (rhalf(self.z_l) - SINK), self.z_l),
                            ro=self.add_xyz(x_r, s * rhalf(self.z_l), self.z_l, outward=True),
                            re=self.add_xyz(x_r, s * rhalf(z_t), z_t))
        self.n_block_v = len(self.verts)
        # The bezel's outline on the face (the face's hole) and at its front; the window at its front and floor.
        bo = [(bw, bh), (-bw, bh), (-bw, -bh), (bw, -bh)]
        hole = [self.add_face(u, self.vc + v, 0.0) for u, v in bo]
        bfront = [self.add_face(u, self.vc + v, PROUD * h) for u, v in bo]
        win = chamfered(hw, hh, c)
        wf = [self.add_face(u, self.vc + v, PROUD * h, inward=True) for u, v in win]
        wb = [self.add_face(u, self.vc + v, -WALL_DEEP * h, inward=True) for u, v in win]
        out = self.N
        up = Vector((0, 0, 1))
        # The sloped face: a ring round the bezel (hole: top right, top left, bottom left, bottom right).
        self.quad(fbl, fbr, hole[3], hole[2], out, "top")
        self.quad(fbr, ftr, hole[0], hole[3], out, "top")
        self.quad(ftr, ftl, hole[1], hole[0], out, "top")
        self.quad(ftl, fbl, hole[2], hole[1], out, "top")
        # The top, the front, the sides, the ledges, the corners in the rear face's plane.
        self.quad(ftl, ftr, tfr, tfl, up, "top")
        fwd = Vector((1, 0, 0))
        for s in (-1, 1):
            sd = sides[s]
            ft, fb, tf = (ftr, fbr, tfr) if s > 0 else (ftl, fbl, tfl)
            side_n = Vector((0.0, s * math.cos(math.radians(SIDE)), math.sin(math.radians(SIDE))))
            self.tri(sd["rb"], fb, ft, side_n, "side")
            self.tri(sd["rb"], ft, tf, side_n, "side")
            self.tri(sd["rb"], tf, sd["fb"], side_n, "side")
            below = Vector((0, 0, -1))  # (the ledge's underside: a fan from its inner front end)
            self.tri(sd["fi"], sd["fb"], sd["rb"], below, "end")
            self.tri(sd["fi"], sd["rb"], sd["ro"], below, "end")
            self.tri(sd["fi"], sd["ro"], sd["ri"], below, "end")
            self.quad(sd["rb"], sd["ro"], sd["re"], fb, Vector((-1, 0, 0)), "end")
            self.tri(tf, sd["fb"], sd["fi"], fwd, "end")
        self.tri(tfl, tfr, sides[1]["fi"], fwd, "end")
        self.tri(tfl, sides[1]["fi"], sides[-1]["fi"], fwd, "end")
        self.n_block = len(self.tris)
        # The bezel: its front (a triangle at each chamfer, a quad along each side), its sides down to the face, the
        # window's walls and its floor.
        for k in range(4):
            self.tri(wf[2 * k], wf[2 * k + 1], bfront[k], out, "paint")
            n = (k + 1) % 4
            self.quad(wf[2 * k + 1], wf[(2 * k + 2) % 8], bfront[n], bfront[k], out, "paint")
        for k in range(4):
            n = (k + 1) % 4
            self.quad(bfront[k], bfront[n], hole[n], hole[k], "out", "bside")
        for k in range(8):
            n = (k + 1) % 8
            self.quad(wf[k], wf[n], wb[n], wb[k], "in", "wall")
        for k in range(1, 7):
            self.tri(wb[0], wb[k], wb[k + 1], out, "floor")
        self.wall_ring = {vi: (k, 0) for k, vi in enumerate(wf)}
        self.wall_ring.update({vi: (k, 1) for k, vi in enumerate(wb)})
        self.n_bezel = len(self.tris) - self.n_block
        self.win_uv = [self.face_uv(self.verts[i]) for i in wf]
        # The rivets: a ring on the bezel's front, a flat head proud of it.
        self.rivets = []
        r = RIVET_R * h
        for su, sv in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
            ru, rv = su * RIVET_AT[0] * h, sv * RIVET_AT[1] * h
            ring = [(math.cos(2 * math.pi * i / RIVET_SIDES), math.sin(2 * math.pi * i / RIVET_SIDES))
                    for i in range(RIVET_SIDES)]
            base = [self.add_face(ru + r * a, self.vc + rv + r * b, PROUD * h) for a, b in ring]
            top = [self.add_face(ru + RIVET_TOP * r * a, self.vc + rv + RIVET_TOP * r * b, (PROUD + RIVET_PROUD) * h)
                   for a, b in ring]
            tip = self.add_face(ru, self.vc + rv, (PROUD + RIVET_PROUD) * h)
            self.rivets.append((ru, rv))
            for i in range(RIVET_SIDES):
                j = (i + 1) % RIVET_SIDES
                self.quad(base[i], base[j], top[j], top[i], ("from", ru, rv), "rivet")
                self.tri(top[i], top[j], tip, out, "rivet")
            for vi in base:
                self.uvinfo[vi] = ("rivet", ru, rv, 0)
            for vi in top:
                self.uvinfo[vi] = ("rivet", ru, rv, 1)
            self.uvinfo[tip] = ("rivet", ru, rv, 2)
        self.glass = self.O + self.vc * self.D + self.glass_w * self.N

    # Snapping.
    def snap_axis(self, axis, x):
        return self.org[axis] + round((x - self.org[axis]) / self.sc[axis]) * self.sc[axis]

    def snap_face(self, p, inward=False):
        """The grid point for the face point p: near the face's plane above all (off it counts double), and with
        inward, no further from the window's centre in the face's u and v."""
        org, sc = self.org, self.sc
        k = [(p[i] - org[i]) / sc[i] for i in range(3)]
        cu = self.R.dot(p - self.O)
        cv = self.D.dot(p - self.O) - self.vc
        best = None
        for ix in range(math.floor(k[0]) - 1, math.floor(k[0]) + 3):
            for iy in range(math.floor(k[1]), math.floor(k[1]) + 2):
                for iz in range(math.floor(k[2]) - 3, math.floor(k[2]) + 5):
                    g = Vector((org[0] + ix * sc[0], org[1] + iy * sc[1], org[2] + iz * sc[2]))
                    d = g - p
                    dn = d.dot(self.N)
                    if inward:
                        gu = self.R.dot(g - self.O)
                        gv = self.D.dot(g - self.O) - self.vc
                        if abs(gu) > abs(cu) + 1e-6 or abs(gv) > abs(cv) + 1e-6 or gu * cu < -1e-9 or gv * cv < -1e-9:
                            continue
                    cost = 4.0 * dn * dn + (d - dn * self.N).length_squared
                    if best is None or cost < best[0]:
                        best = (cost, g)
        if best is None:
            fail("no grid point for a window corner at %.3f %.3f %.3f" % tuple(p))
        return best[1]

    def face_point(self, u, v, w):
        return self.O + u * self.R + v * self.D + w * self.N

    def face_uv(self, p):
        return self.R.dot(p - self.O), self.D.dot(p - self.O), self.N.dot(p - self.O)

    def add_face(self, u, v, w, inward=False):
        p = self.face_point(u, v, w)
        self.verts.append(self.snap_face(p, inward))
        self.face_pts.append((len(self.verts) - 1, (u, v, w)))
        return len(self.verts) - 1

    def add_xyz(self, x, y_off, z, outward=False):
        """A point y_off to the face's right (-Y) of the centre line, snapped (outward: away from the centre in Y)."""
        y = self.cy - y_off
        if outward:
            sy = self.sc[1]
            k = (y - self.org[1]) / sy
            y = self.org[1] + (math.floor(k) if y < self.cy else math.ceil(k)) * sy
        else:
            y = self.snap_axis(1, y)
        self.verts.append(Vector((self.snap_axis(0, x), y, self.snap_axis(2, z))))
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
        u, v, _ = self.face_uv(mid)
        if isinstance(want, tuple):  # (radial from a rivet's axis, in the face)
            want = (u - want[1]) * self.R + (v - self.vc - want[2]) * self.D
        elif want in ("out", "in"):
            w = u * self.R + (v - self.vc) * self.D
            want = w if want == "out" else -w
        return (i, j, k) if nrm.dot(want) >= 0.0 else (i, k, j)

    def texel(self, vi, mode, tri_ids):
        co = self.verts[vi]
        u, v, w = self.face_uv(co)
        hw_max = self.hw_top + self.over
        if mode == "top":  # the face up its length then the top to the front: down the band's skin
            along = v if co.x <= self.x_ts + 1e-3 and w < 0.05 else self.lf + (co.x - self.x_ts)
            total = self.lf + (self.x_f - self.x_ts)
            return (TOP[0] + (TOP[1] - TOP[0]) * (u + hw_max) / (2 * hw_max),
                    TOP[3] - (TOP[3] - TOP[2]) * min(max(along / total, 0.0), 1.0))
        if mode in ("side", "end"):
            a = (co.x - self.x_r) / (self.x_f - self.x_r)
            b = (self.z_top - co.z) / (self.z_top - self.z_l)
            if mode == "end":
                b = 0.6 + 0.4 * b
            return SIDES[0] + (SIDES[1] - SIDES[0]) * min(max(b, 0.0), 1.0), SIDES[3] - (SIDES[3] - SIDES[2]) * a
        if mode == "paint":
            return (PAINT[0] + (PAINT[1] - PAINT[0]) * (u + self.bw) / (2 * self.bw),
                    PAINT[2] + (PAINT[3] - PAINT[2]) * (self.bh - (v - self.vc)) / (2 * self.bh))
        if mode == "bside":
            ids = [self.face_uv(self.verts[t]) for t in tri_ids]
            horiz = max(abs(p[1] - ids[0][1]) for p in ids) < 0.2
            a = (u + self.bw) / (2 * self.bw) if horiz else (self.bh - (v - self.vc)) / (2 * self.bh)
            depth = 1.0 - w / (PROUD * self.h)
            return PAINT[0] + (PAINT[1] - PAINT[0]) * min(max(a, 0.0), 1.0), PAINT[2] + 4.0 + 4.0 * depth
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
        du, dv = u - ru, v - self.vc - rv
        rr = max(math.hypot(du, dv), 1e-6)
        return RIVET_X + (4.0 if ring == 0 else 3.0) * du / rr, RIVET_ROWS[ring]

    def report(self):
        h = self.h
        # The rounding: how far the face's points left their place, off the plane and in it.
        off = max(abs(self.face_uv(self.verts[i])[2] - w) for i, (u, v, w) in self.face_pts)
        inplane = max(math.hypot(self.face_uv(self.verts[i])[0] - u, self.face_uv(self.verts[i])[1] - v)
                      for i, (u, v, w) in self.face_pts)
        wu = max(abs(a) for a, _, _ in self.win_uv)
        wv = max(abs(b - self.vc) for _, b, _ in self.win_uv)
        # The rivets' rings: on the bezel (inside its edge) and clear of the window (its chamfered outline).
        hw_win, hh_win = self.bw - FRAME * h, self.bh - FRAME * h
        ru, rv = abs(self.rivets[0][0]), abs(self.rivets[0][1])
        chamf = RUN[0].style["radius"]
        cpx = -chamf * (h / (2.0 * RUN[0].style["opening"][1]))
        edge, clear = 1e9, 1e9
        for i in range(RIVET_SIDES):
            a = 2 * math.pi * i / RIVET_SIDES
            u, v = abs(ru + RIVET_R * h * math.cos(a)), abs(rv + RIVET_R * h * math.sin(a))
            edge = min(edge, self.bw - u, self.bh - v)
            clear = min(clear, max(u - hw_win, v - hh_win, (u + v - (hw_win + hh_win - cpx)) * 0.7071))
        if edge < -0.01 or clear < 0.02:
            fail("the rivets overhang the bezel (%.3f) or cut into the window (%.3f clear)" % (-edge, clear))
        top_c = self.half(self.z_l)
        return ("window %.3f x %.3f as snapped (h %.3f); bezel %.3f x %.3f, %.3f proud; glass %.3f behind its front; "
                "face %.1f deg, %.3f long, the block %.3f high (top z %.3f), flat top x %.3f..%.3f, %.3f wide at the "
                "band's top (the band %.3f), %.3f at its top (sides %.0f deg, %.3f past the band's edges), down to z "
                "%.3f; the bezel's top corners %.3f inside the sides; face points off the plane <= %.3f, moved in it "
                "<= %.3f; rivets %.3f inside the bezel's edge, %.3f clear of the window; the ledges %.3f wide"
                % (2 * wu, 2 * wv, h, 2 * self.bw, 2 * self.bh, PROUD * h, GLASS_BACK * h, SLOPE, self.lf,
                   self.z_top - self.z_t, self.z_top, self.x_ts, self.x_f, 2 * self.half(self.z_t), 2 * self.hw_top,
                   2 * self.half(self.z_top), SIDE, self.over, self.z_l,
                   self.half(self.z_t + (MARGIN * h + 2 * self.bh) * self.D.z) - self.bw, off, inplane, edge, clear,
                   top_c - self.rhalf(self.z_l)))


def main(h):
    RUN[:] = [h]
    st = h.style
    opening, radius, size, offset = st["opening"], st["radius"], st["size"], st["offset"]
    if len(opening) != 2:
        fail("the \"%s\" entry has no .opening" % STEM)
    wv = weapon_values()
    pix = PIXEL_AT_04 * wv["wpntxt_scale"] / 0.4 * size
    ob = bpy.data.objects[MODEL]
    me = ob.data
    img = bpy.data.images.get(MODEL + "_skin0")
    sw, sh = img.size
    keys = me.shape_keys.key_blocks

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
        return ids, x0, co

    def band():
        """The rear top band: the rear face's x, its top z and the top edge's two ends (Y), checked against the band's
        front (BAND_FRONT, the same top)."""
        ids, x0, co = rear_face()
        f = co[ids]
        zt = f[:, 2].max()
        top = f[np.abs(f[:, 2] - zt) < 0.02]
        if len(top) != 2:
            fail("the rear face's top edge has %d vertices (expected 2)" % len(top))
        flags = housing_flags()
        front = co[(flags == 0) & (np.abs(co[:, 0] - BAND_FRONT) < 0.02) & (np.abs(co[:, 2] - zt) < 0.02)]
        if len(front) < 2:
            fail("no top band front at x %.3f, z %.3f" % (BAND_FRONT, zt))
        return float(x0), float(zt), float(top[:, 1].max()), float(top[:, 1].min())

    def design():
        org, sc = grid()
        return Design(opening, radius, pix, org, sc, band())

    d = design()
    h.say(d.report())

    def uv_at(tx, ty):
        return (tx / sw, 1.0 - ty / sh)

    def build(d):
        """The housing for housing_common's apply: positions, triangles wound outward with each corner's UV."""
        tris = []
        for t in d.tris:
            ids = d.oriented(t)
            tris.append((ids[0], ids[1], ids[2], tuple(uv_at(*d.texel(i, t[4], ids)) for i in ids)))
        return list(d.verts), tris

    def receiver_motion():
        """Per frame, the rear face's rigid motion (R, its centre in frame 0, in that frame) and the fit's residual."""
        _, mdl_blender = hc.addon()
        ids, _, _ = rear_face()
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

    def barrels_clear():
        """The parts that move against the receiver (the spinning barrels), in the receiver's own frame, in every
        frame: their highest point under the housing's footprint, and the housing's lowest."""
        motion, _ = receiver_motion()
        flags = housing_flags()
        n = len(me.vertices)
        buf = np.empty(n * 3, np.float32)
        keys[0].data.foreach_get("co", buf)
        P0 = buf.reshape(n, 3).astype(np.float64)
        hid = np.nonzero(flags)[0]
        lo = P0[hid].min(0)
        hi = P0[hid].max(0)
        top = -1e9
        for kb, (R, c0, c1) in zip(keys, motion):
            kb.data.foreach_get("co", buf)
            P = (buf.reshape(n, 3).astype(np.float64) - c1) @ R + c0  # back into the receiver's frame 0
            moving = (np.linalg.norm(P - P0, axis=1) > 0.05) & (flags == 0)
            under = moving & (P[:, 0] > lo[0] - 1) & (P[:, 0] < hi[0] + 1) & (P[:, 1] > lo[1]) & (P[:, 1] < hi[1])
            if under.any():
                top = max(top, P[under, 2].max())
        return top, lo[2]

    def housing_frames():
        """The housing's vertices and, per frame, where the rear face's motion puts frame 0's."""
        motion, worst = receiver_motion()
        hid = np.nonzero(housing_flags())[0]
        n = len(me.vertices)
        buf = np.empty(n * 3, np.float32)
        keys[0].data.foreach_get("co", buf)
        P0 = buf.reshape(n, 3)[hid].astype(np.float64)
        return hid, [(P0 - c0) @ R.T + c1 for R, c0, c1 in motion], worst

    def set_frames():
        """The housing's place in every frame: frame 0's moved with the rear face (not the nearest vertex the export
        would pick: the barrels spin)."""
        hid, places, worst = housing_frames()
        n = len(me.vertices)
        buf = np.empty(n * 3, np.float32)
        for kb, p in zip(keys, places):
            kb.data.foreach_get("co", buf)
            co = buf.reshape(n, 3)
            co[hid] = p
            kb.data.foreach_set("co", co.ravel())
        me.update()
        return worst

    # New vertices ride their nearest old one in the export whatever their shape keys say: a new housing is exported
    # once (the box grows), then laid on the grown grid with its own frames. Moved in place, the frames are set and the
    # last export (finish) writes them.
    passes = 0
    while True:
        verts, tris = build(d)
        moved = h.apply(me, verts, tris, smooth=True)
        fit = set_frames()
        if moved or h.dry:
            break
        h.export(grow=True)
        h.grid = hc.mdl_grid(MODEL)
        d = design()
        passes += 1
        if passes > 3:
            fail("the housing's grid didn't settle in %d exports" % passes)
    if passes:
        h.say(d.report())

    barrel_top, housing_low = barrels_clear()
    if barrel_top > housing_low - 0.1:
        fail("the barrels come up to z %.3f under the housing, whose lowest point is %.3f" % (barrel_top, housing_low))
    hs1 = Vector((wv["hs1_x"], wv["hs1_y"], wv["hs1_z"]))
    grip = min((v - hs1).length for v in d.verts)

    # Check: what the export writes for the housing in every frame is the rear face's motion of frame 0's housing.
    off = 0.0
    if not h.dry:
        _, mdl_blender = hc.addon()
        mdl_blender.carry_frames(ob)
        hid, places, _ = housing_frames()
        n = len(me.vertices)
        buf = np.empty(n * 3, np.float32)
        for kb, p in zip(keys, places):
            kb.data.foreach_get("co", buf)
            off = max(off, np.abs(buf.reshape(n, 3)[hid] - p).max())
        if off > 1e-3:
            fail("the exported frames wouldn't carry the housing with the rear face (off by %.4f)" % off)

    # The glass: tilted with the face (weapons.txt's wpntxt_pitch: its up as the face's), and the table's offset
    # (world units along the glass's right, up and out) from weapons.txt's place onto the window's.
    up = Vector(GLASS_UP).normalized()
    tilt = math.degrees(up.angle(d.D))
    if tilt > 0.5:
        h.say("WARNING the glass's up (%.3f %.3f %.3f at wpntxt_pitch %.1f) is %.2f degrees off the face's: tilt it "
              "%.2f more" % (*up, wv["wpntxt_pitch"], tilt, math.degrees(math.atan2(up.x, up.z) -
                                                                         math.atan2(d.D.x, d.D.z)) * -1.0))
    right = d.R
    outv = right.cross(up)
    delta = (d.glass - Vector(GLASS_AT_ZERO)) / MODEL_PER_WORLD
    want_off = (delta.dot(right), delta.dot(up), delta.dot(outv))
    if max(abs(a - b) for a, b in zip(want_off, offset)) > 0.002:
        h.say("WARNING the table's offset {%.4f, %.4f, %.4f} doesn't put the glass at %.4f %.4f %.4f: use "
              "{%.4ff, %.4ff, %.4ff}" % (*offset, *d.glass, *want_off))
    grid_before = h.grid

    def after_export():
        if hc.mdl_grid(MODEL) != grid_before:
            h.say("WARNING the export changed the model's grid: run this again to lay the housing on it")

    h.finish("%d vertices, %d triangles (block %d, bezel %d, rivets %d); the glass at %.4f %.4f %.4f, tilted %.1f "
             "deg; the barrels' top under it %.3f (the housing's lowest %.3f); the off hand's grip spot %.2f away; "
             "frames with the rear face (its fit %.4f, the export's %.4f off)"
             % (len(d.verts), len(d.tris), d.n_block, d.n_bezel, len(d.tris) - d.n_block - d.n_bezel, *d.glass,
                90.0 - SLOPE, barrel_top, housing_low, grip, fit, off),
             grow=True, after_export=after_export)


if __name__ == "__main__":
    hc.Run(STEM, MODEL).main(main)
