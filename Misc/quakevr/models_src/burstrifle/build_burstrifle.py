# build_burstrifle.py -- makes the grunts' burst rifle (progs/v_gruntgun.mdl) from nothing, after Vittorio's reference
# (2026-11-06: a compact burst carbine about 65 cm long): a skeletal two-rod stock with a butt plate and a pad, a boxy
# receiver with a raised top (two grooves along it) and a side window, two small lamps on its left side (red, glowing;
# bronze), a box magazine ahead of a wooden pistol grip with a trigger guard, a thin barrel with a clamp band (the other
# hand's grip), a squared muzzle block vented by three holes a side, a rear notch with ears at the receiver's back and a
# front post on the muzzle block, its top exactly level with the ears' tops (the sight line). Dark blued steel and
# gunmetal, an oiled wood grip, dull brass fittings, light grime, little rust; flat faceted, the main edges bevelled.
#
#   blender -b --factory-startup -P Misc/quakevr/models_src/burstrifle/build_burstrifle.py -- --from-scratch
#
# It writes quakevr/progs/v_gruntgun.mdl (nine frames frame1..frame9, the same pose: QuakeC's), its baked normal map,
# slot 21's anchors and hotspot 1 in weapons.txt (and the AD variants' slots 27-30, which repeat them), and then makes the
# model's source .blend from them (source.build, as blend_build.py does): Misc/quakevr/src_models/blend/v_gruntgun/. That
# .blend is the source afterwards (docs/vr-port/MODELS_IN_BLENDER.md section 9): edit it and save. This script
# OVERWRITES it: --from-scratch says you mean that.
#
# The model space is the old burst rifle's (make_enemyguns.py's, cut from soldier.mdl), so slot 21's settings and the AD
# variants' add-ons (Quake/vr/vr_adgunart.cpp) hold it: +x along the barrel (to the muzzle), +y the gun's left, +z up;
# the origin the middle of the pistol grip; packed from the old corner CORNER (the settings scale about it); the bore at
# z BORE_Z, its muzzle face at x MUZZLE_X (slot 21's muzzle anchor); the butt pad's back at x -24.0 (the old one's -29.4:
# the reference is shorter behind the grip).

import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector, noise
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
QVR = os.path.normpath(os.path.join(HERE, "..", ".."))       # Misc/quakevr
ROOT = os.path.normpath(os.path.join(QVR, "..", ".."))
ADDONS = os.path.join(QVR, "blender", "addons")
for p in (QVR, ADDONS):
    if p not in sys.path:
        sys.path.insert(0, p)
import mdlgen  # noqa: E402

NAME = "v_gruntgun.mdl"
PROGS = os.path.join(ROOT, "quakevr", "progs")
SRC_DIR = os.path.join(QVR, "src_models", "blend", "v_gruntgun")
CORNER = (-29.9, -6.170675277709961, -4.599999904632568)  # the old model's scale_origin (vr_adgunart.cpp corner)
FRAMES = ["frame%d" % i for i in range(1, 10)]               # QuakeC's $frame lines (vr_enemyguns.qc)
SKIN_W, SKIN_H = 512, 512
SLOT = 21
# (The AD rifles, slots 27-30, are made from the OLD burst rifle, progs/vr_adrifle_base.mdl: never touched here.)
VARIANT_SLOTS = ()
WEAPON_SCALE = 0.39                                          # slot 21's Scale (the hotspots' frame: hotspot_value)

BORE_Z = 8.08
MUZZLE_X = 33.15
SIGHT_Z = 11.6          # the rear ears' tops and the front post's top: the sight line, level, on y 0
REAR_SIGHT_X = (-2.6, -1.7)
FRONT_POST_X = (27.9, 28.4)
NOTCH = 0.32            # the notch's half width (the post's is 0.22: it shows in the notch with a sliver each side)

# Parts (each face's material, painted by paint_texel).
STEEL, BLUED, BRASS, WOOD, RUBBER, ROD, LAMP_RED, LAMP_BRONZE = range(8)
PART_NAMES = ["steel", "blued", "brass", "wood", "rubber", "rod", "lamp red", "lamp bronze"]

# The receiver and the parts on it (model units: x0, x1, y0, y1, z0, z1). The reference's proportions: the pistol grip
# under the receiver's back, the stock (rods) a little longer than the receiver, the barrel showing between the
# receiver's front cap and the muzzle block; the butt 5.4 units nearer than the old gun's (the old one was long behind
# the grip), the grip and the muzzle where they were.
RECEIVER = (-3.0, 12.8, -2.2, 2.2, 3.1, 10.3)       # (round three: 0.6 taller, mostly downward: the sight line holds)
HOUSING = (-1.0, 12.2, -1.75, 1.75, 10.0, 11.15)
NOSE = (12.4, 15.6, -2.05, 2.05, 3.7, 10.0)         # the receiver's chamfered front block (bolt heads on its sides)
COLLAR_X, COLLAR_R = 17.2, 1.7                      # the octagonal collar round the barrel, its step down to it
BLOCK = (27.1, MUZZLE_X, -1.35, 1.35, BORE_Z - 1.35, BORE_Z + 1.35)
BORE_R, BORE_DEPTH = 0.55, 0.4946                   # the bore (its bottom, the muzzle anchor, on a step of the x grid)
# The stock (the reference's side view: an open triangle tapering to the receiver): a steel adapter block on the
# receiver's back, the upper rod straight back from its top to the butt block's top, the lower rod from its bottom
# (behind the grip), a short elbow, then down and back to the butt block's bottom; the butt block tall (1.5 x the
# receiver), upright, a rubber pad behind it.
ADAPTER = (RECEIVER[0] - 1.9, RECEIVER[0] + 0.2, -1.45, 1.45, 6.2, 9.95)
ROD_TOP_Z = 9.3                                     # the upper rod, level
ROD_LOW = ((ADAPTER[0] + 0.3, 7.0), (ADAPTER[0] - 1.3, 6.75))  # the lower rod's start and elbow (x, z)
ROD_R = 0.58
BUTT = (-23.3, -20.9, -1.7, 1.7, 1.3, 10.5)         # the tall butt block (the pad behind it, to x -24.0)
ROD_BUTT_LOW_Z = BUTT[4] + 1.0                      # the lower rod's end at the butt block's bottom
BAND = (22.0, 23.5)     # the clamp band round the barrel (the other hand's grip: hotspot 1)
BAND_R = 1.2
BARREL_R = 0.75
WINDOW = (2.6, 8.6, 6.6, 8.6)                       # the side window on the left (+y) side: x0, x1, z0, z1
LAMPS = ((11.15, LAMP_RED), (9.55, LAMP_BRONZE))    # their x on the left side, below and ahead of the window
LAMP_Z, LAMP_HALF = 4.95, 0.6
VENTS = (29.6, 30.75, 31.9)                         # the muzzle block's three vents (x), through it, radius VENT_R
VENT_R = 0.52
GROOVES_Y = (-0.62, 0.62)                           # the two grooves along the housing's top

