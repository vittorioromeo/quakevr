# make_screen_housing_ssg.py -- the super shotgun's ammo screen housing, modelled on v_shot2.blend (ROUND24.md, "The super
# shotgun's screen housing").
#
# Usage, from the repository root (the options after the separator: [--dry] [--save other.blend] [--any]):
#   Git Bash:   "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup \
#                   Misc/quakevr/src_models/blend/v_shot2/v_shot2.blend \
#                   --python Misc/quakevr/make_screen_housing_ssg.py -- --dry
#   PowerShell: & "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup `
#                   Misc\quakevr\src_models\blend\v_shot2\v_shot2.blend `
#                   --python Misc\quakevr\make_screen_housing_ssg.py '--' --dry
#   (PowerShell eats a bare --: quote it. Leave out `-- --dry` to export and save.)
# Builds or moves the housing, paints its skin patch, exports the model as blend_export.py does (quakevr/progs,
# weapons.txt) and saves the .blend (in place, or as --save) as the export left it. Ends with "housing_shot2: ok, ..."
# or "housing_shot2: REFUSED: ..." (exit code 1). --dry: builds and checks it, exports and saves nothing.
# The three housing scripts work alike (housing_common.py; docs/vr-port/MODELS_IN_BLENDER.md, "Tweaking the ammo
# screen housings"): run again on a .blend that has the housing (its vertices carry the point attribute `qvr_housing`;
# a .blend from before the tag: the object's `qvr_housing` property and its new vertices), with the same shape (as
# many vertices) its vertices are moved and re-mapped in place, so the model's file keeps its vertices; otherwise the
# old housing is deleted and built anew (its old vertices then stay in the .mdl unused: start from the .blend before
# the housing, in git history, to keep the file lean).
#
# What it adds, on the angled top of the breech block (the two sloped faces behind the rib's sight block):
# - a low bezel round a small chamfered window: an inner wall from under the glass to the top, a flat top (its inner
#   edge lit), a skirt sunk into the gun (the top stands about 0.1-0.2 units proud of the sloped faces); a dark floor
#   under the glass;
# - one rivet each side of the window, level with its middle (symmetrical): a low domed hexagon on the surface.
#
# One definition with the glass: the window's size and chamfer are the "shot2" entry's `opening` and `radius` in
# Quake/vr/vr_screenstyle.cpp, and its `offset` (out) the glass's depth. The model's vertices are bytes, and this gun's
# grid is coarse along its length (0.227 units in x, 0.037 in y, 0.056 in z): the window's top and bottom edges land on
# rows a quarter of a unit apart, so the script snaps the bezel to the grid itself, measures the opening the snapped
# walls make at the glass's depth, and refuses unless the table's opening, chamfer and offset (right, up) match it
# (printing the values that do; --any: build it anyway). The glass's place and axes on the model with the table's
# offset 0 are measured in the game (vr_debug_screen_where 1: GLASS0, RIGHT, UP, PIXEL below).
# Texture: a patch in free skin (texels 200-248 x 122-146 from the top): the gun's own metal (copied from the sloped
# face's texels), lit on the bezel top's inner edge, darker on the walls, near black under the glass, darker rivet heads
# with a lit edge. Painted again on every run (the skin's .png written only when that changes it).

import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import housing_common as hc  # noqa: E402

STEM = "shot2"
MODEL = "v_shot2"

# Measured with vr_debug_screen_where 1 (weapons.txt slot 2's wpntxt offsets and pitch 66.5, the table's offset 0).
GLASS0 = Vector((3.4724, -0.0296, 6.1556))
RIGHT = Vector((0.0, -1.0, 0.0))
UP = Vector((0.9171, 0.0, 0.3988)).normalized()
OUT = RIGHT.cross(UP).normalized()  # (-0.40, 0, 0.92): up and back, as the sloped faces
PIXEL1 = 0.0465                      # a font pixel in model units (wpntxt_scale 0.25, style size 1)
MODEL_PER_UNIT = PIXEL1 / (0.1 * 0.25)  # model units per world unit (the style's offset is in world units)
PIXEL = PIXEL1                       # times the style's size (main)

