# build_revolver.py -- makes revolver.blend from nothing: the heavy revolver's shapes (Body, Crane, Cylinder,
# EjectorRod, Hammer, Trigger, Shell), their UVs, and a first skin painted procedurally in Quake's palette (dark iron
# with carved runes, a riveted brass frame plate, a fluted steel cylinder, a red-brown wooden grip wrapped in worn
# leather), then exports the MDLs (export_mdl.py). README.md says how to edit it by hand instead.
#
#   blender -b --factory-startup -P Misc/quakevr/models_src/revolver/build_revolver.py
#
# It OVERWRITES revolver.blend and the skins: once the .blend has been edited by hand, edit that and run export_mdl.py,
# not this.
#
# Units are Quake model units, as Quake's view weapons (v_nail.mdl, v_shot.mdl): +X forward (the muzzle), +Y left, +Z
# up. The revolver is ~16 units long: drawn at a view weapon's usual scale (vr_wofs_scale ~0.45) that is ~28 cm.
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

SKIN_W, SKIN_H = 384, 384
SHELL_W, SHELL_H = 64, 64

# Parts (a face's material index while building, then its rv_part attribute): what the skin paints there.
(IRON, DARK, BRASS, BORE, WOOD, LEATHER, SIGHT, STEEL, CAP_REAR, CAP_FRONT, HAMMER, HULL, HEAD, CRIMP,
 RIM_BACK) = range(15)
CHAMFER = 16   # added to a part: a chamfer of it
HIDDEN = 40    # a face nobody sees (inside another part): deleted

# The bore and the cylinder.
BORE_Z = 5.0
PITCH = 1.45                                   # the chambers' circle (one chamber's centre on the bore's line)
CYL_AXIS_Z = BORE_Z - PITCH                    # 3.55
CYL_R = 2.25
CYL_X0, CYL_X1 = 0.0, 4.4                      # its rear and front faces
CHAMBER_R = 0.6                                # a chamber's mouth (a 12-gauge hull, 0.59)
CYL_ORIGIN = Vector((CYL_X0, 0.0, CYL_AXIS_Z))  # the Cylinder's origin: its axis at its rear face; spins about X
# The crane (yoke): it swings out to the left (+Y) about an axis parallel to the barrel, low and left of the cylinder.
CRANE_PIVOT = Vector((4.45, 0.7, 0.5))         # (its knuckle in a slot through the frame's bottom)
CRANE_SLOT = (4.38, 4.92)
CRANE_SWING = -95.0                            # degrees about its local X: out to the left
ROD_ORIGIN = Vector((CYL_X1, 0.0, CYL_AXIS_Z))
# The frame's window (the cylinder sits in it) and the brass plate around it: octagons in the side profile (x, z).
WIN = (-0.35, 5.45, 1.15, 6.0)                 # x0, x1, z0, z1
OUT = (-1.15, 6.25, 0.4, 6.75)
CORE_HY, BAND_HY = 0.85, 1.12                  # the iron frame's half width; the brass plates' outer face
THICK_HY = 1.72                                # the plates' outer face round the window's top, front and back
THICK_SEGS = {4, 5, 6, 7, 8}                   # (the slotted octagon's segments: front, top diagonals, top, back)
# The barrel: hexagonal (vertices at the top and the bottom: flat sides, where the runes are).
BARREL_R = 1.55
BORE_R = 0.66
BARREL_X0, BARREL_X1 = 5.95, 12.4
# Iron sights: one line parallel to the bore, through the front blade's top and the rear notch's top edges.
SIGHT_Z = 7.12
FRONT_SIGHT = Vector((12.15, 0.0, SIGHT_Z))    # the blade's top front corner
REAR_NOTCH = Vector((0.0, 0.0, SIGHT_Z))       # the notch's top, at its back face (on the top strap)
NOTCH_BOTTOM = 6.84
# The hammer and the trigger: pivots about Y.
HAMMER_PIVOT = Vector((-1.6, 0.0, 3.6))
HAMMER_COCK = -38.0                            # degrees about its local Y: its spur back and down
TRIGGER_PIVOT = Vector((1.35, 0.0, 0.75))
TRIGGER_PULL = 22.0                            # degrees about its local Y: its blade back
# The grip.
GRIP_TILT = math.radians(31.0)
GRIP_TOP = Vector((-1.65, 0.0, 0.6))
GRIP_FWD = Vector((math.cos(GRIP_TILT), 0.0, -math.sin(GRIP_TILT)))
GRIP_UP = Vector((math.sin(GRIP_TILT), 0.0, math.cos(GRIP_TILT)))
GRIP_A = (1.0, -3.8, -8.4)                     # its sections along GRIP_UP from GRIP_TOP: top (in the frame), mid, butt
GRIP_SEC = ((1.45, 1.05, 0.0), (1.45, 1.1, 0.15), (2.2, 1.22, -0.6))  # (half along GRIP_FWD, half across, shift)
STRAPS = ((-3.3, 0.95, 15.0, 1.07), (-4.35, 0.9, -13.0, 1.09), (-5.4, 0.95, 17.0, 1.07))
HAND = GRIP_TOP - GRIP_UP * 3.4                # the hand point: the grip's middle


def grip_section(a):
    """The grip's section (half along GRIP_FWD, half across, shift along GRIP_FWD) at a along GRIP_UP."""
    k = 0 if a >= GRIP_A[1] else 1
    t = (a - GRIP_A[k]) / (GRIP_A[k + 1] - GRIP_A[k])
    return tuple(GRIP_SEC[k][i] + (GRIP_SEC[k + 1][i] - GRIP_SEC[k][i]) * t for i in range(3))
