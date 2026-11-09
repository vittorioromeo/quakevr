#!/usr/bin/env python3
# make_screen_housing_rocket.py -- the rocket launcher's ammo screen housing, modelled on v_rock2.blend (ROUND24.md, "The
# rocket launcher's ammo screen housing"; his mockup, kit scratch/mockups/rocket_housing.webp: the screen in a raised
# bronze enclosure on the sloped shoulder where the tube steps down to the rear exhaust, near the grip, two round
# rivets each side of the window, made symmetric).
#
# Usage: blender -b --factory-startup Misc/quakevr/src_models/blend/v_rock2/v_rock2.blend
#            --python Misc/quakevr/make_screen_housing_rocket.py -- [--save other.blend] [--dry]
# Builds or moves the housing, exports the model as blend_export.py does (quakevr/progs, weapons.txt) and saves the
# .blend (in place, or as --save) as the export left it. Ends with "housing_rock2: ok, ..." or "housing_rock2: REFUSED:
# ..." (exit code 1). --dry: builds and checks it, exports and saves nothing. The housing scripts work alike
# (housing_common.py; docs/vr-port/MODELS_IN_BLENDER.md, "Tweaking the ammo screen housings").
#
# One definition: the window's half size (`opening`, font pixels), the glass's corner `radius`, `size` and `offset` are
# the "rock2" entry of Quake/vr/vr_screenstyle.cpp (the table the engine shapes and places the glass by). The script
# refuses when the window it builds or the glass's place disagree with the table (by more than 0.3 font pixels, or
# 0.01 units), printing the entry that matches.
#
# The grid. The model's vertices are bytes: X 0.393 a step (coarse), Y 0.085, Z 0.105. The glass leans back 28 degrees
# (weapons.txt slot 6's wpntxt_pitch), and a plane at that lean holds grid points only on "rows" 0.834 apart up the
# plane (one X step and seven Z steps: 61.9 degrees from level, the glass's 62.0); one X step forward and two Z steps
# down is square to it (0.446 deep). A grid plane at a lean is never mirror-symmetric top to bottom, so every vertex
# of the front is chosen on that row lattice (exactly symmetric), and the few that can't be are said below.
# The shape (u up the glass, d out of it towards the eye, y across; in the window's height h = 2 rows = 1.669):
# - The window: rows -1..+1 (1h), 24 Y steps wide (1.22h, the digits at today's size: "4/4 / 96" fits unscaled), its
#   walls straight back square to the glass (0.27h) to a dark floor; the glass GLASS_IN h behind the front, its corners
#   rounded by the shader (the table's radius), the near-black box behind it showing in them.
# - The bezel's front: the two side bands, flat (the window's height, 6 Y steps wide), each holding two rivets; above
#   and below the window an angled bevel from the window's edge out and back to the plate's edge (0.28h out; 0.12h
#   back above, 0.15h below: the one top/bottom difference the grid forces, the outline itself is symmetric); the
#   side bands' outer bevels (4 Y steps) meet them at mitred corners.
# - The enclosure: from the plate's top edge an upright back wall down into the tube, its sides upright, from the
#   bottom edge a skirt square to the glass into the shoulder; closed underneath (inside the gun).
# - 4 round rivets, two each side at +-0.25h (identical: the lower pair is the upper moved one row down, a grid move),
#   in the side bands' middles: a cone from a point RIVET_UP in front of the band to a ring sunk behind it, its
#   points picked on the grid so that where the cone comes through the band it is a circle of RIVET_R (printed:
#   width / height and the worst point).
# Texture, all from the gun's own skin: the bezel and the enclosure's walls in the shoulder's own texels (the pyramid's
# island, 289..327 x 14..59) at the shoulder's own scale: the front as the shoulder under it maps, seen face on, each
# wall unfolded from the front's edge it leaves (no stretched strips); the window's walls and floor on the dark row
# (62); the rivets' points on the shoulder's lit streak, their rings on its darker bronze.

import math
import os
import sys

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import housing_common as hc  # noqa: E402

STEM = "rock2"
MODEL = "v_rock2"

