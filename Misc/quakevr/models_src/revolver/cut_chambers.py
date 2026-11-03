# cut_chambers.py -- bores the heavy revolver's six chambers through its Cylinder in revolver.blend (README.md, "The
# chambers"): real holes to see through, walled inside, instead of black discs painted on its end faces. Run once on the
# .blend (it stops if they are cut already), then export_mdl.py:
#
#   blender -b Misc/quakevr/models_src/revolver/revolver.blend -P Misc/quakevr/models_src/revolver/cut_chambers.py
#
# The Cylinder's outside is left as it is. Its two end faces (planar, 18 corners, UV-mapped flat) become triangles round
# the holes, their UVs from the same mapping, so the hand-painted skin stays where it was: only each hole's rim is drawn
# from a little further in (HOLE_UV_R, the painted bright lip stays at the mouth). The walls take a small patch painted
# inside a chamber disc of the rear end face, texels nothing shows any more.

import math

import bmesh
import bpy
from mathutils import Vector
from mathutils.geometry import tessellate_polygon

CHAMBERS = 6
PITCH = 1.45        # the chambers' circle (README.md)
HOLE_SIDES = 10
HOLE_APOTHEM = 0.6  # the walls' nearest to a chamber's middle (the shell's hull: radius 0.59, the chamber as painted 0.6)
HOLE_R = HOLE_APOTHEM / math.cos(math.pi / HOLE_SIDES)  # 0.631; the flutes come no nearer the middle than 0.716
HOLE_UV_R = 0.48    # where on the painted end face a hole's rim is drawn from
WALL_CHAMBER = 3    # whose painted disc on the rear end face holds the walls' patch
WALL_RAMP = [88, 52, 39, 39, 52, 70]  # the patch's rows, grey: the rear mouth's worn edge, down the bore, the front's
WALL_W = 3          # its columns


def affine_uv(points):
    """(y, z) -> uv by least squares over the face's corners [(co, uv)] (its UVs are snapped to texels)."""
    import numpy as np
    a = np.array([[co.y, co.z, 1.0] for co, _ in points])
    u = np.array([[uv.x, uv.y] for _, uv in points])
    sol = np.linalg.lstsq(a, u, rcond=None)[0]
    return lambda y, z: Vector((sol[0][0] * y + sol[1][0] * z + sol[2][0], sol[0][1] * y + sol[1][1] * z + sol[2][1]))


def chamber_centre(i):
    a = math.radians(90 + 60 * i)
    return PITCH * math.cos(a), PITCH * math.sin(a)


def snap(uv, w, h):
    return Vector(((math.floor(uv.x * w) + 0.5) / w, (math.floor(uv.y * h) + 0.5) / h))


def paint_walls(img, map_uv):
    """The patch in the rear face's chamber disc; returns its texel box (x0, y0, x1, y1)."""
    w, h = img.size
    cy, cz = chamber_centre(WALL_CHAMBER)
    c = map_uv(cy, cz)
    x0, y0 = int(c.x * w) - WALL_W // 2, int(c.y * h) - len(WALL_RAMP) // 2
    px = list(img.pixels)
    for row, grey in enumerate(WALL_RAMP):
        for col in range(WALL_W):
            k = ((y0 + row) * w + x0 + col) * img.channels
            px[k:k + 3] = [grey / 255.0] * 3
    img.pixels = px
    img.save()
    return x0, y0, x0 + WALL_W, y0 + len(WALL_RAMP)