# The wooden pistol grip (round three: curved, bulbous): lofted through sections across its axis, which rakes back
# GRIP_RAKE (dx/dz: 24 deg) through the origin (the fist's middle) and bends back a little more towards the foot. Each
# section: (t 0 top .. 1 bottom, front, back, half width); the front's finger curve under the guard and a bump for the
# middle finger, the back's narrow web under the receiver swelling into the palm, the foot flared, its bottom edge
# bevelled. Eight sides a section.
GRIP_RAKE = 0.45
GRIP_S = (2.9, -4.5)                                # the axis's top (in the steel frame) and bottom, along it
GRIP_BEND = 0.55                                    # how far back the foot bends off the straight rake
GRIP_LEVELS = (
    (0.00, 1.55, 1.40, 1.10),
    (0.17, 1.30, 1.25, 1.05),
    (0.38, 1.62, 1.80, 1.22),
    (0.58, 1.45, 2.18, 1.33),
    (0.80, 1.68, 2.20, 1.33),
    (0.93, 1.95, 2.45, 1.42),
    (1.00, 1.72, 2.20, 1.22),
)
GRIP_FRAME_Z = 1.7                                  # the steel frame over the grip's top, under the receiver: its bottom


def grip_axes():
    d = Vector((GRIP_RAKE, 0.0, 1.0)).normalized()   # up the grip
    f = Vector((1.0, 0.0, -GRIP_RAKE)).normalized()  # across it, forward
    return d, f


def grip_section(t):
    """The grip's section at t (0 top .. 1 bottom): its centre, forward direction, front, back, half width."""
    d, f = grip_axes()
    s = GRIP_S[0] + (GRIP_S[1] - GRIP_S[0]) * t
    c = d * s - f * GRIP_BEND * t * t
    lv = GRIP_LEVELS
    for (t0, *a), (t1, *b) in zip(lv, lv[1:]):
        if t <= t1 or t1 == lv[-1][0]:
            k = max(0.0, min(1.0, (t - t0) / (t1 - t0)))
            fr, bk, w = (x + (y - x) * k for x, y in zip(a, b))
            return c, f, fr, bk, w
    raise AssertionError


def grip_t_at(z):
    """The t whose section's centre is at height z."""
    lo, hi = 0.0, 1.0
    for _ in range(30):
        m = 0.5 * (lo + hi)
        if grip_section(m)[0].z > z:
            lo = m
        else:
            hi = m
    return 0.5 * (lo + hi)


def box(bm, b, bevel=0.0):
    """An axis-aligned box (x0, x1, y0, y1, z0, z1), its edges bevelled."""
    x0, x1, y0, y1, z0, z1 = b
    res = bmesh.ops.create_cube(bm, size=1.0)
    vs = res["verts"]
    for v in vs:
        v.co = Vector((x0 if v.co.x < 0 else x1, y0 if v.co.y < 0 else y1, z0 if v.co.z < 0 else z1))
    if bevel > 0.0:
        es = list({e for v in vs for e in v.link_edges})
        bmesh.ops.bevel(bm, geom=es + vs, offset=bevel, segments=1, affect='EDGES', profile=0.5)
    return vs


def prism_x(bm, x0, x1, r, sides, y=0.0, z=0.0, caps=True, rot=0.0):
    """A prism along x (a regular polygon of `sides`, circumradius r, centred on y, z)."""
    res = bmesh.ops.create_cone(bm, cap_ends=caps, cap_tris=False, segments=sides, radius1=r, radius2=r, depth=1.0)
    for v in res["verts"]:
        a, b, c = v.co.x, v.co.y, v.co.z
        ca, sa = math.cos(rot), math.sin(rot)
        a, b = a * ca - b * sa, a * sa + b * ca
        v.co = Vector((x0 + (c + 0.5) * (x1 - x0), y + a, z + b))
    return res["verts"]


def rod(bm, p0, p1, r, sides=6):
    """A rod from p0 to p1 (no caps: its ends are in the receiver and the butt plate)."""
    vs = prism_x(bm, 0.0, 1.0, r, sides, caps=False, rot=math.pi / sides)
    d = p1 - p0
    length = d.length
    q = Vector((1, 0, 0)).rotation_difference(d.normalized())
    for v in vs:
        v.co = p0 + q @ Vector((v.co.x * length, v.co.y, v.co.z))
    return vs


class Part:
    def __init__(self, name, part, bm):
        self.name, self.part, self.bm = name, part, bm
        bm.normal_update()
        self.planes = [(f.normal.copy(), f.normal.dot(f.calc_center_median())) for f in bm.faces]
        # (only a closed part hides what is in it: an open one's planes bound no volume)
        self.closed = all(len(e.link_faces) == 2 for e in bm.edges)

    def inside(self, p, eps=0.02):
        return self.closed and all(n.dot(p) - d < -eps for n, d in self.planes)


