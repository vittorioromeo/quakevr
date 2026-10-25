# build_widowmaker.py -- makes widowmaker.blend from nothing: the Widowmaker's shapes (AD's upgraded super shotgun as a
# Quake VR two-handed Gatling auto shotgun, after Vittorio's mockup: qvr-kit/scratch/widowmaker/mockup.png), their UVs,
# and a skin painted procedurally in Quake's palette (dark rusty iron, a lighter steel side panel, an olive-brass top,
# brass collars, gunmetal barrels, brick-brown grips), then exports the MDLs (export_mdl.py). README.md says how to edit
# it by hand instead.
#
#   blender -b --factory-startup -P Misc/quakevr/models_src/widowmaker/build_widowmaker.py
#
# It OVERWRITES widowmaker.blend and the skin: once they have been edited by hand, edit those and run export_mdl.py, not
# this.
#
# Units are Quake model units in the frame of the Widowmaker made from AD's view model (Quake/vr/vr_adweapons.cpp
# widowmakerParts: the super shotgun's frame, +X forward, +Y left, +Z up), so weapon slot 34's settings, anchors and
# hotspots hold it: the rear grip where the super shotgun's grip is (the hand anchor 5.10 -0.05 0.71 on its front), the
# front grip where the made one's front handle is (the other hand's anchor 29.08 -0.04 -0.18 on its front), the shell port
# on the receiver's inner side (+y; its middle 6.5 3.6 5.6: vr_view_internal.hpp loadPorts), the ejection slot on the
# outer side (vr_shells.cpp), the muzzle anchor 44 0 8.8 on the spin axis, the ammo screen's anchor on the receiver's top.
# Everything stays past the super shotgun's corner (export_mdl.py CORNER: the settings scale about it).
#
# The two barrels (the mockup's twin barrels) and their collars and muzzle rings are the object Barrels: they turn about
# the spin axis (y 0, z SPIN_Z) in the spin frames (a shot each half turn). The still bearing ring round them carries the
# front grip and the front sight.

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

# Parts (a face's material index while building, then its wm_part attribute): what the skin paints there.
IRON, STEEL, OLIVE, BRASS, BARREL, BRICK, BORE, DARK, SIGHT, PLATE = range(10)
CHAMFER = 16  # added to a part: a chamfer of it

# The receiver: an octagonal box (chamfered), the rear grip under its back end.
RX0, RX1 = 0.4, 21.0
RW, RZ0, RZ1 = 3.6, 3.0, 11.4
R_CHAMFER_TOP, R_CHAMFER_BOT = 1.5, 0.9
RECEIVER = [(-RW, RZ0 + R_CHAMFER_BOT), (-RW + R_CHAMFER_BOT, RZ0), (RW - R_CHAMFER_BOT, RZ0), (RW, RZ0 + R_CHAMFER_BOT),
            (RW, RZ1 - R_CHAMFER_TOP), (RW - R_CHAMFER_TOP, RZ1), (-RW + R_CHAMFER_TOP, RZ1), (-RW, RZ1 - R_CHAMFER_TOP)]
# The barrels: side by side about the spin axis.
SPIN_Z = export_mdl.SPIN_Z
BARREL_Y, BARREL_R, BORE_R, BARREL_SIDES = 1.72, 1.55, 0.95, 12
BARREL_X0, MUZZLE_X, BORE_DEPTH = 20.8, 44.0, 2.4
COLLAR_R = 1.98            # the collars' corners (octagons: flats 1.83 out, the two barrels' collars meeting)
COLLARS = [(22.0, 23.5), (34.2, 35.7)]
GROOVES = [40.1, 41.25, 42.4]  # the raised rings near the muzzle (each 0.6 long)
# The still bearing ring round both barrels (their sweep 3.4 from the axis; the collars pass ahead and behind it), the front grip under it.
RING_X0, RING_X1, RING_APOTHEM = 27.1, 30.6, 3.45
# The sight line (y 0): the rear sight's notch on the receiver's top, the front post on the ring: parallel to the bores.
SIGHT_Z = 12.45
SIGHT_REAR_X, SIGHT_FRONT_X = 1.7, 28.8

