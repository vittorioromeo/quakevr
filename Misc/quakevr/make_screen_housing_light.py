#!/usr/bin/env python3
# make_screen_housing_light.py -- the lightning gun's ammo screen housing, modelled on v_light.blend (ROUND24.md, "The
# lightning gun's ammo screen housing" and its second pass, "trapezoids"; his mockup, kit
# scratch/mockups/light_housing.webp, and markup, light_trapezoid.webp: a flat bronze plate over the receiver's whole
# rear end face, the screen a trapezoid slightly sunk into it, two rivets on each side between the window and the
# plate's slanted edge).
#
# Usage: blender -b --factory-startup Misc/quakevr/src_models/blend/v_light/v_light.blend
#            --python Misc/quakevr/make_screen_housing_light.py -- [--save other.blend] [--dry]
# Builds or moves the housing, exports the model as blend_export.py does (quakevr/progs, weapons.txt's barrel mask)
# and saves the .blend (in place, or as --save) as the export left it. Ends with "housing_light: ok, ..." or
# "housing_light: REFUSED: ..." (exit code 1). --dry: builds and checks it, exports and saves nothing.
# The housing scripts work alike (housing_common.py; docs/vr-port/MODELS_IN_BLENDER.md, "Tweaking the ammo screen
# housings").
#
# One definition: the window's bottom half width and half height (`opening`, font pixels), its `taper` (the top's
# width over the bottom's), its chamfer (`radius` < 0, cut along both edges), the screen's `size` and `offset` are read
# from the "light" entry of Quake/vr/vr_screenstyle.cpp (the table the engine shapes and places the glass by: the
# screen shader draws the same trapezoid) and weapons.txt slot 7's wpntxt_scale. The window is snapped to the model's
# byte grid; the script refuses when the snapped window or the glass's place disagree with the table (by more than
# 0.3 font pixels, or 0.01 units), printing the entry that matches.
#
# Run again on a .blend that has the housing (its vertices carry the point attribute `qvr_housing`): with the same
# shape (as many vertices) its vertices are moved and re-mapped in place, so the model's file keeps its vertices;
# otherwise the old housing is deleted and built anew (its old vertices then stay in the .mdl unused: start from the
# .blend before the housing, in git history (8f4c503d3), to keep the file lean).
#
# The shape (u right = model -Y, v up = model +Z, seen from behind). The end face is the receiver's upright rear face
# under the rear sight (x -2.7194), a trapezoid wider at the bottom: z 8.6697..10.8636 (32 z steps), 96 y steps wide at
# the bottom (on the ledge), 38 at the top, its sides slanting 29 y steps over the 32.
# - The plate: the end face's own outline, one X step proud (a flat front, its outer walls straight back to the face,
#   closed there by a ring facing into the gun: culled, no fight with the face; a closed solid).
# - The window: a trapezoid with the face's middle, `margin` z steps in from its top and bottom, its sides slanting
#   1 y step every `slope` z steps (so the chamfers' points are grid points on them), its corners chamfered; the glass
#   upright, GLASS_IN window heights behind the plate's front, the opening's exact shape; its walls straight back to
#   the face (the face is the back behind the glass).
# - 4 round rivets, two each side, in the triangles between the window's slanted side and the plate's: each as far
#   from the two edges (RIVET_J: their heights in the face's height), a flat octagon and a point one X step out.
# The model's X grid is coarse (0.171 units a step), Y 0.073, Z 0.069: every vertex is on the grid (the file's bytes),
# so the sizes are whole steps (printed).
# Texture, all from the gun's own skin (no fullbright texels): the plate's front and its outer walls mapped as the end
# face under them is (its own triangles' texels, seen along X: the woven bronze block 347..380 x 25..31 that runs on
# into the receiver's sloped top), so the plate reads as one piece with the gun and its walls carry the face's edge
# texels; the window's walls on the dark row (53: the recessed details' brown-black), the rivets on the tubes' brass
# fitting (326, 23: its lit centre on the point).

import math
import os
import re
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import housing_common as hc  # noqa: E402

STEM = "light"
MODEL = "v_light"
SLOT = 7