def lathe_x(bm, rings, sides, z=BORE_Z, rot=math.pi / 8):
    """A surface of revolution about the line y 0, z along x: rings [(x, r)], `sides` facets, open ends."""
    loops = []
    for x, r in rings:
        loops.append([bm.verts.new((x, r * math.cos(rot + 2 * math.pi * k / sides),
                                    z + r * math.sin(rot + 2 * math.pi * k / sides))) for k in range(sides)])
    for a, b in zip(loops, loops[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    return loops


def sweep(bm, path, half_n, half_y, cap_end=False):
    """A thin bar along a path in the x-z plane (y 0): a rectangle `half_n` across the path and `half_y` deep at each
    point (its corners mitred), open at its start (in a part it grows from), capped at its end if asked."""
    rings = []
    path = [(x, 0.0, z) for x, z in path]  # ((x, z) points)
    for i, p in enumerate(path):
        a = Vector(path[max(0, i - 1)]) if i > 0 else Vector(p)
        b = Vector(path[min(len(path) - 1, i + 1)]) if i < len(path) - 1 else Vector(p)
        t = (b - a).normalized()
        n = Vector((-t.z, 0.0, t.x))  # in the x-z plane, across the path
        c = Vector(p)
        rings.append([bm.verts.new(c + n * sn * half_n + Vector((0, sy * half_y, 0)))
                      for sn, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1))])
    for a, b in zip(rings, rings[1:]):
        for k in range(4):
            bm.faces.new((a[k], a[(k + 1) % 4], b[(k + 1) % 4], b[k]))
    if cap_end:
        bm.faces.new(rings[-1])


def tube(bm, path, r, sides=6):
    """A round rod along a path in the x-z plane [(x, z)], mitred at its bends, open at both ends (they are in parts)."""
    pts = [Vector((x, 0.0, z)) for x, z in path]
    rings = []
    for i, p in enumerate(pts):
        t_in = (p - pts[i - 1]).normalized() if i > 0 else None
        t_out = (pts[i + 1] - p).normalized() if i < len(pts) - 1 else None
        one = t_in if t_in is not None else t_out
        t = (t_in + t_out).normalized() if t_in is not None and t_out is not None else one
        miter = 1.0 / max(0.5, t.dot(one))
        n = Vector((-t.z, 0.0, t.x))                  # across the path, in the x-z plane
        ring = []
        for k in range(sides):
            a = math.pi / sides + 2.0 * math.pi * k / sides
            ring.append(bm.verts.new(p + n * (r * math.cos(a) * miter) + Vector((0.0, r * math.sin(a), 0.0))))
        rings.append(ring)
    for a, b in zip(rings, rings[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))


def grip_loft(bm):
    """The wooden grip: GRIP_LEVELS' sections lofted, eight sides each, capped."""
    rings = []
    for t, *_ in GRIP_LEVELS:
        c, f, fr, bk, w = grip_section(t)
        y = Vector((0.0, 1.0, 0.0))
        pts = [(fr, 0.0), (0.74 * fr, 0.86 * w), (0.12 * (fr - bk), w), (-0.74 * bk, 0.86 * w),
               (-bk, 0.0), (-0.74 * bk, -0.86 * w), (0.12 * (fr - bk), -w), (0.74 * fr, -0.86 * w)]
        rings.append([bm.verts.new(c + f * u + y * v) for u, v in pts])
    n = len(rings[0])
    for a, b in zip(rings, rings[1:]):
        for k in range(n):
            bm.faces.new((a[k], a[(k + 1) % n], b[(k + 1) % n], b[k]))
    bm.faces.new(rings[0])
    bm.faces.new(rings[-1])


