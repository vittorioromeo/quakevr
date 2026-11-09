# make_screen_housing_ssg.py -- the super shotgun's ammo screen housing, modelled on v_shot2.blend (ROUND24.md, "The super
# shotgun's screen housing"). Run headless in Blender on the gun's source .blend from before the housing:
#
#   blender -b <v_shot2.blend without the housing> --python Misc/quakevr/make_screen_housing_ssg.py -- --out <v_shot2.blend>
#
# then export it (blend_export.py) as any edit. It refuses a .blend that already has the housing (the object's
# `qvr_housing` property): the .blend is the source from then on; this script is the record of how it was made and
# makes it again from the .blend before it (git history) if the shape needs changing.
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
# (printing the values that do). The glass's place and axes on the model with the table's offset 0 are measured in the
# game (vr_debug_screen_where 1: GLASS0, RIGHT, UP, PIXEL below).
# Texture: a new patch in free skin (texels 200-248 x 122-146 from the top): the gun's own metal (copied from the sloped
# face's texels), lit on the bezel top's inner edge, darker on the walls, near black under the glass, darker rivet heads
# with a lit edge.

import math
import os
import re
import struct
import sys

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE_CPP = os.path.join(HERE, "..", "..", "Quake", "vr", "vr_screenstyle.cpp")
MDL = os.path.join(HERE, "..", "..", "quakevr", "progs", "v_shot2.mdl")

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
RIM_R = 0.16    # the bezel's flat top beside the opening
RIM_U = 1       # and over and under it, in rows
TOP = 0.10      # the top over the glass
FLOOR = 0.06    # the dark floor under the glass
SKIRT = 0.45    # the skirt down from the glass, sunk into the gun

# The rivets (a hexagon two grid steps long in x).
RIVET_HALF_Y = 0.17
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
SHADES = {"top": 1.5, "wall": 0.6, "skirt": 0.9, "floor": 0.0, "rivet": 0.55, "rivettop": 0.6}


def style_entry():
    src = open(STYLE_CPP, encoding="utf-8").read()
    m = re.search(r'\{"shot2",\s*\{(.*?)\}\},', src, re.S)
    if not m:
        sys.exit("make_screen_housing_ssg: no \"shot2\" entry in vr_screenstyle.cpp")
    body = m.group(1)

    def num(name):
        mm = re.search(r"\.%s\s*=\s*(-?[0-9.]+)f?" % name, body)
        return float(mm.group(1)) if mm else 0.0

    def vec(name, n):
        mm = re.search(r"\.%s\s*=\s*\{([^}]*)\}" % name, body)
        return [float(v.strip().rstrip("f")) for v in mm.group(1).split(",")] if mm else [0.0] * n

    return num("radius"), vec("opening", 2), vec("offset", 3), num("size")


def grid():
    d = open(MDL, "rb").read(32)
    return Vector(struct.unpack("<3f", d[8:20])), Vector(struct.unpack("<3f", d[20:32]))


SCALE, ORIGIN = grid()
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
    def __init__(self, bm):
        self.bm = bm
        self.uv = bm.loops.layers.uv.active
        self.ivert = bm.verts.layers.int.get("qvr_vert")
        self.icorner = bm.loops.layers.int.get("qvr_vid")
        self.iface = bm.faces.layers.int.get("qvr_tri")
        self.tris = 0
        self.bad = 0

    def verts(self, cos):
        out = []
        for co in cos:
            v = self.bm.verts.new(co)
            if self.ivert is not None:
                v[self.ivert] = -1
            out.append(v)
        return out

    def tri(self, vs, uvs, want):
        """A triangle, turned so its normal is along `want` (counted bad if it is too thin or stands across it)."""
        n = (vs[1].co - vs[0].co).cross(vs[2].co - vs[0].co)
        if n.dot(want) < 0:
            vs = [vs[0], vs[2], vs[1]]
            uvs = [uvs[0], uvs[2], uvs[1]]
        if n.length < 1e-6 or abs(n.normalized().dot(want.normalized())) < 0.05:
            self.bad += 1
            print("housing: a bad triangle at %.2f %.2f %.2f" % tuple((vs[0].co + vs[1].co + vs[2].co) / 3))
        f = self.bm.faces.new(vs)
        if self.iface is not None:
            f[self.iface] = -1
        for loop, uv in zip(f.loops, uvs):
            loop[self.uv].uv = uv
            if self.icorner is not None:
                loop[self.icorner] = -1
        self.tris += 1
        return f

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
            if (a.co - c.co).length <= (b.co - d.co).length:
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
            d = v.co - centre_co
            return region_uv(region, min(1, max(0, 0.5 + 0.5 * d.dot(ax) / ha)),
                             min(1, max(0, 0.5 + 0.5 * d.dot(ay) / hb)))

        n = len(loop)
        for i in range(n):
            j = (i + 1) % n
            self.tri([c, loop[i], loop[j]], [uv_of(c), uv_of(loop[i]), uv_of(loop[j])], want)


