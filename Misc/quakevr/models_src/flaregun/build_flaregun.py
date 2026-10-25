# build_flaregun.py -- makes flaregun.blend from nothing: the disposable double-barrelled flare gun's shapes, their UVs,
# and a first skin painted procedurally in Quake's palette (signal orange-red paint, rust and chipped paint, brighter worn
# edges, darker crevices, a wooden grip), with its two "fired" overlays, then exports the MDLs (export_mdl.py).
# README.md says how to edit it by hand instead.
#
#   blender -b --factory-startup -P Misc/quakevr/models_src/flaregun/build_flaregun.py
#
# It OVERWRITES flaregun.blend and the skins: once the .blend has been edited by hand, edit that and run export_mdl.py,
# not this.
#
# Units are Quake model units, as Quake's view weapons (v_nail.mdl, v_shot.mdl): +X forward (the muzzles), +Y left, +Z
# up. Below, shapes are placed in "design" coordinates; the Body object's origin is the grip's hand point HAND.
# The two barrels are exact copies along +X (one lathe, two centres on the same vertical line): parallel by construction.

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

# Parts (a face's material index while building, then its fg_part attribute): what the skin paints there.
PAINT, BOLT, BORE, WOOD, CART, BRASS, CAP, SIGHT, PRIMER = range(9)
CHAMFER = 16  # added to a part: a chamfer of it

# The barrels: one shape, two centres on the same vertical line, both along +X.
BARREL_R, BORE_R, BARREL_SIDES = 1.2, 0.8, 12
BARREL_TOP_Z, BARREL_BOT_Z = 5.0, 2.7
BARREL_X0, MUZZLE_X, BORE_DEPTH = 2.4, 10.5, 2.6  # (10.5: the barrels half as long again, Vittorio 2026-10-26; was 7.8)
# The frame's flat top (behind the hinge block, over the breeches).
FRAME_TOP_Z = 6.2
# The sight line: the rear notch's bottom (cut in a low hump at the frame top's rear end) and the front blade's top, both
# at SIGHT_Z: parallel to the bores.
SIGHT_Z = 6.6
SIGHT_REAR_X, SIGHT_FRONT_X = -0.35, MUZZLE_X - 0.25  # (the blade 0.25 behind the muzzle)
NOTCH_HALF_W, EAR_TOP_Z = 0.16, SIGHT_Z + 0.28  # the notch's half width; the hump's top beside it

GRIP_TILT = math.radians(30.0)         # the grip leans back this much from vertical
GRIP_TOP = Vector((-0.5, 0.0, 2.2))    # the grip axis where it meets the frame's bottom
GRIP_FWD = Vector((math.cos(GRIP_TILT), 0.0, -math.sin(GRIP_TILT)))
GRIP_UP = Vector((math.sin(GRIP_TILT), 0.0, math.cos(GRIP_TILT)))
HAND = GRIP_TOP - GRIP_UP * 2.0        # the hand point: two units down the grip's axis

XZ = (Vector((1, 0, 0)), Vector((0, 0, 1)))   # side profiles (x, z), extruded across y (a = -y)


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
    """Chamfers the outline of a prism's two end faces (faces[0], faces[1])."""
    edges = list({e for f in faces[:2] for e in f.edges})
    verts = list({v for e in edges for v in e.verts})
    part = faces[0].material_index
    ret = bmesh.ops.bevel(bm, geom=edges + verts, offset=offset, offset_type="OFFSET", segments=1, profile=0.5,
                          affect="EDGES", clamp_overlap=True)
    for f in ret["faces"]:
        f.material_index = part + CHAMFER  # (painted a little brighter: they catch the light)


def rect(x0, x1, z0, z1):
    return [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]