# A 12-gauge shell in these units (1 unit = 0.45 * 3.81 cm drawn in the hand): 7 cm long, its rim 2.2 cm across.
SHELL_LEN, SHELL_RIM_R, SHELL_HULL_R, SHELL_RIM_T, SHELL_HEAD_LEN = 4.1, 0.66, 0.585, 0.13, 0.95
SHELL_PROUD = 0.1                              # a chambered shell's rim stands this far behind the cylinder's face

XZ = (Vector((1, 0, 0)), Vector((0, 0, 1)))   # side profiles (x, z), extruded across y (a = -y)
YZ = (Vector((0, 1, 0)), Vector((0, 0, 1)))   # sections across x (a = x)


def ccw(poly):
    n = len(poly)
    area = sum(poly[i][0] * poly[(i + 1) % n][1] - poly[(i + 1) % n][0] * poly[i][1] for i in range(n))
    return poly if area > 0 else poly[::-1]


def prism(bm, poly, a0, a1, frame, part, origin=Vector(), poly1=None, shift1=Vector(), caps=(True, True)):
    """poly (2D, in the frame's U V) from a0 to a1 along U x V; poly1 (same count) at a1 if given, moved by shift1."""
    U, V = frame
    A = U.cross(V)
    if ccw(poly) is not poly:
        poly = poly[::-1]
        poly1 = poly1[::-1] if poly1 else None
    poly1 = poly1 or poly
    v0 = [bm.verts.new(origin + U * u + V * v + A * a0) for u, v in poly]
    v1 = [bm.verts.new(origin + shift1 + U * u + V * v + A * a1) for u, v in poly1]
    faces = [bm.faces.new(list(reversed(v0))) if caps[0] else None, bm.faces.new(v1) if caps[1] else None]
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        faces.append(bm.faces.new((v0[i], v0[j], v1[j], v1[i])))
    for f in faces:
        if f is not None:
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


def bevel_rect(hx, hy, b, cx=0.0, cy=0.0):
    return [(cx + x, cy + y) for x, y in ((hx - b, -hy), (hx, -hy + b), (hx, hy - b), (hx - b, hy), (-hx + b, hy),
                                          (-hx, hy - b), (-hx, -hy + b), (-hx + b, -hy))]


def octagon(box, b):
    x0, x1, z0, z1 = box
    return [(x0 + b, z0), (x1 - b, z0), (x1, z0 + b), (x1, z1 - b), (x1 - b, z1), (x0 + b, z1), (x0, z1 - b),
            (x0, z0 + b)]


def ring(bm, outer, inner, y0, y1, part, cap_parts, gaps=()):
    """A side-profile ring (outer and inner outlines [(x, z)], the same count, both counter-clockwise) from y0 to
    y1; cap_parts: the parts of its faces at y0 and y1 (HIDDEN where something covers it); gap: a segment left out
    (a slot through the ring, or where another ring takes over), their ends closed."""
    o0 = [bm.verts.new(Vector((x, y0, z))) for x, z in outer]
    o1 = [bm.verts.new(Vector((x, y1, z))) for x, z in outer]
    i0 = [bm.verts.new(Vector((x, y0, z))) for x, z in inner]
    i1 = [bm.verts.new(Vector((x, y1, z))) for x, z in inner]
    n = len(outer)
    for k in range(n):
        j = (k + 1) % n
        if k in gaps:
            for e, other in ((k, (k - 1) % n), (j, j)):
                if other not in gaps:
                    bm.faces.new((o0[e], o1[e], i1[e], i0[e])).material_index = part + CHAMFER
            continue
        bm.faces.new((o0[k], o0[j], o1[j], o1[k])).material_index = part
        bm.faces.new((i1[k], i1[j], i0[j], i0[k])).material_index = part
        bm.faces.new((o0[k], i0[k], i0[j], o0[j])).material_index = cap_parts[0]
        bm.faces.new((o1[j], i1[j], i1[k], o1[k])).material_index = cap_parts[1]


def slotted(poly):
    """An octagon's outline with the crane's slot's two ends put in its bottom edge (between points 0 and 1)."""
    z0 = poly[0][1]
    return [poly[0], (CRANE_SLOT[0], z0), (CRANE_SLOT[1], z0)] + poly[1:]


def lathe(bm, rings, sides, centre, parts, rot=0.0):
    """Rings [(x, r)] (r a number, or a list of one radius per side) around the x axis through centre, each band k
    painted parts[k]; capped at both ends (the caps painted parts[0] and parts[-1], or parts' extra two entries)."""
    vs = []
    for x, r in rings:
        rs = r if isinstance(r, (list, tuple)) else [r] * sides
        vs.append([bm.verts.new(centre + Vector((x, rs[i] * math.cos(rot + i * 2 * math.pi / sides),
                                                 rs[i] * math.sin(rot + i * 2 * math.pi / sides))))
                   for i in range(sides)])
    for k in range(len(rings) - 1):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((vs[k][i], vs[k][j], vs[k + 1][j], vs[k + 1][i])).material_index = parts[k]
    caps = parts[len(rings) - 1:] if len(parts) > len(rings) - 1 else [parts[0], parts[-1]]
    bm.faces.new(vs[0]).material_index = caps[0]
    bm.faces.new(vs[-1]).material_index = caps[-1]


