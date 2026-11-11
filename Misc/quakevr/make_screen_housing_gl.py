#!/usr/bin/env python3
# make_screen_housing_gl.py -- the grenade launcher's ammo screen housing, modelled on v_rock.blend, and the same one on
# its multi-grenade model v_multi.blend (ROUND24.md, "The grenade launcher's ammo screen housing"; his mockup, kit
# scratch/gl_housing_mockup_1111.webp: a square riveted bronze bezel on the rear face of the top block, the screen in a
# chamfered window, a rivet in each of the bezel's corners).
#
# Usage: blender -b --factory-startup Misc/quakevr/src_models/blend/v_rock/v_rock.blend
#            --python Misc/quakevr/make_screen_housing_gl.py -- [--save other.blend] [--dry]
#        (the same with v_multi/v_multi.blend: the script takes the model from the .blend's mesh)
# Builds or moves the housing, exports the model as blend_export.py does (quakevr/progs, weapons.txt) and saves the
# .blend. Ends with "housing_<stem>: ok, ..." or "housing_<stem>: REFUSED: ..." (exit code 1); --dry exports and saves
# nothing. The housing scripts work alike (housing_common.py; docs/vr-port/MODELS_IN_BLENDER.md, "Tweaking the ammo
# screen housings").
#
# One definition: the window's half size (`opening`, font pixels), its chamfer (`radius`, negative: cut corners) and the
# glass's `offset` are the "rock" / "multi" entry of Quake/vr/vr_screenstyle.cpp; the glass's lean is weapons.txt's
# wpntxt_pitch (slot 5 / 13: 90 minus the face's lean). The script refuses when they disagree with what it builds and
# prints the entry that matches.
#
# The grid. v_rock's vertices are bytes: X 0.305 a step (coarse), Y 0.030, Z 0.115 (v_multi's within 0.3% of these). The
# rear face of the top block (a pyramid's upper face: its top edge the gun's top ridge, its point low at the rear) leans
# about 57 degrees from level (v_multi's 54). The plane of one X step and four Z steps (ROW: 56.4 degrees, a "row" 0.552
# up it) is the grid's nearest: every grid point on it is on a row, and off it the grid points come in "levels" 0.064
# apart (level L: 0.0956 L up the face from a row, modulo a row). The front is built on that plane (exact, symmetric
# left/right; top/bottom as the rows allow) and the glass leans with it.
# The shape (u up the face, d out of it towards the eye, y across; the window's height H = 4 rows = 2.21):
# - The window: rows -2..+2 (1H) by WIN_W H; square walls straight back (BACK: one X step, two Z steps back, square to
#   the face within a degree) to a dark floor; its corners cut at 45 degrees from the front: a facet from the top (or
#   bottom) edge, CHAMFER H in, to the side wall at the grid point three levels back nearest a cut of CHAMFER H (the
#   cut 0.13H above, 0.12H below: the grid). The glass GLASS_IN H behind the front, its corners cut by the shader.
# - The bezel: one flat plate, PROUD H in front of the gun's face at the window's middle (two levels), a band of one row
#   (0.25H) above and below the window and BAND_W H at the sides (the same), its outline square; its walls straight
#   back into the gun (BACK steps), closed there.
# - 4 round rivets, one in each of the bezel's corner squares: a cone from a grid point in front of the plate (three
#   levels: the only level whose points sit in the squares' middles) to a ring of grid points sunk behind it, searched
#   so that the cone comes through the plate on a circle of RIVET_R H (width / height printed).
# Texture, all from the gun's own skin (the same places on both skins): the plate and its walls on the bronze outer
# plate (BRONZE: its clean lower part) at BRONZE_K texels a unit, the walls folded back inward from the front's edge at
# their true lengths; the window's walls, facets and floor on the plate's darker bottom rows; the rivets' points on the
# brass highlight, their rings round it.

import math
import os
import sys

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import housing_common as hc  # noqa: E402