def lathe(bm, rings, sides, centre, parts, rot=0.0, caps=None):
    """Rings [(x, r)] around the x axis through centre, each band k painted parts[k]; capped at both ends (painted
    caps, or the first and last bands' parts)."""
    caps = caps or (parts[0], parts[-1])
    vs = []
    for x, r in rings:
        vs.append([bm.verts.new(centre + Vector((x, r * math.cos(rot + i * 2 * math.pi / sides),
                                                 r * math.sin(rot + i * 2 * math.pi / sides)))) for i in range(sides)])
    for k in range(len(rings) - 1):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((vs[k][i], vs[k][j], vs[k + 1][j], vs[k + 1][i])).material_index = parts[k]
    bm.faces.new(list(reversed(vs[0]))).material_index = caps[0]
    bm.faces.new(vs[-1]).material_index = caps[1]


def hex_bolt(bm, x, z, y_face, sgn, r, h=0.16):
    """A hexagonal bolt head on a side face (y = sgn * y_face), r its corner radius, standing out h."""
    poly = [(x + r * math.cos(math.pi / 6 + i * math.pi / 3), z + r * math.sin(math.pi / 6 + i * math.pi / 3))
            for i in range(6)]
    y0, y1 = (y_face - 0.02, y_face + h) if sgn > 0 else (-y_face - h, -y_face + 0.02)
    side(bm, poly, y0, y1, BOLT)


def finish(bm, name, location):
    """bm (in design coordinates) as an object whose origin is at `location` (design)."""
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(0.5), use_dissolve_boundaries=False, verts=bm.verts[:],
                             edges=bm.edges[:], delimit={"MATERIAL"})
    bmesh.ops.dissolve_degenerate(bm, dist=1e-4, edges=bm.edges[:])
    bm.transform(Matrix.Translation(-location))
    layer = bm.faces.layers.int.new("fg_part")
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


def cartridge(bm, centre, x0, x1, brass_to, r):
    """A flare cartridge along +X: a brass head from x0 to brass_to, a red hull, a crimped red cap (a chamfer and a flat
    front) at x1."""
    lathe(bm, [(x0, r + 0.05), (x0 + 0.12, r + 0.05), (x0 + 0.12, r), (brass_to, r), (x1 - 0.16, r),
               (x1, r * 0.76)], 10, centre, [BRASS, BRASS, BRASS, CART, CAP], rot=math.pi / 10, caps=(PRIMER, CAP))


def grip_xz(u, a):
    """A point of the grip given along GRIP_FWD (u) and GRIP_UP (a) from GRIP_TOP, as design (x, z)."""
    p = GRIP_TOP + GRIP_FWD * u + GRIP_UP * a
    return p.x, p.z