def bolt_heads(bm, centres, side, r=0.27, h=0.2):
    """Low pyramids (bolt heads) on a face whose outward normal is +y (side 1) or -y (side -1): centres [(x, y, z)]."""
    for x, y, z in centres:
        base = [bm.verts.new((x + dx * r, y - side * 0.03, z + dz * r)) for dx, dz in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        tip = bm.verts.new((x, y + side * h, z))
        for k in range(4):
            bm.faces.new((base[k], base[(k + 1) % 4], tip))


def make_parts():
    parts = []

    def add(name, part, build):
        bm = bmesh.new()
        build(bm)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        parts.append(Part(name, part, bm))

    # The receiver, its raised top, and its front: a heavily chamfered nose block (bolt heads in brass on its sides)
    # and an octagonal collar round the barrel stepping down to it.
    add("receiver", STEEL, lambda bm: box(bm, RECEIVER, 0.45))
    add("housing", STEEL, lambda bm: box(bm, HOUSING, 0.35))
    add("nose", STEEL, lambda bm: box(bm, NOSE, 0.85))
    for s in (1.0, -1.0):
        add("nose bolts %+d" % s, BRASS, lambda bm, s=s: bolt_heads(
            bm, [(x, NOSE[3] * s, z) for x in (NOSE[0] + 1.25, NOSE[1] - 1.25) for z in (NOSE[4] + 1.25, NOSE[5] - 1.25)], s))
    add("collar", STEEL, lambda bm: lathe_x(bm, [(NOSE[1] - 0.3, COLLAR_R), (COLLAR_X, COLLAR_R),
                                                 (COLLAR_X + 0.75, BARREL_R + 0.22)], 8))
    add("barrel", BLUED, lambda bm: prism_x(bm, COLLAR_X + 0.5, BLOCK[0] + 0.3, BARREL_R, 8, z=BORE_Z, caps=False,
                                             rot=math.pi / 8))
    add("band", STEEL, lambda bm: prism_x(bm, BAND[0], BAND[1], BAND_R, 8, z=BORE_Z, rot=math.pi / 8))
    bolt_x = 0.5 * (BAND[0] + BAND[1])
    add("band bolt", BRASS, lambda bm: box(bm, (bolt_x - 0.33, bolt_x + 0.33, -0.32, 0.32, BORE_Z + 0.95, BORE_Z + 1.5)))
    add("block", STEEL, lambda bm: box(bm, BLOCK, 0.35))
    add("post base", STEEL, lambda bm: box(bm, (FRONT_POST_X[0] - 0.5, FRONT_POST_X[1] + 0.5, -0.7, 0.7,
                                                 BLOCK[5] - 0.2, BLOCK[5] + 0.5)))
    add("post", BLUED, lambda bm: box(bm, (FRONT_POST_X[0], FRONT_POST_X[1], -0.22, 0.22, BLOCK[5] + 0.4, SIGHT_Z)))
    add("rear base", STEEL, lambda bm: box(bm, (REAR_SIGHT_X[0] - 0.25, REAR_SIGHT_X[1] + 0.35, -1.15, 1.15,
                                                 RECEIVER[5] - 0.2, RECEIVER[5] + 0.45)))
    for s in (1.0, -1.0):
        add("ear %+d" % s, BLUED, lambda bm, s=s: box(bm, (REAR_SIGHT_X[0], REAR_SIGHT_X[1], NOTCH * s, 1.0 * s,
                                                          RECEIVER[5] + 0.35, SIGHT_Z)))
    add("magazine", STEEL, lambda bm: box(bm, (5.6, 9.4, -1.05, 1.05, -3.0, RECEIVER[4] + 0.25), 0.3))

    # The pistol grip, wood, curved and bulbous (grip_loft); the steel frame over its top under the receiver, its back
    # following the grip's back, its front where the trigger guard ends.
    add("grip", WOOD, grip_loft)

    def frame(bm):
        box(bm, (-2.4, 2.6, -1.35, 1.35, GRIP_FRAME_Z, RECEIVER[4] + 0.2))
        for v in bm.verts:
            if v.co.x < 0.0:
                c, f, fr, bk, w = grip_section(grip_t_at(v.co.z))
                v.co.x = (c - f * bk).x - 0.12 * (1.0 if v.co.z < RECEIVER[4] else 0.0)
    add("grip frame", STEEL, frame)
    # A thin trigger guard loop (from the magazine's back round under the trigger to the grip's front) and the trigger.
    add("trigger guard", STEEL, lambda bm: sweep(bm, [(5.55, RECEIVER[4] + 0.1), (5.35, 1.0), (4.3, 0.1),
                                                      (2.7, 0.2), (1.7, 1.3)], 0.17, 0.3))
    add("trigger", BLUED, lambda bm: sweep(bm, [(3.45, RECEIVER[4] + 0.1), (3.35, 2.1), (3.95, 1.35)],
                                           0.14, 0.17, cap_end=True))

    # The stock (the reference's): the adapter block, the upper rod level back to the butt block's top, the lower one
    # from the adapter's bottom down and back to the butt block's bottom, the tall butt block, the pad.
    add("adapter", STEEL, lambda bm: box(bm, ADAPTER, 0.3))
    add("rod top", ROD, lambda bm: tube(bm, [(ADAPTER[0] + 0.3, ROD_TOP_Z), (BUTT[1] + 0.25, ROD_TOP_Z)], ROD_R))
    add("rod low", ROD, lambda bm: tube(bm, list(ROD_LOW) + [(BUTT[1] + 0.25, ROD_BUTT_LOW_Z)], ROD_R))
    add("butt plate", STEEL, lambda bm: box(bm, BUTT, 0.55))
    add("pad", RUBBER, lambda bm: box(bm, (-24.0, BUTT[0] + 0.1, BUTT[2] + 0.15, BUTT[3] - 0.15, BUTT[4] + 0.2,
                                           BUTT[5] - 0.2)))
    for x, part in LAMPS:
        add(PART_NAMES[part], part, lambda bm, x=x: box(bm, (x - LAMP_HALF, x + LAMP_HALF, RECEIVER[3] - 0.1,
                                                            RECEIVER[3] + 0.24, LAMP_Z - LAMP_HALF, LAMP_Z + LAMP_HALF)))
    return parts


def carve_holes(objs):
    """The muzzle block's holes cut for real (Blender's boolean, the float solver: the exact one left the vents uncut): its bore from the front (BORE_DEPTH deep, the
    muzzle anchor at its bottom) and three vents through it side to side, low-sided, dark inside (details)."""
    block = next(o for o in objs if o.name == "block")
    cutters = []

    def cutter(name, verts_fn):
        bm = bmesh.new()
        verts_fn(bm)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        cutters.append(ob)

    cutter("bore cut", lambda bm: prism_x(bm, MUZZLE_X - BORE_DEPTH, MUZZLE_X + 1.0, BORE_R, 8, z=BORE_Z, rot=math.pi / 8))
    for s in (1.0,):
        for x in VENTS:
            def vent(bm, x=x, s=s):
                vs = prism_x(bm, 0.0, 1.0, VENT_R, 6, rot=math.pi / 6)
                y0, y1 = BLOCK[2] - 1.0, BLOCK[3] + 1.0  # through the block, side to side
                for v in vs:  # (its axis along y)
                    v.co = Vector((x - v.co.y, y0 + v.co.x * (y1 - y0), BORE_Z + v.co.z))
            cutter("vent cut", vent)
    bpy.context.view_layer.objects.active = block
    for c in cutters:
        m = block.modifiers.new("cut", "BOOLEAN")
        m.operation = "DIFFERENCE"
        m.solver = "FLOAT"
        m.use_hole_tolerant = True
        m.object = c
        bpy.ops.object.modifier_apply(modifier=m.name)
        print("carve %s: %d faces" % (c.name, len(block.data.polygons)), [tuple(round(min(v.co[k] for v in c.data.vertices), 2) for k in range(3)), tuple(round(max(v.co[k] for v in c.data.vertices), 2) for k in range(3))], sum(1 for e in c.data.edges))
    for c in cutters:
        bpy.data.objects.remove(c)
    bm = bmesh.new()
    bm.from_mesh(block.data)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    bm.to_mesh(block.data)
    bm.free()


def drop_hidden(parts):
    """Faces inside another part (never seen) go: fewer triangles, no texels spent on them."""
    for p in parts:
        gone = [f for f in p.bm.faces
                if any(o is not p and o.inside(f.calc_center_median()) and
                       all(o.inside(v.co, -0.01) for v in f.verts) for o in parts)]
        bmesh.ops.delete(p.bm, geom=gone, context='FACES_ONLY')


def to_objects(parts):
    objs = []
    for p in parts:
        me = bpy.data.meshes.new(p.name)
        p.bm.to_mesh(me)
        ob = bpy.data.objects.new(p.name, me)
        ob["part"] = p.part
        bpy.context.scene.collection.objects.link(ob)
        objs.append(ob)
    return objs


# ----------------------------------------------------------------------------
# The skin

def rgb(*c):
    return Vector(c)


GUNMETAL, GUNMETAL_LIGHT = rgb(88, 72, 62), rgb(142, 118, 102)   # (round three: warmer, the reference's brown-grey)
BLUED_DARK, BLUED_C = rgb(52, 45, 42), rgb(92, 78, 70)
ROD_DARK, ROD_C = rgb(104, 88, 76), rgb(162, 140, 122)
BRASS_DARK, BRASS_C = rgb(96, 74, 38), rgb(150, 118, 62)
WOOD_DARK, WOOD_C, WOOD_LIGHT = rgb(54, 28, 14), rgb(122, 68, 32), rgb(158, 98, 50)
RUBBER_C = rgb(54, 50, 46)
GRIME, RUST = rgb(42, 36, 30), rgb(96, 52, 26)
EDGE = rgb(176, 160, 142)
HOLE = rgb(10, 9, 9)
LAMP_RED_C, LAMP_RED_HOT, LAMP_BRONZE_C = rgb(220, 30, 16), rgb(255, 150, 90), rgb(176, 120, 52)
SKIN_DARKEN = 0.66  # (judged in the game: 0.86 read near white in e1m1's light; the reference's in-game grey)


def mix(a, b, k):
    k = max(0.0, min(1.0, k))
    return a * (1.0 - k) + b * k


def smooth(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.dot(ab), 1e-9)))
    return (a + ab * t - p).length