# The bezel (model units; "rows" along UP are the grid's x steps).
TUCK = 0.02     # the glass this much bigger than the opening all round: the walls hide its last pixel
RIM_R = 0.18    # the bezel's flat top beside the opening
RIM_U = 1       # and over and under it, in rows
TOP = 0.10      # the top over the glass
FLOOR = 0.06    # the dark floor under the glass
SKIRT = 0.45    # the skirt down from the glass, sunk into the gun

# The rivets (a hexagon two grid steps long in x).
RIVET_HALF_Y = 0.18
RIVET_GAP = 0.05  # from the bezel's outer edge to the rivet's nearest point

# The skin patch (texels; Blender's image rows are bottom-up: the patch is the image's bottom 24 rows).
SKIN_W, SKIN_H = 512, 146
PATCH = (200, 0)
REGIONS = {  # x0, y0, x1, y1 within the patch
    "top": (0, 0, 16, 8),
    "wall": (16, 0, 32, 8),
    "skirt": (0, 8, 16, 16),
    "floor": (32, 0, 40, 8),
    "rivet": (40, 0, 48, 8),
    "rivettop": (40, 8, 48, 16),
}
METAL_SRC = (90, 118)  # a 16x16 block of the sloped face's own metal (its UVs: texels 88-108 x 108-145 bottom-up)
SHADES = {"top": 1.8, "wall": 0.6, "skirt": 0.9, "floor": 0.0, "rivet": 0.7, "rivettop": 0.8}

SCALE, ORIGIN = (Vector(v) for v in hc.mdl_grid(MODEL))
ROW = SCALE.x / UP.x  # one x step along UP


def snap(p):
    return Vector(tuple(ORIGIN[i] + round((p[i] - ORIGIN[i]) / SCALE[i]) * SCALE[i] for i in range(3)))


def ring(centre, a, b, c_r, c_u, h):
    """A chamfered rectangle in the glass's plane (half sizes a along RIGHT, b along UP; the corners cut c_r by c_u),
    `h` along OUT, snapped to the model's grid: 8 points, counter-clockwise seen from OUT."""
    pts2 = [(a, b - c_u), (a - c_r, b), (-(a - c_r), b), (-a, b - c_u),
            (-a, -(b - c_u)), (-(a - c_r), -b), (a - c_r, -b), (a, -(b - c_u))]
    return [snap(centre + RIGHT * x + UP * y + OUT * h) for x, y in pts2]


def at_glass(lo, hi, h_lo, h_hi, centre):
    """Where the wall from `lo` (at h_lo) to `hi` (at h_hi) crosses the glass's plane, in the glass's 2D."""
    t = (0.0 - h_lo) / (h_hi - h_lo)
    p = lo.lerp(hi, t) - centre
    return p.dot(RIGHT), p.dot(UP)


def region_uv(name, fx, fy):
    x0, y0, x1, y1 = REGIONS[name]
    px = PATCH[0] + x0 + 0.5 + (x1 - x0 - 1.0) * fx
    py = PATCH[1] + y0 + 0.5 + (y1 - y0 - 1.0) * fy
    return Vector((px / SKIN_W, py / SKIN_H))