def build_body():
    bm = bmesh.new()
    rot = math.pi / BARREL_SIDES  # a flat on top and at the bottom
    # The barrels: over and under, exactly parallel (one lathe about +X at two heights); wide bores BORE_DEPTH deep.
    for z in (BARREL_TOP_Z, BARREL_BOT_Z):
        lathe(bm, [(BARREL_X0, BARREL_R), (MUZZLE_X - 0.2, BARREL_R), (MUZZLE_X, BARREL_R - 0.12),
                   (MUZZLE_X, BORE_R), (MUZZLE_X - BORE_DEPTH, BORE_R)], BARREL_SIDES, Vector((0, 0, z)),
              [PAINT, PAINT + CHAMFER, PAINT + CHAMFER, BORE], rot=rot)
    # The lug under the bottom barrel, from the hinge block to near the muzzle.
    bevel_caps(bm, side(bm, rect(3.0, MUZZLE_X - 0.4, 1.05, 1.75), -0.78, 0.78, PAINT), 0.14)
    # The hinge block: the break-open joint around both barrels' breeches, a raised strap on each side.
    bevel_caps(bm, side(bm, rect(2.0, 3.6, 1.2, 6.28), -1.42, 1.42, PAINT), 0.22)
    for s in (1.0, -1.0):
        y0, y1 = (1.38, 1.55) if s > 0 else (-1.55, -1.38)
        bevel_caps(bm, side(bm, rect(2.45, 3.15, 1.45, 6.0), y0, y1, PAINT), 0.06)
    # The frame: a plain flat top from the hinge block back over the breeches, rising at its rear end into a low hump
    # (the rear sight), then a tang sloping down and back into the grip. Its middle is the notch's bottom (SIGHT_Z).
    frame = [(2.1, 1.5), (2.1, FRAME_TOP_Z), (0.6, FRAME_TOP_Z), (0.3, SIGHT_Z), (SIGHT_REAR_X, SIGHT_Z),
             (-1.1, 5.8), (-1.25, 4.5), (-1.75, 2.9), (-0.6, 1.9), (0.5, 1.5)]
    bevel_caps(bm, side(bm, frame, -1.15, 1.15, PAINT), 0.24)
    # The rear sight: the hump's two cheeks either side of the notch, a ramp in front, a square back at SIGHT_REAR_X.
    ear = [(0.62, FRAME_TOP_Z - 0.05), (0.1, EAR_TOP_Z), (SIGHT_REAR_X, EAR_TOP_Z), (SIGHT_REAR_X, FRAME_TOP_Z - 0.05)]
    for y0, y1 in ((NOTCH_HALF_W, 1.0), (-1.0, -NOTCH_HALF_W)):
        bevel_caps(bm, side(bm, ear, y0, y1, PAINT), 0.05)
    # The front sight: a blade on a ramp on the top barrel, its top at the sight line.
    top_flat = BARREL_TOP_Z + BARREL_R * math.cos(rot)
    side(bm, [(SIGHT_FRONT_X - 1.0, top_flat - 0.1), (SIGHT_FRONT_X, top_flat - 0.1), (SIGHT_FRONT_X, SIGHT_Z),
              (SIGHT_FRONT_X - 0.3, SIGHT_Z), (SIGHT_FRONT_X - 1.0, top_flat + 0.12)], -0.13, 0.13, SIGHT)
    # The grip: wooden, leaning back GRIP_TILT, its front a little hollowed, flaring back at the bottom.
    grip = [(1.25, 1.6), (1.2, 0.0), (1.1, -2.1), (1.1, -4.1), (1.25, -5.9), (1.05, -6.7), (0.0, -7.05),
            (-1.15, -6.9), (-1.8, -6.3), (-1.75, -5.2), (-1.5, -3.6), (-1.35, -1.4), (-1.4, 0.0), (-1.45, 1.6)]
    bevel_caps(bm, side(bm, [grip_xz(u, a) for u, a in grip], -1.05, 1.05, WOOD), 0.26)
    # The trigger: a curved blade under the frame, in front of the grip.
    side(bm, [(1.0, 1.55), (1.55, 1.55), (1.35, 0.9), (1.4, 0.45), (1.65, 0.18), (1.3, 0.08), (1.05, 0.45),
              (0.95, 1.0)], -0.25, 0.25, BOLT)
    # Bolts: three on each hinge strap, a big one at the frame's rear, two through the grip.
    for s in (1.0, -1.0):
        for z in (5.45, 3.85, 2.2):
            hex_bolt(bm, 2.8, z, 1.55, s, 0.24)
        hex_bolt(bm, -0.45, 5.7, 1.15, s, 0.3, 0.2)
        hex_bolt(bm, *grip_xz(-0.2, -5.9), 1.05, s, 0.3)
        hex_bolt(bm, *grip_xz(0.0, -0.45), 1.05, s, 0.22)
    return finish(bm, "Body", HAND)


def build_flare():
    """The projectile (a flare cartridge on its own), in its own frame: its origin its middle, its cap along +X."""
    bm = bmesh.new()
    cartridge(bm, Vector(), -1.25, 1.25, -0.6, 0.42)
    ob = finish(bm, "Flare", Vector())
    ob.location = Vector((0.0, 0.0, -9.0))  # (beside the gun in the .blend; its export is in its own frame)
    return ob


# ----------------------------------------------------------------------------------------------------------------------
# The skin

def rgb(*c):
    return Vector(c)