def hemisphere(n):
    out = []
    for i in range(n):
        z = 1.0 - (i + 0.5) / n
        r = math.sqrt(max(0.0, 1.0 - z * z))
        a = i * 2.399963
        out.append(Vector((r * math.cos(a), r * math.sin(a), z)))
    return out


AO_DIRS = hemisphere(12)


def occlusion(bvh, p, n):
    t = n.orthogonal().normalized()
    b = n.cross(t)
    hit = 0
    for d in AO_DIRS:
        w = t * d.x + b * d.y + n * d.z
        if bvh.ray_cast(p + n * 0.03, w, 2.0)[0] is not None:
            hit += 1
    return 1.0 - hit / len(AO_DIRS)


def in_hole(p):
    """How deep a point of the muzzle block is inside one of its cut holes (0: on the outside)."""
    return max(MUZZLE_X - p.x if math.hypot(p.y, p.z - BORE_Z) < BORE_R + 0.02 else 0.0,
               BLOCK[3] - abs(p.y) if any(math.hypot(p.x - x, p.z - BORE_Z) < VENT_R + 0.02 for x in VENTS) else 0.0)


def details(p, n, part, name):
    """Painted details: (colour or None, how much). The holes' insides, grooves, window, rivets, the scratches."""
    if name == "block":
        d = in_hole(p)
        if d > 0.03:
            return HOLE * (1.4 - 0.9 * smooth(0.0, 0.6, d)), 1.0   # dark inside, darkest at the bottom
        if abs(n.y) > 0.9 and any(VENT_R < math.hypot(p.x - x, p.z - BORE_Z) < VENT_R + 0.14 for x in VENTS):
            return EDGE * 0.85, 0.6                                # the holes' worn rims
        if n.x > 0.9 and BORE_R < math.hypot(p.y, p.z - BORE_Z) < BORE_R + 0.14:
            return EDGE * 0.85, 0.6
    if name == "housing" and n.z > 0.9:
        for y in GROOVES_Y:
            if abs(p.y - y) < 0.17 and HOUSING[0] + 1.4 < p.x < HOUSING[1] - 0.9:
                return HOLE * 1.6, 1.0
            if abs(p.y - y) < 0.27 and HOUSING[0] + 1.3 < p.x < HOUSING[1] - 0.8:
                return EDGE * 0.8, 0.5                             # the grooves' lips
    if name == "receiver" and n.y > 0.9:
        x0, x1, z0, z1 = WINDOW
        if x0 - 0.25 <= p.x <= x1 + 0.25 and z0 - 0.25 <= p.z <= z1 + 0.25:
            inner = min(p.x - x0, x1 - p.x, p.z - z0, z1 - p.z)
            if inner < 0.0:
                return EDGE * 0.75, 0.75                           # the frame round it, raised and worn
            if inner < 0.16:
                return HOLE * 0.8, 1.0                             # its shadowed lip
            # Dark glass, one faint reflection across it.
            t = (p.x - x0) / (x1 - x0) + 0.6 * (p.z - z0) / (z1 - z0)
            return mix(rgb(10, 12, 14), rgb(40, 46, 50), 0.8 * smooth(0.12, 0.0, abs(t - 0.75))), 1.0
    if name == "magazine" and abs(n.y) > 0.9 and abs(p.x - 7.5) < 0.22 and -2.0 < p.z < 2.8:
        return HOLE * 1.6, 1.0                                     # the slot down its side
    # Rivets (brass), painted with a highlight: the receiver's sides, the post base, the butt block.
    rivets = []
    if name == "receiver" and abs(n.y) > 0.9:
        rivets = [(RECEIVER[0] + 0.8, RECEIVER[4] + 0.8), (RECEIVER[0] + 0.8, RECEIVER[5] - 0.8),
                  (RECEIVER[1] - 0.8, RECEIVER[5] - 0.8), (1.2, RECEIVER[4] + 0.8), (8.1, LAMP_Z)]
    elif name == "adapter" and abs(n.y) > 0.9:
        rivets = [(0.5 * (ADAPTER[0] + ADAPTER[1]), ADAPTER[5] - 0.9), (0.5 * (ADAPTER[0] + ADAPTER[1]), ADAPTER[4] + 0.9)]
    elif name == "post base" and abs(n.y) > 0.9:
        rivets = [(0.5 * sum(FRONT_POST_X), BLOCK[5] + 0.15)]
    elif name == "butt plate" and abs(n.y) > 0.9:
        rivets = [(0.5 * (BUTT[0] + BUTT[1]), BUTT[4] + 1.1), (0.5 * (BUTT[0] + BUTT[1]), BUTT[5] - 1.1)]
    for x, z in rivets:
        d = math.hypot(p.x - x, p.z - z)
        if d < 0.32:
            hi = smooth(0.2, 0.0, math.hypot(p.x - x + 0.09, p.z - z - 0.09))
            return mix(mix(BRASS_C * 1.1, BRASS_DARK, d / 0.32), BRASS_C * 1.6, hi), 1.0
        if d < 0.44:
            return GRIME * 0.6, 0.85
    return None, 0.0


