# make_screen_housing_nailpistol.py -- the nail pistol's ammo screen housing, modelled on its source .blend.
#
# Usage (Git Bash, from the repository root):
#   "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b \
#       Misc/quakevr/src_models/blend/v_nailpistol/v_nailpistol.blend \
#       --python Misc/quakevr/make_screen_housing_nailpistol.py [-- --dry]
#   then export it: the same Blender line with --python Misc/quakevr/blend_export.py -- --grow
#
# What it builds (his mockup, 2026-11-09: a chunky bevelled housing on the slide's rear face, a rounded window with the
# glass recessed in it, a grille plate under it; the slide's rear sight ears stand on top as the mockup's two posts):
#   - a block on the slide's rear face, wider than the slide, standing proud of it, its back edges bevelled;
#   - in its back face a window: a pale bevelled lip, then walls going in; the screen's glass sits in it, recessed;
#   - under the window a grille: a recess with a dark floor crossed by two slats;
#   - four dark bolt heads on the back face, a rivet on each side.
# The window is the screen's opening, from ONE definition: vr_screenstyle.cpp's "nailpistol" entry (`.opening`, half
# its size in font pixels, `.radius` > 0 its corners' radius, < 0 a chamfer, `.size`) and weapons.txt's
# `wpntxt_scale` for slot 36 give its size in model units (a font pixel: PIXEL_AT_0205 at wpntxt_scale 0.205, as
# `vr_debug_screen_where 1` prints it). Change the table and run this again: the window follows.
#
# Re-runnable: the faces it made before (face attribute qvr_housing = 1) are removed first. It saves the .blend; it
# never exports (run blend_export.py after it: the housing is past the model's old box, hence --grow). New vertices
# follow the slide in every frame (the export makes a new vertex ride its nearest old one: checked here, each new
# vertex's nearest old vertex must be on the slide).
#
# Textures: the gun's own skin (no new texels): the back face and the sides on its rusted iron, the bevel and the lip
# on its worn pale edge strip, the walls on dark iron, the grille's floor black, bolt heads on dark iron.
import os
import re
import sys

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
STYLE_CPP = os.path.join(ROOT, "Quake", "vr", "vr_screenstyle.cpp")
WEAPONS_TXT = os.path.join(ROOT, "quakevr", "data", "weapons.txt")
STEM = "nailpistol"
SLOT = 36
PIXEL_AT_0205 = 0.04068  # a font pixel in model units at wpntxt_scale 0.205, style size 1 (vr_debug_screen_where)

# The housing (model units; +X forward, +Y left, +Z up). The slide's rear face is the plane x = -1.87.
X_CAP = -1.85     # the block's front (just inside the slide; its overhang's front face)
X_SIDE = -2.12    # its straight sides end here
X_BACK = -2.30    # its back face (0.43 proud of the slide's face)
INSET = 0.14      # the bevel: the back face this much inside the sides
HALF_W = 1.38     # the block's half width (the slide's is 1.2)
Z_LO, Z_HI = 2.66, 5.92
CHAMFER_TOP, CHAMFER_BOT = 0.40, 0.25
WIN_ZC = 4.80     # the window's centre height
LIP = 0.07        # the window's lip: its outer edge this much outside the opening,
LIP_DEPTH = 0.05  # and the opening this deep behind the back face
GLASS_DEPTH = 0.10  # the glass's plane behind the back face
TUCK = 0.012      # the walls this much inside the glass's edge, so no gap shows between glass and walls
WALL_END = -1.92  # the window's and the grille's walls end here (the slide's face behind)
CORNER_SEGS = 4
VENT_ZC, VENT_HW, VENT_HH, VENT_R, VENT_DEPTH = 3.42, 0.80, 0.36, 0.12, 0.16
SLAT_DEPTH = 0.05
BOLTS = [(0.90, 5.60), (-0.90, 5.60), (1.02, 3.00), (-1.02, 3.00)]
BOLT_HALF, BOLT_UP = 0.09, 0.06
RIVET_X, RIVET_Z, RIVET_HALF, RIVET_UP = -1.97, 4.30, 0.07, 0.05

