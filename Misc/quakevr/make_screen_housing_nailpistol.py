# make_screen_housing_nailpistol.py -- the nail pistol's ammo screen housing, modelled on its source .blend.
#
# Usage, from the repository root (the options after the separator: [--dry] [--save other.blend]):
#   Git Bash:   "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup \
#                   Misc/quakevr/src_models/blend/v_nailpistol/v_nailpistol.blend \
#                   --python Misc/quakevr/make_screen_housing_nailpistol.py -- --dry
#   PowerShell: & "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup `
#                   Misc\quakevr\src_models\blend\v_nailpistol\v_nailpistol.blend `
#                   --python Misc\quakevr\make_screen_housing_nailpistol.py '--' --dry
#   (PowerShell eats a bare --: quote it. Leave out `-- --dry` to export and save.)
# Builds or moves the housing, exports the model as blend_export.py -- --grow does (quakevr/progs, weapons.txt: the
# housing is past the model's first box) and saves the .blend (in place, or as --save) as the export left it. Ends with
# "housing_nailpistol: ok, ..." or "housing_nailpistol: REFUSED: ..." (exit code 1). --dry: builds and checks it,
# exports and saves nothing.
# The three housing scripts work alike (housing_common.py; docs/vr-port/MODELS_IN_BLENDER.md, "Tweaking the ammo
# screen housings"): run again on a .blend that has the housing (its vertices carry the point attribute `qvr_housing`;
# a .blend from before the tag: its faces did), with the same shape (as many vertices) its vertices are moved and
# re-mapped in place, so the model's file keeps its vertices; otherwise the old housing is deleted and built anew (its
# old vertices then stay in the .mdl unused: start from the .blend before the housing, in git history, to keep the
# file lean).
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
# The housing moves with the slide in every frame: the export makes a new vertex ride its nearest old one, so each
# new vertex's nearest vertex of the gun must be the slide's (refused otherwise).
#
# Textures: the gun's own skin (no new texels): the back face and the sides on its rusted iron, the bevel and the lip
# on its worn pale edge strip, the walls on dark iron, the grille's floor black, bolt heads on dark iron.
import os
import re
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import housing_common as hc  # noqa: E402

WEAPONS_TXT = os.path.join(hc.ROOT, "quakevr", "data", "weapons.txt")
STEM = "nailpistol"
MODEL = "v_nailpistol"
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


def wpntxt_scale():
    txt = open(WEAPONS_TXT, encoding="utf-8").read()
    m = re.search(r"^weapon %d\s*\{(.*?)^\}" % SLOT, txt, re.S | re.M)
    s = re.search(r'wpntxt_scale "([0-9.]+)"', m.group(1))
    return float(s.group(1))


def build(h, win_hw, win_hh, win_r):
    """The housing on its own: its vertices (in the order made) and triangles (housing_common's (i, j, k, uvs))."""
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new()
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
            h.fail("the window (%.2f %.2f) is past the back face: a smaller opening or a bigger housing" % (y, z))

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
    hs = 2 * VENT_HH / 5
    for k in (1, 3):
        z0, z1 = VENT_ZC - VENT_HH + hs * k, VENT_ZC - VENT_HH + hs * (k + 1)
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
            h.fail("bolt %.2f %.2f past the back face" % (y, z))
        stud((X_BACK + 0.002, y, z), (-1, 0, 0), Vector((0, 1, 0)), Vector((0, 0, 1)), BOLT_HALF, BOLT_UP)
    for s in (1, -1):
        stud((RIVET_X, s * (HALF_W - 0.002), RIVET_Z), (0, s, 0), Vector((1, 0, 0)), Vector((0, 0, 1)), RIVET_HALF,
             RIVET_UP)

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
    bmesh.ops.triangulate(bm, faces=[f for f in groups if f.is_valid])
    index = {v: i for i, v in enumerate(new_verts)}
    cos = [v.co.copy() for v in new_verts]
    tris = []
    for f in bm.faces:
        ids = [index[l.vert] for l in f.loops]
        tris.append((ids[0], ids[1], ids[2], tuple(l[uvl].uv.copy() for l in f.loops)))
    bm.free()
    return cos, tris


