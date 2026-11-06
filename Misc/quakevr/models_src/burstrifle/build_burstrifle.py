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
RECEIVER = (-3.0, 15.0, -2.2, 2.2, 3.6, 10.2)
HOUSING = (-1.0, 14.2, -1.75, 1.75, 9.9, 11.1)
CAP = (14.8, 18.8, -1.85, 1.85, 5.0, 10.0)
BLOCK = (27.1, MUZZLE_X, -1.35, 1.35, BORE_Z - 1.35, BORE_Z + 1.35)
BAND = (22.0, 23.5)     # the clamp band round the barrel (the other hand's grip: hotspot 1)
BAND_R = 1.2
BARREL_R = 0.75
WINDOW = (2.6, 8.6, 6.6, 8.6)                       # the side window on the left (+y) side: x0, x1, z0, z1
LAMPS = ((10.9, LAMP_RED), (9.6, LAMP_BRONZE))      # their x on the left side, below and ahead of the window
LAMP_Z, LAMP_HALF = 5.0, 0.33
VENTS = (29.7, 30.95, 32.2)                         # the muzzle block's three holes a side (x), radius VENT_R
VENT_R = 0.42
GROOVES_Y = (-0.62, 0.62)                           # the two grooves along the housing's top
GRIP_TILT = 0.222                                   # the pistol grip leans back (dx/dz)


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

    def inside(self, p, eps=0.02):
        return all(n.dot(p) - d < -eps for n, d in self.planes)