# Measured with vr_debug_screen_where 1 (weapons.txt slot 6's wpntxt offsets and pitch 28, the table's offset 0): the
# glass's centre, right and up, and a font pixel (wpntxt_scale 0.25, size 1) in model units.
BASE_CENTRE = Vector((0.8195, -0.0512, 7.5943))
BASE_RIGHT = Vector((0.0041, -1.0, 0.0077)).normalized()
BASE_UP = Vector((0.4694, 0.0087, 0.8829)).normalized()
PIXEL1 = 0.06565
MPU = PIXEL1 / (0.1 * 0.25)  # model units a world unit (the offset's unit)

# The design, in grid steps (X, Y, Z) from the window's centre on the front (a grid point; Y the gun's middle line).
CENTRE = (50, 129, 194)
ROW = (1, 7)          # one row up the front
SQUARE = (1, -2)      # one step square to the front, into the gun
WIN_Y = 12            # the window's half width, Y steps
BAND_Y = 18           # the side bands' outer edge
PLATE_Y = 22          # the plate's outer edge (the side bevels' foot)
TOP_OUT = (2, 10)     # the top bevel's foot (X, Z steps from the centre): row 2 less 4 Z steps
BOT_OUT = (-1, -12)   # the bottom bevel's foot: row -1 less 5 Z steps
BACK_FOOT = (2, -11)  # the back wall's foot (straight down from the top bevel's foot, inside the tube)
SKIRT_END = (1, -16)  # the bottom skirt's end (2 steps square to the front from the bottom bevel's foot)
GLASS_IN = 0.02       # the glass behind the front, in window heights
RIVET_Y = 15          # the rivets' middles, Y steps out
RIVET_R = 0.08        # their radius where they come through the band, in window heights
RIVET_TIP = (0, 4)    # the upper rivets' point (X, Z steps): 4 Z steps up from the centre row (0.198 in front)
RIVET_ANGLES = 8      # points round each rivet's ring
# Skin places (texels).
PYRAMID = (289.0, 327.0, 14.0, 59.0)        # the shoulder's island (the fit's sanity box)
INNER_ROWS = (62.5, 64.5)                   # the window's walls (front edge, back) on the dark row
INNER_COLS = (292.0, 330.0)
RIVET_TEX = (297.5, 33.5, 2.5)              # the lit streak (the point) and the ring's radius round it
SHOULDER_POINT = Vector((-0.8935, -0.0402, 4.7805))   # the shoulder pyramid's point (behind the rim, on the middle)


