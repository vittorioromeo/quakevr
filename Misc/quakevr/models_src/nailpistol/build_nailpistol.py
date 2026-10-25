# build_nailpistol.py -- makes nailpistol.blend from nothing: the nail pistol's shapes, their UVs, and a first skin
# painted procedurally in Quake's palette (rust and grime, brighter worn edges, darker crevices, painted rivets), then
# exports the MDLs (export_mdl.py). README.md says how to edit it by hand instead.
#
#   blender -b --factory-startup -P Misc/quakevr/models_src/nailpistol/build_nailpistol.py
#
# It OVERWRITES nailpistol.blend and the skins: once the .blend has been edited by hand, edit that and run
# export_mdl.py, not this.
#
# Units are Quake model units, as Quake's view weapons (v_nail.mdl, v_shot.mdl): +X forward (the muzzle), +Y left, +Z
# up. The pistol is ~13.5 units long: drawn at a view weapon's usual scale (vr_wofs_scale ~0.45) that is ~23 cm.
# Below, shapes are placed in "design" coordinates; the Body object's origin is the grip's hand point HAND.

import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector, noise
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import export_mdl  # noqa: E402

SKIN_W, SKIN_H = 256, 256

# Parts (a face's material index while building, then its np_part attribute): what the skin paints there.
IRON, DARK, COPPER, BRASS, BORE, SLOT, GRIP, NAIL, MAG, PLATE, SIGHT = range(11)
CHAMFER = 16  # added to a part: a chamfer of it

GRIP_TILT = math.radians(12.0)         # the grip leans back this much from vertical
GRIP_TOP = Vector((-2.3, 0.0, 1.0))    # the grip axis where it meets the receiver's bottom
GRIP_FWD = Vector((math.cos(GRIP_TILT), 0.0, -math.sin(GRIP_TILT)))
GRIP_UP = Vector((math.sin(GRIP_TILT), 0.0, math.cos(GRIP_TILT)))
HAND = GRIP_TOP - GRIP_UP * 2.0        # the hand point: the grip's middle, two units down its axis
SLIDE_REST = Vector((-0.1, 0.0, 4.6))  # the slide's origin at rest (its bottom middle); it snaps back along -X
TRIGGER_PIVOT = Vector((1.5, 0.0, 1.0))
MAG_TOP = GRIP_TOP + GRIP_UP * 0.25    # the magazine's origin, its top, in place

XZ = (Vector((1, 0, 0)), Vector((0, 0, 1)))   # side profiles (x, z), extruded across y (a = -y)
YZ = (Vector((0, 1, 0)), Vector((0, 0, 1)))   # sections across x (a = x)


def ccw(poly):
    n = len(poly)
    area = sum(poly[i][0] * poly[(i + 1) % n][1] - poly[(i + 1) % n][0] * poly[i][1] for i in range(n))
    return poly if area > 0 else poly[::-1]


def prism(bm, poly, a0, a1, frame, part, origin=Vector()):
    """poly (2D, in the frame's U V) from a0 to a1 along U x V, closed."""
    U, V = frame
    A = U.cross(V)
    poly = ccw(poly)
    v0 = [bm.verts.new(origin + U * u + V * v + A * a0) for u, v in poly]
    v1 = [bm.verts.new(origin + U * u + V * v + A * a1) for u, v in poly]
    faces = [bm.faces.new(list(reversed(v0))), bm.faces.new(v1)]
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        faces.append(bm.faces.new((v0[i], v0[j], v1[j], v1[i])))
    for f in faces:
        f.material_index = part
    return faces


def side(bm, poly, y0, y1, part):
    """A side profile poly [(x, z)] from y0 to y1."""
    return prism(bm, poly, -y1, -y0, XZ, part)


def bevel_caps(bm, faces, offset):
    """Chamfers the outline of a prism's two end faces (faces[0], faces[1]: a side profile's two sides)."""
    edges = list({e for f in faces[:2] for e in f.edges})
    verts = list({v for e in edges for v in e.verts})
    part = faces[0].material_index
    ret = bmesh.ops.bevel(bm, geom=edges + verts, offset=offset, offset_type="OFFSET", segments=1, profile=0.5,
                          affect="EDGES", clamp_overlap=True)
    for f in ret["faces"]:
        f.material_index = part + CHAMFER  # (painted a little brighter: they catch the light)