def finish(bm, name, location, layer_name="rv_part"):
    """bm (in design coordinates) as an object whose origin is at `location` (design)."""
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index == HIDDEN], context="FACES_ONLY")
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(0.5), use_dissolve_boundaries=False, verts=bm.verts[:],
                             edges=bm.edges[:], delimit={"MATERIAL"})
    bmesh.ops.dissolve_degenerate(bm, dist=1e-4, edges=bm.edges[:])
    bm.transform(Matrix.Translation(-location))
    layer = bm.faces.layers.int.new(layer_name)
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
    win = slotted(octagon(WIN, 0.35))
    out_core = slotted(octagon((OUT[0] + 0.06, OUT[1] - 0.06, OUT[2] + 0.06, OUT[3] - 0.06), 1.22))
    out_band = slotted(octagon(OUT, 1.3))
    # The iron frame around the window (its sides covered by the brass plates), the plates on both sides; a slot
    # through their bottom for the crane's arm.
    # The plates are thick (to THICK_HY) above the window's bottom corners, thin (to BAND_HY) along its bottom, where
    # the cylinder passes swinging out.
    ring(bm, out_core, win, -CORE_HY, CORE_HY, IRON, (HIDDEN, HIDDEN), gaps={1})
    thin, thick = {1} | THICK_SEGS, set(range(len(win))) - THICK_SEGS
    ring(bm, out_band, win, CORE_HY - 0.02, BAND_HY, BRASS, (HIDDEN, BRASS), gaps=thin)
    ring(bm, out_band, win, -BAND_HY, -CORE_HY + 0.02, BRASS, (BRASS, HIDDEN), gaps=thin)
    ring(bm, out_band, win, CORE_HY - 0.02, THICK_HY, BRASS, (HIDDEN, BRASS), gaps=thick)
    ring(bm, out_band, win, -THICK_HY, -CORE_HY + 0.02, BRASS, (BRASS, HIDDEN), gaps=thick)
    # Behind it: the hammer's housing, down into the grip.
    rear = [(-0.5, 0.35), (-0.5, 5.55), (-1.15, 5.8), (-2.15, 5.4), (-2.55, 4.65), (-2.6, 2.9), (-2.85, 1.6),
            (-2.4, 0.35)]
    bevel_caps(bm, side(bm, rear, -0.8, 0.8, DARK), 0.16)
    # The window's iron in front of and behind the cylinder: the recoil shield behind (the chambered rims clear it),
    # the barrel's shank and the crane's seat in front, the ejector rod's tip between them.
    xs = -SHELL_PROUD - 0.06
    fy = THICK_HY - 0.04
    side(bm, [(WIN[0] - 0.05, WIN[2] - 0.05), (xs, WIN[2] - 0.05), (xs, WIN[3] + 0.05), (WIN[0] - 0.05, WIN[3] + 0.05)],
         -fy, fy, DARK)
    side(bm, [(5.1, WIN[2] - 0.05), (WIN[1] + 0.05, WIN[2] - 0.05), (WIN[1] + 0.05, 3.15), (5.1, 3.15)], -fy, fy, DARK)
    side(bm, [(4.9, 4.15), (WIN[1] + 0.05, 4.15), (WIN[1] + 0.05, WIN[3] + 0.05), (4.9, WIN[3] + 0.05)], -fy, fy,
         DARK)
    # The barrel: hexagonal, flat sides; a hexagonal bore.
    lathe(bm, [(BARREL_X0, BARREL_R), (BARREL_X1, BARREL_R), (BARREL_X1, BORE_R), (BARREL_X1 - 0.9, BORE_R)], 6,
          Vector((0, 0, BORE_Z)), [IRON, IRON + CHAMFER, BORE, HIDDEN, BORE], rot=math.pi / 2)
    # The lug under the barrel at the frame's front.
    lug = [(5.9, 2.75), (7.75, 2.75), (8.3, 3.25), (8.3, 4.0), (5.9, 4.0)]
    side(bm, lug, -0.72, 0.72, IRON)
    # The trigger guard (brass): a loop under the frame, its back running into the grip.
    guard = [(-1.5, -1.62), (2.15, -1.62), (2.95, -0.95), (3.25, 0.7), (2.65, 0.7), (2.45, -0.55), (2.0, -1.08),
             (-1.5, -1.08)]

    side(bm, guard, -0.36, 0.36, BRASS)
    # The grip: an octagonal section leaning back, flaring towards its butt.
    gf = (GRIP_FWD, Vector((0, 1, 0)))
    top = bevel_rect(GRIP_SEC[0][0], GRIP_SEC[0][1], 0.34, cx=GRIP_SEC[0][2])
    mid = bevel_rect(GRIP_SEC[1][0], GRIP_SEC[1][1], 0.34, cx=GRIP_SEC[1][2])
    butt = bevel_rect(GRIP_SEC[2][0], GRIP_SEC[2][1], 0.38, cx=GRIP_SEC[2][2])
    prism(bm, mid, GRIP_A[1], GRIP_A[0], gf, WOOD, GRIP_TOP, poly1=top, caps=(False, True))[1].material_index = HIDDEN
    prism(bm, butt, GRIP_A[2], GRIP_A[1], gf, WOOD, GRIP_TOP, poly1=mid, caps=(True, False))
    # Worn leather straps wrapped round it, each a little askew.
    for a, h, tilt, k in STRAPS:
        hu, hy, cx = grip_section(a)
        sec = bevel_rect(hu * k + 0.04, hy * k + 0.05, 0.4, cx=cx)
        f0 = Matrix.Rotation(math.radians(tilt), 3, Vector((0, 1, 0))) @ GRIP_FWD  # askew, seen from the side
        prism(bm, sec, -h / 2, h / 2, (f0, Vector((0, 1, 0))), LEATHER, GRIP_TOP + GRIP_UP * a, caps=(False, False))
    # Sights: a blade on the muzzle's top ridge; a notch between two ears on the top strap's back.
    side(bm, [(FRONT_SIGHT.x - 0.95, BORE_Z + BARREL_R - 0.25), (FRONT_SIGHT.x, BORE_Z + BARREL_R - 0.25),
              (FRONT_SIGHT.x, SIGHT_Z), (FRONT_SIGHT.x - 0.3, SIGHT_Z)], -0.13, 0.13, SIGHT)
    rx0, rx1 = REAR_NOTCH.x, REAR_NOTCH.x + 0.8
    side(bm, [(rx0, OUT[3] - 0.15), (rx1 + 0.25, OUT[3] - 0.15), (rx1 + 0.25, OUT[3] - 0.05), (rx1, NOTCH_BOTTOM),
              (rx0, NOTCH_BOTTOM)], -0.5, 0.5, SIGHT)
    for y0, y1 in ((0.15, 0.5), (-0.5, -0.15)):
        side(bm, [(rx0, NOTCH_BOTTOM - 0.05), (rx1, NOTCH_BOTTOM - 0.05), (rx0 + 0.35, SIGHT_Z), (rx0, SIGHT_Z)],
             y0, y1, SIGHT)
    return finish(bm, "Body", HAND)