# (Quake's palette has one red-orange ramp, 102..107 (115,55,35)..(191,119,47), and the deep reds 64..79: the signal
# paint lives on the first, flecked with the second.)
ORANGE_DARK, ORANGE, ORANGE_LIGHT = rgb(140, 66, 48), rgb(160, 80, 50), rgb(184, 106, 48)
SIGNAL_RED = rgb(122, 6, 2)
RUST, RUST_DARK, METAL = rgb(84, 58, 44), rgb(48, 38, 32), rgb(104, 102, 98)
WOOD_C, WOOD_DARK = rgb(126, 80, 56), rgb(88, 44, 24)
BASE = {BOLT: rgb(86, 74, 62), BORE: rgb(26, 22, 18), CART: rgb(134, 8, 4), BRASS: rgb(186, 142, 60),
        CAP: rgb(120, 6, 3), SIGHT: rgb(92, 88, 82), PRIMER: rgb(170, 130, 58)}
EDGE = {BRASS: rgb(236, 200, 118), PRIMER: rgb(220, 184, 108), CART: rgb(166, 72, 52), CAP: rgb(160, 62, 48),
        WOOD: rgb(168, 118, 72)}
EDGE_IRON = rgb(150, 142, 128)
SOOT = rgb(30, 26, 22)

# Painted rivets (design coordinates: a point on a side face, the face's |y|, the radius), on both sides.
RIVETS_XZ = [((1.6, 5.4), 1.15, 0.2), ((1.6, 1.95), 1.15, 0.2), ((-1.35, 3.2), 1.15, 0.2),
             ((2.2, 4.0), 1.42, 0.17), ((3.4, 4.0), 1.42, 0.17)]


def rivets():
    out = []
    for (x, z), hy, r in RIVETS_XZ:
        for s in (1.0, -1.0):
            out.append((Vector((x, s * hy, z)) - HAND, Vector((0, s, 0)), r))
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


