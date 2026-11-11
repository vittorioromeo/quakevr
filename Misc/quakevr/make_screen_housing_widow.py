#!/usr/bin/env python3
# make_screen_housing_widow.py -- AD's Widowmaker's ammo screen housing, modelled on the hand-made Widowmaker
# (Misc/quakevr/models_src/widowmaker/widowmaker.blend; ROUND24.md, "The Widowmaker's and the AD Plasma Gun's ammo
# screen housings"; his mockup, kit scratch/widowmaker_housing_mockup_1111.webp: a small bevelled bronze bezel with four
# corner rivets on the receiver's top-rear chamfer, the screen's digits as they are).
#
# Usage: blender -b --factory-startup Misc/quakevr/models_src/widowmaker/widowmaker.blend
#            --python Misc/quakevr/make_screen_housing_widow.py -- [--save other.blend] [--dry]
# Builds or moves the housing (its own object, ScreenHousing: the Body untouched), exports the model with the
# Widowmaker's own exporter (models_src/widowmaker/export_mdl.py: v_widowmaker.mdl and its preview) and saves the .blend.
# Ends with "housing_shot3.mdl#vr: ok, ..." or "...: REFUSED: ..." (exit code 1); --dry exports and saves nothing. The
# housing scripts work alike (housing_common.py; docs/vr-port/MODELS_IN_BLENDER.md, "Tweaking the ammo screen housings").
#
# One definition: the window's half size (`opening`, font pixels), its chamfer (`radius`, negative: cut corners) and the
# glass's `offset` are the "shot3.mdl#vr" entry of Quake/vr/vr_screenstyle.cpp. The script refuses when they disagree
# with what it builds and prints the entry that matches. The glass keeps weapons.txt's lean (slot 34 wpntxt_pitch 38.8:
# the digits as they were), 3.1 degrees off the plate's.
#
# The grid. v_widowmaker.mdl's vertices are bytes: X 0.234 a step (coarse), Y 0.033, Z 0.065. The chamfer leans 45
# degrees; the grid's nearest plane is one X and three Z steps (ROW: 39.6 degrees, a "row" 0.304 up it); off it the grid
# points come in "levels" 0.050 apart (level L: 0.041 L up the face from a row, modulo a row). The front is built on that
# plane (exact, symmetric left/right; top/bottom as the rows allow).
# The shape (u up the face, d out of it towards the eye, y across; h the window's height = 3 rows = 0.91):
# - The window: 3 rows by 2 WIN_Y Y steps (the glass the screen had without a housing: its text's size and padding, so
#   that the digits keep the font's size, vr_screenstyle.hpp fillMax 1); walls straight back (BACK: one X step, four Z
#   steps back, square to the plate within 3 degrees) to a dark floor; its corners cut from the front: a facet from the
#   top (or bottom) edge, CHAMFER h in, to the side wall at the grid point some levels back nearest the same cut. The
#   glass GLASS_IN h behind the front, its corners cut by the shader.
# - The bezel: one flat plate, PROUD in front of the chamfer at the window's middle; a band of one row above and below
#   the window (0.33h: the grid's least) and BAND_Y Y steps at the sides; its outer edge bevelled back to BEVEL_LEVEL
#   (0.15 back: about 45 degrees, the top 40, the bottom 50), then walls straight back into the gun, closed there.
# - 4 round rivets in the top and bottom bands at +-RIVET_X of the window's width: a cone from a grid point in front of
#   the plate (RIVET_LEVELS: the level whose points sit nearest the band's middle) to a ring of grid points sunk behind
#   it, searched so that the cone comes through the plate on a circle of RIVET_R h (width / height printed).
# Texture, all from the gun's own skin, each face laid on a part of the gun at that part's own texel density: the plate
# from the receiver's top (the rusty casing beside the strip); the bevels and outer walls from the receiver's back
# beside the rear plate (the darker brown casing metal); the window's walls and floor from the shell port's dark
# inside; the rivets' points on a painted rivet of the rear plate, their rings round it.