def build_cylinder():
    """Six chambers round its axis (the top one on the bore's line), six deep flutes between them."""
    bm = bmesh.new()
    sides = 18
    # Three vertices a chamber: two on its lobe (12 degrees either side of it), one down in the flute between it and
    # the next (the walls stay round the shells: 0.65 from a chamber's middle at the least).
    angles = []
    for i in range(6):
        c = math.pi / 2 + i * math.pi / 3
        angles += [c - math.radians(12), c + math.radians(12), c + math.pi / 6]
    full = [CYL_R] * sides
    fl = [CYL_R if k % 3 != 2 else 1.6 for k in range(sides)]
    face = [2.15] * sides
    rings = [(CYL_X0, face), (CYL_X0 + 0.85, full), (CYL_X0 + 1.3, fl), (CYL_X1 - 1.15, fl), (CYL_X1 - 0.7, full),
             (CYL_X1, face)]
    vs = [[bm.verts.new(CYL_ORIGIN + Vector((x, r[k] * math.cos(a), r[k] * math.sin(a))))
           for k, a in enumerate(angles)] for x, r in rings]
    parts = [STEEL + CHAMFER, STEEL, STEEL, STEEL, STEEL + CHAMFER]
    for b in range(len(rings) - 1):
        for k in range(sides):
            j = (k + 1) % sides
            bm.faces.new((vs[b][k], vs[b][j], vs[b + 1][j], vs[b + 1][k])).material_index = parts[b]
    bm.faces.new(vs[0]).material_index = CAP_REAR
    bm.faces.new(vs[-1]).material_index = CAP_FRONT
    return finish(bm, "Cylinder", CYL_ORIGIN)


def build_rod():
    bm = bmesh.new()
    lathe(bm, [(CYL_X1 - 0.1, 0.21), (5.08, 0.21), (5.08, 0.3), (5.42, 0.3)], 6, ROD_ORIGIN - Vector(
        (CYL_X1, 0, 0)), [IRON, IRON + CHAMFER, IRON, IRON, IRON + CHAMFER])

    return finish(bm, "EjectorRod", ROD_ORIGIN)


def build_crane():
    bm = bmesh.new()
    x0, x1 = CRANE_PIVOT.x, CRANE_PIVOT.x + 0.4
    hub = Vector((0, 0, CYL_AXIS_Z))
    lathe(bm, [(x0, 0.5), (x1, 0.5)], 6, Vector((0, 0, CYL_AXIS_Z)), [DARK])
    lathe(bm, [(x0, 0.27), (x1, 0.27)], 6, Vector((0, CRANE_PIVOT.y, CRANE_PIVOT.z)), [DARK])
    kn = Vector((0, CRANE_PIVOT.y, CRANE_PIVOT.z))
    d = (kn - hub).normalized()
    n = Vector((0, -d.z, d.y))
    pts = [hub + n * 0.3, hub - n * 0.3, kn - n * 0.24, kn + n * 0.24]
    prism(bm, [(p.y, p.z) for p in pts], x0 + 0.12, x1 - 0.12, YZ, DARK)  # (recessed: apart from the caps after the MDL's quantizing)
    return finish(bm, "Crane", CRANE_PIVOT)


def build_hammer():
    bm = bmesh.new()
    # A thick body in the housing's slot, its top no higher than the rear notch's bottom; its spur two prongs that
    # rise and hook back on either side of the sight line (the eye looks between them, through the notch).
    prof = [(-1.25, 3.2), (-1.25, 6.1), (-1.55, 6.5), (-2.45, NOTCH_BOTTOM - 0.02), (-3.0, NOTCH_BOTTOM - 0.02),
            (-3.15, 6.4), (-2.75, 6.1), (-2.35, 5.4), (-2.25, 3.2)]
    side(bm, prof, -0.48, 0.48, HAMMER)
    spur = [(-2.0, 6.4), (-2.4, 6.8), (-2.85, 7.45), (-3.35, 7.75), (-3.9, 7.6), (-3.5, 7.35), (-3.15, 6.95),
            (-3.05, 6.3)]
    for y0, y1 in ((0.17, 0.45), (-0.45, -0.17)):  # (inside the body's sides: no coplanar faces)
        side(bm, spur, y0, y1, HAMMER)


    return finish(bm, "Hammer", HAMMER_PIVOT)


def build_trigger():
    bm = bmesh.new()
    # A curved blade, its tip hooked forward for the finger.
    side(bm, [(1.05, 0.75), (1.65, 0.75), (1.55, 0.15), (1.7, -0.35), (2.0, -0.7), (1.72, -0.82), (1.35, -0.5),
              (1.1, 0.1)], -0.2, 0.2, HAMMER)
    return finish(bm, "Trigger", TRIGGER_PIVOT)