# Measured with vr_debug_screen_where 1 (weapons.txt slot 7's wpntxt offsets, pitch 0, the table's offset 0): the
# glass's centre (right model -Y, up +Z, upright), and a font pixel 0.07654 model units at wpntxt_scale 0.3: 2.5513 model
# units a world unit (the offset's unit; a font pixel is 0.1 * scale * size of them).
BASE_CENTRE = Vector((-2.9642, -0.2184, 9.7616))
MPU = 2.5513
# The end face: its plane, its bottom and top, their ends across (model Y).
FACE_X = -2.7194
FACE_Z = (8.6697, 10.8636)
FACE_BOTTOM_Y = (-3.6933, 3.3502)
FACE_TOP_Y = (-1.5656, 1.2225)
# The design.
MARGIN = 2           # the plate's frame above and below the window, z steps
SLOPE = 4            # the window's sides: one y step out every SLOPE z steps down
GLASS_IN = 0.06      # the glass behind the plate's front, in window heights
RIVET_R = 2          # the rivets' radius, y/z steps
RIVET_J = (0.22, 0.59)  # their heights, in the face's height from its bottom
RIVET_CLEAR = 0.03   # at least this far (units) inside the two edges
# Skin places (texels): the window's walls (the plate's front and outer walls: face_uv in main).
WALL_COLS = (357.5, 369.5)
INNER_ROWS = (53.3, 53.8)                   # the window's walls (and the closing ring behind)
RIVET_TEX = (326.5, 23.5, 1.2)              # the brass fitting's centre and the base ring's radius


def wpntxt_scale():
    txt = open(os.path.join(hc.ROOT, "quakevr", "data", "weapons.txt"), encoding="utf-8").read()
    m = re.search(r"^weapon %d\s*\{(.*?)^\}" % SLOT, txt, re.S | re.M)
    return float(re.search(r'wpntxt_scale "([0-9.]+)"', m.group(1)).group(1))