def scratches(p, n):
    """Thin light scratches, mostly along the gun (x), sparse: 0..1."""
    q = Vector((p.x * 0.45, p.y * 5.5, p.z * 5.5)) if abs(n.x) < 0.9 else Vector((p.x * 5.5, p.y * 0.8, p.z * 5.5))
    line = abs(noise.noise(q + Vector((11.0, 3.0, 7.0))))
    mask = smooth(0.25, 0.45, noise.noise(p * 0.6 + Vector((2.0, 9.0, 4.0))))
    return smooth(0.035, 0.0, line) * mask


def paint_texel(p, n, part, name, edges, ao, rnd):
    big = noise.noise(p * 0.3)
    mid = noise.noise(p * 1.05 + Vector((3.1, 7.7, 1.3)))
    fine = noise.noise(p * 3.1 + Vector((9.2, 1.4, 5.5)))
    speck = (rnd - 0.5) * 0.2
    if part == STEEL:
        # Hand-painted gunmetal: broad light and dark patches, a mottle, a blued cast in places.
        col = mix(GUNMETAL, GUNMETAL_LIGHT, 0.42 + 0.45 * mid + 0.22 * fine + speck)
        col = mix(col, BLUED_C, 0.1 * smooth(0.0, 0.5, big))
        col = mix(col, GUNMETAL * 0.7, 0.35 * smooth(0.25, 0.55, -big))
    elif part == BLUED:
        col = mix(BLUED_DARK, BLUED_C, 0.45 + 0.5 * mid + 0.25 * fine + speck)
    elif part == ROD:
        # Round rods polished along their length: streaks along x.
        streak = noise.noise(Vector((p.x * 0.25, p.y * 3.0, p.z * 3.0)))
        col = mix(ROD_DARK, ROD_C, 0.45 + 0.4 * streak + 0.15 * fine + speck)
        col *= 0.75 + 0.35 * max(0.0, n.z)  # their tops catch the light
    elif part == BRASS:
        col = mix(BRASS_DARK, BRASS_C * 1.15, 0.55 + 0.45 * mid + 0.3 * fine)
        col = mix(col, BRASS_C * 1.6, 0.6 * max(0.0, n.z) * smooth(0.0, 0.4, fine + 0.3))  # catching the light
    elif part == WOOD:
        # Oiled walnut: the grain's rings round the grip's (curved) axis, running along it, wavy; open pores; worn
        # lighter where the palm rubs, darker at the foot.
        d, _ = grip_axes()
        t = max(0.0, min(1.0, (GRIP_S[0] - p.dot(d)) / (GRIP_S[0] - GRIP_S[1])))
        c, f, *_ = grip_section(t)
        ax = Vector(((p - c).dot(f), p.y, t * (GRIP_S[0] - GRIP_S[1])))  # across, side, along the axis
        warp = noise.noise(Vector((ax.x * 0.7, ax.y * 0.7, ax.z * 0.18)) + Vector((5.0, 1.0, 2.0)))
        rings = math.sin((ax.x * 1.9 + ax.y * 0.6 + 2.2 * warp) * math.pi * 2.0)
        grain = smooth(0.55, 0.95, rings)
        col = mix(WOOD_C, WOOD_DARK, 0.75 * grain)
        col = mix(col, WOOD_LIGHT, 0.45 * smooth(-0.95, -0.6, rings) + 0.15 * mid)
        if rnd < 0.025:
            col = mix(col, WOOD_DARK * 0.7, 0.7)                  # a pore
        col = mix(col, WOOD_LIGHT * 1.1, 0.25 * smooth(0.5, 1.0, n.y * n.y) * smooth(-2.5, 0.5, p.z))
        col = mix(col, WOOD_DARK, 0.3 * smooth(-1.5, -3.8, p.z))
    elif part == RUBBER:
        col = RUBBER_C * (0.9 + 0.25 * fine + speck)
        if abs(n.x) > 0.9 and (int((p.z - BUTT[4]) * 2.2) % 2 == 0):
            col *= 0.6                                             # the pad's ribs
    elif part == LAMP_RED:
        lens = math.hypot(abs(p.x - LAMPS[0][0]), abs(p.z - LAMP_Z))
        if n.y > 0.9 and lens < LAMP_HALF - 0.06:
            # Glowing: a hot middle, a red rim (fullbright).
            return mix(LAMP_RED_HOT, LAMP_RED_C, smooth(0.04, LAMP_HALF - 0.08, lens)), "lamp glow"
        col = mix(BRASS_DARK, BRASS_C, 0.5 + 0.5 * fine)
    else:  # LAMP_BRONZE: a domed bronze lens with a highlight, in a brass bezel
        lens = math.hypot(abs(p.x - LAMPS[1][0]), abs(p.z - LAMP_Z))
        if n.y > 0.9 and lens < LAMP_HALF - 0.06:
            hi = smooth(0.12, 0.0, math.hypot(p.x - LAMPS[1][0] + 0.07, p.z - LAMP_Z - 0.08))
            col = mix(mix(LAMP_BRONZE_C * 1.1, LAMP_BRONZE_C * 0.6, lens / LAMP_HALF), rgb(230, 200, 140), hi)
        else:
            col = mix(BRASS_DARK, BRASS_C, 0.5 + 0.5 * fine)
    region = PART_NAMES[part]
    # Grime in the recesses (the occlusion), a little rust in the grime.
    grime = 0.75 * (1.0 - ao) + 0.12 * smooth(0.2, 0.6, big)
    if part not in (WOOD, RUBBER):
        col = mix(col, GRIME, grime)
        if part != BRASS:
            r = smooth(0.45, 0.65, 0.7 * big + 0.5 * fine) * (0.3 + (1.0 - ao))
            col = mix(col, RUST, 0.45 * r)
            col = mix(col, EDGE * 0.9, 0.6 * scratches(p, n))
    else:
        col = mix(col, GRIME, 0.6 * (1.0 - ao))
    # Worn bright on the convex edges (wider and lighter on the steel).
    if part not in (RUBBER, LAMP_RED):
        wear = 0.0
        for a, b, convex in edges:
            if convex:
                wear = max(wear, smooth(0.36, 0.03, seg_dist(p, a, b)))
        if wear > 0.0:
            bright = EDGE if part not in (WOOD, BRASS) else (WOOD_LIGHT * 1.15 if part == WOOD else BRASS_C * 1.5)
            col = mix(col, bright, wear * (0.55 + 0.35 * smooth(-0.3, 0.4, fine)))
    return col * ao ** 0.45, region


