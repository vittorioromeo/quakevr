#!/usr/bin/env python3
# make_screen_housing_rocket.py -- the rocket launcher's ammo screen housing, modelled on v_rock2.blend (ROUND24.md, "The
# rocket launcher's ammo screen housing" and its second pass, "less steep, softer"; his mockup, kit
# scratch/mockups/rocket_housing.webp: the screen in a raised bronze enclosure on the sloped shoulder where the tube
# steps down to the rear exhaust, near the grip, two round rivets each side of the window, made symmetric).
#
# Usage: blender -b --factory-startup Misc/quakevr/src_models/blend/v_rock2/v_rock2.blend
#            --python Misc/quakevr/make_screen_housing_rocket.py -- [--save other.blend] [--dry]
# Builds or moves the housing, exports the model as blend_export.py does (quakevr/progs, weapons.txt) and saves the
# .blend (in place, or as --save) as the export left it. Ends with "housing_rock2: ok, ..." or "housing_rock2: REFUSED:
# ..." (exit code 1). --dry: builds and checks it, exports and saves nothing. The housing scripts work alike
# (housing_common.py; docs/vr-port/MODELS_IN_BLENDER.md, "Tweaking the ammo screen housings").
#
# One definition: the window's half size (`opening`, font pixels), the glass's corner `radius`, `size` and `offset` are
# the "rock2" entry of Quake/vr/vr_screenstyle.cpp (the table the engine shapes and places the glass by), and the
# glass's lean is weapons.txt slot 6's wpntxt_pitch (51.26: the face's). The script refuses when the window it builds or
# the glass's place disagree with the table (by more than 0.3 font pixels, or 0.01 units), printing the entry that
# matches.
#
# The grid. The model's vertices are bytes: X 0.393 a step (coarse), Y 0.085, Z 0.105. The face leans 38.7 degrees
# from level: one X step and three Z steps make a "row" up it (0.504), and every grid point on the face is on a row.
# Off the face the grid points come in "levels" one Z step apart (0.082 out, 0.066 up the face each). A grid plane at
# a lean is never mirror-symmetric top to bottom, so the face is built on rows (exact), and what can't be is said.
# The shape (u up the face, d out of it towards the eye, y across; the window's height h = 4 rows = 2.016):
# - The window: rows -2..+2 (1h), 24 Y steps wide (1.01h): square; its walls back to a dark floor; the glass GLASS_IN h
#   behind the front, rounded by the shader (the table's radius), the near-black box behind it showing in its corners.
# - The front: the two side bands, flat (8 Y steps), each holding two rivets; their outer corners cut across a row.
# - Softened edges: from the front's outline (the window's top and bottom edges, the bands' cut corners) a chamfer
#   all round out and back to a foot 4 levels behind the face (0.33 back, 0.25 out above and below, 3 Y steps out at
#   the sides), whose outline seen face on is a rounded rectangle (corner radius FOOT_R, ARC_N points between): each
#   foot point the grid point nearest its place on that outline (within DEPTHS back: the corners' points sit a
#   little deeper above, a little shallower below, the grid's lean). Then the enclosure's walls straight back from the
#   foot (one X step, five Z steps: 2 degrees off square) into the gun, closed there (inside the gun).
# - 4 round rivets, two each side at +-0.25h (on rows +-1: identical, the lower pair the upper moved two rows down;
#   right and left mirror images), in the side bands' middles: a cone from a point one level in front of the band to a
#   ring of grid points sunk behind it, the ring's points searched so that the cone comes through the band on a circle
#   of RIVET_R h (printed: width / height and the worst point).
# Texture, all from the gun's own skin: the bezel and the enclosure's walls in the shoulder's own texels (the pyramid's
# island, 289..327 x 14..59) at the shoulder's own scale: the front as the shoulder under it maps, seen face on; the
# chamfers and the walls unfolded outward from the front's edge, each at its true length (no stretched texels); the
# window's walls and floor on the dark row (62); the rivets' points on the shoulder's lit streak, their rings on its
# darker bronze.

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

# Measured with vr_debug_screen_where 1 (weapons.txt slot 6's wpntxt offsets and pitch 51.26, the table's offset 0):
# the glass's centre, right and up, and a font pixel (wpntxt_scale 0.25, size 1) in model units.
BASE_CENTRE = Vector((0.6940, -0.0511, 7.4683))
BASE_RIGHT = Vector((0.0068, -1.0, 0.0055)).normalized()
BASE_UP = Vector((0.7800, 0.0087, 0.6258)).normalized()
PIXEL1 = 0.06565
MPU = PIXEL1 / (0.1 * 0.25)  # model units a world unit (the offset's unit)