STEMS = {"v_rock": "rock", "v_multi": "multi"}

# Measured with vr_debug_screen_where 1 (Debug > Reports > Ammo Screen on Model) at weapons.txt's wpntxt values and
# the table's offset 0: the glass's centre, right and up, and a font pixel at size 1, in model units.
BASE = {
    "v_rock": (Vector((-2.4239, 0.0882, 8.0505)), Vector((0.0, -1.0, 0.0)), Vector((0.5531, 0.0, 0.8331)), 0.07049),
    "v_multi": (Vector((-2.6001, 0.1012, 8.1752)), Vector((0.0001, -1.0, 0.0)), Vector((0.5531, 0.0001, 0.8331)), 0.07049),
}
WPNTXT_SCALE = 0.3   # weapons.txt slots 5 and 13

ROW = (1, 4)          # one row up the face (X, Z steps)
BACK = (1, -2)        # a step straight back into the gun (X, Z steps)
WIN_ROWS = 2          # the window's half height, rows
WIN_W = 1.0           # its width, in H
BAND_ROWS = 1         # the bezel's band above and below, rows
BAND_W = 0.25         # its bands at the sides, in H
PROUD = (0.10, 0.17)  # the plate's front in front of the gun's face at the window's middle (units, a range)
TOP_GAP = 0.05        # the plate's top at least this far below the face's top edge (along the face)
CHAMFER = 0.12        # the window's corners cut, in H (the top/bottom edge; the side's the grid's nearest)
CUT_LEVELS = (-3, -4)  # the levels the cuts' side points may be on
GLASS_IN = 0.04       # the glass behind the front, in H
RIVET_R = 0.08        # the rivets' radius where they come through the plate, in H
RIVET_LEVELS = (1, 3)  # their points this many levels in front of the plate (a range)
RIVET_ANGLES = 8
# Skin places (texels; both skins).
BRONZE = (412.0, 104.5)       # the bronze plate's clean lower part: the plate's middle maps here
BRONZE_K = 9.0                # texels a unit (the plate 3.3 units: 30 texels, rows 90..119)
DARK_ROWS = (120.5, 122.5)    # the plate's darker bottom rows: the window's walls (front edge, back)
DARK_COLS = (380.0, 444.0)
RIVET_TEX = (387.5, 47.5, 2.0)  # the brass highlight (the point) and the ring's radius round it