def build_shell():
    """In its own frame: origin the rim's back face's centre, the hull along +X (as it goes into a chamber)."""
    bm = bmesh.new()
    lathe(bm, [(0.0, SHELL_RIM_R), (SHELL_RIM_T, SHELL_RIM_R), (SHELL_RIM_T, SHELL_HULL_R),
               (SHELL_LEN - 0.16, SHELL_HULL_R), (SHELL_LEN, SHELL_HULL_R - 0.13)], 8, Vector(),
          [HEAD, HEAD, HULL, HULL + CHAMFER, RIM_BACK, CRIMP], rot=math.pi / 8)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    layer = bm.faces.layers.int.new("rv_part")
    for f in bm.faces:
        f[layer] = f.material_index
        f.material_index = 0
    me = bpy.data.meshes.new("Shell")
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = False
    ob = bpy.data.objects.new("Shell", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = Vector((2.0, -6.0, -3.0))  # (beside the gun: where it is does not matter, it has its own MDL)
    return ob


# ----------------------------------------------------------------------------------------------------------------------
# The skin

def rgb(*c):
    return Vector(c)


BASE = {IRON: rgb(86, 76, 66), DARK: rgb(74, 66, 58), BRASS: rgb(158, 120, 56), BORE: rgb(22, 20, 18),
        WOOD: rgb(128, 58, 30), LEATHER: rgb(112, 90, 70)
, SIGHT: rgb(74, 70, 66), STEEL: rgb(90, 87, 82),

        CAP_REAR: rgb(104, 100, 96), CAP_FRONT: rgb(96, 92, 88), HAMMER: rgb(90, 86, 80)}
EDGE = {BRASS: rgb(222, 190, 110), WOOD: rgb(176, 92, 50), LEATHER: rgb(124, 100, 80), STEEL: rgb(196, 192, 184),
        CAP_REAR: rgb(186, 182, 172), CAP_FRONT: rgb(176, 172, 164)}
EDGE_IRON = rgb(150, 144, 136)
RUST = rgb(110, 76, 50)
GRIME = rgb(66, 50, 32)
RUSTY = (IRON, DARK, SIGHT, HAMMER)
RUNE_INLAY, RUNE_DEEP = rgb(164, 120, 64), rgb(40, 32, 26)

# Runes (Elder Futhark-like strokes in a 0.5 x 1 box), carved along both flat sides of the barrel.
GLYPHS = [
    [((0, 0), (0, 1)), ((0, 0.5), (0.5, 0.82)), ((0, 0.78), (0.45, 1.04))],             # fehu
    [((0, 0), (0, 1)), ((0, 1), (0.5, 0.72)), ((0.5, 0.72), (0.5, 0))],                 # uruz
    [((0, 0), (0, 1)), ((0, 0.78), (0.42, 0.5)), ((0.42, 0.5), (0, 0.22))],             # thurisaz
    [((0, 0), (0, 1)), ((0, 1), (0.45, 0.74)), ((0, 0.68), (0.45, 0.42))],              # ansuz
    [((0, 0), (0, 1)), ((0, 1), (0.45, 0.8)), ((0.45, 0.8), (0, 0.55)), ((0, 0.55), (0.45, 0))],  # raidho
    [((0.45, 1), (0, 0.5)), ((0, 0.5), (0.45, 0))],                                      # kenaz
    [((0, 0), (0.5, 1)), ((0.5, 0), (0, 1))],                                            # gebo
    [((0.25, 0), (0.25, 1)), ((0.25, 0.5), (0, 1)), ((0.25, 0.5), (0.5, 1))],           # algiz
    [((0, 0), (0.5, 0.55)), ((0.5, 0), (0, 0.55)), ((0, 0.55), (0.25, 1)), ((0.5, 0.55), (0.25, 1))],  # othala
    [((0.25, 0), (0.25, 1)), ((0.25, 1), (0, 0.68)), ((0.25, 1), (0.5, 0.68))],         # tiwaz
    [((0.42, 1), (0.05, 0.62)), ((0.05, 0.62), (0.45, 0.38)), ((0.45, 0.38), (0.08, 0))],  # sowilo
]
RUNE_TEXT = [4, 0, 7, 10, 2, 6, 9]
RUNE_X0, RUNE_STEP, RUNE_H = 7.2, 0.62, 0.8


def rune_dist(u, v):
    """Distance from (u, v) (along the barrel from RUNE_X0, up from the bore's height) to the nearest stroke."""
    best = 9.0
    k = int(math.floor(u / RUNE_STEP))
    for i in (k - 1, k, k + 1):
        if 0 <= i < len(RUNE_TEXT):
            ox = i * RUNE_STEP + 0.08
            for (a0, a1), (b0, b1) in GLYPHS[RUNE_TEXT[i]]:
                a = Vector((ox + a0 * RUNE_H * 0.6, (a1 - 0.5) * RUNE_H, 0))
                b = Vector((ox + b0 * RUNE_H * 0.6, (b1 - 0.5) * RUNE_H, 0))
                best = min(best, seg_dist(Vector((u, v, 0)), a, b))
    return best


def octagon_corners(box, b, inset):
    x0, x1, z0, z1 = box
    pts = octagon(box, b)
    c = Vector(((x0 + x1) / 2, (z0 + z1) / 2, 0))
    out = []
    for x, z in pts:
        p = Vector((x, z, 0))
        out.append(p + (c - p).normalized() * inset)
    return out


def rivets():
    """[(centre (Body frame), the face's normal, radius)]: domes painted on the brass plates' corners, the housing."""
    out = []
    pts = octagon_corners(OUT, 1.3, 0.38)
    plate = []
    for i in range(len(pts)):  # one at each corner of the octagon, between them on the long sides
        plate.append((pts[i].x, pts[i].y + (0.22 if i in (2, 7) else 0.0), 0.21, BAND_HY if i < 2 else THICK_HY))
    plate += [((OUT[0] + OUT[1]) / 2, OUT[2] + 0.3, 0.18, BAND_HY), ((OUT[0] + OUT[1]) / 2, OUT[3] - 0.3, 0.2,
                                                                      THICK_HY)]
    for x, z, r, hy in plate:
        for s in (1.0, -1.0):
            out.append((Vector((x, s * hy, z)) - HAND, Vector((0, s, 0)), r))
    for x, z, r in ((-2.35, 2.2, 0.2), (-2.0, 4.6, 0.18), (7.4, 3.75, 0.17), (0.2, -1.45, 0.14), (2.3, -1.45, 0.14)):
        hy = 0.8 if x < 0 and z > 1 else (0.62 if x > 5 else 0.36)
        for s in (1.0, -1.0):
            out.append((Vector((x, s * hy, z)) - HAND, Vector((0, s, 0)), r))
    for u, a in ((0.0, -0.9),):
        for s in (1.0, -1.0):
            out.append((GRIP_TOP + GRIP_FWD * u + GRIP_UP * a + Vector((0, s * 0.92, 0)) - HAND, Vector((0, s, 0)),
                        0.17))
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


def chambers_paint(col, d, part):
    """The cylinder's end faces: six chamber mouths (dark, a bright worn lip), the ratchet or the rod's hole."""
    r = Vector((0, d.y, d.z - CYL_AXIS_Z))
    for i in range(6):
        ang = math.pi / 2 + i * math.pi / 3
        c = Vector((0, PITCH * math.cos(ang), PITCH * math.sin(ang)))
        k = (r - c).length
        if k < CHAMBER_R - 0.1:
            return rgb(20, 18, 16) if part == CAP_FRONT else mix(rgb(16, 14, 12), rgb(40, 36, 32), k / CHAMBER_R)
        if k < CHAMBER_R:
            return mix(col, EDGE[part], 0.7)
        if k < CHAMBER_R + 0.12:
            return col * 0.55
    if part == CAP_REAR:
        # The extractor star: a six-pointed plate round the middle.
        ang = math.atan2(r.z, r.y)
        star = 0.48 + 0.18 * abs(math.cos(3 * ang))
        if r.length < star:
            return mix(col, EDGE[part], 0.35) if r.length < star - 0.1 else col * 0.6
    elif r.length < 0.32:
        return rgb(24, 22, 20)
    return col


def paint_texel(p, n, part, chamfer, edges, rvts, ao, speck, ob_name):
    d = p + HAND  # design coordinates
    col = BASE[part].copy()
    big = noise.noise(d * 0.8)
    mid = noise.noise(d * 2.6 + Vector((3.1, 7.7, 1.3)))
    fine = noise.noise(d * 7.5 + Vector((9.2, 1.4, 5.5)))
    if part in RUSTY:
        rust = smooth(0.15, 0.65, 0.55 * big + 0.45 * mid + 0.25 * fine)
        col = mix(col, RUST, 0.55 * rust)
        col *= 0.86 + 0.18 * mid + 0.14 * fine
        if fine > 0.45:
            col *= 0.72  # pitting
    elif part == BRASS:
        col *= 0.88 + 0.14 * mid + 0.12 * fine
        col = mix(col, GRIME, 0.55 * smooth(0.2, 0.7, big * 0.6 + fine * 0.5))
    elif part in (STEEL, CAP_REAR, CAP_FRONT):
        col *= 0.88 + 0.12 * mid + 0.14 * fine
        col = mix(col, RUST, 0.35 * smooth(0.3, 0.8, big * 0.5 + mid * 0.5))
    elif part == WOOD:
        # Grain along the grip.
        rel = d - GRIP_TOP
        u, a = rel.dot(GRIP_FWD), rel.dot(GRIP_UP)
        g = noise.noise(Vector((u * 5.0, d.y * 5.0, a * 0.6)))
        col *= 0.86 + 0.22 * g + 0.08 * fine
        if g > 0.32:
            col *= 0.8
    elif part == LEATHER:
        col *= 0.84 + 0.2 * mid + 0.16 * fine
        if fine > 0.38:
            col = mix(col, EDGE[LEATHER], 0.35)  # scuffs
    else:
        col *= 0.92 + 0.1 * fine
    col *= 1.0 + speck
    # The barrel's flat sides: carved runes, inlaid with a dull gold.
    if ob_name == "Body" and part == IRON and abs(n.y) > 0.98 and abs(d.y) > 0.95 and RUNE_X0 - 0.2 < d.x < 12.3 \
            and abs(d.z - BORE_Z) < 0.7:
        u = (d.x - RUNE_X0) if n.y < 0 else (RUNE_X0 + len(RUNE_TEXT) * RUNE_STEP - d.x)
        k = rune_dist(u, d.z - BORE_Z)
        if k < 0.055:
            col = RUNE_INLAY * (0.9 + 0.2 * fine)
        elif k < 0.095:
            col = RUNE_DEEP
    if part in (CAP_REAR, CAP_FRONT):
        col = chambers_paint(col, d, part)
    # The hammer's spur: grooved for the thumb.
    if part == HAMMER and n.z > 0.3 and d.x < -1.9 and ((d.x * 4.0) % 1.0) < 0.4:
        col *= 0.55
    # Wood under the leather: darker at the straps' edges; the straps' worn edges.
    # Worn, brighter convex edges; darker concave ones.
    wear = smooth(-0.35, 0.35, noise.noise(d * 3.3 + Vector((5.0, 2.0, 8.0))))
    edge_col = EDGE.get(part) or col * 1.05 + EDGE_IRON * 0.45  # (worn iron: the colour lighter, towards steel)
    if chamfer:
        col = mix(col, edge_col, 0.25 + 0.4 * wear)
    for a, b, convex in edges:
        dist = seg_dist(p, a, b)
        if convex and dist < 0.08 and not chamfer:
            col = mix(col, edge_col, (0.4 + 0.5 * wear) * (1.0 - dist / 0.08) + 0.15)
        elif not convex and dist < 0.16:
            col *= 0.55 + 0.45 * dist / 0.16
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
            col = mix(rgb(60, 46, 26), rgb(236, 206, 130), lit ** 1.4)
        elif k < 1.45:
            col *= 0.45 if (v.z < 0.0 or v.x < -0.5 * r) else 0.7
    col *= ao
    col *= 0.94 + 0.1 * n.z
    return col


def shell_texel(p, n, part, chamfer, slug, spent):
    """A shotgun shell's skin: a brass head with a primer; the hull red (or the slug's blue), a six-fold star crimp
    (spent: opened, dark inside)."""
    hull = rgb(40, 64, 150) if slug else rgb(150, 30, 24)
    hull_hi = rgb(96, 120, 210) if slug else rgb(214, 70, 52)
    fine = noise.noise(p * 9.0 + Vector((2.0, 5.0, 1.0)))
    r = math.hypot(p.y, p.z)
    if part in (HEAD, RIM_BACK) or (part == HULL and p.x < SHELL_HEAD_LEN):
        col = rgb(186, 146, 70) * (0.9 + 0.12 * fine)
        if part == HULL and p.x > SHELL_HEAD_LEN - 0.1:
            col = rgb(110, 82, 40)  # the head's crimped edge on the hull
        if part == RIM_BACK:
            if r < 0.2:
                col = rgb(150, 150, 140) if not spent else rgb(90, 86, 80)
                if spent and r < 0.08:
                    col = rgb(40, 36, 30)
            elif r < 0.27:
                col = rgb(90, 66, 34)
            elif r > SHELL_RIM_R - 0.1:
                col = rgb(232, 196, 120)
            # the headstamp ring
            elif abs(r - 0.45) < 0.04:
                col *= 0.7
        if part == HEAD and p.x < SHELL_RIM_T + 0.05:
            col = rgb(232, 196, 120) if n.x < 0.5 else col
    elif part == HULL:
        col = hull * (0.88 + 0.14 * fine)
        if abs(math.atan2(p.z, p.y) % (math.pi / 4) - math.pi / 8) < 0.12:
            col = mix(col, hull_hi, 0.35)  # a highlight along each facet
        if chamfer:
            col = mix(col, hull_hi, 0.4)
    else:  # CRIMP
        ang = math.atan2(p.z, p.y)
        fold = abs(math.cos(3 * ang))
        col = hull * (0.7 + 0.4 * fold)
        if r < 0.06 or fold < 0.12:
            col = hull * 0.45
        if spent:
            col = rgb(26, 20, 16) if r < 0.38 else hull * 0.6
    if spent:
        col *= 0.78
    col *= 0.94 + 0.1 * max(0.0, n.z) - 0.06 * max(0.0, -n.z)
    return col


def raster(ob, w, h, fn):
    """Calls fn(p, n, part, chamfer, face, object-space) for every texel the object's UV islands cover."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.transform(ob.matrix_world)
    bm.normal_update()
    uv = bm.loops.layers.uv.active
    part_layer = bm.faces.layers.int.get("rv_part")
    for tri in bm.calc_loop_triangles():
        f = tri[0].face
        part, chamfer = f[part_layer] % CHAMFER, CHAMFER <= f[part_layer] < HIDDEN
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
                fn(s, t, ps[0] * b0 + ps[1] * b1 + ps[2] * b2, f.normal.copy(), part, chamfer, f)
    bm.free()


def face_edges(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.transform(ob.matrix_world)
    out = {}
    for f in bm.faces:
        es = []
        for e in f.edges:
            if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(25):
                es.append((e.verts[0].co.copy(), e.verts[1].co.copy(), e.is_convex))
        out[f.index] = es
    bm.free()
    return out


def bleed(tex, w, h, passes=4):
    """Each island's edge texels grown outward (filtered and mipmapped, no dark seams), the rest dark."""
    for _ in range(passes):
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


def bake(objs, image):
    """Paints image (all of objs' UV islands) procedurally; returns the RGB texels (top row first)."""
    w, h = image.size
    tex = [None] * (w * h)
    rvts = rivets()
    rng = random.Random(1)
    verts, polys = [], []
    for oc in objs:  # every part occludes every other (the gun as assembled)
        m = oc.matrix_world
        base = len(verts)
        verts += [m @ v.co for v in oc.data.vertices]
        polys += [[base + i for i in p.vertices] for p in oc.data.polygons]
    bvh = BVHTree.FromPolygons(verts, polys)
    for ob in objs:
        edges = face_edges(ob)

        def put(s, t, p, n, part, chamfer, f, ob=ob, edges=edges):
            ao = occlusion(bvh, p, n)
            speck = (rng.random() - 0.5) * 0.22
            tex[t * w + s] = paint_texel(p, n, part, chamfer, edges[f.index], rvts, 0.3 + 0.7 * ao, speck, ob.name)
        raster(ob, w, h, put)
    return bleed(tex, w, h)


def bake_shell(ob, w, h, slug, spent):
    tex = [None] * (w * h)
    rng = random.Random(3)
    inv = ob.matrix_world.inverted()

    def put(s, t, p, n, part, chamfer, f):
        q = inv @ p
        tex[t * w + s] = shell_texel(q, n, part, chamfer, slug, spent) * (1.0 + (rng.random() - 0.5) * 0.12)
    raster(ob, w, h, put)
    return bleed(tex, w, h, 3)


def save_quantized(name, texels, w, h, path):
    q = export_mdl.qpal.Quantizer()
    pal = export_mdl.qpal.PALETTE
    img = bpy.data.images.get(name) or bpy.data.images.new(name, w, h, alpha=False)
    px = [0.0] * (w * h * 4)
    for t in range(h):
        for s in range(w):
            c = pal[q.nearest(texels[t * w + s], False)]
            i = ((h - 1 - t) * w + s) * 4
            px[i:i + 4] = [c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, 1.0]
    img.pixels = px
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    return img


def snap_uvs(objs, w, h):
    for ob in objs:
        for d in ob.data.uv_layers.active.data:
            s = min(w - 1, max(0, math.floor(d.uv.x * w)))
            t = min(h - 1, max(0, math.floor((1.0 - d.uv.y) * h)))
            d.uv = ((s + 0.5) / w, 1.0 - (t + 0.5) / h)


def unwrap(objs, active, margin):
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = active
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(50), island_margin=margin, area_weight=0.0,
                             correct_aspect=True, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")


def material(name, img):
    mat = bpy.data.materials.new(name)
    if bpy.app.version < (5, 0, 0):
        mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = nodes.get("Principled BSDF")
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = "Closest"
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.9
    return mat


def swing_check(body, moving):
    """Prints any angle of the crane's swing (rest to out) where the cylinder, the rod or the crane touch the frame
    or the hammer and trigger (it must swing out clear)."""
    crane = bpy.data.objects["Crane"]
    fixed = [body, bpy.data.objects["Hammer"], bpy.data.objects["Trigger"]]

    def tree(obs):
        verts, polys = [], []
        for oc in obs:
            m = oc.matrix_world
            base = len(verts)
            verts += [m @ v.co for v in oc.data.vertices]
            oc.data.calc_loop_triangles()  # (a concave face as drawn, not fanned)
            polys += [[base + i for i in t.vertices] for t in oc.data.loop_triangles]
        return BVHTree.FromPolygons(verts, polys, epsilon=0.0)
    hits = []
    still = tree(fixed)
    steps = 20
    for k in range(steps + 1):
        crane.rotation_euler.x = math.radians(CRANE_SWING) * k / steps
        bpy.context.view_layer.update()
        n = len(still.overlap(tree(moving)))
        if n:
            hits.append("%.0f deg: %d" % (CRANE_SWING * k / steps, n))
    crane.rotation_euler.x = 0.0
    bpy.context.view_layer.update()
    print("swing check: %s" % ("clear" if not hits else "TOUCHES at " + ", ".join(hits)))


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = "NONE"
    body = build_body()
    crane = build_crane()
    cyl = build_cylinder()
    rod = build_rod()
    hammer = build_hammer()
    trigger = build_trigger()
    shell = build_shell()
    # The hierarchy: Body > Crane > Cylinder > EjectorRod; Body > Hammer, Trigger. Children keep their places.
    for child, parent in ((crane, body), (cyl, crane), (rod, cyl), (hammer, body), (trigger, body)):
        child.parent = parent
    # (under a parent, a location is relative to it: none of them is turned at rest)
    crane.location = CRANE_PIVOT - HAND
    cyl.location = CYL_ORIGIN - CRANE_PIVOT
    rod.location = ROD_ORIGIN - CYL_ORIGIN
    hammer.location = HAMMER_PIVOT - HAND
    trigger.location = TRIGGER_PIVOT - HAND
    body["preview_scale"] = 0.45
    body["front_sight"] = FRONT_SIGHT - HAND  # the sight line (Body frame): the blade's top, the notch's top
    body["rear_notch"] = REAR_NOTCH - HAND
    crane["swing"] = CRANE_SWING
    hammer["cock"] = HAMMER_COCK
    trigger["pull"] = TRIGGER_PULL
    rod["travel"] = 1.0
    shell["proud"] = SHELL_PROUD
    shell["skins"] = "revolver_shell.png revolver_shell_slug.png revolver_shell_spent.png revolver_shell_slug_spent.png"
    cyl["pitch"] = PITCH
    for ob, text in ((body, "Quake axes: +X forward (muzzle), +Y left, +Z up. Origin: the grip's hand point."),
                     (crane, "Origin: its pivot. Swings out about its local X by 'swing' degrees (to the left)."),
                     (cyl, "Origin: its axis at its rear face. Spins about its local X (60 degrees a chamber). "
                           "Chambers at radius 'pitch', the top one on the bore's line."),
                     (rod, "Pushed back along its local -X by 'travel' (ejecting)."),
                     (hammer, "Origin: its pivot. Cocks about its local Y by 'cock' degrees."),
                     (trigger, "Origin: its pivot. Pulled about its local Y by 'pull' degrees."),
                     (shell, "Origin: the rim's back face's centre; the hull along +X. Its own skin and MDL.")):
        ob["note"] = text
    gun = [body, crane, cyl, rod, hammer, trigger]
    bpy.context.view_layer.update()
    swing_check(body, [crane, cyl, rod])

    # UVs: one atlas for the gun (one skin), one for the shell; every corner on a texel's centre (as the MDL stores).
    unwrap(gun, body, 0.01)
    snap_uvs(gun, SKIN_W, SKIN_H)
    unwrap([shell], shell, 0.03)
    snap_uvs([shell], SHELL_W, SHELL_H)
    bpy.context.view_layer.update()

    img = bpy.data.images.new("revolver_skin", SKIN_W, SKIN_H, alpha=False)
    img = save_quantized("revolver_skin", bake(gun, img), SKIN_W, SKIN_H, os.path.join(HERE, "revolver_skin.png"))
    mat = material("RevolverSkin", img)
    for ob in gun:
        ob.data.materials.append(mat)
    names = shell["skins"].split()
    shell_imgs = []
    for k, (slug, spent) in enumerate(((False, False), (True, False), (False, True), (True, True))):
        name = os.path.splitext(names[k])[0]
        shell_imgs.append(save_quantized(name, bake_shell(shell, SHELL_W, SHELL_H, slug, spent), SHELL_W, SHELL_H,
                                         os.path.join(HERE, names[k])))
    shell.data.materials.append(material("RevolverShellSkin", shell_imgs[0]))

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "revolver.blend"), relative_remap=True)
    export_mdl.export()


main()