class Builder:
    """The housing's vertices (positions, in the order made) and triangles (housing_common's (i, j, k, uvs))."""

    def __init__(self):
        self.co = []
        self.tris = []
        self.bad = 0

    def verts(self, cos):
        self.co.extend(cos)
        return list(range(len(self.co) - len(cos), len(self.co)))

    def tri(self, vs, uvs, want):
        """A triangle, turned so its normal is along `want` (counted bad if it is too thin or stands across it)."""
        p = [self.co[i] for i in vs]
        n = (p[1] - p[0]).cross(p[2] - p[0])
        if n.dot(want) < 0:
            vs = [vs[0], vs[2], vs[1]]
            uvs = [uvs[0], uvs[2], uvs[1]]
        if n.length < 1e-6 or abs(n.normalized().dot(want.normalized())) < 0.05:
            self.bad += 1
            print("housing: a bad triangle at %.2f %.2f %.2f" % tuple((p[0] + p[1] + p[2]) / 3))
        self.tris.append((vs[0], vs[1], vs[2], tuple(uvs)))

    def band(self, inner, outer, region, want_fn):
        """Quads between two rings of equal length; want_fn(i) the outward direction of the i-th quad."""
        n = len(inner)
        for i in range(n):
            j = (i + 1) % n
            u0, u1 = i / n, (i + 1) / n
            a, b, c, d = inner[i], inner[j], outer[j], outer[i]
            ua, ub, uc, ud = (region_uv(region, u0, 0), region_uv(region, u1, 0), region_uv(region, u1, 1),
                              region_uv(region, u0, 1))
            want = want_fn(i)
            # split along the shorter diagonal
            if (self.co[a] - self.co[c]).length <= (self.co[b] - self.co[d]).length:
                self.tri([a, b, c], [ua, ub, uc], want)
                self.tri([a, c, d], [ua, uc, ud], want)
            else:
                self.tri([a, b, d], [ua, ub, ud], want)
                self.tri([b, c, d], [ub, uc, ud], want)

    def fan(self, loop, centre_co, region, want, axes):
        """A fan over a closed ring, planar-mapped into `region` (axes: the two directions and the half sizes)."""
        (c,) = self.verts([centre_co])
        (ax, ha), (ay, hb) = axes

        def uv_of(v):
            d = self.co[v] - centre_co
            return region_uv(region, min(1, max(0, 0.5 + 0.5 * d.dot(ax) / ha)),
                             min(1, max(0, 0.5 + 0.5 * d.dot(ay) / hb)))

        n = len(loop)
        for i in range(n):
            j = (i + 1) % n
            self.tri([c, loop[i], loop[j]], [uv_of(c), uv_of(loop[i]), uv_of(loop[j])], want)


def paint(image):
    """Paints the skin patch. Returns whether that changed the image."""
    w, h = image.size
    before = list(image.pixels[:])
    px = list(before)

    def get(x, y):
        i = 4 * (y * w + x)
        return px[i:i + 3]

    def put(x, y, rgb):
        i = 4 * (y * w + x)
        px[i:i + 4] = [min(1.0, max(0.0, c)) for c in rgb] + [1.0]

    for name, (x0, y0, x1, y1) in REGIONS.items():
        s = SHADES[name]
        for y in range(y0, y1):
            for x in range(x0, x1):
                src = get(METAL_SRC[0] + (x - x0) % 16, METAL_SRC[1] + (y - y0) % 16)
                if name == "floor":
                    rgb = [0.05, 0.06, 0.055]
                elif name == "top" and y - y0 < 2:
                    rgb = [c * 2.4 for c in src]  # the lit inner edge (the band's inner ring is v 0)
                elif name == "rivettop" and (x - x0) + (y - y0) >= 9:
                    rgb = [c * 2.3 for c in src]  # the lit upper edge of the head
                else:
                    rgb = [c * s for c in src]
                put(PATCH[0] + x, PATCH[1] + y, rgb)
    image.pixels[:] = px
    after = image.pixels[:]  # (as the image holds them)
    return any(abs(a - b) > 1e-6 for a, b in zip(before, after))