SKIN_W, SKIN_H = 256, 512
PPU = 10.0  # texels per model unit (the slide's rear face has 10)
# Skin rectangles (x0, y0, x1, y1 in texels, top-left origin): none holds a fullbright texel (checked below).
REGION = {
    "back": (78, 257, 167, 287),    # rusted iron panel
    "side": (26, 301, 119, 329),    # rusted iron
    "bevel": (19, 411, 40, 417),    # worn pale edge strip
    "lip": (1, 422, 22, 428),       # worn pale edge strip
    "wall": (26, 422, 77, 428),     # dark iron
    "floor": (150, 446, 236, 496),  # black
    "bolt": (33, 263, 49, 285),     # dark iron (the old rear face's middle)
}


def table_entry():
    src = open(STYLE_CPP, encoding="utf-8").read()
    m = re.search(r'\{"%s",\s*\{(.*?)\}\},' % STEM, src, re.S)
    if not m:
        raise SystemExit("make_screen_housing: no \"%s\" entry in %s" % (STEM, STYLE_CPP))
    body = m.group(1)

    def num(name, default):
        n = re.search(r"\.%s\s*=\s*(-?[0-9.]+)f?" % name, body)
        return float(n.group(1)) if n else default

    op = re.search(r"\.opening\s*=\s*\{\s*(-?[0-9.]+)f?\s*,\s*(-?[0-9.]+)f?\s*\}", body)
    if not op:
        raise SystemExit("make_screen_housing: the entry has no .opening")
    return num("radius", 4.0), num("size", 1.0), (float(op.group(1)), float(op.group(2)))


def wpntxt_scale():
    txt = open(WEAPONS_TXT, encoding="utf-8").read()
    m = re.search(r"^weapon %d\s*\{(.*?)^\}" % SLOT, txt, re.S | re.M)
    s = re.search(r'wpntxt_scale "([0-9.]+)"', m.group(1))
    return float(s.group(1))


def rounded_rect(cy, cz, hw, hh, r, segs):
    """The opening's outline (y, z), counter-clockwise seen from behind; r > 0 rounded corners, < 0 chamfered."""
    import math
    pts = []
    corners = [(1, 1), (-1, 1), (-1, -1), (1, -1)]
    start = [0.0, 90.0, 180.0, 270.0]
    rr = abs(r)
    for (sy, sz), a0 in zip(corners, start):
        ccy, ccz = cy + sy * (hw - rr), cz + sz * (hh - rr)
        n = 1 if r < 0 else segs  # a chamfer: the arc's two ends, the cut between them
        for i in range(n + 1):
            a = math.radians(a0 + 90.0 * i / n)
            pts.append((ccy + rr * math.cos(a), ccz + rr * math.sin(a)))
    return pts  # (each face's normal is set by the side it should face, not by this winding)


def chamfered_box(hw, zlo, zhi, ctop, cbot):
    return [(hw, zhi - ctop), (hw - ctop, zhi), (-hw + ctop, zhi), (-hw, zhi - ctop),
            (-hw, zlo + cbot), (-hw + cbot, zlo), (hw - cbot, zlo), (hw, zlo + cbot)]


def offset_polygon(pts, d):
    """Each edge moved d inwards (a convex polygon, either winding), the new corners where neighbours meet."""
    n = len(pts)
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    s = 1.0 if area > 0 else -1.0
    lines = []
    for i in range(n):
        a, b = Vector(pts[i]), Vector(pts[(i + 1) % n])
        e = (b - a).normalized()
        nrm = Vector((-e.y, e.x)) * s  # inwards
        lines.append((a + nrm * d, e))
    out = []
    for i in range(n):
        p1, d1 = lines[i - 1]
        p2, d2 = lines[i]
        den = d1.x * d2.y - d1.y * d2.x
        t = ((p2.x - p1.x) * d2.y - (p2.y - p1.y) * d2.x) / den
        q = p1 + d1 * t
        out.append((q.x, q.y))
    return out


def inside(poly, y, z):
    c = False
    n = len(poly)
    for i in range(n):
        (y1, z1), (y2, z2) = poly[i], poly[(i + 1) % n]
        if (z1 > z) != (z2 > z) and y < (y2 - y1) * (z - z1) / (z2 - z1) + y1:
            c = not c
    return c