def bevel_rect(hx, hy, b):
    return [(hx - b, -hy), (hx, -hy + b), (hx, hy - b), (hx - b, hy), (-hx + b, hy), (-hx, hy - b), (-hx, -hy + b),
            (-hx + b, -hy)]


def lathe(bm, rings, sides, centre, parts, rot=0.0):
    """Rings [(x, r)] around the x axis through centre, each band k painted parts[k]; capped at both ends."""
    vs = []
    for x, r in rings:
        vs.append([bm.verts.new(centre + Vector((x, r * math.cos(rot + i * 2 * math.pi / sides),
                                                 r * math.sin(rot + i * 2 * math.pi / sides)))) for i in range(sides)])
    for k in range(len(rings) - 1):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((vs[k][i], vs[k][j], vs[k + 1][j], vs[k + 1][i])).material_index = parts[k]
    bm.faces.new(vs[0]).material_index = parts[0]
    bm.faces.new(vs[-1]).material_index = parts[-1]


def cut_slots(bm, half_y, slots, depth):
    """Recessed vent slots [(x0, x1, z0, z1)] in both side faces (y = +-half_y) of what bm holds."""
    for sgn in (1.0, -1.0):
        def side_faces():
            bm.normal_update()
            return [f for f in bm.faces
                    if f.normal.y * sgn > 0.999 and abs(f.calc_center_median().y - sgn * half_y) < 1e-4]
        cuts = [(Vector((x, 0, 0)), Vector((1, 0, 0))) for s in slots for x in s[:2]]
        cuts += [(Vector((0, 0, z)), Vector((0, 0, 1))) for s in slots for z in s[2:]]
        for co, no in cuts:
            fs = side_faces()
            geom = fs + list({e for f in fs for e in f.edges}) + list({v for f in fs for v in f.verts})
            bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co, plane_no=no)
        for x0, x1, z0, z1 in slots:
            fs = [f for f in side_faces() if x0 < f.calc_center_median().x < x1 and z0 < f.calc_center_median().z < z1]
            ret = bmesh.ops.extrude_face_region(bm, geom=fs)
            bmesh.ops.delete(bm, geom=fs, context="FACES_ONLY")  # (the faces extruded stay behind: the slot's lid)
            verts = [g for g in ret["geom"] if isinstance(g, bmesh.types.BMVert)]
            for g in ret["geom"]:
                if isinstance(g, bmesh.types.BMFace):
                    g.material_index = SLOT
                    for e in g.edges:
                        for wall in e.link_faces:
                            wall.material_index = SLOT
            bmesh.ops.translate(bm, vec=Vector((0, -sgn * depth, 0)), verts=verts)


def finish(bm, name, location, parts_layer=True):
    """bm (in design coordinates) as an object whose origin is at `location` (design)."""
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(0.5), use_dissolve_boundaries=False, verts=bm.verts[:],
                             edges=bm.edges[:], delimit={"MATERIAL"})
    bmesh.ops.dissolve_degenerate(bm, dist=1e-4, edges=bm.edges[:])
    bm.transform(Matrix.Translation(-location))
    layer = bm.faces.layers.int.new("np_part")
    for f in bm.faces:
        f[layer] = f.material_index
        f.material_index = 0
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = False
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = location - HAND
    return ob