def cut(ob, img):
    me = ob.data
    bm = bmesh.new()
    bm.from_mesh(me)
    uv_layer = bm.loops.layers.uv.active
    part = bm.faces.layers.int["rv_part"]
    w, h = img.size
    caps = sorted((f for f in bm.faces if abs(f.normal.x) > 0.9 and len(f.verts) >= 12), key=lambda f: f.verts[0].co.x)
    if len(caps) != 2:
        raise SystemExit("cut_chambers: the Cylinder's end faces: %d found, 2 wanted (cut already?)" % len(caps))
    rims = []
    box = None
    for cap in caps:
        x = cap.verts[0].co.x
        corners = [(l.vert, l[uv_layer].uv.copy()) for l in cap.loops]
        map_uv = affine_uv([(v.co, uv) for v, uv in corners])
        if box is None:
            box = paint_walls(img, map_uv)  # (the rear end face comes first)
        outline = [(v.co.y, v.co.z) for v, _ in corners]
        holes = []
        for i in range(CHAMBERS):
            cy, cz = chamber_centre(i)
            holes.append([(cy + HOLE_R * math.cos(k * 2 * math.pi / HOLE_SIDES), cz + HOLE_R * math.sin(k * 2 * math.pi / HOLE_SIDES))
                          for k in range(HOLE_SIDES)])
        flat = [v for v, _ in corners]
        uvs = [uv for _, uv in corners]
        rim = []
        for i, hole in enumerate(holes):
            cy, cz = chamber_centre(i)
            ring = []
            for k, (y, z) in enumerate(hole):
                v = bm.verts.new((x, y, z))
                ring.append(v)
                flat.append(v)
                a = k * 2 * math.pi / HOLE_SIDES
                uvs.append(snap(map_uv(cy + HOLE_UV_R * math.cos(a), cz + HOLE_UV_R * math.sin(a)), w, h))
            rim.append(ring)
        rims.append(rim)
        tris = tessellate_polygon([[(y, z) for y, z in outline]] + holes)
        keep = cap[part]
        uv_of = {v: uv for v, uv in zip(flat, uvs)}
        for t in tris:
            f = bm.faces.new([flat[j] for j in t])
            f[part] = keep
            f.smooth = False
            for l in f.loops:
                l[uv_layer].uv = uv_of[l.vert]
        bm.faces.remove(cap)
    # The walls, rear rim to front rim: down the patch's ramp, two of its columns across a side.
    x0, y0, x1, y1 = box
    for rear, front in zip(rims[0], rims[1]):
        for k in range(HOLE_SIDES):
            j = (k + 1) % HOLE_SIDES
            f = bm.faces.new((rear[k], rear[j], front[j], front[k]))
            f[part] = keep
            f.smooth = False
            uv_of = {rear[k]: (x0 + 0.5, y0 + 0.5), rear[j]: (x0 + 1.5, y0 + 0.5), front[j]: (x0 + 1.5, y1 - 0.5), front[k]: (x0 + 0.5, y1 - 0.5)}
            for l in f.loops:
                u, t = uv_of[l.vert]
                l[uv_layer].uv = Vector((u / w, t / h))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bad = [e for e in bm.edges if len(e.link_faces) != 2]
    if bad:
        raise SystemExit("cut_chambers: %d edges not shared by two faces" % len(bad))
    # (Outward: the ends still face out.)
    ends = sorted((f for f in bm.faces if abs(f.normal.x) > 0.9), key=lambda f: f.calc_center_median().x)
    if ends[0].normal.x > 0 or ends[-1].normal.x < 0:
        raise SystemExit("cut_chambers: the normals came out inside out")
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = False
    me.update()


def main():
    ob = bpy.data.objects["Cylinder"]
    if len(ob.data.vertices) != 108:
        raise SystemExit("cut_chambers: the Cylinder has %d vertices, not the 108 of before: cut already?" % len(ob.data.vertices))
    img = None
    for node in ob.material_slots[0].material.node_tree.nodes:
        if node.type == "TEX_IMAGE":
            img = node.image
    cut(ob, img)
    ob.data.calc_loop_triangles()
    print("cut_chambers: the Cylinder: %d vertices, %d triangles" % (len(ob.data.vertices), len(ob.data.loop_triangles)))
    bpy.ops.wm.save_mainfile()


main()