def bake(objs, w, h):
    """Paints the skin; returns (texels RGB top row first, glow flags, regions)."""
    tex = [None] * (w * h)
    glow = [False] * (w * h)
    regions = [None] * (w * h)
    rng = random.Random(7)
    verts, polys = [], []
    for ob in objs:
        base = len(verts)
        verts += [v.co.copy() for v in ob.data.vertices]
        polys += [[base + i for i in p.vertices] for p in ob.data.polygons]
    bvh = BVHTree.FromPolygons(verts, polys)
    for ob in objs:
        part = int(ob["part"])
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bm.normal_update()
        uv = bm.loops.layers.uv.active
        face_edges = {}
        for f in bm.faces:
            es = []
            for e in f.edges:
                if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(25):
                    es.append((e.verts[0].co.copy(), e.verts[1].co.copy(), e.is_convex))
                elif len(e.link_faces) == 1:
                    es.append((e.verts[0].co.copy(), e.verts[1].co.copy(), True))
            face_edges[f.index] = es
        for tri in bm.calc_loop_triangles():
            f = tri[0].face
            n = f.normal.copy()
            st = [Vector((l[uv].uv.x * w, (1.0 - l[uv].uv.y) * h)) for l in tri]
            ps = [l.vert.co.copy() for l in tri]
            area = (st[1] - st[0]).cross(st[2] - st[0])
            if abs(area) < 1e-9:
                continue
            s0 = max(0, int(math.floor(min(q.x for q in st) - 0.5)))
            s1 = min(w - 1, int(math.ceil(max(q.x for q in st))))
            t0 = max(0, int(math.floor(min(q.y for q in st) - 0.5)))
            t1 = min(h - 1, int(math.ceil(max(q.y for q in st))))
            for t in range(t0, t1 + 1):
                for s in range(s0, s1 + 1):
                    c = Vector((s + 0.5, t + 0.5))
                    b0 = (st[1] - c).cross(st[2] - c) / area
                    b1 = (st[2] - c).cross(st[0] - c) / area
                    b2 = 1.0 - b0 - b1
                    if min(b0, b1, b2) < -0.02:
                        continue
                    p = ps[0] * b0 + ps[1] * b1 + ps[2] * b2
                    ao = 0.4 + 0.6 * occlusion(bvh, p, n)
                    col, region = paint_texel(p, n, part, ob.name, face_edges[f.index], ao, rng.random())
                    dcol, k = details(p, n, part, ob.name)
                    if dcol is not None:
                        col = mix(col, dcol * (0.6 + 0.4 * ao), k)
                        region = "details"
                    i = t * w + s
                    tex[i], regions[i] = col, region
                    glow[i] = region == "lamp glow"
        bm.free()
    # Bleed each island's edge texels outward (filtered and mipmapped, no dark seams).
    for _ in range(4):
        grown = list(tex)
        for i in range(w * h):
            if tex[i] is None:
                s, t = i % w, i // w
                nb = [tex[j] for j in (i - 1 if s > 0 else -1, i + 1 if s < w - 1 else -1, i - w if t > 0 else -1,
                                       i + w if t < h - 1 else -1) if j >= 0 and tex[j] is not None]
                if nb:
                    grown[i] = sum(nb, Vector()) / len(nb)
        tex = grown
    out = [tuple(max(0, min(255, int(round(c[k] * (1.0 if glow[i] else SKIN_DARKEN))))) for k in range(3))
           if c is not None else (24, 22, 20) for i, c in enumerate(tex)]
    return out, glow, regions


def quantize(texels, glow):
    from quakevr_models import qpal
    q = qpal.Quantizer()
    idx = bytearray(len(texels))
    stats = {}
    for i, c in enumerate(texels):
        idx[i] = q.nearest(c, glow[i])
    return idx, qpal.PALETTE


# ----------------------------------------------------------------------------
# The model

def gun_mesh(objs, w, h):
    """mdlgen.Mesh of every object's triangles (Quake's winding; vertices shared within a flat face) and each
    triangle's part name."""
    mesh = mdlgen.Mesh(w, h, {})
    names = []
    index = {}
    for ob in objs:
        me = ob.data
        me.calc_loop_triangles()
        uv = me.uv_layers.active
        for lt in me.loop_triangles:
            ps = [me.vertices[me.loops[li].vertex_index].co.copy() for li in lt.loops]
            n = lt.normal.copy()
            sts = []
            for li in lt.loops:
                u, v = uv.data[li].uv
                sts.append((min(w - 1, max(0, int(math.floor(u * w)))), min(h - 1, max(0, int(math.floor((1.0 - v) * h))))))
            ps, sts = (ps[0], ps[2], ps[1]), (sts[0], sts[2], sts[1])  # counter-clockwise -> Quake's clockwise
            corners = []
            for p, st in zip(ps, sts):
                key = (round(p.x, 4), round(p.y, 4), round(p.z, 4), round(n.x, 3), round(n.y, 3), round(n.z, 3), st)
                k = index.get(key)
                if k is None:
                    k = index[key] = len(mesh.verts)
                    mesh.verts.append(((p.x, p.y, p.z), (n.x, n.y, n.z), st))
                corners.append(k)
            if len(set(corners)) == 3 and (ps[1] - ps[0]).cross(ps[2] - ps[0]).length > 1e-6:
                mesh.tris.append(tuple(corners))
                names.append(ob.name)
    return mesh, names


def fmt(x):
    s = "%.4f" % x
    return "0.0" if s in ("0.0000", "-0.0000") else s.rstrip("0").rstrip(".") if "." in s else s


def point(p):
    return " ".join("%.4f" % c for c in p)