def main(h):
    st = h.style
    (ow, oh), orad, size, offset = st["opening"], st["radius"], st["size"], st["offset"]
    if ow <= 0 or orad <= 0:
        h.fail("the \"%s\" entry needs .opening and a rounded .radius (> 0)" % STEM)
    px = PIXEL1 * size
    sc, og = h.grid
    i0, j0, k0 = CENTRE

    def P(a, b, j=0):
        """The grid point a X steps, b Z steps and j Y steps (to the gun's left) from the centre."""
        return Vector((og[0] + sc[0] * (i0 + a), og[1] + sc[1] * (j0 + j), og[2] + sc[2] * (k0 + b)))

    centre = P(0, 0)
    row = P(*ROW) - centre
    up = row.normalized()
    out = Vector((-up.z, 0.0, up.x))          # square to the front, towards the eye
    right = Vector((0.0, -1.0, 0.0))

    def uvd(co):
        """(across to the right, up, out) of a point from the window's centre, model units."""
        rel = co - centre
        return rel.dot(right), rel.dot(up), rel.dot(out)

    hgt = 2 * row.length
    wwin = WIN_Y * sc[1]
    sq = P(*SQUARE) - centre
    if abs(sq.normalized().dot(up)) > 1e-3:
        h.fail("SQUARE isn't square to the rows")
    glass = centre - out * (GLASS_IN * hgt)

    # The table against the window: the glass must be the window's size and sit in it.
    off_px = (abs(wwin - ow * px) / px, abs(hgt / 2 - oh * px) / px, abs(0.15 * hgt - orad * px) / px)
    out_base = BASE_RIGHT.cross(BASE_UP).normalized()
    have = BASE_CENTRE + (BASE_RIGHT * offset[0] + BASE_UP * offset[1] + out_base * offset[2]) * MPU
    d = glass - BASE_CENTRE
    h.say("font pixel %.5f; window %d x 2 rows = %.4f x %.4f (%.2f x %.2f px, h %.4f), rows %.4f apart, square step "
          "%.4f; glass %.4f behind the front at %.4f %.4f %.4f; the glass's up %.2f degrees off the front's"
          % (px, 2 * WIN_Y, 2 * wwin, hgt, 2 * wwin / px, hgt / px, hgt, row.length, sq.length, GLASS_IN * hgt,
             *glass, math.degrees(BASE_UP.angle(up))))
    if max(off_px) > 0.3 or (glass - have).length > 0.01:
        h.fail("the table disagrees with the window (off %.2f %.2f %.2f px, the glass %.4f units off); a matching "
               "entry: .radius = %.2ff, .opening = {%.2ff, %.2ff}, .offset = {%.4ff, %.4ff, %.4ff}"
               % (off_px + ((glass - have).length, 0.15 * hgt / px, wwin / px, hgt / 2 / px,
                            d.dot(BASE_RIGHT) / MPU, d.dot(BASE_UP) / MPU, d.dot(out_base) / MPU)))

    ob = bpy.data.objects[MODEL]
    if ob.mode != 'OBJECT':
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='OBJECT')
    me = ob.data
    sw, sh = bpy.data.images.get(MODEL + "_skin0").size

    # The gun alone (not an old housing): its triangles, for the checks and the shoulder's texels.
    tag = me.attributes.get(hc.TAG)
    flags = [0] * len(me.vertices)
    if tag is not None:
        tag.data.foreach_get("value", flags)
    gun_tris, gun_vs = [], [v.co.copy() for v in me.vertices]
    uvl = me.uv_layers.active.data
    for p in me.polygons:
        if any(flags[i] > 0 for i in p.vertices):
            continue
        gun_tris.append(list(p.vertices))
    bvh = BVHTree.FromPolygons(gun_vs, gun_tris)

    # The shoulder's pyramid (four faces round a point behind the rim): its two upper faces' texels as seen face on.
    apex = min(range(len(gun_vs)), key=lambda i: (gun_vs[i] - SHOULDER_POINT).length)
    if (gun_vs[apex] - SHOULDER_POINT).length > 1e-3:
        h.fail("the shoulder's point (%.4f %.4f %.4f) wasn't found" % tuple(SHOULDER_POINT))
    fit = {}
    for p in me.polygons:
        if apex in p.vertices and p.normal.z > 0.3 and not any(flags[i] > 0 for i in p.vertices):
            for vi, li in zip(p.vertices, p.loop_indices):
                y_, u_, d_ = uvd(me.vertices[vi].co)
                fit[vi] = (y_, u_, uvl[li].uv[0] * sw, (1.0 - uvl[li].uv[1]) * sh)
    fit = list(fit.values())
    if len(fit) != 4:
        h.fail("the shoulder's two upper faces weren't found (%d corners)" % len(fit))
    # least squares: texel = a*y + b*u + c (each texel axis)
    import numpy as np
    m = np.array([[f[0], f[1], 1.0] for f in fit])
    cs = np.linalg.lstsq(m, np.array([f[2] for f in fit]), rcond=None)[0]
    ct = np.linalg.lstsq(m, np.array([f[3] for f in fit]), rcond=None)[0]
    resid = max(math.hypot(cs @ (f[0], f[1], 1) - f[2], ct @ (f[0], f[1], 1) - f[3]) for f in fit)

    def shoulder_tex(yy, uu):
        return float(cs @ (yy, uu, 1.0)), float(ct @ (yy, uu, 1.0))

    verts, tris = [], []   # tris: (i, j, k, want_normal, mode)

    def add(co):
        verts.append(co)
        return len(verts) - 1

    def pt(ab, j):
        return add(P(ab[0], ab[1], j))

    r1, rm1 = ROW, (-ROW[0], -ROW[1])
    # The front ring: per side s (+1 left = -right, -1 right), the crest points.
    V = {}
    for s in (1, -1):
        V[("wt", s)] = pt(r1, s * WIN_Y)       # window top corner
        V[("wb", s)] = pt(rm1, s * WIN_Y)
        V[("bt", s)] = pt(r1, s * BAND_Y)      # side band's outer top corner
        V[("bb", s)] = pt(rm1, s * BAND_Y)
        V[("ot", s)] = pt(TOP_OUT, s * PLATE_Y)   # the plate's corners (the bevels' feet)
        V[("ob", s)] = pt(BOT_OUT, s * PLATE_Y)
        V[("ft", s)] = pt((ROW[0] + SQUARE[0], ROW[1] + SQUARE[1]), s * WIN_Y)   # the window's floor
        V[("fb", s)] = pt((-ROW[0] + SQUARE[0], -ROW[1] + SQUARE[1]), s * WIN_Y)
        V[("kf", s)] = pt(BACK_FOOT, s * PLATE_Y)   # the back wall's foot
        V[("ke", s)] = pt(SKIRT_END, s * PLATE_Y)   # the skirt's end

    def tri(a, b, c, want, mode):
        tris.append((a, b, c, want, mode))

    def quad(a, b, c, e, want, mode):
        tri(a, b, c, want, mode)
        tri(a, c, e, want, mode)

    OUT, UPV = out, up
    for s in (1, -1):
        lat = right * -s  # (towards this side)
        # side band (flat front)
        quad(V[("wt", s)], V[("bt", s)], V[("bb", s)], V[("wb", s)], OUT, "front")
        # side bevel: crest (bt, bb) to the plate's edge (ot, ob)
        quad(V[("bt", s)], V[("ot", s)], V[("ob", s)], V[("bb", s)], OUT + lat, "front")
        # window side wall
        quad(V[("wt", s)], V[("wb", s)], V[("fb", s)], V[("ft", s)], -lat, "inner")
        # side wall of the enclosure: (ob, ot, kf, ke), convex
        tri(V[("ob", s)], V[("ot", s)], V[("kf", s)], lat, "side")
        tri(V[("ob", s)], V[("kf", s)], V[("ke", s)], lat, "side")
    # the top and bottom bevels: the crest (b+, w+, w-, b-) to the feet (o-, o+), mirror-symmetric triangles
    for c, w, o, want in (("bt", "wt", "ot", OUT + UPV), ("bb", "wb", "ob", OUT - UPV)):
        for s in (1, -1):
            tri(V[(c, s)], V[(w, s)], V[(o, s)], want, "front")
        quad(V[(w, 1)], V[(w, -1)], V[(o, -1)], V[(o, 1)], want, "front")
    # window top/bottom walls and floor
    quad(V[("wt", 1)], V[("wt", -1)], V[("ft", -1)], V[("ft", 1)], -UPV, "inner")
    quad(V[("wb", 1)], V[("wb", -1)], V[("fb", -1)], V[("fb", 1)], UPV, "inner")
    quad(V[("ft", 1)], V[("ft", -1)], V[("fb", -1)], V[("fb", 1)], OUT, "inner")
    # the enclosure: back wall (ot, kf), skirt (ob, ke), underside (ke, kf)
    fwd = Vector((1.0, 0.0, 0.0))
    quad(V[("ot", 1)], V[("ot", -1)], V[("kf", -1)], V[("kf", 1)], fwd, "back")
    quad(V[("ob", 1)], V[("ob", -1)], V[("ke", -1)], V[("ke", 1)], -UPV, "under")
    quad(V[("ke", 1)], V[("ke", -1)], V[("kf", -1)], V[("kf", 1)], Vector((0.0, 0.0, -1.0)), "under")

    # The rivets: per rivet the point and a ring of grid points behind the band, chosen so that the cone meets the
    # band's plane (d 0) on a circle of RIVET_R h round its middle.
    rr = RIVET_R * hgt
    tip_u, _, tip_d = None, None, None
    tip = P(*RIVET_TIP, RIVET_Y)
    _, tip_u, tip_d = uvd(tip)
    if tip_d <= 0.02:
        h.fail("the rivets' point isn't in front of the band")
    mid_u = row.length / 2
    # Where each candidate ring point (a grid point behind the band, under it) puts the cone's edge in the band's
    # plane: (across to the right from the rivet's middle, up), for the left rivet (Y steps go left).
    cands = []
    for a in range(-2, 4):
        for b in range(-24, 25):
            _, qu, qd = uvd(P(a, b))
            if not -0.6 < qd < -0.02 or abs(qu) > 0.79:
                continue
            t = tip_d / (tip_d - qd)
            for dj in range(-6, 7):
                if WIN_Y <= RIVET_Y + dj <= PLATE_Y - 1:
                    cands.append((t * (-dj * sc[1]), tip_u + t * (qu - tip_u), a, b, dj))
    best_ring = None
    for rot in range(0, 45, 3):   # the ring's angles turned, the roundest kept
        ring, seen, worst = [], [], 0.0
        for k in range(RIVET_ANGLES):
            th = math.radians(90 - rot) - 2 * math.pi * k / RIVET_ANGLES
            want = (rr * math.cos(th), mid_u + rr * math.sin(th))
            hy, hu, a, b, dj = min(cands, key=lambda c: math.hypot(c[0] - want[0], c[1] - want[1]))
            worst = max(worst, math.hypot(hy - want[0], hu - want[1]))
            ring.append((a, b, dj))
            seen.append((hy, hu))
        if best_ring is None or worst < best_ring[0]:
            best_ring = (worst, rot, ring, seen)
    worst, rot, ring, seen = best_ring
    ys = [p[0] for p in seen]
    us = [p[1] for p in seen]
    ratio = (max(ys) - min(ys)) / (max(us) - min(us))
    cen_u = (max(us) + min(us)) / 2
    for s in (1, -1):
        for drow in (0, -1):
            sh_ = (ROW[0] * drow, ROW[1] * drow)
            t_i = pt((RIVET_TIP[0] + sh_[0], RIVET_TIP[1] + sh_[1]), s * RIVET_Y)
            # (found for the left one, s 1; the right one its mirror image across the gun's middle)
            base = [pt((a + sh_[0], b + sh_[1]), s * (RIVET_Y + dj)) for a, b, dj in ring]
            ring_mid = sum((verts[b_] for b_ in base), Vector()) / len(base)
            for n_ in range(len(base)):
                mid = (verts[base[n_]] + verts[base[(n_ + 1) % len(base)]] + verts[t_i]) / 3.0
                tri(base[n_], base[(n_ + 1) % len(base)], t_i, mid - ring_mid, "rivet")
    h.say("rivets: r %.4f (%.3fh), point %.4f in front, %.4f %s the middle; ring %s; through the band: width/height "
          "%.3f, middle %.4f off, worst point %.4f off the circle (ring turned %d deg)" % (
              rr, RIVET_R, tip_d, abs(tip_u - mid_u), "below" if tip_u < mid_u else "above", ring, ratio,
              cen_u - mid_u, worst, rot))
    if not 0.9 <= ratio <= 1.1:
        h.fail("the rivets aren't round (width / height %.3f)" % ratio)

    # Checks: nothing of the gun in front of the glass (rays out from it), and the enclosure's hidden edges inside
    # the gun (rays from them hit the gun's inside: a back face).
    for fy in (-0.95, -0.5, 0.0, 0.5, 0.95):
        for fu in (-0.95, -0.5, 0.0, 0.5, 0.95):
            o = glass + right * (fy * wwin) + up * (fu * hgt / 2)
            hit = bvh.ray_cast(o, out, 3.0)
            if hit[0] is not None:
                h.fail("the gun is in front of the glass at (%.2f, %.2f): %.3f out" % (fy, fu, hit[3]))

    def inside(co):
        hits = 0
        for dirv in (Vector((0.0, 0.0, 1.0)), Vector((0.31, 0.2, 0.93)).normalized(), Vector((-0.3, -0.25, 0.92)).normalized()):
            loc, nrm, _, _ = bvh.ray_cast(co, dirv, 50.0)
            hits += 1 if loc is not None and nrm.dot(dirv) > 0 else 0
        return hits >= 2

    buried = [k for k in ("kf", "ke") for s in (1, -1) if not inside(verts[V[(k, s)]])]
    if buried:
        h.fail("the enclosure's feet %s aren't inside the gun" % buried)

    # Texture.
    lo_u, hi_u = uvd(verts[V[("ob", 1)]])[1], uvd(verts[V[("ot", 1)]])[1]
    plate_w = PLATE_Y * sc[1]
    d_top, d_bot = uvd(verts[V[("ot", 1)]])[2], uvd(verts[V[("ob", 1)]])[2]

    def tex(vi, mode, nrm):
        co = verts[vi]
        yy, uu, dd = uvd(co)
        if mode == "inner":
            along = (yy + wwin) / (2 * wwin) if abs(nrm.dot(up)) > 0.5 else (uu + hgt / 2) / hgt
            return (INNER_COLS[0] + (INNER_COLS[1] - INNER_COLS[0]) * along,
                    INNER_ROWS[0] if dd > -0.01 else INNER_ROWS[1])
        if mode == "rivet":
            best = None
            for s in (1, -1):
                for drow in (0, -1):
                    cy = -s * RIVET_Y * sc[1]
                    cu = mid_u + drow * row.length
                    dist = math.hypot(yy - cy, uu - cu)
                    if best is None or dist < best[0]:
                        best = (dist, yy - cy, uu - cu, dd)
            dist, dy, du, ddd = best
            if ddd > tip_d - 1e-3:
                return RIVET_TEX[0], RIVET_TEX[1]
            return (RIVET_TEX[0] + RIVET_TEX[2] * dy / max(dist, 1e-6),
                    RIVET_TEX[1] - RIVET_TEX[2] * du / max(dist, 1e-6))
        ny, nu, nd = nrm.dot(right), nrm.dot(up), nrm.dot(out)
        if abs(nd) >= max(abs(ny), abs(nu)) or mode == "back":
            return shoulder_tex(yy, uu)
        if abs(ny) >= abs(nu):   # a side wall: unfolded across from the side bevel's foot
            edge_d = d_top + (d_bot - d_top) * (uu - hi_u) / (lo_u - hi_u)
            return shoulder_tex(math.copysign(plate_w + (edge_d - dd), yy), uu)
        edge = (hi_u, d_top) if uu > 0 else (lo_u, d_bot)   # the skirt / underside: unfolded from the bevel's foot
        return shoulder_tex(yy, math.copysign(abs(edge[0]) + (edge[1] - dd), uu))

    built = []
    for i, j, k, want, mode in tris:
        p0, p1, p2 = verts[i], verts[j], verts[k]
        nrm = (p1 - p0).cross(p2 - p0)
        if nrm.length < 1e-9:
            h.fail("a degenerate triangle (%d %d %d, %s)" % (i, j, k, mode))
        ijk = (i, j, k) if nrm.dot(want) >= 0.0 else (i, k, j)
        nrm = nrm if nrm.dot(want) >= 0.0 else -nrm
        built.append(ijk + (tuple(((tx / sw), 1.0 - ty / sh) for tx, ty in
                                  (tex(vi, mode, nrm.normalized()) for vi in ijk)),))

    lo = [min(c[i] for c in verts) for i in range(3)]
    hi = [max(c[i] for c in verts) for i in range(3)]
    h.say("plate %.4f wide (%.2fh), %.4f tall up the glass (%.2fh): bevels %.4f out (%.2fh), %.4f / %.4f back; side "
          "bands %.4f (%.2fh), bevels %.4f; the box x %.3f..%.3f y %.3f..%.3f z %.3f..%.3f; shoulder fit %.2f texels"
          % (2 * plate_w, 2 * plate_w / hgt, hi_u - lo_u, (hi_u - lo_u) / hgt, hi_u - hgt / 2, (hi_u - hgt / 2) / hgt,
             -d_top, -d_bot, (BAND_Y - WIN_Y) * sc[1], (BAND_Y - WIN_Y) * sc[1] / hgt, (PLATE_Y - BAND_Y) * sc[1],
             lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], resid))

    moved = h.apply(me, verts, built, smooth=True)
    if not moved:
        h.export()  # (the housing's vertices are now the file's)
    _, mdl_blender = hc.addon()
    mdl_blender.carry_frames(ob)
    h.finish("%d vertices, %d triangles (%s; opening %.2f x %.2f px, radius %.2f px)"
             % (len(verts), len(tris), "moved" if moved else "new", ow, oh, orad))


if __name__ == "__main__":
    hc.Run(STEM, MODEL).main(main)