def make_parts():
    parts = []

    def add(name, part, build):
        bm = bmesh.new()
        build(bm)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        parts.append(Part(name, part, bm))

    add("receiver", STEEL, lambda bm: box(bm, RECEIVER, 0.45))
    add("housing", STEEL, lambda bm: box(bm, HOUSING, 0.35))
    add("cap", STEEL, lambda bm: box(bm, CAP, 0.6))
    add("barrel", BLUED, lambda bm: prism_x(bm, CAP[1] - 0.3, BLOCK[0] + 0.3, BARREL_R, 8, z=BORE_Z, caps=False,
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

    def grip(bm):
        box(bm, (-1.65, 1.65, -1.2, 1.2, -3.9, RECEIVER[4] + 0.2), 0.42)
        for v in bm.verts:
            v.co.x += GRIP_TILT * (v.co.z - 0.2)
    add("grip", WOOD, grip)
    add("guard bar", STEEL, lambda bm: box(bm, (0.9, 5.4, -0.36, 0.36, 0.0, 0.55)))
    add("guard post", STEEL, lambda bm: box(bm, (4.75, 5.4, -0.36, 0.36, 0.15, RECEIVER[4] + 0.1)))

    def trigger(bm):
        box(bm, (2.95, 3.45, -0.2, 0.2, 1.4, RECEIVER[4] + 0.1))
        for v in bm.verts:
            v.co.x += 0.18 * (RECEIVER[4] - v.co.z)  # curled back at its foot
    add("trigger", BLUED, trigger)
    add("socket top", STEEL, lambda bm: box(bm, (-4.0, -2.8, -0.9, 0.9, 8.6, 9.9)))
    add("socket low", STEEL, lambda bm: box(bm, (-4.0, -2.8, -0.9, 0.9, 4.4, 5.7)))
    # The stock drops a little (its top 2.6 under the sight line at the butt: the cheek's room over it).
    add("rod top", ROD, lambda bm: rod(bm, Vector((-3.8, 0.0, 9.25)), Vector((-21.7, 0.0, 8.2)), 0.52))
    add("rod low", ROD, lambda bm: rod(bm, Vector((-3.8, 0.0, 5.05)), Vector((-21.7, 0.0, 3.4)), 0.52))
    add("butt plate", STEEL, lambda bm: box(bm, (-22.9, -21.5, -1.6, 1.6, 2.1, 9.0), 0.35))
    add("pad", RUBBER, lambda bm: box(bm, (-24.0, -22.8, -1.48, 1.48, 2.25, 8.85)))
    for x, part in LAMPS:
        add(PART_NAMES[part], part, lambda bm, x=x: box(bm, (x - LAMP_HALF, x + LAMP_HALF, RECEIVER[3] - 0.1,
                                                            RECEIVER[3] + 0.22, LAMP_Z - LAMP_HALF, LAMP_Z + LAMP_HALF)))
    return parts


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


GUNMETAL, GUNMETAL_LIGHT = rgb(72, 69, 66), rgb(110, 106, 101)
BLUED_DARK, BLUED_C = rgb(44, 46, 54), rgb(78, 80, 90)
ROD_DARK, ROD_C = rgb(76, 76, 78), rgb(124, 122, 120)
BRASS_DARK, BRASS_C = rgb(96, 74, 38), rgb(150, 118, 62)
WOOD_DARK, WOOD_C, WOOD_LIGHT = rgb(50, 30, 18), rgb(88, 54, 30), rgb(114, 72, 40)
RUBBER_C = rgb(30, 28, 27)
GRIME, RUST = rgb(42, 36, 30), rgb(96, 52, 26)
EDGE = rgb(150, 150, 148)
HOLE = rgb(10, 9, 9)
LAMP_RED_C, LAMP_BRONZE_C = rgb(255, 40, 24), rgb(176, 120, 52)
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


def details(p, n, part, name):
    """Painted details: (colour or None, how much). The holes, grooves, window, rivets."""
    # The muzzle: the bore and the vents.
    if name == "block":
        if n.x > 0.9 and math.hypot(p.y, p.z - BORE_Z) < 0.55:
            return HOLE, 1.0
        if abs(n.y) > 0.9:
            for x in VENTS:
                d = math.hypot(p.x - x, p.z - BORE_Z)
                if d < VENT_R:
                    return HOLE * (0.6 + 0.6 * d / VENT_R), 1.0
                if d < VENT_R + 0.12:
                    return EDGE * 0.8, 0.7
    if name == "housing" and n.z > 0.9:
        for y in GROOVES_Y:
            if abs(p.y - y) < 0.17 and HOUSING[0] + 1.4 < p.x < HOUSING[1] - 0.9:
                return HOLE * 2.2, 1.0
    if name == "receiver" and n.y > 0.9:
        x0, x1, z0, z1 = WINDOW
        if x0 <= p.x <= x1 and z0 <= p.z <= z1:
            inner = min(p.x - x0, x1 - p.x, p.z - z0, z1 - p.z)
            if inner < 0.22:
                return GRIME * 0.6, 1.0               # the frame's shadowed lip
            glass = mix(rgb(22, 26, 30), rgb(48, 56, 60), (p.z - z0) / (z1 - z0))
            return glass, 1.0
    if name == "magazine" and abs(n.y) > 0.9 and abs(p.x - 7.5) < 0.22 and -2.0 < p.z < 2.8:
        return HOLE * 2.0, 1.0                        # the slot down its side
    # Rivets (brass): on the cap's sides, the receiver's sides, the block's post base, the butt plate.
    rivets = []
    if name == "cap" and abs(n.y) > 0.9:
        rivets = [(x, z) for x in (CAP[0] + 0.95, CAP[1] - 0.95) for z in (CAP[4] + 0.95, CAP[5] - 0.95)]
    elif name == "receiver" and abs(n.y) > 0.9:
        rivets = [(RECEIVER[0] + 0.8, 4.4), (RECEIVER[0] + 0.8, 9.4), (RECEIVER[1] - 0.8, 9.4), (1.2, 4.4)]
    elif name == "post base" and abs(n.y) > 0.9:
        rivets = [(0.5 * sum(FRONT_POST_X), BLOCK[5] + 0.15)]
    elif name == "butt plate" and abs(n.y) > 0.9:
        rivets = [(-22.2, 2.9), (-22.2, 8.2)]
    elif name == "socket top" and abs(n.y) > 0.9:
        rivets = [(-3.4, 9.25)]
    for x, z in rivets:
        d = math.hypot(p.x - x, p.z - z)
        if d < 0.3:
            return mix(BRASS_C * 1.15, BRASS_DARK, d / 0.3), 1.0
        if d < 0.4:
            return GRIME * 0.7, 0.8
    return None, 0.0


def paint_texel(p, n, part, name, edges, ao, rnd):
    big = noise.noise(p * 0.3)
    mid = noise.noise(p * 1.05 + Vector((3.1, 7.7, 1.3)))
    fine = noise.noise(p * 3.1 + Vector((9.2, 1.4, 5.5)))
    speck = (rnd - 0.5) * 0.16
    if part == STEEL:
        col = mix(GUNMETAL, GUNMETAL_LIGHT, 0.4 + 0.22 * mid + 0.14 * fine + speck)
        col = mix(col, BLUED_C, 0.2 * smooth(0.0, 0.5, big))
    elif part == BLUED:
        col = mix(BLUED_DARK, BLUED_C, 0.45 + 0.5 * mid + 0.25 * fine + speck)
    elif part == ROD:
        # Round rods polished along their length: streaks along x.
        streak = noise.noise(Vector((p.x * 0.25, p.y * 3.0, p.z * 3.0)))
        col = mix(ROD_DARK, ROD_C, 0.45 + 0.4 * streak + 0.15 * fine + speck)
        col *= 0.75 + 0.35 * max(0.0, n.z)  # their tops catch the light
    elif part == BRASS:
        col = mix(BRASS_DARK, BRASS_C, 0.5 + 0.5 * mid + 0.3 * fine)
    elif part == WOOD:
        # Oiled wood, its grain along the grip (up it), darker at the bottom from the hand.
        g = noise.noise(Vector((p.x * 2.4 + 0.6 * fine, p.y * 2.4, p.z * 0.35)))
        col = mix(WOOD_DARK, WOOD_C, 0.55 + 0.6 * g + 0.15 * mid + speck)
        if abs(g) < 0.06:
            col = mix(col, WOOD_LIGHT, 0.6)
        col = mix(col, WOOD_DARK, 0.25 * smooth(-1.0, -4.0, p.z))
    elif part == RUBBER:
        col = RUBBER_C * (0.9 + 0.25 * fine + speck)
    elif part == LAMP_RED:
        lens = max(abs(p.x - LAMPS[0][0]), abs(p.z - LAMP_Z))
        if n.y > 0.9 and lens < LAMP_HALF - 0.1:
            return LAMP_RED_C * (1.0 - 0.6 * lens / LAMP_HALF), "lamp glow"
        col = mix(BRASS_DARK, BRASS_C, 0.5 + 0.5 * fine)
    else:  # LAMP_BRONZE
        col = mix(LAMP_BRONZE_C * 0.8, LAMP_BRONZE_C * 1.1, 0.5 + 0.6 * fine)
    region = PART_NAMES[part]
    # Light grime in the hollows (the occlusion) and low down; a little rust in grime.
    grime = 0.55 * (1.0 - ao) + 0.12 * smooth(0.2, 0.6, big)
    if part not in (WOOD, RUBBER):
        col = mix(col, GRIME, grime)
        if part != BRASS:
            r = smooth(0.42, 0.62, 0.7 * big + 0.5 * fine) * (0.4 + (1.0 - ao))
            col = mix(col, RUST, 0.45 * r)
    else:
        col = mix(col, GRIME, 0.5 * (1.0 - ao))
    # Worn bright on the convex edges.
    if part not in (RUBBER, LAMP_RED):
        wear = 0.0
        for a, b, convex in edges:
            if convex:
                wear = max(wear, smooth(0.28, 0.02, seg_dist(p, a, b)))
        if wear > 0.0:
            bright = EDGE if part not in (WOOD, BRASS) else (WOOD_LIGHT * 1.1 if part == WOOD else BRASS_C * 1.3)
            col = mix(col, bright, wear * (0.45 + 0.35 * smooth(-0.3, 0.4, fine)))
    return col * ao ** 0.35, region


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
        "muzzle_av": (MUZZLE_X, 0.0, BORE_Z),
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