def paint_texel(p, n, part, chamfer, edges, rvts, ao, rnd, dofs, ob_name):
    """The texel's colour in the three skins: [loaded, the top barrel fired, both fired]. p: world; p + dofs: design
    coordinates (the Flare's: its own)."""
    d = p + dofs
    # Across the Flare's axis (only the Flare has cartridge parts).
    cy, cz = d.y, d.z
    big = noise.noise(d * 0.7)
    mid = noise.noise(d * 2.2 + Vector((3.1, 7.7, 1.3)))
    fine = noise.noise(d * 6.5 + Vector((9.2, 1.4, 5.5)))
    speck = (rnd[0] - 0.5) * 0.18
    # How near a worn convex edge: paint chips there first.
    near_edge = 0.0
    for a, b, convex in edges:
        if convex:
            near_edge = max(near_edge, 1.0 - seg_dist(p, a, b) / 0.3)
    if part == PAINT:
        col = mix(ORANGE_DARK, ORANGE_LIGHT, 0.5 + 0.6 * mid + 0.3 * fine)
        rust = smooth(0.08, 0.28, 0.65 * big + 0.45 * mid + 0.2 * fine + 0.4 * max(0.0, near_edge) - 0.05)
        if rust > 0.0:
            under = mix(RUST, RUST_DARK, smooth(-0.2, 0.4, fine + 0.3 * mid))
            if rnd[1] < 0.22 + 0.25 * max(0.0, near_edge):
                under = mix(METAL, under, 0.35)  # bare metal glinting through
            col = mix(col, under, rust)
        elif rnd[1] < 0.05:
            col = mix(col, RUST, 0.6)  # a fleck
        elif rnd[1] < 0.05 + 0.09 * smooth(-0.3, 0.3, mid):
            col = mix(col, SIGNAL_RED, 0.7)  # the paint's red, where it is thickest
    elif part == WOOD:
        rel = d - GRIP_TOP
        u, a = rel.dot(GRIP_FWD), rel.dot(GRIP_UP)
        grain = noise.noise(Vector((u * 5.0, d.y * 5.0, a * 0.7)) + Vector((2.0, 4.0, 6.0)))
        col = mix(WOOD_DARK, WOOD_C, 0.6 + 0.8 * grain)
        if abs(grain) < 0.06:
            col *= 0.72  # a grain line
        if rnd[1] < 0.08:
            col *= 0.7
    elif part in (BOLT, SIGHT):
        col = BASE[part] * (0.9 + 0.15 * mid)
        col = mix(col, RUST, 0.6 * smooth(0.0, 0.5, big + 0.5 * fine))
        if part == SIGHT and d.z > SIGHT_Z - 0.3 and n.x < 0.5:
            col = rgb(214, 176, 104) * (0.92 + 0.12 * rnd[0])  # the blade's top and back: bright brass, to catch the eye
    elif part == CART:
        col = BASE[CART] * (0.92 + 0.12 * mid) * (0.8 + 0.3 * max(0.0, n.z))
        if rnd[1] < 0.06:
            col *= 0.75
    elif part in (BRASS, PRIMER):
        col = BASE[part] * (0.9 + 0.15 * mid)
        col = mix(col, rgb(100, 80, 44), 0.5 * smooth(0.3, 0.7, big * 0.6 + fine * 0.5))  # tarnish
    else:
        col = BASE[part] * (0.92 + 0.1 * fine)
    col *= 1.0 + speck
    # Worn, brighter convex edges; darker concave ones.
    wear = smooth(-0.35, 0.35, noise.noise(d * 3.3 + Vector((5.0, 2.0, 8.0))))
    if part == PAINT:
        edge_col = mix(ORANGE_LIGHT, METAL * 1.15, wear)
    else:
        edge_col = EDGE.get(part) or col * 1.05 + EDGE_IRON * 0.45
    if chamfer:
        col = mix(col, edge_col, 0.25 + 0.35 * wear)
    for a, b, convex in edges:
        dist = seg_dist(p, a, b)
        if convex and dist < 0.075 and not chamfer:
            col = mix(col, edge_col, (0.35 + 0.5 * wear) * (1.0 - dist / 0.075) + 0.15)
        elif not convex and dist < 0.16:
            col *= 0.55 + 0.45 * dist / 0.16
    # Painted rivets: domes lit from above and in front, a shadow below and behind.
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
            col = mix(rgb(60, 40, 30), rgb(220, 150, 110), max(0.0, dome.dot(light)) ** 1.5)
        elif k < 1.4 and (v.z < 0.0 or v.x < -0.5 * r):
            col *= 0.55
    # The loaded cap: crimp lines on its front.
    if part == CAP and n.x > 0.9:
        ang = math.atan2(cz, cy)
        if (ang * 3.0 / math.pi) % 1.0 < 0.22 or math.hypot(cy, cz) < 0.07:
            col *= 0.62
    # The primer: a small disc in the head's middle.
    if part == PRIMER and n.x < -0.9 and math.hypot(cy, cz) < 0.14:
        col = mix(rgb(150, 150, 140), EDGE_IRON, 0.5)
    col *= ao
    col *= 0.94 + 0.1 * n.z
    out = [col, col, col]
    if ob_name != "Body":
        return out
    # The fired skins: a fired barrel's muzzle sooted (skin 1 the top one, skin 2 both).
    for skin in (1, 2):
        fired_bar = (abs(d.z - BARREL_TOP_Z) < BARREL_R + 0.1) if skin == 1 else True
        c = out[skin]
        if fired_bar and d.x > MUZZLE_X - 0.9 and part in (PAINT, BORE) and (chamfer or part == BORE):
            near_bore = abs(d.z - BARREL_TOP_Z) < BARREL_R + 0.1 or abs(d.z - BARREL_BOT_Z) < BARREL_R + 0.1
            if near_bore:
                c = mix(c, SOOT, 0.55 + 0.3 * rnd[1])
        out[skin] = c
    return out


def bake(objs, occluders, w, h):
    """Paints the three skins (all of objs' UV islands) procedurally; returns their RGB texels (top row first)."""
    texs = [[None] * (w * h) for _ in range(3)]
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
        dofs = HAND if ob.name == "Body" else -ob.matrix_world.translation
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bm.transform(ob.matrix_world)
        bm.normal_update()
        uv = bm.loops.layers.uv.active
        part_layer = bm.faces.layers.int.get("fg_part")
        rv = rvts if ob.name == "Body" else []
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
                    rnd = (rng.random(), rng.random())
                    cols = paint_texel(p, n, part, chamfer, face_edges[f.index], rv, 0.3 + 0.7 * ao, rnd, dofs,
                                       ob.name)
                    for k in range(3):
                        texs[k][t * w + s] = cols[k]
        bm.free()
    # Bleed each island's edge texels outward (filtered and mipmapped, no dark seams), the rest dark.
    out = []
    for tex in texs:
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
        out.append([tuple(max(0, min(255, int(round(c[k])))) for k in range(3)) if c is not None else (40, 34, 28)
                    for c in tex])
    return out