def build_body():
    bm = bmesh.new()
    # The receiver: chamfered at the front, top and bottom; three vent slots in each side.
    receiver = [(-2.6, 1.0), (6.95, 1.0), (7.4, 1.45), (7.4, 4.15), (6.95, 4.6), (-2.6, 4.6)]
    bevel_caps(bm, side(bm, receiver, -1.2, 1.2, IRON), 0.2)
    cut_slots(bm, 1.2, [(2.2, 4.9, 3.42, 3.84), (2.2, 4.9, 2.66, 3.08), (2.2, 4.9, 1.9, 2.32)], 0.32)
    # (the rest is added after the cuts: they only looked at the receiver)
    # The rear housing: a little wider and taller, its back-bottom corner cut away; the slide snaps back into it.
    housing = [(-2.4, 1.0), (-2.4, 5.05), (-4.3, 5.05), (-4.6, 4.75), (-4.6, 1.9), (-3.7, 1.0)]
    bevel_caps(bm, side(bm, housing, -1.3, 1.3, DARK), 0.22)
    # The muzzle: a hexagonal nozzle with a deep hexagonal bore.
    lathe(bm, [(7.2, 1.0), (8.35, 1.0), (8.55, 0.85), (8.55, 0.52), (7.7, 0.52)], 6, Vector((0, 0, 2.8)),
          [IRON, IRON, IRON, BORE, BORE])
    # The grip: an octagonal section, leaning back; its bottom open to the magazine (the baseplate covers it).
    prism(bm, bevel_rect(1.5, 1.1, 0.32), -5.0, 0.4, (GRIP_FWD, Vector((0, 1, 0))), GRIP, GRIP_TOP)
    # The trigger guard: one L-shaped piece from the grip's front to the receiver, thick.
    guard = [(-1.4, -1.3), (3.3, -1.3), (3.85, -0.85), (3.35, 1.1), (2.5, 1.1), (2.9, -0.45), (-1.4, -0.45)]
    bevel_caps(bm, side(bm, guard, -0.6, 0.6, IRON), 0.12)
    # The front sight: a post on a little base. The rear sight: two ears and a notch on the housing.
    side(bm, [(5.95, 4.5), (6.85, 4.5), (6.85, 4.62), (6.7, 4.74), (5.95, 4.74)], -0.4, 0.4, SIGHT)
    side(bm, [(6.2, 4.6), (6.6, 4.6), (6.6, 5.2), (6.45, 5.42), (6.2, 5.42)], -0.17, 0.17, SIGHT)
    side(bm, [(-4.4, 4.95), (-3.45, 4.95), (-3.45, 5.18), (-4.4, 5.18)], -0.92, 0.92, SIGHT)
    for y0, y1 in ((0.28, 0.92), (-0.92, -0.28)):
        side(bm, [(-4.4, 5.1), (-3.55, 5.1), (-3.55, 5.42), (-4.22, 5.42), (-4.4, 5.3)], y0, y1, SIGHT)
    return finish(bm, "Body", HAND)


def build_slide():
    bm = bmesh.new()
    slide = [(-2.3, 4.5), (2.1, 4.5), (2.1, 4.82), (1.92, 5.0), (-2.15, 5.0), (-2.3, 4.85)]
    bevel_caps(bm, side(bm, slide, -0.92, 0.92, COPPER), 0.1)
    return finish(bm, "Slide", SLIDE_REST)


def build_trigger():
    bm = bmesh.new()
    side(bm, [(1.3, 1.15), (1.78, 1.15), (1.62, 0.5), (1.72, 0.0), (1.98, -0.3), (1.72, -0.36), (1.4, 0.0),
              (1.26, 0.5)], -0.22, 0.22, SIGHT)
    return finish(bm, "Trigger", TRIGGER_PIVOT)


def build_magazine():
    """In its own frame (its origin the magazine's top; it slides out along -Z), then placed in the grip."""
    bm = bmesh.new()
    prism(bm, bevel_rect(1.12, 0.74, 0.18), -5.3, 0.0, (Vector((1, 0, 0)), Vector((0, 1, 0))), MAG)
    side(bm, [(-1.62, -5.2), (1.7, -5.2), (1.7, -5.55), (1.42, -5.82), (-1.35, -5.82), (-1.62, -5.55)],
         -1.18, 1.18, PLATE)
    # The top two nails of the stack, lying point forward, half out of the feed lips.
    for y, z in ((0.24, 0.1), (-0.24, -0.02)):
        side(bm, [(-0.9, z - 0.11), (0.55, z - 0.11), (0.55, z + 0.11), (-0.9, z + 0.11)], y - 0.11, y + 0.11, NAIL)
        side(bm, [(-1.02, z - 0.2), (-0.88, z - 0.2), (-0.88, z + 0.2), (-1.02, z + 0.2)], y - 0.2, y + 0.2, NAIL)
        tip = [bm.verts.new(Vector((0.55, y + dy, z + dz))) for dy, dz in ((-0.11, -0.11), (0.11, -0.11),
                                                                            (0.11, 0.11), (-0.11, 0.11))]
        point = bm.verts.new(Vector((1.0, y, z)))
        bm.faces.new(list(reversed(tip))).material_index = NAIL
        for i in range(4):
            bm.faces.new((tip[i], tip[(i + 1) % 4], point)).material_index = NAIL
    ob = finish(bm, "Magazine", Vector())
    ob.location = MAG_TOP - HAND
    ob.rotation_euler = (0.0, GRIP_TILT, 0.0)
    return ob