# The grips: square posts leaning back as the super shotgun's grip (vr_adweapons.cpp grafts it), along GRIP_UP.
GRIP_UP = Vector((0.18, 0.0, 1.0)).normalized()
GRIP_FWD = Vector((1.0, 0.0, -0.18)).normalized()
REAR_GRIP = (Vector((3.6, 0.0, 0.5)), 1.5, 1.3, RZ0 + 0.45, -3.05)   # a point of its axis, half depth, half width, top z, bottom z
FRONT_GRIP = (Vector((27.76, 0.0, -0.36)), 1.32, 1.15, SPIN_Z - RING_APOTHEM + 0.45, -3.15)
FOOT_H = 0.6

XZ = (Vector((1, 0, 0)), Vector((0, 0, 1)))   # side profiles (x, z), extruded across y
YZ = (Vector((0, 1, 0)), Vector((0, 0, 1)))   # sections across x (a = x)
GRIP_FRAME = (GRIP_FWD, Vector((0, 1, 0)))     # sections across the grips' axis (a along GRIP_UP)


def ccw(poly):
    n = len(poly)
    area = sum(poly[i][0] * poly[(i + 1) % n][1] - poly[(i + 1) % n][0] * poly[i][1] for i in range(n))
    return poly if area > 0 else poly[::-1]


def prism(bm, poly, a0, a1, frame, part, origin=Vector()):
    """poly (2D, in the frame's U V) from a0 to a1 along U x V, closed. Returns [cap0, cap1, sides...]."""
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
    return prism(bm, poly, -y1, -y0, XZ, part)


def section(bm, poly, x0, x1, part):
    return prism(bm, poly, x0, x1, YZ, part)


def bevel_caps(bm, faces, offset):
    """Chamfers the outline of a prism's two end faces (faces[0], faces[1])."""
    edges = list({e for f in faces[:2] for e in f.edges})
    verts = list({v for e in edges for v in e.verts})
    part = faces[0].material_index % CHAMFER
    ret = bmesh.ops.bevel(bm, geom=edges + verts, offset=offset, offset_type="OFFSET", segments=1, profile=0.5,
                          affect="EDGES", clamp_overlap=True)
    for f in ret["faces"]:
        f.material_index = part + CHAMFER
    return ret["faces"]


def rect(x0, x1, z0, z1):
    return [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]


def box(bm, lo, hi, part, bevel=0.0):
    faces = section(bm, [(lo.y, lo.z), (hi.y, lo.z), (hi.y, hi.z), (lo.y, hi.z)], lo.x, hi.x, part)
    if bevel > 0.0:
        bevel_caps(bm, faces, bevel)
    return faces


def lathe(bm, rings, sides, centre, parts, rot=0.0, caps=None):
    """Rings [(x, r)] around the x axis through centre, band k painted parts[k]; capped at both ends."""
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


def octagon(apothem, y=0.0, z=0.0):
    r = apothem / math.cos(math.pi / 8)
    return [(y + r * math.cos(math.pi / 8 + i * math.pi / 4), z + r * math.sin(math.pi / 8 + i * math.pi / 4))
            for i in range(8)]


def finish(bm, name):
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(0.5), use_dissolve_boundaries=False, verts=bm.verts[:],
                             edges=bm.edges[:], delimit={"MATERIAL"})
    bmesh.ops.dissolve_degenerate(bm, dist=1e-4, edges=bm.edges[:])
    layer = bm.faces.layers.int.new("wm_part")
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
    return ob


def by_normal(faces, centre_z=None):
    """The receiver's faces painted by where they face: the sides steel, the top and upper chamfers olive, the bottom
    and lower chamfers iron (a cap keeps its part)."""
    for f in faces:
        n = f.normal
        if f.material_index >= CHAMFER:
            f.material_index = OLIVE + CHAMFER  # the ends' chamfered frames
            continue
        if abs(n.x) > 0.9:
            continue
        ch = False
        if abs(n.y) > 0.95:
            part = STEEL
        elif n.z > 0.3:
            part = OLIVE
        else:
            part = IRON
        f.material_index = part + (CHAMFER if ch else 0)