def main():
    dry = "--dry" in sys.argv
    radius_px, size, (open_w, open_h) = table_entry()
    pix = PIXEL_AT_0205 * wpntxt_scale() / 0.205 * size
    win_hw, win_hh, win_r = open_w * pix, open_h * pix, radius_px * pix
    print("housing: font pixel %.5f, window half %.3f x %.3f, corner %.3f (%s)" % (
        pix, win_hw, win_hh, win_r, "rounded" if radius_px > 0 else "chamfered"))

    ob = bpy.data.objects["v_nailpistol"]
    me = ob.data
    bm = bmesh.new()
    bm.from_mesh(me)
    tag = bm.faces.layers.int.get("qvr_housing") or bm.faces.layers.int.new("qvr_housing")
    old_faces = [f for f in bm.faces if f[tag] == 1]
    old_verts = {v for f in old_faces for v in f.verts if all(g[tag] == 1 for g in v.link_faces)}
    bmesh.ops.delete(bm, geom=old_faces, context='FACES_ONLY')
    bmesh.ops.delete(bm, geom=list(old_verts), context='VERTS')
    print("housing: removed %d faces of a housing before" % len(old_faces))
    old = list(bm.verts)
    nold = len(old)

    lay_vert = bm.verts.layers.int.get("qvr_vert")
    lay_tri = bm.faces.layers.int.get("qvr_tri")
    lay_vid = bm.loops.layers.int.get("qvr_vid")
    refs = [bm.verts.layers.float_vector.get("qvr_ref%d" % p) for p in range(2)]
    shapes = list(bm.verts.layers.shape.values())
    uvl = bm.loops.layers.uv.active

    # The slide: the old vertices that move between the frames (the export makes new ones follow their nearest).
    slide = set(i for i, v in enumerate(old) if (v[shapes[1]] - v[shapes[0]]).length > 1e-4)
    delta = Vector(old[next(iter(slide))][shapes[1]] - old[next(iter(slide))][shapes[0]])

    new_verts, groups = [], {}

    def vert(x, y, z):
        v = bm.verts.new((x, y, z))
        new_verts.append(v)
        return v

    def face(vs, want, group, shared=False):
        f = bm.faces.new(vs)
        f.normal_update()
        if f.normal.dot(Vector(want)) < 0:
            f.normal_flip()
        groups[f] = (group, shared)
        return f

    def ring(x, pts):
        return [vert(x, y, z) for (y, z) in pts]

    def bridge(r1, r2, want_fn, group):
        n = len(r1)
        for i in range(n):
            j = (i + 1) % n
            quad = [r1[i], r1[j], r2[j], r2[i]]
            c = sum((v.co for v in quad), Vector()) / 4
            face(quad, want_fn(c), group)

    # The block.
    outline = chamfered_box(HALF_W, Z_LO, Z_HI, CHAMFER_TOP, CHAMFER_BOT)
    back_outline = offset_polygon(outline, INSET)
    r_cap = ring(X_CAP, outline)
    r_side = ring(X_SIDE, outline)
    r_back = ring(X_BACK, back_outline)
    mid_z = (Z_LO + Z_HI) / 2

    def outward(c):
        return Vector((0, c.y, c.z - mid_z)).normalized()

    face(r_cap, (1, 0, 0), "side")
    bridge(r_cap, r_side, outward, "side")
    bridge(r_side, r_back, lambda c: (outward(c) + Vector((-1, 0, 0))).normalized(), "bevel")

    # The window: lip ring on the back face -> opening ring LIP_DEPTH in -> walls to WALL_END.
    win_in = rounded_rect(0.0, WIN_ZC, win_hw - TUCK, win_hh - TUCK, (win_r - TUCK) if win_r > 0 else (win_r + TUCK),
                          CORNER_SEGS)
    win_lip = rounded_rect(0.0, WIN_ZC, win_hw + LIP, win_hh + LIP, (win_r + LIP) if win_r > 0 else (win_r - LIP * 0.4),
                           CORNER_SEGS)
    for (y, z) in win_lip:
        if not inside(back_outline, y, z):
            raise SystemExit("make_screen_housing: the window (%.2f %.2f) is past the back face: a smaller opening "
                             "or a bigger housing" % (y, z))

    def to_axis(zc):
        return lambda c: Vector((0, -c.y, zc - c.z)).normalized()

    r_lip = ring(X_BACK, win_lip)
    r_open = ring(X_BACK + LIP_DEPTH, win_in)
    r_wend = ring(WALL_END, win_in)
    bridge(r_lip, r_open, lambda c: (to_axis(WIN_ZC)(c) + Vector((-1, 0, 0))).normalized(), "lip")
    bridge(r_open, r_wend, to_axis(WIN_ZC), "wall")
    face(r_wend, (-1, 0, 0), "floor", shared=True)  # the window's back, behind the glass (closed: no see-through)

    # The grille: a recess, its floor dark, two slats across it.
    vent = rounded_rect(0.0, VENT_ZC, VENT_HW, VENT_HH, VENT_R, 2)
    r_vent = ring(X_BACK, vent)
    r_vfloor = ring(X_BACK + VENT_DEPTH, vent)
    bridge(r_vent, r_vfloor, to_axis(VENT_ZC), "wall")
    face(r_vfloor, (-1, 0, 0), "floor", shared=True)
    h = 2 * VENT_HH / 5
    for k in (1, 3):
        z0, z1 = VENT_ZC - VENT_HH + h * k, VENT_ZC - VENT_HH + h * (k + 1)
        xb, xf = X_BACK + SLAT_DEPTH, X_BACK + VENT_DEPTH
        a = [vert(xb, VENT_HW, z0), vert(xb, -VENT_HW, z0), vert(xb, -VENT_HW, z1), vert(xb, VENT_HW, z1)]
        b = [vert(xf, VENT_HW, z0), vert(xf, -VENT_HW, z0), vert(xf, -VENT_HW, z1), vert(xf, VENT_HW, z1)]
        face(a, (-1, 0, 0), "side")
        face([a[0], a[1], b[1], b[0]], (0, 0, -1), "wall")
        face([a[3], a[2], b[2], b[3]], (0, 0, 1), "side")

    # The back face: the outline round the window's lip and the grille, filled.
    edges = []
    for r in (r_back, r_lip, r_vent):
        for i in range(len(r)):
            edges.append(bm.edges.get((r[i], r[(i + 1) % len(r)])) or bm.edges.new((r[i], r[(i + 1) % len(r)])))
    filled = bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=edges, normal=(-1, 0, 0))
    holes = [win_lip, vent]
    drop = []
    for f in [g for g in filled["geom"] if isinstance(g, bmesh.types.BMFace)]:
        c = f.calc_center_median()
        if any(inside(hp, c.y, c.z) for hp in holes):
            drop.append(f)
            continue
        f.normal_update()
        if f.normal.x > 0:
            f.normal_flip()
        groups[f] = ("back", True)
    bmesh.ops.delete(bm, geom=drop, context='FACES_ONLY')
    print("housing: back face %d triangles (%d in the holes dropped)" % (
        sum(1 for f, g in groups.items() if g[0] == "back" and f.is_valid), len(drop)))

    # Bolt heads (back face) and rivets (sides): small square heads with bevelled sides.
    def stud(centre, axis_out, u, v, half, up):
        c = Vector(centre)
        o = Vector(axis_out)
        base = [c + (u * su + v * sv) * half for su, sv in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
        top = [c + o * up + (u * su + v * sv) * half * 0.6 for su, sv in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
        bv = [vert(*p) for p in base]
        tv = [vert(*p) for p in top]
        face(tv, o, "bolt")
        for i in range(4):
            j = (i + 1) % 4
            mid = (base[i] + base[j]) / 2 - c
            face([bv[i], bv[j], tv[j], tv[i]], (mid.normalized() + o).normalized(), "bevel")  # the head dark, its sides catching the light

    for (y, z) in BOLTS:
        if not inside(back_outline, y + BOLT_HALF, z) or not inside(back_outline, y - BOLT_HALF, z):
            raise SystemExit("make_screen_housing: bolt %.2f %.2f past the back face" % (y, z))
        stud((X_BACK + 0.002, y, z), (-1, 0, 0), Vector((0, 1, 0)), Vector((0, 0, 1)), BOLT_HALF, BOLT_UP)
    for s in (1, -1):
        stud((RIVET_X, s * (HALF_W - 0.002), RIVET_Z), (0, s, 0), Vector((1, 0, 0)), Vector((0, 0, 1)), RIVET_HALF,
             RIVET_UP)

    # Attributes: new everything (the export appends them), tagged as the housing.
    hfaces = [f for f in groups if f.is_valid]
    for v in new_verts:
        v[lay_vert] = -1
        for r in refs:
            if r is not None:
                v[r] = v.co
    # The export's rule: a new vertex rides its nearest old vertex. Each must be the slide's.
    bad = 0
    for v in new_verts:
        best, bi = 1e9, -1
        for i, o in enumerate(old):
            d = (o.co - v.co).length_squared
            if d < best:
                best, bi = d, i
        if bi not in slide:
            bad += 1
            print("housing: new vertex %s rides %s (not the slide's)" % (tuple(round(c, 2) for c in v.co),
                                                                       tuple(round(c, 2) for c in old[bi].co)))
        v[shapes[0]] = v.co
        v[shapes[1]] = v.co + (delta if bi in slide else Vector())
    if bad:
        raise SystemExit("make_screen_housing: %d new vertices are nearer the frame than the slide: they would not "
                         "move with it" % bad)

    # UVs: each group on its skin rectangle, planar (the back face and the floor shared, so they don't break up).
    def plane(f):
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        return [(1, 2), (0, 2), (0, 1)][ax]

    shared_bounds = {}
    for f, (g, shared) in groups.items():
        if f.is_valid and shared:
            a, b = plane(f)
            lo, hi = shared_bounds.get(g, ((1e9, 1e9), (-1e9, -1e9)))
            for v in f.verts:
                lo = (min(lo[0], v.co[a]), min(lo[1], v.co[b]))
                hi = (max(hi[0], v.co[a]), max(hi[1], v.co[b]))
            shared_bounds[g] = (lo, hi)
    for f, (g, shared) in groups.items():
        if not f.is_valid:
            continue
        x0, y0, x1, y1 = REGION[g]
        a, b = plane(f)
        if shared:
            lo, hi = shared_bounds[g]
        else:
            lo = (min(v.co[a] for v in f.verts), min(v.co[b] for v in f.verts))
            hi = (max(v.co[a] for v in f.verts), max(v.co[b] for v in f.verts))
        ka = min(PPU, (x1 - x0) / max(hi[0] - lo[0], 1e-6))
        kb = min(PPU, (y1 - y0) / max(hi[1] - lo[1], 1e-6))
        for l in f.loops:
            # u along the first axis (seen from behind, +Y is to the left: flip so the texture isn't mirrored);
            # v down the second (z up = up the skin).
            pa = (hi[0] - l.vert.co[a]) * ka if a == 1 else (l.vert.co[a] - lo[0]) * ka
            pb = (hi[1] - l.vert.co[b]) * kb
            l[uvl].uv = ((x0 + pa) / SKIN_W, 1.0 - (y0 + pb) / SKIN_H)
            if lay_vid is not None:
                l[lay_vid] = -1
        f[tag] = 1
        if lay_tri is not None:
            f[lay_tri] = -1
    tri = bmesh.ops.triangulate(bm, faces=hfaces)
    ntri = sum(1 for f in bm.faces if f[tag] == 1)
    print("housing: %d new vertices, %d triangles (the gun's own: %d)" % (len(new_verts), ntri,
                                                                         len(bm.faces) - ntri))
    print("housing: glass centre %.3f 0 %.3f (its plane %.3f behind the back face), half %.3f x %.3f" % (
        X_BACK + GLASS_DEPTH, WIN_ZC, GLASS_DEPTH, win_hw, win_hh))
    if dry:
        bm.free()
        print("housing: --dry, not saved")
        return
    bm.to_mesh(me)
    bm.free()
    me.update()
    # The ammo screen's and button's anchors were 0.107 off the slide's left side (y 1.3 on a side at 1.2): the grown
    # box's grid moves the surface under them a little, and the export then refuses an anchor off the surface. On it.
    for name in ("av wpntxt_av", "av wpnbtn_av"):
        e = bpy.data.objects.get(name)
        if e is not None and abs(e.location.y - 1.3) < 1e-3:
            e.location.y = 1.2
            print("housing: %s put on the slide's side: %s" % (name, tuple(round(c, 3) for c in e.location)))
    bpy.ops.wm.save_mainfile(compress=True)
    print("housing: saved %s" % bpy.data.filepath)


main()