# ----------------------------------------------------------------------------------------------------------------------
# The skin

def rgb(*c):
    return Vector(c)


BASE = {IRON: rgb(80, 68, 56), DARK: rgb(68, 60, 52), COPPER: rgb(168, 96, 46), BRASS: rgb(164, 128, 60),
        BORE: rgb(26, 22, 18), SLOT: rgb(36, 30, 24), GRIP: rgb(72, 60, 48), NAIL: rgb(128, 126, 120),
        MAG: rgb(72, 62, 52), PLATE: rgb(150, 84, 40), SIGHT: rgb(66, 62, 58)}
EDGE = {COPPER: rgb(232, 164, 92), BRASS: rgb(220, 184, 108), NAIL: rgb(196, 196, 186), PLATE: rgb(214, 140, 76)}
EDGE_IRON = rgb(134, 128, 118)
RUST = rgb(128, 72, 40)
RUSTY = (IRON, DARK, GRIP, MAG, SIGHT)

# Rivets (design coordinates: a point on a side face and its radius), on both sides.
RIVETS_XZ = [((6.4, 4.05), 1.2, 0.24), ((6.4, 1.55), 1.2, 0.24), ((1.35, 4.05), 1.2, 0.24), ((1.35, 1.55), 1.2, 0.24),
             ((-1.7, 2.8), 1.2, 0.24), ((-3.15, 4.5), 1.3, 0.24), ((-3.15, 1.6), 1.3, 0.24), ((-4.15, 3.1), 1.3, 0.24),
             ((3.25, -0.88), 0.6, 0.16)]
RIVETS_GRIP = [(0.78, -4.45), (-0.78, -4.45), (0.0, -0.75)]   # (along GRIP_FWD, along GRIP_UP from GRIP_TOP)


def rivets():
    out = []
    for (x, z), hy, r in RIVETS_XZ:
        for s in (1.0, -1.0):
            out.append((Vector((x, s * hy, z)) - HAND, Vector((0, s, 0)), r))
    for u, a in RIVETS_GRIP:
        for s in (1.0, -1.0):
            out.append((GRIP_TOP + GRIP_FWD * u + GRIP_UP * a + Vector((0, s * 1.1, 0)) - HAND, Vector((0, s, 0)), 0.2))
    return out


def hemisphere(n):
    rng = random.Random(7)
    out = []
    for i in range(n):
        u1, u2 = (i + rng.random()) / n, rng.random()
        r, phi = math.sqrt(u1), 2 * math.pi * u2
        out.append((r * math.cos(phi), r * math.sin(phi), math.sqrt(max(0.0, 1.0 - u1))))
    return out


AO_DIRS = hemisphere(28)
AO_DIST = 1.4


def occlusion(bvh, p, n):
    t1 = n.orthogonal().normalized()
    t2 = n.cross(t1)
    o = p + n * 0.012
    occ = 0.0
    for a, b, c in AO_DIRS:
        hit, _, _, dist = bvh.ray_cast(o, t1 * a + t2 * b + n * c, AO_DIST)
        if hit is not None:
            occ += 1.0 - (dist / AO_DIST) ** 0.7
    return 1.0 - occ / len(AO_DIRS)


def seg_dist(p, a, b):
    ab = b - a
    if ab.length_squared < 1e-12:
        return (a - p).length
    t = max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
    return (a + ab * t - p).length


def mix(a, b, k):
    return a + (b - a) * max(0.0, min(1.0, k))