def grip_post(bm, spec, part=BRICK):
    """A square post (its corners cut) along GRIP_UP through spec's axis point, from spec's top z down to its bottom z,
    a riveted iron foot plate under it."""
    p0, hd, hw, ztop, zbot = spec
    a_top = (ztop - p0.z) / GRIP_UP.z
    a_bot = (zbot - p0.z) / GRIP_UP.z
    c = 0.32
    poly = [(-hd + c, -hw), (hd - c, -hw), (hd, -hw + c), (hd, hw - c), (hd - c, hw), (-hd + c, hw), (-hd, hw - c),
            (-hd, -hw + c)]
    prism(bm, poly, a_bot, a_top, GRIP_FRAME, part, p0)
    fd, fw = hd + 0.38, hw + 0.38
    foot = [(-fd, -fw), (fd, -fw), (fd, fw), (-fd, fw)]
    faces = prism(bm, foot, a_bot - FOOT_H / GRIP_UP.z, a_bot + 0.02, GRIP_FRAME, PLATE, p0)
    bevel_caps(bm, faces, 0.14)


def tetra(bm, c, s, part):
    """A tiny tetrahedron (the muzzle anchor's mark)."""
    vs = [bm.verts.new(c + Vector(d) * s) for d in ((1, 1, 1), (1, -1, -1), (-1, 1, -1), (-1, -1, 1))]
    for tri in ((0, 1, 2), (0, 3, 1), (0, 2, 3), (1, 3, 2)):
        bm.faces.new([vs[i] for i in tri]).material_index = part