def main(h):
    ob = bpy.data.objects[MODEL]
    me = ob.data
    global PIXEL
    radius, opening, offset, size = h.style["radius"], h.style["opening"], h.style["offset"], h.style["size"]
    PIXEL = PIXEL1 * size
    if opening[0] <= 0 or opening[1] <= 0 or radius > 0:
        h.fail("the \"shot2\" entry needs an opening, square or chamfered (radius <= 0)")
    # The glass's place: at its depth (the table's out) and moved along its plane (the table's right and up).
    glass = GLASS0 + OUT * offset[2] * MODEL_PER_UNIT
    target = (opening[0] * PIXEL - TUCK, opening[1] * PIXEL - TUCK,
              offset[0] * MODEL_PER_UNIT, offset[1] * MODEL_PER_UNIT)  # half sizes, middle (right, up)
    if radius < 0:
        c_r = c_u = -radius * PIXEL - TUCK * (math.sqrt(2.0) - 1.0)
    else:
        c_r, c_u = SCALE.y, 0.0  # square: the corner's second point one grid step along the edge

    def measure(a, b, sr, su):
        """The opening the snapped walls make at the glass's depth, designed as half sizes a, b about the glass's place
        moved sr, su: (half right, half up, middle right, middle up, cut up, cut right), and the two rings."""
        centre = glass + RIGHT * sr + UP * su
        floor = ring(centre, a, b, c_r, c_u, -FLOOR)
        wall_top = ring(centre, a, b, c_r, c_u, TOP)
        cut = [at_glass(lo, hi, -FLOOR, TOP, glass) for lo, hi in zip(floor, wall_top)]
        xs, ys = [p[0] for p in cut], [p[1] for p in cut]
        mr, mu = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2
        hr, hu = (max(xs) - min(xs)) / 2, (max(ys) - min(ys)) / 2
        return (hr, hu, mr, mu, hu - abs(cut[0][1] - mu), hr - abs(cut[1][0] - mr)), floor, wall_top, centre

    # The snapped design nearest the table's opening: along UP first (the coarse rows), then along RIGHT.
    steps = [k * 0.01 for k in range(-15, 16)]
    best_u = min(((b, su) for b in (target[1] + d for d in steps) for su in (target[3] + d for d in steps)),
                 key=lambda bs: (lambda m: abs(m[1] - target[1]) + abs(m[3] - target[3]))(
                     measure(target[0], bs[0], target[2], bs[1])[0]))
    fine = [k * 0.005 for k in range(-6, 7)]
    best_r = min(((a, sr) for a in (target[0] + d for d in fine) for sr in (target[2] + d for d in fine)),
                 key=lambda ar: (lambda m: abs(m[0] - target[0]) + abs(m[2] - target[2]))(
                     measure(ar[0], best_u[0], ar[1], best_u[1])[0]))
    a, b = best_r[0], best_u[0]
    (half_r, half_u, mid_r, mid_u, cut_u, cut_r), floor, wall_top, centre = measure(a, b, best_r[1], best_u[1])
    # The bezel's outside: its corners cut a row by a row (the grid's coarsest step), whatever the window's.
    a_out, b_out = a + RIM_R, b + RIM_U * ROW
    top_out = ring(centre, a_out, b_out, ROW, ROW, TOP)
    skirt = ring(centre, a_out, b_out, ROW, ROW, -SKIRT)
    want = {"opening": ((half_r + TUCK) / PIXEL, (half_u + TUCK) / PIXEL),
            "radius": 0.0 if cut_u < 0.01 else -((cut_u + cut_r) / 2 + TUCK * (math.sqrt(2.0) - 1.0)) / PIXEL,
            "offset": (mid_r / MODEL_PER_UNIT, mid_u / MODEL_PER_UNIT)}
    print("housing: the snapped opening %.3f x %.3f (half), cut %.3f along up, %.3f along right, its middle %.3f right %.3f up"
          % (half_r, half_u, cut_u, cut_r, mid_r, mid_u))
    print("housing: the table that matches: .radius = %.2ff, .offset = {%.3ff, %.3ff, %.3ff}, .opening = {%.2ff, %.2ff}" % (
        want["radius"], want["offset"][0], want["offset"][1], offset[2], *want["opening"]))
    off = (abs(want["opening"][0] - opening[0]) > 0.3 or abs(want["opening"][1] - opening[1]) > 0.3 or
           abs(want["radius"] - radius) > 0.6 or abs(want["offset"][0] - offset[0]) * MODEL_PER_UNIT > 0.3 * PIXEL or
           abs(want["offset"][1] - offset[1]) * MODEL_PER_UNIT > 0.3 * PIXEL)
    if off and "--any" not in h.argv:
        h.fail("the table's \"shot2\" entry doesn't match the modelled opening (the line above has the values that do); "
               "set it and run again (--any: build it anyway)")

    def legacy(bm):
        """A housing made before the tag: the object's property, its vertices the ones the export hasn't numbered."""
        lay = bm.verts.layers.int.get("qvr_vert")
        if "qvr_housing" not in ob.keys() or lay is None:
            return None
        return sorted((v for v in bm.verts if v[lay] < 0), key=lambda v: v.index)

    # The gun's surface without the housing (the rivets stand on it).
    bm = bmesh.new()
    bm.from_mesh(me)
    tag = bm.verts.layers.int.get(hc.TAG)
    house = [v for v in bm.verts if v[tag] > 0] if tag is not None else (legacy(bm) or [])
    bmesh.ops.delete(bm, geom=house, context='VERTS')
    surface = BVHTree.FromBMesh(bm)
    bm.free()

    def surface_z(x, y):
        """The gun's top surface's height at (x, y)."""
        loc = surface.ray_cast(Vector((x, y, 12.0)), Vector((0, 0, -1)), 20.0)[0]
        if loc is None:
            h.fail("no surface under %.2f %.2f" % (x, y))
        return loc.z

    bld = Builder()
    floor_v, wall_v, top_v, skirt_v = (bld.verts(r) for r in (floor, wall_top, top_out, skirt))

    def radial(i, loop):
        j = (i + 1) % len(loop)
        m = (loop[i] + loop[j]) * 0.5 - centre
        m -= OUT * m.dot(OUT)
        return m.normalized()

    bld.band(floor_v, wall_v, "wall", lambda i: -radial(i, floor))
    bld.band(wall_v, top_v, "top", lambda i: OUT)
    bld.band(top_v, skirt_v, "skirt", lambda i: radial(i, top_out))
    bld.fan(floor_v, snap(centre - OUT * FLOOR), "floor", OUT, ((RIGHT, a), (UP, b)))
    bld.fan(skirt_v, snap(centre - OUT * SKIRT), "skirt", -OUT, ((RIGHT, a_out), (UP, b_out)))  # closed
    bezel_tris = len(bld.tris)

    # The rivets, one each side, level with the window's middle: a hexagon two x steps long, on the surface.
    sx = SCALE.x
    for side in (-1.0, 1.0):
        p = glass + UP * mid_u + RIGHT * (side * (a_out + RIVET_GAP + RIVET_HALF_Y) + mid_r)
        xc, yc = snap(p).x, p.y
        hy = RIVET_HALF_Y
        foot = [(xc + sx, yc - hy * 0.5), (xc + sx, yc + hy * 0.5), (xc, yc + hy),
                (xc - sx, yc + hy * 0.5), (xc - sx, yc - hy * 0.5), (xc, yc - hy)]

        def lift(dz):
            return [snap(Vector((x, y, surface_z(x, y) + dz))) for x, y in foot]

        base, rim = bld.verts(lift(-0.06)), bld.verts(lift(SCALE.z * 1.5))
        zc = surface_z(xc, yc)
        apex = snap(Vector((xc, yc, zc + SCALE.z * 3.6)))
        bottom = snap(Vector((xc, yc, zc - 0.06)))
        up_n = Vector((-0.3, 0.0, 0.95))

        def out_of(i):
            j = (i + 1) % 6
            m = Vector(((foot[i][0] + foot[j][0]) / 2 - xc, (foot[i][1] + foot[j][1]) / 2 - yc, 0.0))
            return m.normalized()

        bld.band(base, rim, "rivet", out_of)
        bld.fan(rim, apex, "rivettop", up_n, ((Vector((1, 0, 0)), sx), (Vector((0, 1, 0)), hy)))
        bld.fan(base, bottom, "rivet", -up_n, ((Vector((1, 0, 0)), sx), (Vector((0, 1, 0)), hy)))  # closed
        print("housing: rivet %+d at %.3f %.3f %.3f" % (side, xc, yc, zc))

    h.apply(me, bld.co, bld.tris, legacy=legacy)
    ob["qvr_housing"] = "ssg screen housing (make_screen_housing_ssg.py)"

    # The skin patch: painted again; its .png written after the export when that changed it.
    image = bpy.data.images[MODEL + "_skin0"]
    image.pixels[0]  # (loads it)
    painted = paint(image)

    def save_skin():
        if painted:
            image.save()
            print("  wrote %s" % bpy.path.abspath(image.filepath))

    h.finish("%d vertices, %d triangles (bezel %d, rivets %d, %d bad; opening %.2f x %.2f px, radius %.2f); skin patch "
             "%s" % (len(bld.co), len(bld.tris), bezel_tris, len(bld.tris) - bezel_tris, bld.bad, opening[0],
                     opening[1], radius, "painted" if painted else "unchanged"), after_export=save_skin)


hc.Run(STEM, MODEL).main(main)