import math
import os
import sys

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "models_src", "widowmaker"))
import housing_common as hc  # noqa: E402
import export_mdl  # noqa: E402  (the Widowmaker's exporter)

STEM = "shot3.mdl#vr"
MODEL = "v_widowmaker"
HOUSING = "ScreenHousing"

# Measured with vr_debug_screen_where 1 (Debug > Reports > Ammo Screen on Model) at weapons.txt's wpntxt values (slot
# 34) and the table's offset 0: the glass's centre, right and up, and a font pixel at size 1, in model units.
BASE = (Vector((0.2323, 0.0107, 10.9433)), Vector((-0.0009, -1.0, 0.0001)), Vector((0.7349, -0.0005, 0.6781)), 0.04650)
WPNTXT_SCALE = 0.25

ROW = (1, 3)           # one row up the face (X, Z steps)
BACK = (1, -4)         # a step straight back into the gun (X, Z steps)
WIN_ROWS = 3           # the window's height, rows (its middle half way between two)
WIN_Y = 14             # its half width, Y steps
BAND_Y = 3             # the bezel's bands at the sides, Y steps (above and below: one row)
BEVEL_LEVEL = -3       # the bevel's foot this many levels behind the plate
BEVEL_Y = 4            # and this many Y steps out at the sides
PROUD = (0.07, 0.12)   # the plate's front in front of the chamfer at the window's middle (units, a range)
CHAMFER = 0.12         # the window's corners cut, in h (the top/bottom edge; the side's the grid's nearest)
CUT_LEVELS = (-3, -4, -5)  # the levels the cuts' side points may be on
GLASS_IN = 0.05        # the glass behind the front, in h
RIVET_R = 0.08         # the rivets' radius where they come through the plate, in h
RIVET_X = 0.40         # their places across, in the window's width from its middle
RIVET_LEVELS = (2, 3)  # their points this many levels in front of the plate (a range)
RIVET_ANGLES = 8
LEAN_MAX = 5.0         # the glass's lean off the plate (degrees) refused past this

# Where the texels come from: a point on the gun and the way in to it (a ray), per material.
SRC_PLATE = (Vector((5.0, 1.35, 13.0)), Vector((0.0, 0.0, -1.0)))   # the receiver's top beside the strip
SRC_BEVEL = (Vector((-2.0, 3.24, 7.15)), Vector((1.0, 0.0, 0.0)))   # the receiver's back beside the rear plate
SRC_DARK = (Vector((6.5, 7.0, 5.6)), Vector((0.0, -1.0, 0.0)))      # the shell port's dark inside
SRC_RIVET = (Vector((-2.0, 0.0, 10.074)), Vector((1.0, 0.0, 0.0)))  # the rear plate's top middle rivet (painted, r 0.3)
RIVET_TEX_R = 0.15     # the rings' texels this far (units on the plate) round the painted rivet's middle


class Run(hc.Run):
    """The Widowmaker's own .blend and exporter: the gun's faces are Body's and Barrels' (the housing its own object)."""

    def gun_uvs(self):
        faces = []
        for name in ("Body", "Barrels"):
            me = bpy.data.objects[name].data
            uv = me.uv_layers.active.data
            for p in me.polygons:
                faces.append((name, tuple(sorted((me.loops[li].vertex_index, round(uv[li].uv[0], 6),
                                                  round(uv[li].uv[1], 6)) for li in p.loop_indices))))
        return sorted(faces)

    def export(self, grow=False):
        self.check_uvs("before the export")
        if self.dry:
            return 0
        export_mdl.export(log=lambda m: print("  " + m))
        self.exported += 2
        return 2