# The design, in grid steps (X, Y, Z) from the window's centre on the face (a grid point; Y the gun's middle line).
CENTRE = (50, 129, 194)
ROW = (1, 3)          # one row up the face
BACK = (1, -5)        # the walls' step back into the gun (0.656; 0.022 down the face)
WIN_ROWS = 2          # the window's half height, rows
WIN_Y = 12            # its half width, Y steps
BAND_Y = 21          # the side bands' outer edge
CUT_Y = 3             # the bands' outer corners cut this far across (and one row up)
FOOT_Y = 24          # the foot's sides
FOOT_LEVEL = -4       # the foot above and below: this many levels from the row past the window's edge
FOOT_R = 0.45         # the foot outline's corner radius (units, face on)
ARC_N = 2             # points between a corner's ends
DEPTHS = (-0.55, -0.15)  # how far back a foot point may sit (units)
GLASS_IN = 0.02       # the glass behind the face, in window heights
RIVET_Y = 16          # the rivets' middles, Y steps out
RIVET_ROW = 1         # their rows (+-)
RIVET_R = 0.08        # their radius where they come through the band, in window heights
RIVET_ANGLES = 8      # points round each rivet's ring
# Skin places (texels).
INNER_ROWS = (62.5, 64.5)                   # the window's walls (front edge, back) on the dark row
INNER_COLS = (292.0, 330.0)
RIVET_TEX = (297.5, 33.5, 2.5)              # the lit streak (the point) and the ring's radius round it
SHOULDER_POINT = Vector((-0.8935, -0.0402, 4.7805))   # the shoulder pyramid's point (behind the rim, on the middle)
TUBE_TOP = 7.8293                           # the tube's top line (for the printout)


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
    out = Vector((-up.z, 0.0, up.x))          # square to the face, towards the eye
    right = Vector((0.0, -1.0, 0.0))

    def uvd(co):
        """(across to the right, up, out) of a point from the window's centre, model units."""
        rel = co - centre
        return rel.dot(right), rel.dot(up), rel.dot(out)

    def ab_uvd(a, b):
        return uvd(P(a, b))[1:]

    rows = lambda r: (ROW[0] * r, ROW[1] * r)  # noqa: E731
    hgt = 2 * WIN_ROWS * row.length
    wwin = WIN_Y * sc[1]
    glass = centre - out * (GLASS_IN * hgt)

    # The table against the window: the glass must be the window's size and sit in it.
    off_px = (abs(wwin - ow * px) / px, abs(hgt / 2 - oh * px) / px, abs(0.15 * hgt - orad * px) / px)
    out_base = BASE_RIGHT.cross(BASE_UP).normalized()
    have = BASE_CENTRE + (BASE_RIGHT * offset[0] + BASE_UP * offset[1] + out_base * offset[2]) * MPU
    dg = glass - BASE_CENTRE
    lean = math.degrees(math.atan2(row.z, row.x))
    h.say("font pixel %.5f; face %.2f degrees from level (the glass's %.2f); window %d Y steps x %d rows = %.4f x %.4f "
          "(%.2f x %.2f px, h %.4f), rows %.4f apart; glass %.4f behind the face at %.4f %.4f %.4f"
          % (px, lean, math.degrees(math.atan2(BASE_UP.z, BASE_UP.x)), 2 * WIN_Y, 2 * WIN_ROWS, 2 * wwin, hgt, 2 * wwin / px,
             hgt / px, hgt, row.length, GLASS_IN * hgt, *glass))
    if max(off_px) > 0.3 or (glass - have).length > 0.01:
        h.fail("the table disagrees with the window (off %.2f %.2f %.2f px, the glass %.4f units off); a matching "
               "entry: .radius = %.2ff, .opening = {%.2ff, %.2ff}, .offset = {%.4ff, %.4ff, %.4ff}"
               % (off_px + ((glass - have).length, 0.15 * hgt / px, wwin / px, hgt / 2 / px,
                            dg.dot(BASE_RIGHT) / MPU, dg.dot(BASE_UP) / MPU, dg.dot(out_base) / MPU)))
    if abs(BASE_UP.angle(up)) > math.radians(0.6):
        h.fail("the glass leans %.2f degrees off the face: weapons.txt slot 6 wpntxt_pitch %.2f"
               % (math.degrees(BASE_UP.angle(up)), 90 - lean))

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
    gun_vs = [v.co.copy() for v in me.vertices]
    gun_tris = [list(p.vertices) for p in me.polygons if not any(flags[i] > 0 for i in p.vertices)]
    bvh = BVHTree.FromPolygons(gun_vs, gun_tris)
    uvl = me.uv_layers.active.data

    # The shoulder's pyramid: its two upper faces' texels as seen face on (an affine fit).
    apex = min(range(len(gun_vs)), key=lambda i: (gun_vs[i] - SHOULDER_POINT).length)
    if (gun_vs[apex] - SHOULDER_POINT).length > 1e-3:
        h.fail("the shoulder's point (%.4f %.4f %.4f) wasn't found" % tuple(SHOULDER_POINT))
    fit = {}
    for p in me.polygons:
        if apex in p.vertices and p.normal.z > 0.3 and not any(flags[i] > 0 for i in p.vertices):
            for vi, li in zip(p.vertices, p.loop_indices):
                y_, u_, _ = uvd(me.vertices[vi].co)
                fit[vi] = (y_, u_, uvl[li].uv[0] * sw, (1.0 - uvl[li].uv[1]) * sh)
    fit = list(fit.values())
    if len(fit) != 4:
        h.fail("the shoulder's two upper faces weren't found (%d corners)" % len(fit))
    import numpy as np
    m = np.array([[f[0], f[1], 1.0] for f in fit])
    cs = np.linalg.lstsq(m, np.array([f[2] for f in fit]), rcond=None)[0]
    ct = np.linalg.lstsq(m, np.array([f[3] for f in fit]), rcond=None)[0]
    resid = max(math.hypot(cs @ (f[0], f[1], 1) - f[2], ct @ (f[0], f[1], 1) - f[3]) for f in fit)

    def shoulder_tex(yy, uu):
        return float(cs @ (yy, uu, 1.0)), float(ct @ (yy, uu, 1.0))

    verts, tris, index = [], [], {}
    unf = {}   # vertex -> its place unfolded onto the face (across, up), for the texels

    def vid(a, b, j):
        key = (a, b, j)
        if key not in index:
            index[key] = len(verts)
            verts.append(P(a, b, j))
        return index[key]

    def tri(a, b, c, want, mode):
        tris.append((a, b, c, want, mode))

    def quad(a, b, c, e, want, mode):
        tri(a, b, c, want, mode)
        tri(a, c, e, want, mode)

    # The front's outline (on the face, d 0), counter-clockwise seen face on (from the left middle, down first):
    # (a, b, j) of each point. Y steps go left (+j), so "right" is -j.
    r2, r1 = rows(WIN_ROWS), rows(WIN_ROWS - 1)
    m1, m2 = (-r1[0], -r1[1]), (-r2[0], -r2[1])
    crest = [(r1, BAND_Y), (m1, BAND_Y), (m2, BAND_Y - CUT_Y), (m2, WIN_Y),        # left side down, bottom left
             (m2, -WIN_Y), (m2, -(BAND_Y - CUT_Y)), (m1, -BAND_Y), (r1, -BAND_Y),  # bottom right, right side up
             (r2, -(BAND_Y - CUT_Y)), (r2, -WIN_Y), (r2, WIN_Y), (r2, BAND_Y - CUT_Y)]  # the top, right to left
    crest_ids = [vid(ab[0], ab[1], j) for ab, j in crest]

    # The foot: a rounded rectangle face on; each point the grid point nearest its place, DEPTHS back.
    top_u = ab_uvd(r2[0] + ROW[0], r2[1] + ROW[1] + FOOT_LEVEL)
    bot_u = ab_uvd(-r2[0], -r2[1] + FOOT_LEVEL)
    cands = []
    for a in range(-6, 8):
        for b in range(-30, 31):
            u_, d_ = ab_uvd(a, b)
            if DEPTHS[0] <= d_ <= DEPTHS[1]:
                cands.append((u_, d_, a, b))
    wfoot = FOOT_Y * sc[1]
    targets = []   # (across to the right, up), counter-clockwise from the left middle going down
    for quarter in range(4):
        # corners: bottom-left, bottom-right, top-right, top-left
        sy_, su = ((-1, -1), (1, -1), (1, 1), (-1, 1))[quarter]
        edge_u = top_u[0] if su > 0 else bot_u[0]
        cy, cu = sy_ * (wfoot - FOOT_R), edge_u - su * FOOT_R
        for k in range(ARC_N + 2):
            # from the side's end to the top/bottom's end (for the left-bottom: from the left side down to the bottom)
            ph = math.pi / 2 * k / (ARC_N + 1)
            if quarter in (0, 2):
                ang = ph
            else:
                ang = math.pi / 2 - ph
            targets.append((cy + sy_ * FOOT_R * math.cos(ang), cu + su * FOOT_R * math.sin(ang)))
    foot = []
    worst_foot = 0.0
    for ty, tu in targets:
        jj = round(-ty / sc[1])
        u_, d_, a, b = min(cands, key=lambda c: (round(abs(c[0] - tu), 3), abs(c[1] + 0.33)))
        worst_foot = max(worst_foot, abs(u_ - tu))
        foot.append((a, b, jj))
    foot_ids = [vid(a, b, j) for a, b, j in foot]

    def ang2(vi):
        y_, u_, _ = uvd(verts[vi])
        return math.atan2(u_ / (top_u[0] - bot_u[0]), y_ / (2 * wfoot))

    # The side bands (flat), the window's walls and floor.
    for s in (1, -1):
        band = [vid(*r2, s * WIN_Y), vid(*r2, s * (BAND_Y - CUT_Y)), vid(*r1, s * BAND_Y),
                vid(-r1[0], -r1[1], s * BAND_Y), vid(-r2[0], -r2[1], s * (BAND_Y - CUT_Y)),
                vid(-r2[0], -r2[1], s * WIN_Y)]
        for n_ in range(1, len(band) - 1):
            tri(band[0], band[n_], band[n_ + 1], out, "front")
    win = {}
    for su in (1, -1):
        for s in (1, -1):
            ab = r2 if su > 0 else (-r2[0], -r2[1])
            win[(su, s)] = vid(ab[0], ab[1], s * WIN_Y)
            win[(su, s, "f")] = vid(ab[0] + BACK[0], ab[1] + BACK[1], s * WIN_Y)
    quad(win[(1, 1)], win[(1, -1)], win[(1, -1, "f")], win[(1, 1, "f")], -up, "inner")
    quad(win[(-1, 1)], win[(-1, -1)], win[(-1, -1, "f")], win[(-1, 1, "f")], up, "inner")
    for s in (1, -1):
        lat = right * -s
        quad(win[(1, s)], win[(-1, s)], win[(-1, s, "f")], win[(1, s, "f")], -lat, "inner")
    quad(win[(1, 1, "f")], win[(1, -1, "f")], win[(-1, -1, "f")], win[(-1, 1, "f")], out, "inner")

    # The chamfer: the ring between the front's outline and the foot, walked round by angle.
    def unwrap(ids):
        start = min(range(len(ids)), key=lambda i: ang2(ids[i]))
        seq = ids[start:] + ids[:start]
        angs, prev = [], None
        for v in seq:
            a_ = ang2(v)
            while prev is not None and a_ < prev:
                a_ += 2 * math.pi
            angs.append(a_)
            prev = a_
        return seq, angs

    cseq, cang = unwrap(crest_ids)
    fseq, fang = unwrap(foot_ids)
    cseq.append(cseq[0])
    cang.append(cang[0] + 2 * math.pi)
    fseq.append(fseq[0])
    fang.append(fang[0] + 2 * math.pi)
    i = k = 0
    while i < len(cseq) - 1 or k < len(fseq) - 1:
        if k >= len(fseq) - 1 or (i < len(cseq) - 1 and cang[i + 1] <= fang[k + 1]):
            tri(cseq[i], cseq[i + 1], fseq[k], None, "front")
            i += 1
        else:
            tri(cseq[i], fseq[k + 1], fseq[k], None, "front")
            k += 1

    # The enclosure's walls: the foot moved straight back into the gun, as few steps as put it all inside.
    def inside(co):
        hits = 0
        for dirv in (Vector((0.0, 0.0, 1.0)), Vector((0.31, 0.2, 0.93)).normalized(),
                     Vector((-0.3, -0.25, 0.92)).normalized()):
            loc, nrm, _, _ = bvh.ray_cast(co, dirv, 50.0)
            hits += 1 if loc is not None and nrm.dot(dirv) > 0 else 0
        return hits >= 2

    steps = None
    for kk in range(1, 6):
        if all(inside(P(a + kk * BACK[0], b + kk * BACK[1], j)) for a, b, j in foot):
            steps = kk
            break
    if steps is None:
        h.fail("the enclosure's walls don't reach inside the gun in 5 steps")
    back_ids = [vid(a + steps * BACK[0], b + steps * BACK[1], j) for a, b, j in foot]
    nf = len(foot_ids)
    for n_ in range(nf):
        a_, b_ = foot_ids[n_], foot_ids[(n_ + 1) % nf]
        quad(a_, b_, back_ids[(n_ + 1) % nf], back_ids[n_], None, "wall")
    for n_ in range(1, nf - 1):
        tri(back_ids[0], back_ids[n_], back_ids[n_ + 1], -out, "under")

    # The rivets: per rivet a point one level in front of the band and a ring of grid points behind it, chosen so
    # that the cone meets the band's plane (d 0) on a circle of RIVET_R h round its middle (the row).
    rr = RIVET_R * hgt
    tip_ab = (ROW[0] * RIVET_ROW, ROW[1] * RIVET_ROW + 1)
    tip_u, tip_d = ab_uvd(*tip_ab)
    mid_u = RIVET_ROW * row.length
    rcands = []
    for a in range(-3, 5):
        for b in range(-20, 21):
            qu, qd = ab_uvd(a, b)
            if not -0.7 < qd < -0.02:
                continue
            t = tip_d / (tip_d - qd)
            for dj in range(-8, 9):
                if WIN_Y - 2 <= RIVET_Y + dj <= BAND_Y - 1:
                    rcands.append((t * (-dj * sc[1]), tip_u + t * (qu - tip_u), a, b, dj))
    best_ring = None
    for rot in range(0, 45, 3):
        ring, seen, worst = [], [], 0.0
        for k_ in range(RIVET_ANGLES):
            th = math.radians(90 - rot) - 2 * math.pi * k_ / RIVET_ANGLES
            want = (rr * math.cos(th), mid_u + rr * math.sin(th))
            hy, hu, a, b, dj = min(rcands, key=lambda c: math.hypot(c[0] - want[0], c[1] - want[1]))
            worst = max(worst, math.hypot(hy - want[0], hu - want[1]))
            ring.append((a, b, dj))
            seen.append((hy, hu))
        if best_ring is None or worst < best_ring[0]:
            best_ring = (worst, rot, ring, seen)
    worst, rot, ring, seen = best_ring
    ys = [p_[0] for p_ in seen]
    us = [p_[1] for p_ in seen]
    ratio = (max(ys) - min(ys)) / (max(us) - min(us))
    cen_u = (max(us) + min(us)) / 2
    tips = []
    for s in (1, -1):
        for rsign in (1, -1):
            shift = (0, 0) if rsign > 0 else rows(-2 * RIVET_ROW)
            t_i = vid(tip_ab[0] + shift[0], tip_ab[1] + shift[1], s * RIVET_Y)
            tips.append(t_i)
            base = [vid(a + shift[0], b + shift[1], s * (RIVET_Y + dj)) for a, b, dj in ring]
            ring_mid = sum((verts[b_] for b_ in base), Vector()) / len(base)
            for n_ in range(len(base)):
                mid = (verts[base[n_]] + verts[base[(n_ + 1) % len(base)]] + verts[t_i]) / 3.0
                tri(base[n_], base[(n_ + 1) % len(base)], t_i, mid - ring_mid, "rivet")
    h.say("rivets: r %.4f (%.3fh), point %.4f in front, %.4f %s the middle; through the band: width/height %.3f, "
          "middle %.4f off, worst point %.4f off the circle (ring turned %d deg)" % (
              rr, RIVET_R, tip_d, abs(tip_u - mid_u), "below" if tip_u < mid_u else "above", ratio, cen_u - mid_u,
              worst, rot))
    if not 0.9 <= ratio <= 1.1:
        h.fail("the rivets aren't round (width / height %.3f)" % ratio)

    # Checks: nothing of the gun in front of the glass (rays out from it).
    for fy in (-0.95, -0.5, 0.0, 0.5, 0.95):
        for fu in (-0.95, -0.5, 0.0, 0.5, 0.95):
            o = glass + right * (fy * wwin) + up * (fu * hgt / 2)
            hit = bvh.ray_cast(o, out, 3.0)
            if hit[0] is not None:
                h.fail("the gun is in front of the glass at (%.2f, %.2f): %.3f out" % (fy, fu, hit[3]))

    # Texels: each vertex's place unfolded onto the face (the chamfer and the walls laid out flat beyond the
    # front's outline, at their true lengths).
    crest2 = [uvd(verts[v])[:2] for v in crest_ids]

    def nearest_on_crest(p2):
        best = None
        for n_ in range(len(crest2)):
            a2, b2 = Vector(crest2[n_]), Vector(crest2[(n_ + 1) % len(crest2)])
            ab_ = b2 - a2
            t = max(0.0, min(1.0, (p2 - a2).dot(ab_) / ab_.length_squared))
            q = a2 + ab_ * t
            if best is None or (p2 - q).length < best[0]:
                best = ((p2 - q).length, q)
        return best[1]

    for n_, fv in enumerate(foot_ids):
        y_, u_, d_ = uvd(verts[fv])
        p2 = Vector((y_, u_))
        q = nearest_on_crest(p2)
        dirn = (p2 - q).normalized()
        flat = (p2 - q).length
        unf[fv] = q + dirn * math.hypot(flat, d_)
        bv = back_ids[n_]
        unf[bv] = unf[fv] + dirn * (verts[bv] - verts[fv]).length

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
                for rsign in (1, -1):
                    cy = -s * RIVET_Y * sc[1]
                    cu = rsign * mid_u
                    dist = math.hypot(yy - cy, uu - cu)
                    if best is None or dist < best[0]:
                        best = (dist, yy - cy, uu - cu)
            dist, dy, du = best
            if vi in tips:
                return RIVET_TEX[0], RIVET_TEX[1]
            return (RIVET_TEX[0] + RIVET_TEX[2] * dy / max(dist, 1e-6),
                    RIVET_TEX[1] - RIVET_TEX[2] * du / max(dist, 1e-6))
        if vi in unf:
            return shoulder_tex(*unf[vi])
        return shoulder_tex(yy, uu)

    built = []
    for i_, j_, k_, want, mode in tris:
        p0, p1, p2 = verts[i_], verts[j_], verts[k_]
        nrm = (p1 - p0).cross(p2 - p0)
        if nrm.length < 1e-9:
            h.fail("a degenerate triangle (%d %d %d, %s)" % (i_, j_, k_, mode))
        if want is None:   # the chamfer and the walls: out of the face's middle
            mid = (p0 + p1 + p2) / 3.0
            y_, u_, _ = uvd(mid)
            want = right * y_ / wfoot + up * u_ / (top_u[0] - bot_u[0]) * 2 + (out * 0.5 if mode == "front" else
                                                                             Vector())
        ijk = (i_, j_, k_) if nrm.dot(want) >= 0.0 else (i_, k_, j_)
        nrm = nrm if nrm.dot(want) >= 0.0 else -nrm
        built.append(ijk + (tuple(((tx / sw), 1.0 - ty / sh) for tx, ty in
                                  (tex(vi, mode, nrm.normalized()) for vi in ijk)),))

    lo = [min(c[i] for c in verts) for i in range(3)]
    hi = [max(c[i] for c in verts) for i in range(3)]
    fu_ = [uvd(verts[v])[1] for v in foot_ids]
    fd_ = [uvd(verts[v])[2] for v in foot_ids]
    h.say("plate (the foot) %.4f wide (%.2fh), %.4f tall up the face (%.2fh, top %.4f / bottom %.4f: %.4f off "
          "symmetric); foot points %.4f..%.4f back, worst %.4f off the outline; walls %d steps back; the highest "
          "point %.3f above the tube's top line; the box x %.3f..%.3f y %.3f..%.3f z %.3f..%.3f; shoulder fit %.2f "
          "texels" % (2 * wfoot, 2 * wfoot / hgt, max(fu_) - min(fu_), (max(fu_) - min(fu_)) / hgt, max(fu_), min(fu_),
                      max(fu_) + min(fu_), -max(fd_), -min(fd_), worst_foot, steps, hi[2] - TUBE_TOP, lo[0], hi[0],
                      lo[1], hi[1], lo[2], hi[2], resid))

    moved = h.apply(me, verts, built, smooth=True)
    if not moved:
        h.export()  # (the housing's vertices are now the file's)
    _, mdl_blender = hc.addon()
    mdl_blender.carry_frames(ob)
    h.finish("%d vertices, %d triangles (%s; opening %.2f x %.2f px, radius %.2f px)"
             % (len(verts), len(tris), "moved" if moved else "new", ow, oh, orad))


if __name__ == "__main__":
    hc.Run(STEM, MODEL).main(main)