def main(h):
    ob = next(o for o in bpy.data.objects if o.type == 'MESH' and o.name in STEMS)
    model = ob.name
    st = h.style
    (ow, oh), orad, size, offset = st["opening"], st["radius"], st["size"], st["offset"]
    base_c, base_r, base_u, pixel1 = BASE[model]
    base_r, base_u = base_r.normalized(), base_u.normalized()
    px = pixel1 * size
    mpu = pixel1 / (0.1 * WPNTXT_SCALE)
    sc, og = h.grid
    if ob.mode != 'OBJECT':
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='OBJECT')
    me = ob.data
    sw, sh = bpy.data.images.get(model + "_skin0").size

    # The gun alone (not an old housing).
    tag = me.attributes.get(hc.TAG)
    flags = [0] * len(me.vertices)
    if tag is not None:
        tag.data.foreach_get("value", flags)
    gun_vs = [v.co.copy() for v in me.vertices]
    gun_polys = [p for p in me.polygons if not any(flags[i] > 0 for i in p.vertices)]
    bvh = BVHTree.FromPolygons(gun_vs, [list(p.vertices) for p in gun_polys])

    # The rear face: the biggest triangle facing back and up, square across.
    face = max((p for p in gun_polys if p.normal.x < -0.7 and abs(p.normal.y) < 0.05 and p.normal.z > 0.3),
               key=lambda p: p.area, default=None)
    if face is None or len(face.vertices) != 3:
        h.fail("the top block's rear face wasn't found")
    fv = sorted((gun_vs[i] for i in face.vertices), key=lambda c: c.z)
    apex, top_mid = fv[0], (fv[1] + fv[2]) / 2
    up_f = (top_mid - apex).normalized()
    n_f = face.normal.copy()

    def G(i, k, j):
        return Vector((og[0] + sc[0] * i, og[1] + sc[1] * j, og[2] + sc[2] * k))

    row_v = Vector((sc[0] * ROW[0], 0.0, sc[2] * ROW[1]))
    up = row_v.normalized()
    out = Vector((-up.z, 0.0, up.x))
    right = Vector((0.0, -1.0, 0.0))
    rowlen = row_v.length
    hgt = 2 * WIN_ROWS * rowlen
    j0 = round((top_mid.y - og[1]) / sc[1])

    # The window's middle: the highest grid point whose plate sits PROUD in front of the face, its top TOP_GAP below
    # the face's top edge.
    best = None
    i_c = round((top_mid.x - og[0]) / sc[0])
    k_c = round((top_mid.z - og[2]) / sc[2])
    for i in range(i_c - 20, i_c + 6):
        for k in range(k_c - 60, k_c + 6):
            p = G(i, k, j0)
            d = (p - top_mid).dot(n_f)
            if not PROUD[0] <= d <= PROUD[1]:
                continue
            top = p + up * (WIN_ROWS + BAND_ROWS) * rowlen
            s_top = (top_mid - top).dot(up_f)
            if s_top < TOP_GAP:
                continue
            if best is None or s_top < best[0]:
                best = (s_top, d, i, k)
    if best is None:
        h.fail("no grid point puts the plate %.2f..%.2f in front of the face" % PROUD)
    s_top, proud, i0, k0 = best

    def P(a, b, j=0):
        return G(i0 + a, k0 + b, j0 + j)

    centre = P(0, 0)

    def uvd(co):
        rel = co - centre
        return rel.dot(right), rel.dot(up), rel.dot(out)

    rows = lambda r: (ROW[0] * r, ROW[1] * r)  # noqa: E731
    win_y = round(WIN_W * hgt / 2 / sc[1])
    band_y = round(BAND_W * hgt / sc[1])
    wwin = win_y * sc[1]
    glass = centre - out * (GLASS_IN * hgt)

    # The cuts: on the top edge CHAMFER H in; on the side the grid point nearest the same cut, on CUT_LEVELS.
    cut_y = round(CHAMFER * hgt / sc[1])
    cands = []
    for a in range(-4, 5):
        for b in range(-24, 25):
            yy, uu, dd = uvd(P(a, b))
            lv = round(dd / (sc[0] * sc[2] / rowlen))
            if lv in CUT_LEVELS:
                cands.append((uu, dd, a, b))
    cuts = {}
    for su in (1, -1):
        want_u = su * (WIN_ROWS * rowlen - CHAMFER * hgt)
        uu, dd, a, b = min(cands, key=lambda c: (abs(c[0] - want_u), -c[1]))
        cuts[su] = (a, b, abs(WIN_ROWS * rowlen - abs(uu)) / hgt, dd)
    cut_px = cut_y * sc[1] / px

    # The table against the window: the glass the window's size, its chamfer the cut, sitting in it.
    out_base = base_r.cross(base_u).normalized()
    have = base_c + (base_r * offset[0] + base_u * offset[1] + out_base * offset[2]) * mpu
    dg = glass - base_c
    lean = math.degrees(math.atan2(up.z, up.x))
    want = (wwin / px, hgt / 2 / px, -cut_px)
    h.say("%s: face %.2f deg from level, the plate's %.2f (pitch %.2f); font pixel %.5f; window %d Y steps x %d rows = "
          "%.4f x %.4f (%.2f x %.2f px); plate %.3f in front of the face, its top %.3f below the edge; cuts %.3fH "
          "(top/bottom edge), %.3fH / %.3fH at the sides (%.3f / %.3f back); glass %.4f %.4f %.4f"
          % (model, math.degrees(math.atan2(up_f.z, up_f.x)), lean, 90 - lean, px, 2 * win_y, 2 * WIN_ROWS, 2 * wwin,
             hgt, 2 * wwin / px, hgt / px, proud, s_top, cut_y * sc[1] / hgt, cuts[1][2], cuts[-1][2], -cuts[1][3],
             -cuts[-1][3], *glass))
    if (abs(ow - want[0]) > 0.3 or abs(oh - want[1]) > 0.3 or abs(orad - want[2]) > 0.3
            or (glass - have).length > 0.01):
        h.fail("the table disagrees with the window (the glass %.4f units off); a matching entry: .radius = %.2ff, "
               ".opening = {%.2ff, %.2ff}, .offset = {%.4ff, %.4ff, %.4ff}"
               % ((glass - have).length, want[2], want[0], want[1], dg.dot(base_r) / mpu, dg.dot(base_u) / mpu,
                  dg.dot(out_base) / mpu))
    if abs(base_u.angle(up)) > math.radians(0.6):
        h.fail("the glass leans %.2f degrees off the plate: weapons.txt wpntxt_pitch %.2f"
               % (math.degrees(base_u.angle(up)), 90 - lean))

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

    r_w, r_o = rows(WIN_ROWS), rows(WIN_ROWS + BAND_ROWS)
    neg = lambda ab: (-ab[0], -ab[1])  # noqa: E731
    oy = win_y + band_y

    # The plate's front: a square ring between the outline and the window's square.
    outer = {(su, s): vid(*(r_o if su > 0 else neg(r_o)), s * oy) for su in (1, -1) for s in (1, -1)}
    inner = {(su, s): vid(*(r_w if su > 0 else neg(r_w)), s * win_y) for su in (1, -1) for s in (1, -1)}
    ring = [(1, 1), (1, -1), (-1, -1), (-1, 1)]
    for n_ in range(4):
        c0, c1 = ring[n_], ring[(n_ + 1) % 4]
        quad(outer[c0], outer[c1], inner[c1], inner[c0], out, "front")

    # The window: square walls back one BACK step to the floor; the corners' facets.
    back_in = {c: vid(*((r_w if c[0] > 0 else neg(r_w))[0] + BACK[0], (r_w if c[0] > 0 else neg(r_w))[1] + BACK[1]),
                      c[1] * win_y) for c in ring}
    for n_ in range(4):
        c0, c1 = ring[n_], ring[(n_ + 1) % 4]
        mid = (Vector(uvd(verts[inner[c0]])) + Vector(uvd(verts[inner[c1]]))) / 2
        inward = -(right * mid[0] + up * mid[1]).normalized()
        quad(inner[c0], inner[c1], back_in[c1], back_in[c0], inward, "inner")
    quad(back_in[(1, 1)], back_in[(1, -1)], back_in[(-1, -1)], back_in[(-1, 1)], out, "inner")
    for su in (1, -1):
        a, b, _, _ = cuts[su]
        for s in (1, -1):
            e_ = vid(*(r_w if su > 0 else neg(r_w)), s * (win_y - cut_y))
            side = vid(a, b, s * win_y)
            tri(e_, inner[(su, s)], side, out - up * su - right * -s, "inner")

    # The plate's walls: the outline straight back into the gun, as few steps as put it all inside, closed there.
    def inside(co):
        hits = 0
        for dirv in (Vector((0.0, 0.0, 1.0)), Vector((0.31, 0.2, 0.93)).normalized(),
                     Vector((-0.3, -0.25, 0.92)).normalized()):
            loc, nrm, _, _ = bvh.ray_cast(co, dirv, 50.0)
            hits += 1 if loc is not None and nrm.dot(dirv) > 0 else 0
        return hits >= 2

    corner_ab = {c: (r_o if c[0] > 0 else neg(r_o)) for c in ring}
    steps = None
    for kk in range(1, 9):
        if all(inside(P(corner_ab[c][0] + kk * BACK[0], corner_ab[c][1] + kk * BACK[1], c[1] * oy)) for c in ring):
            steps = kk
            break
    if steps is None:
        h.fail("the plate's walls don't reach inside the gun in 8 steps")
    back_out = {c: vid(corner_ab[c][0] + steps * BACK[0], corner_ab[c][1] + steps * BACK[1], c[1] * oy) for c in ring}
    for n_ in range(4):
        c0, c1 = ring[n_], ring[(n_ + 1) % 4]
        mid = (Vector(uvd(verts[outer[c0]])) + Vector(uvd(verts[outer[c1]]))) / 2
        quad(outer[c0], outer[c1], back_out[c1], back_out[c0], (right * mid[0] + up * mid[1]).normalized(), "wall")
    quad(back_out[(1, 1)], back_out[(1, -1)], back_out[(-1, -1)], back_out[(-1, 1)], -out, "under")

    # The rivets: in the corner squares' middles; a point RIVET_LEVELS in front, a ring of grid points behind the plate
    # through which the cone crosses the plate (d 0) on a circle of RIVET_R H round the point.
    rr = RIVET_R * hgt
    ry = win_y + band_y / 2.0
    level = sc[0] * sc[2] / rowlen
    tips, rivet_info, rivet_mid = [], [], {}
    for su in (1, -1):
        want_u = su * (WIN_ROWS + BAND_ROWS / 2.0) * rowlen
        tcands = []
        for a in range(-5, 6):
            for b in range(-30, 31):
                yy, uu, dd = uvd(P(a, b))
                if RIVET_LEVELS[0] - 0.5 <= dd / level <= RIVET_LEVELS[1] + 0.5:
                    tcands.append((abs(uu - want_u), dd, uu, a, b))
        _, tip_d, tip_u, ta, tb = min(tcands)
        rcands = []
        for a in range(-6, 7):
            for b in range(-40, 41):
                qy, qu, qd = uvd(P(a, b))
                if not -0.6 < qd < -0.02:
                    continue
                t = tip_d / (tip_d - qd)
                for dj in range(-10, 11):
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
            if best_ring is None or worst < best_ring[0]:
                best_ring = (worst, rot, ring_pts, seen)
        worst, rot, ring_pts, seen = best_ring
        ys = [p_[0] for p_ in seen]
        us = [p_[1] for p_ in seen]
        ratio = (max(ys) - min(ys)) / (max(us) - min(us))
        rivet_info.append("%s: point %.3f in front, middle %.3f (the square's %.3f), w/h %.3f, worst %.3f" % (
            "upper" if su > 0 else "lower", tip_d, tip_u, want_u, ratio, worst))
        if not 0.9 <= ratio <= 1.1:
            h.fail("the %s rivets aren't round (width / height %.3f)" % ("upper" if su > 0 else "lower", ratio))
        for s in (1, -1):
            jy = round(s * ry)
            t_i = vid(ta, tb, jy)
            tips.append(t_i)
            rivet_mid[t_i] = (-jy * sc[1], tip_u)
            base = [vid(a, b, jy + s * dj) for a, b, dj in ring_pts]  # (mirrored: the left one the right's image)
            ring_mid = sum((verts[b_] for b_ in base), Vector()) / len(base)
            for n_ in range(len(base)):
                mid = (verts[base[n_]] + verts[base[(n_ + 1) % len(base)]] + verts[t_i]) / 3.0
                tri(base[n_], base[(n_ + 1) % len(base)], t_i, mid - ring_mid, "rivet")
                for b_ in (base[n_],):
                    rivet_mid.setdefault(b_, (-jy * sc[1], tip_u))
    h.say("rivets r %.4f (%.2fH): %s" % (rr, RIVET_R, "; ".join(rivet_info)))

    # Checks: nothing of the gun in front of the glass.
    for fy in (-0.95, -0.5, 0.0, 0.5, 0.95):
        for fu in (-0.95, -0.5, 0.0, 0.5, 0.95):
            o = glass + right * (fy * wwin) + up * (fu * hgt / 2)
            hit = bvh.ray_cast(o, out, 3.0)
            if hit[0] is not None:
                h.fail("the gun is in front of the glass at (%.2f, %.2f): %.3f out" % (fy, fu, hit[3]))

    # Texels.
    def bronze(yy, uu):
        return BRONZE[0] + BRONZE_K * yy, BRONZE[1] - BRONZE_K * uu

    def tex(vi, mode, nrm):
        yy, uu, dd = uvd(verts[vi])
        if mode == "rivet":
            my, mu = rivet_mid[vi]
            if vi in tips:
                return RIVET_TEX[0], RIVET_TEX[1]
            dy, du = yy - my, uu - mu
            dist = max(math.hypot(dy, du), 1e-6)
            return RIVET_TEX[0] + RIVET_TEX[2] * dy / dist, RIVET_TEX[1] - RIVET_TEX[2] * du / dist
        if mode == "inner":
            along = (yy + wwin) / (2 * wwin) if abs(nrm.dot(up)) > 0.5 else (uu + hgt / 2) / hgt
            return (DARK_COLS[0] + (DARK_COLS[1] - DARK_COLS[0]) * along,
                    DARK_ROWS[0] if dd > -0.05 else DARK_ROWS[1])
        if mode == "wall" and dd < -1e-4:
            # folded back inward from the front's edge (the corner it went back from), at its true length
            cy = math.copysign(oy * sc[1], yy)
            cu = math.copysign((WIN_ROWS + BAND_ROWS) * rowlen, uu)
            fold = (verts[vi] - (centre + right * cy + up * cu)).length
            if abs(nrm.dot(up)) > abs(nrm.dot(right)):
                return bronze(cy, cu - math.copysign(fold, cu))
            return bronze(cy - math.copysign(fold, cy), cu)
        return bronze(yy, uu)

    built = []
    for i_, j_, k_, want_n, mode in tris:
        p0, p1, p2 = verts[i_], verts[j_], verts[k_]
        nrm = (p1 - p0).cross(p2 - p0)
        if nrm.length < 1e-9:
            h.fail("a degenerate triangle (%d %d %d, %s)" % (i_, j_, k_, mode))
        ijk = (i_, j_, k_) if nrm.dot(want_n) >= 0.0 else (i_, k_, j_)
        nrm = nrm if nrm.dot(want_n) >= 0.0 else -nrm
        built.append(ijk + (tuple(((tx / sw), 1.0 - ty / sh) for tx, ty in
                                  (tex(vi, mode, nrm.normalized()) for vi in ijk)),))

    lo = [min(c[i] for c in verts) for i in range(3)]
    hi = [max(c[i] for c in verts) for i in range(3)]
    h.say("plate %.4f wide x %.4f tall (%.2fH x %.2fH), walls %d steps back; the box x %.3f..%.3f y %.3f..%.3f "
          "z %.3f..%.3f; the face's top edge z %.3f" % (2 * oy * sc[1], 2 * (WIN_ROWS + BAND_ROWS) * rowlen,
                                                       2 * oy * sc[1] / hgt, 2 * (WIN_ROWS + BAND_ROWS) * rowlen / hgt,
                                                       steps, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], top_mid.z))

    moved = h.apply(me, verts, built, smooth=False)
    if not moved:
        h.export()
    _, mdl_blender = hc.addon()
    mdl_blender.carry_frames(ob)
    h.finish("%d vertices, %d triangles (%s; opening %.2f x %.2f px, radius %.2f px)"
             % (len(verts), len(tris), "moved" if moved else "new", ow, oh, orad))


if __name__ == "__main__":
    model_ob = next((o for o in bpy.data.objects if o.type == 'MESH' and o.name in STEMS), None)
    if model_ob is None:
        print("housing_gl: REFUSED: no v_rock or v_multi mesh in this .blend")
        sys.exit(1)
    hc.Run(STEMS[model_ob.name], model_ob.name).main(main)