class FaceMap:
    """The texels of the gun's face met by a ray (`src`: a point and the way in): an affine map of points on its plane."""

    def __init__(self, h, body, bvh, src, sw, sh):
        loc, nrm, idx, _ = bvh.ray_cast(src[0], src[1], 50.0)
        if loc is None:
            h.fail("no face of the gun met from %s along %s" % (tuple(src[0]), tuple(src[1])))
        me = body.data
        p = me.polygons[idx]
        uv = me.uv_layers.active.data
        li = list(p.loop_indices)[:3]
        self.ps = [me.vertices[me.loops[i].vertex_index].co.copy() for i in li]
        self.ts = [Vector((uv[i].uv[0] * sw, (1.0 - uv[i].uv[1]) * sh)) for i in li]
        self.hit = loc
        self.normal = p.normal.copy()
        e1, e2 = self.ps[1] - self.ps[0], self.ps[2] - self.ps[0]
        self.e1, self.e2 = e1, e2
        self.g = (e1.dot(e1), e1.dot(e2), e2.dot(e2))

    def at(self, co):
        r = co - self.ps[0]
        a, b, c = self.g
        d1, d2 = r.dot(self.e1), r.dot(self.e2)
        det = a * c - b * b
        s, t = (c * d1 - b * d2) / det, (a * d2 - b * d1) / det
        tx = self.ts[0] + (self.ts[1] - self.ts[0]) * s + (self.ts[2] - self.ts[0]) * t
        return tx.x, tx.y

    def density(self):
        """Texels a unit (its first edge's)."""
        return ((self.ts[1] - self.ts[0]).length / max(self.e1.length, 1e-6))


def housing_object(body):
    ob = bpy.data.objects.get(HOUSING)
    if ob is None:
        me = bpy.data.meshes.new(HOUSING)
        me.uv_layers.new(name="UVMap")
        me.materials.append(body.data.materials[0])
        ob = bpy.data.objects.new(HOUSING, me)
        for col in body.users_collection:
            col.objects.link(ob)
        ob.matrix_world = body.matrix_world.copy()
    return ob