def hotspot_value(p):
    """A hotspot's place (weapons.txt hs<n>_x/y/z) for a point of the model: the weapon frame's, Scale about CORNER."""
    return tuple((1.0 - WEAPON_SCALE) * c + WEAPON_SCALE * x for c, x in zip(CORNER, p))


def main():
    if "--from-scratch" not in sys.argv:
        sys.exit("build_burstrifle.py: Misc/quakevr/src_models/blend/v_gruntgun/v_gruntgun.blend is the source (edit "
                 "it, save: export on save); this would overwrite it and the shipped model. --from-scratch to make "
                 "them anew all the same.")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    parts = make_parts()
    drop_hidden(parts)
    objs = to_objects(parts)
    carve_holes(objs)
    empty = [o.name for o in objs if not o.data.polygons]
    if empty:
        print("parts wholly hidden (dropped): %s" % ", ".join(empty))
    objs = [o for o in objs if o.data.polygons]
    for o in objs:
        if not o.data.uv_layers:
            o.data.uv_layers.new()

    # UVs: one atlas, every corner on a texel's centre (as the MDL stores it).
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(40), island_margin=0.008, area_weight=0.0, correct_aspect=True,
                             scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    for ob in objs:
        for d in ob.data.uv_layers.active.data:
            s = min(SKIN_W - 1, max(0, math.floor(d.uv.x * SKIN_W)))
            t = min(SKIN_H - 1, max(0, math.floor((1.0 - d.uv.y) * SKIN_H)))
            d.uv = ((s + 0.5) / SKIN_W, 1.0 - (t + 0.5) / SKIN_H)
    bpy.context.view_layer.update()

    texels, glow, regions = bake(objs, SKIN_W, SKIN_H)
    idx, pal = quantize(texels, glow)
    # The top-left corner texel is the background the engine flood-fills (Mod_FloodFillSkin): never an island's.
    assert regions[0] is None, "the skin's corner texel is painted"
    stats = {}
    for i, r in enumerate(regions):
        if r is not None:
            c = pal[idx[i]]
            e = stats.setdefault(r, [0, 0.0])
            e[0] += 1
            e[1] += 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]
    print("skin %dx%d, mean luma by region: %s" % (SKIN_W, SKIN_H, ", ".join(
        "%s %.1f (%d)" % (k, e[1] / e[0], e[0]) for k, e in sorted(stats.items()))))
    print("skin coverage %.1f%%, glowing texels %d" % (100.0 * sum(1 for r in regions if r) / len(regions),
                                                     sum(1 for i in range(len(idx)) if idx[i] >= 224)))

    mesh, tri_names = gun_mesh(objs, SKIN_W, SKIN_H)
    os.makedirs(PROGS, exist_ok=True)
    mdlgen.write_mdl(os.path.join(PROGS, NAME), mesh, [bytes(idx)], FRAMES[0], [mesh] * (len(FRAMES) - 1), FRAMES,
                     CORNER)
    lo = [min(v[0][k] for v in mesh.verts) for k in range(3)]
    hi = [max(v[0][k] for v in mesh.verts) for k in range(3)]
    print("%s: %d triangles, %d vertices, %d frames; box %.2f %.2f %.2f .. %.2f %.2f %.2f" % (
        NAME, len(mesh.tris), len(mesh.verts), len(FRAMES), *lo, *hi))
    counts = {}
    for nm in tri_names:
        counts[nm] = counts.get(nm, 0) + 1
    print("  " + ", ".join("%s %d" % kv for kv in counts.items()))

    # The normal map, baked from the model and its skin (as bake_normals.py).
    from quakevr_models import normalmaps
    from quakevr_models import normalbake as nb
    for p, img in normalmaps.bake(PROGS, NAME).items():
        nb.write_png(p, img)
        print("normal map: %s" % p)

    # weapons.txt: the anchors on the new surfaces, hotspot 1 on the band's left (the off hand's grip), the barrel
    # mask (the barrel, band and muzzle block glow when hot).
    from quakevr_models import source
    band_mid = 0.5 * (BAND[0] + BAND[1])
    a = math.radians(-22.5)  # on the band's octagon: the face just below its left side
    apothem = BAND_R * math.cos(math.pi / 8)
    hs1 = (band_mid, apothem * math.cos(a) * 0.999, BORE_Z + apothem * math.sin(a))
    side_r = (2.5, RECEIVER[2], 8.0)  # the receiver's right side, near where the old hand anchor was
    anchors = {
        "muzzle_av": (MUZZLE_X - BORE_DEPTH, 0.0, BORE_Z),  # the bore's bottom
        "hand_av": side_r,
        "wpnbtn_av": side_r,
        "2h_hand_av": (band_mid, 0.0, BORE_Z - apothem),  # the band's underside
        "wpntxt_av": (-0.2, 0.0, HOUSING[5]),             # the housing's top, at its back
    }
    changes = {}
    for slot in (SLOT,) + VARIANT_SLOTS:
        for k, v in anchors.items():
            changes[(slot, k)] = point(v)
        for c, v in zip("xyz", hotspot_value(hs1)):
            changes[(slot, "hs1_%s" % c)] = fmt(v)
    hot = ("barrel", "band", "block", "post base", "post", "band bolt")
    changes[(SLOT, "fx_gfx_mask")] = source.mask_encode([nm in hot for nm in tri_names])
    source.write_weapons_txt(ROOT, changes)
    print("weapons.txt: slots %s: %s; hs1 %s (the band at %s)" % (
        ", ".join(str(s) for s in (SLOT,) + VARIANT_SLOTS), ", ".join("%s %s" % kv for kv in anchors.items()),
        " ".join(fmt(v) for v in hotspot_value(hs1)), point(hs1)))
    print("sight line (vr_sightalign_table.inc): rear %.2f 0 %.3f front %.2f 0 %.3f" % (
        0.5 * sum(REAR_SIGHT_X), SIGHT_Z, 0.5 * sum(FRONT_POST_X), SIGHT_Z))

    # The source .blend, made from the files just written (blend_build.py's way); its files owned by it.
    path = source.build(bpy.context, ROOT, NAME, SRC_DIR)
    import genguard
    genguard.own(path, [os.path.join(PROGS, f) for f in source.owned_files(source.source_object())])
    print("source: %s" % path)


if __name__ == "__main__":
    main()