def surface_z(ob, dg, x, y):
    """The gun's top surface's height at (x, y)."""
    ok, loc, nrm, _ = ob.ray_cast(Vector((x, y, 12.0)), Vector((0, 0, -1)), distance=20.0, depsgraph=dg)
    if not ok:
        sys.exit("make_screen_housing_ssg: no surface under %.2f %.2f" % (x, y))
    return loc.z


def paint(image):
    w, h = image.size
    px = list(image.pixels[:])

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
                    rgb = [c * 2.0 for c in src]  # the lit inner edge (the band's inner ring is v 0)
                elif name == "rivettop" and (x - x0) + (y - y0) >= 9:
                    rgb = [c * 2.3 for c in src]  # the lit upper edge of the head
                else:
                    rgb = [c * s for c in src]
                put(PATCH[0] + x, PATCH[1] + y, rgb)
    image.pixels[:] = px


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if "--out" not in args:
        sys.exit("usage: blender -b <v_shot2.blend> --python make_screen_housing_ssg.py -- --out <v_shot2.blend>")
    out = os.path.abspath(args[args.index("--out") + 1])

    ob = bpy.data.objects["v_shot2"]
    if "qvr_housing" in ob.keys():
        sys.exit("make_screen_housing_ssg: %s already has the housing (%s)" % (bpy.data.filepath, ob["qvr_housing"]))
    global PIXEL
    radius, opening, offset, size = style_entry()
    PIXEL = PIXEL1 * size
    if opening[0] <= 0 or opening[1] <= 0 or radius > 0:
        sys.exit("make_screen_housing_ssg: the \"shot2\" entry needs an opening, square or chamfered (radius <= 0)")
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
    if off and "--any" not in args:
        sys.exit("make_screen_housing_ssg: the table's \"shot2\" entry doesn't match the modelled opening (above); "
                 "set it and run again (--any: write anyway)")

    dg = bpy.context.evaluated_depsgraph_get()
    me = ob.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bld = Builder(bm)
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
    bezel_tris = bld.tris

    # The rivets, one each side, level with the window's middle: a hexagon two x steps long, on the surface.
    sx = SCALE.x
    for side in (-1.0, 1.0):
        p = glass + UP * mid_u + RIGHT * (side * (a_out + RIVET_GAP + RIVET_HALF_Y) + mid_r)
        xc, yc = snap(p).x, p.y
        hy = RIVET_HALF_Y
        foot = [(xc + sx, yc - hy * 0.5), (xc + sx, yc + hy * 0.5), (xc, yc + hy),
                (xc - sx, yc + hy * 0.5), (xc - sx, yc - hy * 0.5), (xc, yc - hy)]

        def lift(h):
            return [snap(Vector((x, y, surface_z(ob, dg, x, y) + h))) for x, y in foot]

        base, rim = bld.verts(lift(-0.06)), bld.verts(lift(SCALE.z * 1.5))
        zc = surface_z(ob, dg, xc, yc)
        apex = snap(Vector((xc, yc, zc + SCALE.z * 3.2)))
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

    bm.to_mesh(me)
    bm.free()
    me.update()
    print("housing: %d triangles (bezel %d, rivets %d), %d bad" % (bld.tris, bezel_tris, bld.tris - bezel_tris, bld.bad))

    # The skin patch, saved beside the new .blend.
    image = bpy.data.images["v_shot2_skin0"]
    image.pixels[0]  # (loads it)
    paint(image)
    png = os.path.join(os.path.dirname(out), "v_shot2_skin0.png")
    image.filepath_raw = png
    image.file_format = 'PNG'
    image.save()
    ob["qvr_housing"] = "ssg screen housing (make_screen_housing_ssg.py)"
    bpy.ops.wm.save_as_mainfile(filepath=out, relative_remap=True)
    image.filepath = "//v_shot2_skin0.png"
    bpy.ops.wm.save_mainfile()
    print("housing: wrote %s and %s" % (out, png))


main()