def main(h):
    body = bpy.data.objects["Body"]
    st = h.style
    (ow, oh), orad, size, offset = st["opening"], st["radius"], st["size"], st["offset"]
    base_c, base_r, base_u, pixel1 = BASE
    base_r, base_u = base_r.normalized(), base_u.normalized()
    px = pixel1 * size
    mpu = pixel1 / (0.1 * WPNTXT_SCALE)
    sc, og = h.grid
    hi_grid = [og[i] + 255 * sc[i] for i in range(3)]
    ob = housing_object(body)
    me = ob.data
    img = export_mdl.fg.skin_image(body)
    sw, sh = img.size

    # The gun as the model file has it: its vertices on the grid (the chamfer's corners move up to 0.11 in X: it stands
    # 0.06 further out than in the .blend).
    gun_vs = [Vector(tuple(og[i] + round((v.co[i] - og[i]) / sc[i]) * sc[i] for i in range(3)))
              for v in body.data.vertices]
    bvh = BVHTree.FromPolygons(gun_vs, [list(p.vertices) for p in body.data.polygons])

    # The chamfer: the biggest face leaning back and up, square across.
    face = max((p for p in body.data.polygons if p.normal.x < -0.5 and abs(p.normal.y) < 0.05 and p.normal.z > 0.5),
               key=lambda p: p.area, default=None)
    if face is None:
        h.fail("the receiver's top-rear chamfer wasn't found")
    fq = [gun_vs[i] for i in face.vertices]
    n_f = (fq[1] - fq[0]).cross(fq[2] - fq[0]).normalized()
    n_f = n_f if n_f.dot(face.normal) > 0 else -n_f
    f0 = fq[0]

    def G(i, k, j):
        return Vector((og[0] + sc[0] * i, og[1] + sc[1] * j, og[2] + sc[2] * k))

    row_v = Vector((sc[0] * ROW[0], 0.0, sc[2] * ROW[1]))
    up = row_v.normalized()
    out = Vector((-up.z, 0.0, up.x))
    right = Vector((0.0, -1.0, 0.0))
    rowlen = row_v.length
    level = sc[0] * sc[2] / rowlen
    hgt = WIN_ROWS * rowlen
    j0 = round((base_c.y - og[1]) / sc[1])

    # The window's bottom edge's middle: the grid point whose plate sits PROUD in front of the chamfer and whose glass
    # is nearest the old glass up the face.
    best = None
    i_c = round((base_c.x - og[0]) / sc[0])
    k_c = round((base_c.z - og[2]) / sc[2])
    for i in range(i_c - 8, i_c + 8):
        for k in range(k_c - 40, k_c + 40):
            centre = G(i, k, j0) + row_v * (WIN_ROWS / 2.0)
            d = (centre - f0).dot(n_f)
            if not PROUD[0] <= d <= PROUD[1]:
                continue
            glass = centre - out * (GLASS_IN * hgt)
            miss = abs((glass - base_c).dot(up))
            if best is None or miss < best[0]:
                best = (miss, d, i, k)
    if best is None:
        h.fail("no grid point puts the plate %.2f..%.2f in front of the chamfer" % PROUD)
    _, proud, ib, kb = best

    def P(a, b, j=0):
        return G(ib + a, kb + b, j0 + j)

    def rows(r):
        return (ROW[0] * r, ROW[1] * r)

    centre = P(0, 0) + row_v * (WIN_ROWS / 2.0)

    def uvd(co):
        rel = co - centre
        return rel.dot(right), rel.dot(up), rel.dot(out)

    wwin = WIN_Y * sc[1]
    glass = centre - out * (GLASS_IN * hgt)
    top_u, bot_u = hgt / 2.0, -hgt / 2.0

    def lattice_near(want_u, levels, j=0, span=(-8, 9, -40, 41), side=0):
        """The grid point (a, b) on one of `levels` nearest `want_u` up the face (side > 0: above it, < 0: below)."""
        cands = []
        for a in range(*span[:2]):
            for b in range(*span[2:]):
                _, uu, dd = uvd(P(a, b, j))
                lv = round(dd / level)
                if lv not in levels or abs(dd - lv * level) > 1e-3:
                    continue
                if side and (uu - want_u) * side <= 1e-6:
                    continue
                cands.append((abs(uu - want_u), -dd, a, b, uu, dd))
        if not cands:
            h.fail("no grid point on levels %s near %.3f up the face" % (levels, want_u))
        c = min(cands)
        return c[2], c[3], c[4], c[5]

    # The window's cuts: on the top/bottom edge CHAMFER h in; on the side the grid point nearest the same cut.
    cut_y = round(CHAMFER * hgt / sc[1])
    cuts = {}
    for su, edge in ((1, top_u), (-1, bot_u)):
        a, b, uu, dd = lattice_near(edge - su * CHAMFER * hgt, CUT_LEVELS)
        cuts[su] = (a, b, abs(edge - uu) / hgt, dd)
    cut_px = cut_y * sc[1] / px

    # The table against the window: the glass the window's size, its chamfer the cut, sitting in it.
    out_base = base_r.cross(base_u).normalized()
    have = base_c + (base_r * offset[0] + base_u * offset[1] + out_base * offset[2]) * mpu
    dg = glass - base_c
    lean = math.degrees(math.atan2(up.z, up.x))
    tilt = math.degrees(base_u.angle(up))
    want = (wwin / px, hgt / 2 / px, -cut_px)
    h.say("plate %.2f deg from level (the chamfer 45, the glass %.2f: %.2f off); font pixel %.5f; window %d Y steps x "
          "%d rows = %.4f x %.4f (%.2f x %.2f px); plate %.3f in front of the chamfer (%.2fh); cuts %.3fh (top/bottom "
          "edge), %.3fh / %.3fh at the sides (%.3f / %.3f back); glass %.4f %.4f %.4f"
          % (lean, math.degrees(math.atan2(base_u.z, base_u.x)), tilt, px, 2 * WIN_Y, WIN_ROWS, 2 * wwin, hgt,
             2 * wwin / px, hgt / px, proud, proud / hgt, cut_y * sc[1] / hgt, cuts[1][2], cuts[-1][2], -cuts[1][3],
             -cuts[-1][3], *glass))
    if (abs(ow - want[0]) > 0.3 or abs(oh - want[1]) > 0.3 or abs(orad - want[2]) > 0.3
            or (glass - have).length > 0.01):
        h.fail("the table disagrees with the window (the glass %.4f units off); a matching entry: .radius = %.2ff, "
               ".opening = {%.2ff, %.2ff}, .offset = {%.4ff, %.4ff, %.4ff}"
               % ((glass - have).length, want[2], want[0], want[1], dg.dot(base_r) / mpu, dg.dot(base_u) / mpu,
                  dg.dot(out_base) / mpu))
    if tilt > LEAN_MAX:
        h.fail("the glass leans %.2f degrees off the plate (at most %.1f)" % (tilt, LEAN_MAX))
    # Its top and bottom edges stay between the plate's front and the window's floor.
    for su in (1, -1):
        e = glass + base_u * (su * hgt / 2.0)
        de = (e - centre).dot(out)
        if not -0.3 < de < -0.005:
            h.fail("the glass's %s edge %.3f off the plate's front (inside: -0.005 .. -0.3)"
                   % ("top" if su > 0 else "bottom", de))

    verts, tris, index = [], [], {}

    def vid(a, b, j):
        key = (a, b, j)
        if key not in index:
            index[key] = len(verts)
            verts.append(P(a, b, j))
        return index[key]

    def tri(a, b, c, want_n, mode):
        tris.append((a, b, c, want_n, mode))

    def quad(a, b, c, e, want_n, mode):
        tri(a, b, c, want_n, mode)
        tri(a, c, e, want_n, mode)

    ring = [(1, 1), (1, -1), (-1, -1), (-1, 1)]  # (up/down, left/right)
    r_top, r_bot = rows(WIN_ROWS), rows(0)
    o_top, o_bot = rows(WIN_ROWS + 1), rows(-1)
    oy = WIN_Y + BAND_Y

    def ab(c, top, bot):
        return top if c[0] > 0 else bot

    # The plate's front: a ring between its outline and the window.
    outer = {c: vid(*ab(c, o_top, o_bot), c[1] * oy) for c in ring}
    inner = {c: vid(*ab(c, r_top, r_bot), c[1] * WIN_Y) for c in ring}
    for n_ in range(4):
        c0, c1 = ring[n_], ring[(n_ + 1) % 4]
        quad(outer[c0], outer[c1], inner[c1], inner[c0], out, "front")

    # The window: walls straight back one BACK step to the floor; the corners' facets.
    back_in = {c: vid(ab(c, r_top, r_bot)[0] + BACK[0], ab(c, r_top, r_bot)[1] + BACK[1], c[1] * WIN_Y) for c in ring}
    for n_ in range(4):
        c0, c1 = ring[n_], ring[(n_ + 1) % 4]
        mid = (Vector(uvd(verts[inner[c0]])) + Vector(uvd(verts[inner[c1]]))) / 2
        inward = -(right * mid[0] + up * mid[1]).normalized()
        quad(inner[c0], inner[c1], back_in[c1], back_in[c0], inward, "inner")
    quad(back_in[(1, 1)], back_in[(1, -1)], back_in[(-1, -1)], back_in[(-1, 1)], out, "inner")
    for su in (1, -1):
        a, b, _, _ = cuts[su]
        for s in (1, -1):
            e_ = vid(*ab((su, s), r_top, r_bot), s * (WIN_Y - cut_y))
            side = vid(a, b, s * WIN_Y)
            tri(e_, inner[(su, s)], side, out - up * su + right * s, "inner")

    # The bevel: from the outline back to BEVEL_LEVEL, out to the grid's nearest point past the outline (above: up,
    # below: down) and BEVEL_Y steps at the sides.
    o_top_u, o_bot_u = uvd(verts[outer[(1, 1)]])[1], uvd(verts[outer[(-1, 1)]])[1]
    bev = {}
    for su, edge in ((1, o_top_u), (-1, o_bot_u)):
        a, b, uu, dd = lattice_near(edge, (BEVEL_LEVEL,), side=su)
        bev[su] = (a, b, abs(uu - edge), dd)
    by = oy + BEVEL_Y
    foot = {c: vid(bev[c[0]][0], bev[c[0]][1], c[1] * by) for c in ring}
    for n_ in range(4):
        c0, c1 = ring[n_], ring[(n_ + 1) % 4]
        mid = (Vector(uvd(verts[outer[c0]])) + Vector(uvd(verts[outer[c1]]))) / 2
        quad(outer[c0], outer[c1], foot[c1], foot[c0], (right * mid[0] + up * mid[1]).normalized() + out, "bevel")

    # The walls: the bevel's foot straight back into the gun, as few steps as put it all inside, closed there.
    def inside(co):
        hits = 0
        for dirv in (Vector((0.0, 0.0, 1.0)), Vector((0.31, 0.2, 0.93)).normalized(),
                     Vector((-0.3, -0.25, 0.92)).normalized()):
            loc, nrm, _, _ = bvh.ray_cast(co, dirv, 50.0)
            hits += 1 if loc is not None and nrm.dot(dirv) > 0 else 0
        return hits >= 2

    foot_ab = {c: bev[c[0]][:2] for c in ring}
    steps = None
    for kk in range(1, 9):
        if all(inside(P(foot_ab[c][0] + kk * BACK[0], foot_ab[c][1] + kk * BACK[1], c[1] * by)) for c in ring):
            steps = kk
            break
    if steps is None:
        h.fail("the bezel's walls don't reach inside the gun in 8 steps")
    back_out = {c: vid(foot_ab[c][0] + steps * BACK[0], foot_ab[c][1] + steps * BACK[1], c[1] * by) for c in ring}
    for n_ in range(4):
        c0, c1 = ring[n_], ring[(n_ + 1) % 4]
        mid = (Vector(uvd(verts[foot[c0]])) + Vector(uvd(verts[foot[c1]]))) / 2
        quad(foot[c0], foot[c1], back_out[c1], back_out[c0], (right * mid[0] + up * mid[1]).normalized(), "wall")
    quad(back_out[(1, 1)], back_out[(1, -1)], back_out[(-1, -1)], back_out[(-1, 1)], -out, "wall")

    # The rivets: in the top and bottom bands at +-RIVET_X of the window's width; a point RIVET_LEVELS in front (the one
    # nearest the band's middle), a ring of grid points behind the plate through which the cone crosses the plate (d 0)
    # on a circle of RIVET_R h round the point.
    rr = RIVET_R * hgt
    rj = round(RIVET_X * 2 * wwin / sc[1])
    tips, rivet_mid, rivet_info = [], {}, []
    for su, mid_u in ((1, top_u + rowlen / 2), (-1, bot_u - rowlen / 2)):
        ta, tb, tip_u, tip_d = lattice_near(mid_u, tuple(range(RIVET_LEVELS[0], RIVET_LEVELS[1] + 1)))
        rcands = []
        for a in range(-8, 9):
            for b in range(-40, 41):
                _, qu, qd = uvd(P(a, b))
                if not -0.6 < qd < -0.02:
                    continue
                t = tip_d / (tip_d - qd)
                for dj in range(-8, 9):
                    rcands.append((t * (-dj * sc[1]), tip_u + t * (qu - tip_u), a, b, dj))
        best_ring = None
        for rot in range(0, 45, 3):
            ring_pts, seen, worst = [], [], 0.0
            for k_ in range(RIVET_ANGLES):
                th = math.radians(90 - rot) - 2 * math.pi * k_ / RIVET_ANGLES
                w2 = (rr * math.cos(th), tip_u + rr * math.sin(th))
                hy, hu, a, b, dj = min(rcands, key=lambda c: math.hypot(c[0] - w2[0], c[1] - w2[1]))
                worst = max(worst, math.hypot(hy - w2[0], hu - w2[1]))
                ring_pts.append((a, b, dj))
                seen.append((hy, hu))
            if len(set(ring_pts)) == RIVET_ANGLES and (best_ring is None or worst < best_ring[0]):
                best_ring = (worst, rot, ring_pts, seen)
        if best_ring is None:
            h.fail("no ring of %d grid points round the %s rivets" % (RIVET_ANGLES, "upper" if su > 0 else "lower"))
        worst, rot, ring_pts, seen = best_ring
        ys = [p_[0] for p_ in seen]
        us = [p_[1] for p_ in seen]
        ratio = (max(ys) - min(ys)) / (max(us) - min(us))
        rivet_info.append("%s: point %.3f in front, %.3f from the band's middle, w/h %.3f, worst %.3f" % (
            "upper" if su > 0 else "lower", tip_d, tip_u - mid_u, ratio, worst))
        if not 0.85 <= ratio <= 1.15:
            h.fail("the %s rivets aren't round (width / height %.3f)" % ("upper" if su > 0 else "lower", ratio))
        if min(us) < (top_u + 0.005 if su > 0 else o_bot_u + 0.005) or max(us) > (o_top_u - 0.005 if su > 0
                                                                                    else bot_u - 0.005):
            h.fail("the %s rivets aren't inside their band" % ("upper" if su > 0 else "lower"))
        for s in (1, -1):
            jy = s * rj
            t_i = vid(ta, tb, jy)
            tips.append(t_i)
            rivet_mid[t_i] = (-jy * sc[1], tip_u)
            base = [vid(a, b, jy + s * dj) for a, b, dj in ring_pts]  # (mirrored: the left one the right's image)
            ring_mid = sum((verts[b_] for b_ in base), Vector()) / len(base)
            for n_ in range(len(base)):
                m3 = (verts[base[n_]] + verts[base[(n_ + 1) % len(base)]] + verts[t_i]) / 3.0
                tri(base[n_], base[(n_ + 1) % len(base)], t_i, m3 - ring_mid, "rivet")
                rivet_mid.setdefault(base[n_], (-jy * sc[1], tip_u))
    h.say("rivets r %.4f (%.2fh) at +-%.3f across (%.2f of the window's width): %s"
          % (rr, RIVET_R, rj * sc[1], rj * sc[1] / (2 * wwin), "; ".join(rivet_info)))

    # Checks: nothing of the gun in front of the glass; every vertex inside the model file's grid (else its scale moves).
    for fy in (-0.95, -0.5, 0.0, 0.5, 0.95):
        for fu in (-0.95, -0.5, 0.0, 0.5, 0.95):
            o = glass + right * (fy * wwin) + up * (fu * hgt / 2)
            hit = bvh.ray_cast(o, out, 3.0)
            if hit[0] is not None:
                h.fail("the gun is in front of the glass at (%.2f, %.2f): %.3f out" % (fy, fu, hit[3]))
    for co in verts:
        if any(co[i] < og[i] - 1e-4 or co[i] > hi_grid[i] + 1e-4 for i in range(3)):
            h.fail("a vertex (%.3f %.3f %.3f) outside the model's grid %s .. %s" % (*co, og, hi_grid))

    # Texels.
    plate_map = FaceMap(h, body, bvh, SRC_PLATE, sw, sh)
    bevel_map = FaceMap(h, body, bvh, SRC_BEVEL, sw, sh)
    dark_map = FaceMap(h, body, bvh, SRC_DARK, sw, sh)
    rivet_map = FaceMap(h, body, bvh, SRC_RIVET, sw, sh)
    fwd = Vector((1.0, 0.0, 0.0))
    zup = Vector((0.0, 0.0, 1.0))
    yin = Vector((0.0, 1.0, 0.0))

    def tex(vi, mode, nrm):
        yy, uu, dd = uvd(verts[vi])
        if mode == "rivet":
            my, mu = rivet_mid[vi]
            if vi in tips:
                return rivet_map.at(rivet_map.hit)
            dy, du = yy - my, uu - mu
            dist = max(math.hypot(dy, du), 1e-6)
            return rivet_map.at(rivet_map.hit + right * (RIVET_TEX_R * dy / dist) + zup * (RIVET_TEX_R * du / dist))
        if mode == "inner":
            # (the walls' long way along the dark face's x, their depth down it)
            along = yy if abs(nrm.dot(up)) > 0.5 else uu
            return dark_map.at(dark_map.hit + fwd * along - zup * (-dd))
        if mode in ("bevel", "wall"):
            # The receiver's back beside the plate: the bevel's long way up it, its width (from the outline, folded
            # back at its true length) across it.
            if abs(nrm.dot(up)) > abs(nrm.dot(right)):
                along, edge = yy, abs(uu) - (o_top_u if uu > 0 else -o_bot_u)
            else:
                along, edge = uu, abs(yy) - oy * sc[1]
            fold = math.hypot(max(edge, 0.0), dd)
            return bevel_map.at(bevel_map.hit + zup * along + yin * fold)
        # The plate: the receiver's top, the plate's up along the gun.
        return plate_map.at(plate_map.hit + right * yy + fwd * uu)

    built = []
    for i_, j_, k_, want_n, mode in tris:
        p0, p1, p2 = verts[i_], verts[j_], verts[k_]
        nrm = (p1 - p0).cross(p2 - p0)
        if nrm.length < 1e-9:
            h.fail("a degenerate triangle (%d %d %d, %s)" % (i_, j_, k_, mode))
        ijk = (i_, j_, k_) if nrm.dot(want_n) >= 0.0 else (i_, k_, j_)
        nrm = nrm if nrm.dot(want_n) >= 0.0 else -nrm
        built.append(ijk + (tuple((tx / sw, 1.0 - ty / sh) for tx, ty in
                                  (tex(vi, mode, nrm.normalized()) for vi in ijk)),))

    lo = [min(c[i] for c in verts) for i in range(3)]
    hi = [max(c[i] for c in verts) for i in range(3)]
    h.say("plate %.4f wide x %.4f tall (%.2fh x %.2fh); bevel %.3f above (%.0f deg), %.3f below (%.0f deg), %.3f at the "
          "sides, %.3f back; walls %d steps back; texels a unit: plate %.1f, bevel %.1f, dark %.1f; the box x %.3f..%.3f "
          "y %.3f..%.3f z %.3f..%.3f"
          % (2 * oy * sc[1], o_top_u - o_bot_u, 2 * oy * sc[1] / hgt, (o_top_u - o_bot_u) / hgt, bev[1][2],
             math.degrees(math.atan2(-bev[1][3], bev[1][2])), bev[-1][2], math.degrees(math.atan2(-bev[-1][3], bev[-1][2])),
             BEVEL_Y * sc[1], -bev[1][3], steps, plate_map.density(), bevel_map.density(), dark_map.density(),
             lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))

    moved = h.apply(me, verts, built, smooth=False)
    h.finish("%d vertices, %d triangles (%s; opening %.2f x %.2f px, radius %.2f px)"
             % (len(verts), len(tris), "moved" if moved else "new", ow, oh, orad))


if __name__ == "__main__":
    if "Body" not in bpy.data.objects or "Barrels" not in bpy.data.objects:
        print("housing_%s: REFUSED: not widowmaker.blend (no Body and Barrels)" % STEM)
        sys.exit(1)
    Run(STEM, MODEL).main(main)