def smooth(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def paint_texel(p, n, part, chamfer, edges, rvts, ao, speck, ob_name):
    d = p + HAND  # design coordinates
    col = BASE[part].copy()
    big = noise.noise(d * 0.8)
    mid = noise.noise(d * 2.6 + Vector((3.1, 7.7, 1.3)))
    fine = noise.noise(d * 7.5 + Vector((9.2, 1.4, 5.5)))
    if part in RUSTY:
        rust = smooth(0.05, 0.55, 0.55 * big + 0.45 * mid + 0.25 * fine)
        col = mix(col, RUST, 0.75 * rust)
        col *= 0.88 + 0.16 * mid + 0.12 * fine
        if fine > 0.45:
            col *= 0.72  # pitting
    elif part in (COPPER, PLATE, BRASS):
        col *= 0.9 + 0.12 * mid + 0.1 * fine
        green = smooth(0.35, 0.7, big * 0.6 + fine * 0.5)  # a little tarnish
        col = mix(col, rgb(92, 72, 44), 0.5 * green)
    else:
        col *= 0.92 + 0.1 * fine
    col *= 1.0 + speck
    if part == SLOT and n.z > 0.5:
        col *= 1.9  # a slot's floor catches the light
    # The slide: grip serrations at its back, a feed window with the nails' heads showing.
    if ob_name == "Slide":
        if d.x < -0.5 and (abs(n.y) > 0.9 or n.z > 0.9):
            g = ((d.x + 2.25) / 0.3) % 1.0
            col = col * 0.5 if g < 0.38 else (mix(col, EDGE[COPPER], 0.6) if g < 0.55 else col)
        if n.z > 0.9 and 0.25 < d.x < 1.65 and abs(d.y) < 0.46:
            col = rgb(30, 24, 18)
            for ny in (-0.2, 0.2):
                if abs(d.y - ny) < 0.09:
                    col = mix(BASE[BRASS], EDGE[BRASS], 0.5 if abs(d.y - ny) < 0.04 else 0.0)
            if abs(d.y) > 0.4 or d.x < 0.31 or d.x > 1.59:
                col = EDGE[COPPER] * 0.8
    # The magazine: a witness slot down each side, the stacked nails' heads showing in it.
    if part == MAG and abs(n.y) > 0.9:
        rel = d - MAG_TOP
        u, a = rel.dot(GRIP_FWD), rel.dot(GRIP_UP)
        if abs(u + 0.55) < 0.2 and -4.7 < a < -0.6:
            col = rgb(28, 24, 20)
            if abs(u + 0.55) < 0.12 and ((a + 0.6) / 0.34) % 1.0 < 0.55:
                col = mix(BASE[BRASS], EDGE[BRASS], 0.6 if ((a + 0.6) / 0.34) % 1.0 < 0.2 else 0.0)
        elif abs(u + 0.55) < 0.27 and -4.77 < a < -0.53:
            col = mix(col, EDGE_IRON, 0.45)  # the slot's worn rim
    # The grip: a raised panel of cross-hatched checkering.
    if part == GRIP and abs(n.y) > 0.9:
        rel = d - GRIP_TOP
        u, a = rel.dot(GRIP_FWD), rel.dot(GRIP_UP)
        if abs(u) < 1.0 and -4.0 < a < -1.15:
            g1, g2 = ((u + a) / 0.36) % 1.0, ((u - a) / 0.36) % 1.0
            if g1 < 0.22 or g2 < 0.22:
                col *= 0.58
            elif g1 < 0.4 and g2 > 0.4:
                col *= 1.18
            if abs(u) > 0.9 or a < -3.9 or a > -1.25:
                col *= 0.6
    # Worn, brighter convex edges; darker concave ones.
    wear = smooth(-0.35, 0.35, noise.noise(d * 3.3 + Vector((5.0, 2.0, 8.0))))
    edge_col = EDGE.get(part) or col * 1.05 + EDGE_IRON * 0.45  # (worn iron: the colour lighter, towards steel)
    if chamfer:
        col = mix(col, edge_col, 0.2 + 0.35 * wear)
    for a, b, convex in edges:
        dist = seg_dist(p, a, b)
        if convex and dist < 0.075 and not chamfer:
            col = mix(col, edge_col, (0.35 + 0.5 * wear) * (1.0 - dist / 0.075) + 0.15)
        elif not convex and dist < 0.16:
            col *= 0.6 + 0.4 * dist / 0.16
    # Rivets: domes lit from above and in front, a shadow below and behind.
    for c, rn, r in rvts:
        if n.dot(rn) < 0.95:
            continue
        v = p - c
        if abs(v.dot(rn)) > 0.1:
            continue
        v -= rn * v.dot(rn)
        k = v.length / r
        if k < 1.0:
            dome = (rn * math.sqrt(1.0 - k * k) + v / r).normalized()
            light = (rn * 0.6 + Vector((0.35, 0, 0.75))).normalized()
            lit = max(0.0, dome.dot(light))
            col = mix(rgb(52, 44, 36), rgb(196, 182, 156), lit ** 1.5)
        elif k < 1.4 and (v.z < 0.0 or v.x < -0.5 * r):
            col *= 0.55
    col *= ao
    col *= 0.94 + 0.1 * n.z
    return col


def bake(objs, occluders, image):
    """Paints image (all of objs' UV islands) procedurally; returns the RGB texels (top row first)."""
    w, h = image.size
    tex = [None] * (w * h)
    rvts = rivets()
    rng = random.Random(1)
    for ob in objs:
        verts, polys = [], []
        for oc in occluders[ob.name]:
            m = oc.matrix_world
            base = len(verts)
            verts += [m @ v.co for v in oc.data.vertices]
            polys += [[base + i for i in p.vertices] for p in oc.data.polygons]
        bvh = BVHTree.FromPolygons(verts, polys)
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bm.transform(ob.matrix_world)
        bm.normal_update()
        uv = bm.loops.layers.uv.active
        part_layer = bm.faces.layers.int.get("np_part")
        face_edges = {}
        for f in bm.faces:
            es = []
            for e in f.edges:
                if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(25):
                    es.append((e.verts[0].co.copy(), e.verts[1].co.copy(), e.is_convex))
            face_edges[f.index] = es
        for tri in bm.calc_loop_triangles():
            f = tri[0].face
            part, chamfer = f[part_layer] % CHAMFER, f[part_layer] >= CHAMFER
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
                    if min(b0, b1, b2) < -1e-4:
                        continue
                    p = ps[0] * b0 + ps[1] * b1 + ps[2] * b2
                    ao = occlusion(bvh, p, n)
                    speck = (rng.random() - 0.5) * 0.22
                    tex[t * w + s] = paint_texel(p, n, part, chamfer, face_edges[f.index], rvts, 0.3 + 0.7 * ao,
                                                 speck, ob.name)
        bm.free()
    # Bleed each island's edge texels outward (filtered and mipmapped, no dark seams), the rest dark.
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
    return [tuple(max(0, min(255, int(round(c[k])))) for k in range(3)) if c is not None else (40, 34, 28)
            for c in tex]


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = "NONE"
    body = build_body()
    slide = build_slide()
    trigger = build_trigger()
    mag = build_magazine()
    for ob in (slide, trigger, mag):
        ob.parent = body
    slide["travel"] = 1.0
    body["preview_scale"] = 0.45
    for ob, text in ((body, "Quake axes: +X forward (muzzle), +Y left, +Z up. Origin: the grip's hand point."),
                     (slide, "Snaps back along its local -X by 'travel' units (export frame 1)."),
                     (mag, "Origin: the magazine's top. Comes out along its local -Z.")):
        ob["note"] = text
    objs = [body, slide, trigger, mag]

    # UVs: one atlas for all of them (one skin), every corner on a texel's centre (as the MDL stores it).
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(50), island_margin=0.012, area_weight=0.0, correct_aspect=True,
                             scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    for ob in objs:
        for d in ob.data.uv_layers.active.data:
            s = min(SKIN_W - 1, max(0, math.floor(d.uv.x * SKIN_W)))
            t = min(SKIN_H - 1, max(0, math.floor((1.0 - d.uv.y) * SKIN_H)))
            d.uv = ((s + 0.5) / SKIN_W, 1.0 - (t + 0.5) / SKIN_H)

    bpy.context.view_layer.update()
    img = bpy.data.images.new("nailpistol_skin", SKIN_W, SKIN_H, alpha=False)
    occluders = {ob.name: objs for ob in (body, slide, trigger)}
    occluders[mag.name] = [mag]
    texels = bake(objs, occluders, img)
    q = export_mdl.qpal.Quantizer()
    pal = export_mdl.qpal.PALETTE
    px = [0.0] * (SKIN_W * SKIN_H * 4)
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            c = pal[q.nearest(texels[t * SKIN_W + s], False)]
            i = ((SKIN_H - 1 - t) * SKIN_W + s) * 4
            px[i:i + 4] = [c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, 1.0]
    img.pixels = px
    img.filepath_raw = os.path.join(HERE, "nailpistol_skin.png")
    img.file_format = "PNG"
    img.save()

    mat = bpy.data.materials.new("NailPistolSkin")
    if bpy.app.version < (5, 0, 0):
        mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = nodes.get("Principled BSDF")
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = "Closest"
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.9
    for ob in objs:
        ob.data.materials.append(mat)

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "nailpistol.blend"), relative_remap=True)
    export_mdl.export()


main()