def build_body():
    bm = bmesh.new()
    # The receiver: its back and front chamfered (the rear plate's frame), its faces painted by where they face.
    faces = section(bm, RECEIVER, RX0, RX1, OLIVE)
    bevel_caps(bm, faces, 0.5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.normal_update()
    by_normal([f for f in bm.faces])
    for f in bm.faces:
        if f.normal.x < -0.9:
            f.material_index = IRON  # the back
    # The rear plate: an octagon standing proud of the back, riveted (painted).
    plate = [(y * 0.8, (z - 7.15) * 0.8 + 7.15) for y, z in RECEIVER]
    pf = section(bm, plate, RX0 - 0.28, RX0 + 0.05, PLATE)
    bevel_caps(bm, pf, 0.1)
    # The top: a raised strip along its middle; the rear sight (two ears, a notch to SIGHT_Z) at its back.
    bevel_caps(bm, section(bm, [(-0.55, RZ1 - 0.05), (0.55, RZ1 - 0.05), (0.55, RZ1 + 0.34), (-0.55, RZ1 + 0.34)],
                           3.4, 18.6, OLIVE), 0.1)
    for s in (1.0, -1.0):
        ear = [(0.2 * s, RZ1 - 0.05), (0.9 * s, RZ1 - 0.05), (0.9 * s, SIGHT_Z + 0.32), (0.2 * s, SIGHT_Z + 0.32)]
        bevel_caps(bm, section(bm, ear, SIGHT_REAR_X - 0.6, SIGHT_REAR_X + 0.6, SIGHT), 0.07)
    section(bm, [(-0.2, RZ1 - 0.05), (0.2, RZ1 - 0.05), (0.2, SIGHT_Z), (-0.2, SIGHT_Z)], SIGHT_REAR_X - 0.6,
            SIGHT_REAR_X + 0.6, SIGHT)
    # The shell port (+y: the inner side): a dark opening (x 4.55..8.45, z 5.05..6.15) in a recessed plate framed by
    # iron bars, as the mockup's slot.
    y = RW
    box(bm, Vector((4.55, y - 0.3, 5.05)), Vector((8.45, y + 0.03, 6.15)), DARK)
    for lo, hi in (((4.35, 6.15), (8.65, 6.35)), ((4.35, 4.85), (8.65, 5.05)), ((4.35, 5.05), (4.55, 6.15)),
                   ((8.45, 5.05), (8.65, 6.15))):
        box(bm, Vector((lo[0], y - 0.1, lo[1])), Vector((hi[0], y + 0.14, hi[1])), IRON)
    for lo, hi in (((3.3, 6.95), (9.7, 7.35)), ((3.3, 3.95), (9.7, 4.35)), ((3.3, 4.35), (3.7, 6.95)),
                   ((9.3, 4.35), (9.7, 6.95))):
        box(bm, Vector((lo[0], y - 0.1, lo[1])), Vector((hi[0], y + 0.2, hi[1])), STEEL, 0.06)
    # The ejection slot (-y: the outer side), where its spent shells leave (vr_shells.cpp: 7.25 -3.9 7.7).
    box(bm, Vector((5.0, -y - 0.03, 7.0)), Vector((9.5, -y + 0.3, 8.4)), DARK)
    for lo, hi in (((4.8, 8.4), (9.7, 8.6)), ((4.8, 6.8), (9.7, 7.0)), ((4.8, 7.0), (5.0, 8.4)), ((9.5, 7.0), (9.7, 8.4))):
        box(bm, Vector((lo[0], -y - 0.16, lo[1])), Vector((hi[0], -y + 0.1, hi[1])), IRON)
    # The bearing ring round the turning barrels (still): the front grip under it, the front sight on it.
    lathe(bm, [(RING_X0, RING_APOTHEM / math.cos(math.pi / 8) - 0.25), (RING_X0 + 0.25, RING_APOTHEM / math.cos(math.pi / 8)),
               (RING_X1 - 0.25, RING_APOTHEM / math.cos(math.pi / 8)), (RING_X1, RING_APOTHEM / math.cos(math.pi / 8) - 0.25)],
          8, Vector((0, 0, SPIN_Z)), [BRASS + CHAMFER, BRASS, BRASS + CHAMFER], rot=math.pi / 8)
    top = SPIN_Z + RING_APOTHEM
    side(bm, [(SIGHT_FRONT_X - 0.9, top - 0.05), (SIGHT_FRONT_X + 0.25, top - 0.05), (SIGHT_FRONT_X + 0.25, SIGHT_Z),
              (SIGHT_FRONT_X - 0.15, SIGHT_Z), (SIGHT_FRONT_X - 0.9, top + 0.12)], -0.16, 0.16, SIGHT)
    # The grips: brick-brown posts on iron feet.
    grip_post(bm, REAR_GRIP)
    grip_post(bm, FRONT_GRIP)
    # The trigger and its guard, in front of the rear grip (where the super shotgun's are).
    side(bm, [(6.2, RZ0 + 0.05), (6.95, RZ0 + 0.05), (6.75, 2.3), (6.85, 1.85), (7.15, 1.6), (6.75, 1.5), (6.4, 1.9),
              (6.3, 2.5)], -0.22, 0.22, IRON)
    bevel_caps(bm, side(bm, [(8.7, RZ0 + 0.05), (9.15, RZ0 + 0.05), (9.15, 1.05), (8.8, 0.7), (4.9, 0.7), (4.9, 1.1),
                             (8.7, 1.1)], -0.3, 0.3, IRON), 0.06)
    # The muzzle anchor's mark: a speck on the spin axis in the muzzle's plane, between the barrels (still).
    tetra(bm, Vector((MUZZLE_X, 0.0, SPIN_Z)), 0.06, BORE)
    return finish(bm, "Body")


def build_barrels():
    bm = bmesh.new()
    rot = math.pi / BARREL_SIDES
    for y in (BARREL_Y, -BARREL_Y):
        c = Vector((0.0, y, SPIN_Z))
        lathe(bm, [(BARREL_X0, BARREL_R), (MUZZLE_X - 0.6, BARREL_R), (MUZZLE_X - 0.6, BARREL_R + 0.12),
                   (MUZZLE_X - 0.12, BARREL_R + 0.12), (MUZZLE_X, BARREL_R - 0.02), (MUZZLE_X, BORE_R),
                   (MUZZLE_X - BORE_DEPTH, BORE_R)], BARREL_SIDES, c,
              [BARREL, BARREL + CHAMFER, BARREL, BARREL + CHAMFER, BARREL + CHAMFER, BORE], rot=rot,
              caps=(BARREL, BORE))
        for x0, x1 in COLLARS:
            lathe(bm, [(x0, COLLAR_R - 0.2), (x0 + 0.2, COLLAR_R), (x1 - 0.2, COLLAR_R), (x1, COLLAR_R - 0.2)], 8, c,
                  [BRASS + CHAMFER, BRASS, BRASS + CHAMFER], rot=math.pi / 8)
        for x in GROOVES:
            lathe(bm, [(x, BARREL_R + 0.02), (x + 0.12, BARREL_R + 0.22), (x + 0.48, BARREL_R + 0.22),
                       (x + 0.6, BARREL_R + 0.02)], BARREL_SIDES, c,
                  [BARREL + CHAMFER, IRON, BARREL + CHAMFER], rot=rot)
    return finish(bm, "Barrels")


# ----------------------------------------------------------------------------------------------------------------------
# The skin

def rgb(*c):
    return Vector(c)


# (Quake's palette: the neutral greys 0..15, the warm greys 160..175, the browns 16..31 and 112..127, the grey-greens
# 176..191, the rust reds 96..111.)
IRON_C, IRON_DARK, IRON_RUST, IRON_STAIN = rgb(80, 66, 54), rgb(42, 34, 28), rgb(104, 64, 40), rgb(118, 50, 34)
STEEL_C, STEEL_LIGHT, STEEL_DARK = rgb(140, 138, 134), rgb(214, 212, 206), rgb(92, 90, 86)
OLIVE_C, OLIVE_GREEN, OLIVE_RUST, OLIVE_DARK = rgb(104, 90, 64), rgb(71, 88, 76), rgb(104, 62, 42), rgb(58, 50, 38)
OLIVE_YELLOW = rgb(76, 76, 12)
BRASS_C, BRASS_DARK, BRASS_LIGHT = rgb(100, 78, 34), rgb(66, 50, 28), rgb(140, 108, 40)
BARREL_C, BARREL_LIGHT, BARREL_DARK = rgb(78, 74, 70), rgb(112, 108, 102), rgb(40, 38, 36)
BARREL_BROWN = rgb(92, 76, 60)
BRICK_C, BRICK_LIGHT, MORTAR = rgb(100, 64, 46), rgb(130, 86, 62), rgb(46, 34, 24)
BORE_C, DARK_C = rgb(22, 20, 18), rgb(30, 26, 22)
EDGE_IRON = rgb(150, 142, 128)


def rivet_ring(poly, scale, x, nx, r, count_per_edge=2):
    """Rivets along an octagonal outline (y, z) scaled about its middle, on a face of normal (nx, 0, 0) at x."""
    cy, cz = 0.0, 7.15
    pts = [(cy + (y - cy) * scale, cz + (z - cz) * scale) for y, z in poly]
    out = []
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        for k in range(count_per_edge):
            t = k / count_per_edge
            out.append((Vector((x, a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)), Vector((nx, 0, 0)), r))
    return out


def rivets():
    out = []
    # The rear plate: a ring of rivets near its edge.
    plate = [(y * 0.8, (z - 7.15) * 0.8 + 7.15) for y, z in RECEIVER]
    out += rivet_ring(plate, 0.84, RX0 - 0.28, -1.0, 0.3, 2)
    # The receiver's sides: a rivet at each corner of the side panel, and along its top edge.
    for s in (1.0, -1.0):
        for x in (1.3, 11.0, 20.0):
            for z in (4.35, RZ1 - R_CHAMFER_TOP - 0.45):
                if s > 0 and 3.0 < x < 10.0 and z < 8.0:
                    continue
                out.append((Vector((x, s * RW, z)), Vector((0, s, 0)), 0.27))
    # The top: rivets at its corners and along it beside the strip.
    for x in (1.4, 6.0, 11.0, 16.0, 20.1):
        for s in (1.0, -1.0):
            if x < 3.0:
                continue
            out.append((Vector((x, s * 1.55, RZ1)), Vector((0, 0, 1)), 0.25))
    out.append((Vector((17.6, 0.0, RZ1 + 0.34)), Vector((0, 0, 1)), 0.2))  # the strip's bolt
    # The bearing ring: on its side flats and its top.
    for s in (1.0, -1.0):
        out.append((Vector(((RING_X0 + RING_X1) / 2, s * RING_APOTHEM, SPIN_Z)), Vector((0, s, 0)), 0.22))
    # The foot plates: their sides.
    for p0, hd, hw, ztop, zbot in (REAR_GRIP, FRONT_GRIP):
        a = (zbot - p0.z) / GRIP_UP.z - FOOT_H / GRIP_UP.z * 0.5
        c = p0 + GRIP_UP * a
        for s in (1.0, -1.0):
            for u in (-hd + 0.1, hd - 0.1):
                out.append((c + GRIP_FWD * u + Vector((0, s * (hw + 0.38), 0)), Vector((0, s, 0)), 0.13))
            out.append((c + GRIP_FWD * (s * (hd + 0.38)), GRIP_FWD * s, 0.13))
    return out


def hemisphere(n):
    rng = random.Random(7)
    out = []
    for i in range(n):
        u1, u2 = (i + rng.random()) / n, rng.random()
        r, phi = math.sqrt(u1), 2 * math.pi * u2
        out.append((r * math.cos(phi), r * math.sin(phi), math.sqrt(max(0.0, 1.0 - u1))))
    return out


AO_DIRS = hemisphere(24)
AO_DIST = 2.2


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


def brick(p, n):
    """The grips' bricks: (is mortar, the brick's own shade) at p on a post face (rows along GRIP_UP)."""
    a = p.dot(GRIP_UP)
    if abs(n.y) > 0.7:
        u = p.dot(GRIP_FWD)
    else:
        u = p.y + (10.0 if n.dot(GRIP_FWD) > 0 else 20.0)
    row_h, brick_w = 0.78, 1.45
    row = math.floor(a / row_h)
    uu = u + (0.5 * brick_w if row % 2 else 0.0)
    col = math.floor(uu / brick_w)
    fa, fu = a / row_h - row, uu / brick_w - col
    mortar = fa < 0.24 or fu < 0.13
    shade = noise.noise(Vector((row * 1.7, col * 2.3, 0.5)))
    return mortar, shade


def paint_texel(p, n, part, chamfer, edges, rvts, ao, rnd):
    big = noise.noise(p * 0.35)
    mid = noise.noise(p * 1.1 + Vector((3.1, 7.7, 1.3)))
    fine = noise.noise(p * 3.2 + Vector((9.2, 1.4, 5.5)))
    speck = (rnd[0] - 0.5) * (0.16 if part == PLATE else 0.34)
    if part == PLATE:
        # The rear plate: dark rusty iron, red-brown stains in swirls (the mockup's).
        col = mix(IRON_DARK, IRON_C, 0.5 + 0.5 * mid + 0.2 * fine)
        swirl = noise.noise(Vector((0.0, p.y * 0.9, p.z * 0.9)) + Vector((1.0, 5.0, 3.0)))
        col = mix(col, IRON_STAIN, 0.7 * smooth(0.04, 0.0, abs(swirl) - 0.05) + 0.25 * smooth(0.2, 0.5, big))
    elif part == IRON:
        col = mix(IRON_DARK, IRON_C, 0.55 + 0.7 * mid + 0.3 * fine)
        col = mix(col, IRON_RUST, 0.75 * smooth(0.15, 0.45, 0.8 * big + 0.5 * fine))
    elif part == STEEL:
        # The light steel side panel: big bright and dull patches (the mockup's), darker olive bars across it.
        blot = noise.noise(Vector((p.x * 0.42, p.y, p.z * 0.55)) + Vector((4.0, 2.0, 1.0)))
        col = mix(STEEL_DARK, STEEL_LIGHT, 0.5 + 1.1 * blot + 0.25 * fine)
        bar = min(abs(p.x - x) for x in (2.6, 11.4, 16.0, 19.6))
        if bar < 0.45 and RZ0 + 1.2 < p.z < RZ1 - R_CHAMFER_TOP - 0.7:
            col = mix(OLIVE_DARK, OLIVE_C, 0.4 + 0.5 * fine)
        col = mix(col, IRON_RUST, 0.6 * smooth(0.35, 0.6, big + 0.4 * fine))
    elif part == OLIVE:
        col = mix(OLIVE_DARK, OLIVE_C, 0.6 + 0.6 * mid + 0.3 * fine)
        if rnd[1] < 0.18:
            col = mix(col, OLIVE_GREEN, 0.8)
        elif rnd[1] < 0.24:
            col = mix(col, OLIVE_YELLOW, 0.6)
        elif rnd[1] < 0.38:
            col = mix(col, OLIVE_RUST, 0.7)
        col = mix(col, OLIVE_RUST, 0.45 * smooth(0.3, 0.6, big + 0.3 * fine))
    elif part == BRASS:
        col = mix(BRASS_DARK, BRASS_C, 0.6 + 0.6 * mid + 0.3 * fine)
        col = mix(col, OLIVE_DARK, 0.5 * smooth(0.25, 0.55, big + 0.4 * fine))
    elif part == BARREL:
        col = mix(BARREL_DARK, BARREL_LIGHT, 0.45 + 0.7 * mid + 0.35 * fine)
        if rnd[1] < 0.3:
            col = mix(col, BARREL_BROWN, 0.7)
        elif rnd[1] < 0.38:
            col = mix(col, IRON_RUST, 0.5)
    elif part == BRICK:
        mortar, shade = brick(p, n)
        col = MORTAR * (0.9 + 0.2 * fine) if mortar else mix(BRICK_C, BRICK_LIGHT, 0.4 + 0.8 * shade + 0.3 * fine)
    elif part == SIGHT:
        col = mix(IRON_DARK, IRON_C, 0.6 + 0.5 * mid)
        if (n.z > 0.5 or n.x < -0.5) and p.z > SIGHT_Z - 0.25:
            col = BRASS_LIGHT * (0.92 + 0.12 * rnd[0])  # the ears' and the post's tops catch the eye
    elif part == BORE:
        col = BORE_C * (0.9 + 0.2 * fine)
    else:
        col = DARK_C * (0.9 + 0.2 * fine)
    if part not in (BORE, DARK):
        col *= 1.0 + speck
    # Worn, brighter convex edges; darker concave ones.
    wear = smooth(-0.35, 0.35, noise.noise(p * 1.7 + Vector((5.0, 2.0, 8.0))))
    edge_col = {BRASS: BRASS_LIGHT, OLIVE: rgb(124, 110, 70), STEEL: STEEL_LIGHT,
                BRICK: BRICK_LIGHT}.get(part) or col * 1.05 + EDGE_IRON * 0.4
    if chamfer:
        col = mix(col, edge_col, 0.15 + 0.3 * wear)
    for a, b, convex in edges:
        dist = seg_dist(p, a, b)
        if convex and dist < 0.12 and not chamfer and part not in (BORE, DARK):
            col = mix(col, edge_col, (0.3 + 0.45 * wear) * (1.0 - dist / 0.12) + 0.12)
        elif not convex and dist < 0.22:
            col *= 0.55 + 0.45 * dist / 0.22
    # Painted rivets: domes lit from above and behind (the view's side), a shadow below.
    for c, rn, r in rvts:
        if n.dot(rn) < 0.95:
            continue
        v = p - c
        if abs(v.dot(rn)) > 0.12:
            continue
        v -= rn * v.dot(rn)
        k = v.length / r
        if k < 1.0:
            dome = (rn * math.sqrt(1.0 - k * k) + v / r).normalized()
            light = (rn * 0.6 + Vector((-0.3, 0.2, 0.75))).normalized()
            col = mix(rgb(52, 42, 34), rgb(200, 180, 150), max(0.0, dome.dot(light)) ** 1.4)
        elif k < 1.45 and v.z < 0.0:
            col *= 0.55
    col *= ao
    col *= (0.92 + 0.12 * n.z) * 0.9
    return col


def bake(objs, w, h):
    """Paints the skin (every object's UV islands) procedurally; returns its RGB texels (top row first)."""
    tex = [None] * (w * h)
    rvts = rivets()
    rng = random.Random(1)
    verts, polys = [], []
    for oc in objs:
        m = oc.matrix_world
        base = len(verts)
        verts += [m @ v.co for v in oc.data.vertices]
        polys += [[base + i for i in p.vertices] for p in oc.data.polygons]
    bvh = BVHTree.FromPolygons(verts, polys)
    for ob in objs:
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bm.transform(ob.matrix_world)
        bm.normal_update()
        uv = bm.loops.layers.uv.active
        part_layer = bm.faces.layers.int.get("wm_part")
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
                    tex[t * w + s] = paint_texel(p, n, part, chamfer, face_edges[f.index], rvts, 0.35 + 0.65 * ao, rnd)
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
    return [tuple(max(0, min(255, int(round(c[k])))) for k in range(3)) if c is not None else (40, 34, 28) for c in tex]


def save_image(name, texels):
    q = export_mdl.qpal.Quantizer()
    pal = export_mdl.qpal.PALETTE
    img = bpy.data.images.new(name, SKIN_W, SKIN_H, alpha=False)
    px = [0.0] * (SKIN_W * SKIN_H * 4)
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            c = pal[q.nearest(texels[t * SKIN_W + s], False)]
            i = ((SKIN_H - 1 - t) * SKIN_W + s) * 4
            px[i:i + 4] = [c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, 1.0]
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
    barrels = build_barrels()
    body["preview_scale"] = 0.48
    body["note"] = ("Quake axes: +X forward (muzzles), +Y left, +Z up. Origin: the model's (the super shotgun's frame, "
                    "weapon slot 34's settings). Barrels turns about y 0, z %.1f in the spin frames." % SPIN_Z)
    barrels["note"] = "Turns about the spin axis (y 0, z %.1f, along x) in the spin frames: keep it centred there." % SPIN_Z
    # Markers (not exported: names starting with _): the muzzle, the sight line, the anchors and the ports.
    marks = (("_Muzzle", (MUZZLE_X, 0, SPIN_Z)), ("_SightRear", (SIGHT_REAR_X, 0, SIGHT_Z)),
             ("_SightFront", (SIGHT_FRONT_X, 0, SIGHT_Z)), ("_HandAnchor", (5.0979, -0.0482, 0.7118)),
             ("_TwoHHandAnchor", (29.08, -0.04, -0.18)), ("_ShellPort", (6.5, 3.6, 5.6)),
             ("_EjectSlot", (7.25, -3.9, 7.7)))
    for name, pt in marks:
        e = bpy.data.objects.new(name, None)
        e.empty_display_type = "SINGLE_ARROW" if "Muzzle" in name else "PLAIN_AXES"
        e.empty_display_size = 1.5
        e.rotation_euler = (0.0, math.pi / 2, 0.0) if "Muzzle" in name else (0.0, 0.0, 0.0)
        sc.collection.objects.link(e)
        e.parent = body
        e.location = Vector(pt)
    objs = [body, barrels]

    # UVs: one atlas for both, every corner on a texel's centre (as the MDL stores it).
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(50), island_margin=0.006, area_weight=0.0, correct_aspect=True,
                             scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    for ob in objs:
        for d in ob.data.uv_layers.active.data:
            s = min(SKIN_W - 1, max(0, math.floor(d.uv.x * SKIN_W)))
            t = min(SKIN_H - 1, max(0, math.floor((1.0 - d.uv.y) * SKIN_H)))
            d.uv = ((s + 0.5) / SKIN_W, 1.0 - (t + 0.5) / SKIN_H)

    bpy.context.view_layer.update()
    img = save_image("widowmaker_skin", bake(objs, SKIN_W, SKIN_H))

    mat = bpy.data.materials.new("WidowmakerSkin")
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

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "widowmaker.blend"), relative_remap=True)
    export_mdl.export()


if __name__ == "__main__":
    main()