def main(h):
    (ow, oh), orad, size, offset = h.style["opening"], h.style["radius"], h.style["size"], h.style["offset"]
    taper = h.style["taper"]
    if ow <= 0 or orad >= 0 or not 0.2 < taper < 1.0:
        h.fail("the \"%s\" entry needs .opening, a .radius < 0 (a chamfer) and a .taper in (0.2, 1)" % STEM)
    px = 0.1 * wpntxt_scale() * size * MPU
    sc, og = h.grid
    sy, sz = sc[1], sc[2]

    def step(v, i):
        k = (v - og[i]) / sc[i]
        if abs(k - round(k)) > 1e-3:
            h.fail("the end face's %.4f isn't on the grid (%.4f steps)" % (v, k))
        return round(k)

    def gx(k):
        return og[0] + sc[0] * k

    def gy(k):
        return og[1] + sy * k

    def gz(k):
        return og[2] + sz * k

    # The end face in steps from its middle (u right = -Y).
    k_face = step(FACE_X, 0)
    k_front = k_face - 1
    kb, kt = step(FACE_Z[0], 2), step(FACE_Z[1], 2)
    yb, yt = [step(v, 1) for v in FACE_BOTTOM_Y], [step(v, 1) for v in FACE_TOP_Y]
    ky = (yb[0] + yb[1]) // 2
    if (yb[0] + yb[1]) % 2 or yt[0] + yt[1] != 2 * ky or (kt - kb) % 2:
        h.fail("the end face isn't symmetric on the grid")
    kz = (kb + kt) // 2
    fh = (kt - kb) // 2                    # its half height
    fb, ft = (yb[1] - yb[0]) // 2, (yt[1] - yt[0]) // 2   # its half widths, bottom and top

    # The window: its half height, its top and bottom half widths, its chamfers' cuts (steps).
    wz = fh - MARGIN
    wb = round(ow * px / sy)
    run = 2 * wz // SLOPE
    if 2 * wz % SLOPE:
        h.fail("the window's height (%d z steps) isn't a whole number of slopes (%d)" % (2 * wz, SLOPE))
    wt = wb - run
    side = Vector((run * sy, 2 * wz * sz))           # a side, bottom corner to top, (out, up) in units
    cut_side = max(1, round(-orad * px / side.length * run))  # how many slope steps along it
    cs = Vector((sy, SLOPE * sz)) * cut_side          # the cut along the side
    ct = max(1, round(cs.length / sy))                # and along the top and bottom (y steps)
    chamfer = (cs.length + ct * sy) * 0.5
    hgt = 2 * wz * sz
    snapped_taper = wt / wb
    x_face, x_front = gx(k_face), gx(k_front)
    x_glass = x_front + GLASS_IN * hgt
    yc, zc = gy(ky), gz(kz)

    # The table against the snapped window: the glass must be the opening's shape and sit in it.
    off_px = (abs(wb * sy - ow * px) / px, abs(wz * sz - oh * px) / px, abs(chamfer + orad * px) / px,
              abs(snapped_taper - taper) * ow)
    want = Vector((x_glass, yc, zc))
    have = BASE_CENTRE + Vector((-offset[2], -offset[0], offset[1])) * MPU
    d = want - BASE_CENTRE

    # The rivets: at their heights, as far from the window's side as from the plate's edge (on the side's middle line).
    def line_dist(p, a, b):
        """How far (u, v) is from the line a-b (units), > 0 on the side away from the window's middle."""
        t = Vector((b[0] - a[0], b[1] - a[1])).normalized()
        n = Vector((t.y, -t.x))
        if n.dot(Vector((-a[0], -a[1]))) > 0:
            n = -n
        return n.dot(Vector((p[0] - a[0], p[1] - a[1])))

    def units(ku, kv):
        return (ku * sy, kv * sz)

    win_side = (units(wb, -wz), units(wt, wz))
    plate_side = (units(fb, -fh), units(ft, fh))
    rivets = []
    for jf in RIVET_J:
        kv = round(-fh + 2 * fh * jf)
        best = None
        for ku in range(wt, fb + 1):
            p = units(ku, kv)
            a, b = line_dist(p, *win_side), -line_dist(p, *plate_side)
            score = abs(a - b)
            if a > 0 and b > 0 and (best is None or score < best[0]):
                best = (score, ku, a, b)
        if best is None:
            h.fail("no room for a rivet at %.2f of the face's height" % jf)
        _, ku, a, b = best
        r = RIVET_R * max(sy, sz)
        if min(a, b) - r < RIVET_CLEAR:
            h.fail("the rivet at %.2f of the face's height is %.4f / %.4f from the window and the plate's edge, its "
                   "radius %.4f: less than %.3f clear" % (jf, a, b, r, RIVET_CLEAR))
        rivets.append((ku, kv, a - r, b - r))

    h.say("font pixel %.5f; face %d x %d (top %d) y/z steps; window bottom %d top %d (taper %.4f) y steps, %d z steps "
          "(half %.4f x %.4f = %.2f x %.2f px), sides 1:%d, chamfer %d slope steps / %d y steps (%.4f / %.4f = %.2f px); "
          "window height H %.4f: %.3fH wide at the bottom, %.3fH at the top, frame %.3fH above and below; proud %.4f, "
          "glass %.4f behind the front; rivets r %d steps at %s"
          % (px, 2 * fb, 2 * fh, 2 * ft, 2 * wb, 2 * wt, snapped_taper, 2 * wz, wb * sy, wz * sz, wb * sy / px,
             wz * sz / px, SLOPE, cut_side, ct, cs.length, ct * sy, chamfer / px, hgt, 2 * wb * sy / hgt,
             2 * wt * sy / hgt, MARGIN * sz / hgt, x_face - x_front, x_glass - x_front, RIVET_R,
             ", ".join("(%d, %d: clear %.3f window, %.3f edge)" % r for r in rivets)))
    h.say("x face %.4f, plate front %.4f, glass %.4f; centre y %.4f z %.4f" % (x_face, x_front, x_glass, yc, zc))
    if max(off_px) > 0.3 or (want - have).length > 0.01:
        h.fail("the table disagrees with the snapped design (off %.2f %.2f %.2f %.2f px, the glass %.4f units off); a "
               "matching entry: .radius = %.2ff, .opening = {%.2ff, %.2ff}, .taper = %.4ff, "
               ".offset = {%.4ff, %.4ff, %.4ff}"
               % (off_px + ((want - have).length, -chamfer / px, wb * sy / px, wz * sz / px, snapped_taper,
                            -d.y / MPU, d.z / MPU, -d.x / MPU)))

    ob = bpy.data.objects[MODEL]
    if ob.mode != 'OBJECT':  # (a .blend saved in edit mode: its mesh can't be written then)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='OBJECT')
    me = ob.data
    sw, sh = bpy.data.images.get(MODEL + "_skin0").size

    def uv_at(tx, ty):
        return (tx / sw, 1.0 - ty / sh)

    def place(ku, kv, x):
        """Grid steps right (u: -Y) and up from the centre."""
        return Vector((x, gy(ky - ku), gz(kz + kv)))

    verts, tris = [], []   # tris: (i, j, k, kind, centre, mode)

    def add(co):
        verts.append(co)
        return len(verts) - 1

    # The window's 8 points, counter-clockwise from the top right corner's (u, v): each corner's pair (the point on
    # the side, then the point on the top or bottom, in the ring's order).
    cu, cv = cut_side, SLOPE * cut_side
    window = [(wt + cu, wz - cv), (wt - ct, wz), (-(wt - ct), wz), (-(wt + cu), wz - cv),
              (-(wb - cu), -wz + cv), (-(wb - ct), -wz), (wb - ct, -wz), (wb - cu, -wz + cv)]
    plate = [(ft, fh), (-ft, fh), (-fb, -fh), (fb, -fh)]   # its corners, in the same order (window[2c], window[2c+1])
    centre = place(0, 0, x_front)
    outer_back = [add(place(u, v, x_face)) for u, v in plate]
    outer_front = [add(place(u, v, x_front)) for u, v in plate]
    inner_front = [add(place(u, v, x_front)) for u, v in window]
    inner_back = [add(place(u, v, x_face)) for u, v in window]

    def quad(a, b, c, e, kind, mode):
        tris.append((a, b, c, kind, centre, mode))
        tris.append((a, c, e, kind, centre, mode))

    for i in range(4):
        j = (i + 1) % 4
        quad(outer_back[i], outer_back[j], outer_front[j], outer_front[i], "out", "outer")
    for i in range(8):
        j = (i + 1) % 8
        quad(inner_front[i], inner_front[j], inner_back[j], inner_back[i], "in", "inner")
    # The front and the closing ring: plate corner c fans to its chamfer (window points 2c, 2c+1); the plate's edge
    # from corner c to the next faces the window's edge from 2c+1 to 2c+2 (a convex quad).
    for ring_o, ring_i, kind, mode in ((outer_front, inner_front, "back", "front"),
                                       (outer_back, inner_back, "fwd", "inner")):
        for c in range(4):
            n = (c + 1) % 4
            tris.append((ring_o[c], ring_i[2 * c], ring_i[2 * c + 1], kind, centre, mode))
            quad(ring_o[c], ring_i[2 * c + 1], ring_i[(2 * c + 2) % 8], ring_o[n], kind, mode)

    half = max(1, RIVET_R // 2)
    ring_pts = [(RIVET_R, -half), (RIVET_R, half), (half, RIVET_R), (-half, RIVET_R), (-RIVET_R, half),
                (-RIVET_R, -half), (-half, -RIVET_R), (half, -RIVET_R)]
    studs = [(su * ru, rv) for ru, rv, _, _ in rivets for su in (-1, 1)]
    for ru, rv in studs:
        base = [add(place(ru + du, rv + dv, x_front)) for du, dv in ring_pts]
        tip = add(place(ru, rv, gx(k_front - 1)))
        rc = place(ru, rv, x_front)
        for i in range(8):
            tris.append((base[i], base[(i + 1) % 8], tip, "outback", rc, "rivet"))

    lo_u, hi_u = -fb * sy, fb * sy
    lo_v, hi_v = -fh * sz, fh * sz

    # The end face's own triangles (not the housing's) and their texels: the plate's front and outer walls are mapped
    # as the face is under them (seen along X), so the woven bronze continues across the plate and its walls carry
    # the face's edge texels drawn back.
    tag = me.attributes.get(hc.TAG)
    uvl = me.uv_layers.active.data
    face_tris = []
    for p in me.polygons:
        vs = [me.vertices[i] for i in p.vertices]
        if len(vs) != 3 or any(abs(w.co.x - FACE_X) > 1e-3 or not FACE_Z[0] - 1e-3 <= w.co.z <= FACE_Z[1] + 1e-3
                               for w in vs):
            continue
        if tag is not None and any(tag.data[i].value > 0 for i in p.vertices):
            continue
        face_tris.append([((w.co.y, w.co.z), (uvl[li].uv.x * sw, (1.0 - uvl[li].uv.y) * sh))
                          for w, li in zip(vs, p.loop_indices)])
    if not face_tris:
        h.fail("the end face's triangles weren't found (x %.4f)" % FACE_X)

    def face_uv(y, z):
        best = None
        for t in face_tris:
            (a, ta), (b, tb), (c, tc) = t
            den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
            l0 = ((b[1] - c[1]) * (y - c[0]) + (c[0] - b[0]) * (z - c[1])) / den
            l1 = ((c[1] - a[1]) * (y - c[0]) + (a[0] - c[0]) * (z - c[1])) / den
            l2 = 1.0 - l0 - l1
            out = -min(l0, l1, l2)
            if best is None or out < best[0]:
                best = (out, tuple(l0 * ta[k] + l1 * tb[k] + l2 * tc[k] for k in range(2)))
        if best[0] > 1e-3:
            h.fail("a plate vertex (y %.4f z %.4f) is off the end face" % (y, z))
        return best[1]

    def tex(vi, mode, nrm):
        co = verts[vi]
        u, v = -(co.y - yc), co.z - zc
        if mode in ("front", "outer"):
            return face_uv(co.y, co.z)
        if mode == "inner":
            along = (u - lo_u) / (hi_u - lo_u) if abs(nrm.z) >= abs(nrm.y) else (hi_v - v) / (hi_v - lo_v)
            return (WALL_COLS[0] + (WALL_COLS[1] - WALL_COLS[0]) * along,
                    INNER_ROWS[0] if abs(co.x - x_front) < 1e-4 else INNER_ROWS[1])
        # rivet: the tip on the fitting's lit centre, the base ring round it
        for ru, rv in studs:
            du, dv = u - ru * sy, v - rv * sz
            dist = math.hypot(du, dv)
            if dist < (RIVET_R + 1) * sy:
                if dist < 1e-5:
                    return RIVET_TEX[0], RIVET_TEX[1]
                return RIVET_TEX[0] + RIVET_TEX[2] * du / dist, RIVET_TEX[1] - RIVET_TEX[2] * dv / dist
        return RIVET_TEX[0], RIVET_TEX[1]

    built = []
    for i, j, k, kind, c, mode in tris:
        p0, p1, p2 = verts[i], verts[j], verts[k]
        nrm = (p1 - p0).cross(p2 - p0)
        if nrm.length < 1e-9:
            h.fail("a degenerate triangle (%d %d %d, %s)" % (i, j, k, mode))
        mid = (p0 + p1 + p2) / 3.0
        radial = Vector((0.0, mid.y - c.y, mid.z - c.z))
        if radial.length > 1e-6:
            radial.normalize()
        want_n = {"back": Vector((-1, 0, 0)), "fwd": Vector((1, 0, 0)), "out": radial, "in": -radial,
                  "outback": radial + Vector((-1, 0, 0))}[kind]
        ijk = (i, j, k) if nrm.dot(want_n) >= 0.0 else (i, k, j)
        nrm = nrm if nrm.dot(want_n) >= 0.0 else -nrm
        built.append(ijk + (tuple(uv_at(*tex(vi, mode, nrm)) for vi in ijk),))

    moved = h.apply(me, verts, built, smooth=True)
    if not moved:
        h.export()  # (the housing's vertices are now the file's)
    # The frames: the housing carried along with the end face as the export does it (written into the shape keys, so
    # the .blend shows it in every frame).
    _, mdl_blender = hc.addon()
    mdl_blender.carry_frames(ob)
    h.finish("%d vertices, %d triangles (%s; opening %.2f x %.2f px, taper %.4f, chamfer %.2f px)"
             % (len(verts), len(tris), "moved" if moved else "new", ow, oh, taper, -orad))


if __name__ == "__main__":
    hc.Run(STEM, MODEL).main(main)