def save_image(name, texels, overlay_of=None):
    """texels as a palette-quantized PNG next to this script; with overlay_of, only the texels that differ from it
    (the rest transparent): a fired skin is the skin with its overlay on top (export_mdl.py)."""
    q = export_mdl.qpal.Quantizer()
    pal = export_mdl.qpal.PALETTE
    img = bpy.data.images.new(name, SKIN_W, SKIN_H, alpha=overlay_of is not None)
    px = [0.0] * (SKIN_W * SKIN_H * 4)
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            k = t * SKIN_W + s
            c = pal[q.nearest(texels[k], False)]
            a = 1.0
            if overlay_of is not None and c == pal[q.nearest(overlay_of[k], False)]:
                c, a = (0, 0, 0), 0.0
            i = ((SKIN_H - 1 - t) * SKIN_W + s) * 4
            px[i:i + 4] = [c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, a]
    img.pixels = px
    img.filepath_raw = os.path.join(HERE, name + ".png")
    img.file_format = "PNG"
    img.save()
    img.filepath = "//" + name + ".png"
    return img


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = "NONE"
    body = build_body()
    flare = build_flare()
    body["preview_scale"] = 0.45
    body["note"] = "Quake axes: +X forward (muzzles), +Y left, +Z up. Origin: the grip's hand point."
    flare["note"] = "The flare projectile: exported alone (vr_flare.mdl), in its own frame; its cap along +X."
    # Markers (not exported: names starting with _): the muzzles (their axes +X) and the sight line's two points.
    for name, pt in (("_MuzzleTop", Vector((MUZZLE_X, 0, BARREL_TOP_Z))),
                     ("_MuzzleBottom", Vector((MUZZLE_X, 0, BARREL_BOT_Z))),
                     ("_SightRear", Vector((SIGHT_REAR_X, 0, SIGHT_Z))),
                     ("_SightFront", Vector((SIGHT_FRONT_X, 0, SIGHT_Z)))):
        e = bpy.data.objects.new(name, None)
        e.empty_display_type = "SINGLE_ARROW" if "Muzzle" in name else "PLAIN_AXES"
        e.empty_display_size = 1.5
        e.rotation_euler = (0.0, math.pi / 2, 0.0) if "Muzzle" in name else (0.0, 0.0, 0.0)  # the arrow along +X
        sc.collection.objects.link(e)
        e.parent = body
        e.location = pt - HAND
    objs = [body, flare]

    # UVs: one atlas for both (one skin), every corner on a texel's centre (as the MDL stores it).
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(50), island_margin=0.01, area_weight=0.0, correct_aspect=True,
                             scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    for ob in objs:
        for d in ob.data.uv_layers.active.data:
            s = min(SKIN_W - 1, max(0, math.floor(d.uv.x * SKIN_W)))
            t = min(SKIN_H - 1, max(0, math.floor((1.0 - d.uv.y) * SKIN_H)))
            d.uv = ((s + 0.5) / SKIN_W, 1.0 - (t + 0.5) / SKIN_H)

    bpy.context.view_layer.update()
    occluders = {body.name: [body], flare.name: [flare]}
    skins = bake(objs, occluders, SKIN_W, SKIN_H)
    img = save_image("flaregun_skin", skins[0])
    save_image("flaregun_skin_fired1", skins[1], skins[0])
    save_image("flaregun_skin_fired2", skins[2], skins[0])

    mat = bpy.data.materials.new("FlareGunSkin")
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

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "flaregun.blend"), relative_remap=True)
    export_mdl.export()


if __name__ == "__main__":
    main()