def main(h):
    radius_px, size, (open_w, open_h) = h.style["radius"], h.style["size"], h.style["opening"]
    if open_w <= 0 or open_h <= 0:
        h.fail("the \"%s\" entry has no .opening" % STEM)
    pix = PIXEL_AT_0205 * wpntxt_scale() / 0.205 * size
    win_hw, win_hh, win_r = open_w * pix, open_h * pix, radius_px * pix
    print("housing: font pixel %.5f, window half %.3f x %.3f, corner %.3f (%s)" % (
        pix, win_hw, win_hh, win_r, "rounded" if radius_px > 0 else "chamfered"))
    cos, tris = build(h, win_hw, win_hh, win_r)

    ob = bpy.data.objects[MODEL]
    me = ob.data

    def legacy(bm):
        """A housing made before the vertex tag: its faces carried it (a face attribute, dropped here)."""
        lay = bm.faces.layers.int.get(hc.TAG)
        if lay is None:
            return None
        faces = [f for f in bm.faces if f[lay] == 1]
        old = sorted({v for f in faces for v in f.verts if all(g[lay] == 1 for g in v.link_faces)},
                     key=lambda v: v.index)
        ids = [v.index for v in old]
        bm.faces.layers.int.remove(lay)
        bm.verts.ensure_lookup_table()
        return [bm.verts[i] for i in ids]

    # The slide: the gun's vertices that move between the frames (the housing's own left out). The export's rule: a new
    # vertex rides its nearest old vertex: each of the housing's must be the slide's.
    bm = bmesh.new()
    bm.from_mesh(me)
    tag = bm.verts.layers.int.get(hc.TAG)
    house = set(v for v in bm.verts if v[tag] > 0) if tag is not None else set(legacy(bm) or [])
    shapes = list(bm.verts.layers.shape.values())
    old = [(v.co.copy(), (v[shapes[1]] - v[shapes[0]]).length > 1e-4) for v in bm.verts if v not in house]
    delta = next(Vector(v[shapes[1]] - v[shapes[0]]) for v in bm.verts
                 if v not in house and (v[shapes[1]] - v[shapes[0]]).length > 1e-4)
    bm.free()
    rides, bad = [], 0
    for co in cos:
        near_co, on_slide = min(old, key=lambda o: (o[0] - co).length_squared)
        rides.append(on_slide)
        if not on_slide:
            bad += 1
            print("housing: new vertex %s rides %s (not the slide's)" % (tuple(round(c, 2) for c in co),
                                                                       tuple(round(c, 2) for c in near_co)))
    if bad:
        h.fail("%d of the housing's vertices are nearer the frame than the slide: they would not move with it" % bad)

    def new_vert(bm, v, i):
        sl = list(bm.verts.layers.shape.values())
        v[sl[0]] = cos[i]
        v[sl[1]] = cos[i] + (delta if rides[i] else Vector())
        for p in range(len(sl)):
            r = bm.verts.layers.float_vector.get("qvr_ref%d" % p)
            if r is not None:
                v[r] = cos[i]

    h.apply(me, cos, tris, new_vert=new_vert, legacy=legacy)
    print("housing: glass centre %.3f 0 %.3f (its plane %.3f behind the back face), half %.3f x %.3f" % (
        X_BACK + GLASS_DEPTH, WIN_ZC, GLASS_DEPTH, win_hw, win_hh))
    # The ammo screen's and button's anchors were 0.107 off the slide's left side (y 1.3 on a side at 1.2): the grown
    # box's grid moves the surface under them a little, and the export then refuses an anchor off the surface. On it.
    for name in ("av wpntxt_av", "av wpnbtn_av"):
        e = bpy.data.objects.get(name)
        if e is not None and abs(e.location.y - 1.3) < 1e-3:
            e.location.y = 1.2
            print("housing: %s put on the slide's side: %s" % (name, tuple(round(c, 3) for c in e.location)))
    h.finish("%d vertices, %d triangles (opening %.1f x %.1f px, radius %.1f)" % (
        len(cos), len(tris), open_w, open_h, radius_px), grow=True)


hc.Run(STEM, MODEL).main(main)
