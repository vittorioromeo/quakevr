#!/usr/bin/env python3
# make_screen_housing_light.py -- the lightning gun's ammo screen housing, modelled on v_light.blend (ROUND24.md, "The
# lightning gun's ammo screen housing"; his mockup, kit scratch/mockups/light_housing.webp: the screen set in a shallow
# flat bronze bezel with chamfered corners on the receiver's rear end face, under the rear sight, two round rivets on
# each side frame; below it the end face keeps its striped texture).
#
# Usage: blender -b --factory-startup Misc/quakevr/src_models/blend/v_light/v_light.blend
#            --python Misc/quakevr/make_screen_housing_light.py -- [--save other.blend] [--dry]
# Builds or moves the housing, exports the model as blend_export.py does (quakevr/progs, weapons.txt's barrel mask)
# and saves the .blend (in place, or as --save) as the export left it. --dry: only prints the design (nothing changed).
# Ends with "housing_light: ok, ...".
#
# One definition: the opening's half size and chamfer (`opening`, `radius` < 0, in font pixels), the screen's `size`
# and `offset` are read from the "light" entry of Quake/vr/vr_screenstyle.cpp (the table the engine shapes and places
# the glass by) and weapons.txt slot 7's wpntxt_scale. The window is snapped to the model's byte grid; the script
# refuses when the snapped window or the glass's place disagree with the table (by more than 0.3 font pixels, or 0.01
# units), printing the numbers that match.
#
# Run again on a .blend that has the housing (its vertices carry the point attribute `qvr_housing`): with the same
# shape (as many vertices) its vertices are moved and re-mapped in place, so the model's file keeps its vertices;
# otherwise the old housing is deleted and built anew (its old vertices then stay in the .mdl unused: start from the
# .blend before the housing, in git history, to keep the file lean).
#
# The shape (his build spec, in visible glass heights H; u right = model -Y, v up = model +Z, seen from behind):
# - the window 1.64H x 1H, chamfered corners 0.22H; the glass upright, 0.06H behind the bezel's front, the opening's
#   exact shape;
# - the bezel: a flat frame 0.18H wide round it (its corners chamfered too), 0.12H proud of the end face, no bevel:
#   a flat front, its outer walls and the opening's walls straight back to the end face (the face is the back
#   behind the glass), closed there by a ring facing into the gun (culled, so no fight with the face: a closed solid);
# - 4 round rivets (radius 0.08H) on the side frames, centred in the frame's width, 0.32H above and below the middle.
# The end face is the receiver's upright rear face (x -2.7194, z 8.67..10.86, a trapezoid wider at the bottom) under the
# rear sight. The model's X grid is coarse (0.171 units a step: 0.12H is one step), Y 0.073, Z 0.069: every vertex is
# on the grid (the file's bytes), so the sizes are whole steps (printed).
# Texture, all from the gun's own skin: the bezel's front planar on the worn bronze of the lower tube (324..340 x
# 92..100), its outer walls on the upper body's metal row (357..369, row 55), the opening's walls on the dark row above
# it (row 53: the recessed details' brown-black), the rivets on the tubes' brass fitting (326, 23: lit centre).

import math
import os
import re
import struct
import sys

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
STEM = "light"
MODEL = "v_light"
SLOT = 7

# Measured with vr_debug_screen_where 1 (weapons.txt slot 7's wpntxt offsets, pitch 0, the table's offset 0): the
# glass's centre (right model -Y, up +Z, upright), and a font pixel 0.07654 model units at wpntxt_scale 0.3: 2.5513 model
# units a world unit (the offset's unit; a font pixel is 0.1 * scale * size of them).
BASE_CENTRE = Vector((-2.9642, -0.2184, 9.7616))
MPU = 2.5513
# The end face: its plane and its height (and its middle across: y -0.172).
FACE_X = -2.7194
FACE_Z = (8.6697, 10.8636)
FACE_Y = -0.172
# The spec, in glass heights.
FRAME = 0.18
PROUD = 0.12
GLASS_IN = 0.06
RIVET_R = 0.08
RIVET_V = 0.32
# Skin places (texels).
FRONT_PATCH = (357.5, 56.2, 369.5, 58.8)   # the bezel's front, planar (the lit neck: brightest mid-top and bottom)
WALL_COLS = (357.5, 369.5)
OUTER_ROWS = (55.3, 55.8)                   # the outer walls: front edge, back edge
INNER_ROWS = (53.3, 53.8)                   # the opening's walls
RIVET_TEX = (326.5, 23.5, 1.2)              # the brass fitting's centre and the base ring's radius


def read_style():
    """The "light" entry: opening (half w, half h), radius, size, offset (right, up, out)."""
    src = open(os.path.join(ROOT, "Quake", "vr", "vr_screenstyle.cpp"), encoding="utf-8").read()
    m = re.search(r'\{"%s",(.*?)\n\s*(?:\{"|\};|//)' % STEM, src, re.S)
    if not m:
        raise SystemExit("housing_light: no \"%s\" entry in vr_screenstyle.cpp" % STEM)
    body = m.group(1)
    f = r"(-?[0-9.]+)f?"
    op = re.search(r"\.opening\s*=\s*\{\s*%s\s*,\s*%s\s*\}" % (f, f), body)
    rad = re.search(r"\.radius\s*=\s*" + f, body)
    size = re.search(r"\.size\s*=\s*" + f, body)
    off = re.search(r"\.offset\s*=\s*\{\s*%s\s*,\s*%s\s*,\s*%s\s*\}" % (f, f, f), body)
    if not (op and rad and size and off):
        raise SystemExit("housing_light: the \"%s\" entry needs .radius, .size, .offset and .opening" % STEM)
    return ((float(op.group(1)), float(op.group(2))), float(rad.group(1)), float(size.group(1)),
            tuple(float(off.group(i)) for i in (1, 2, 3)))


def wpntxt_scale():
    txt = open(os.path.join(ROOT, "quakevr", "data", "weapons.txt"), encoding="utf-8").read()
    m = re.search(r"^weapon %d\s*\{(.*?)^\}" % SLOT, txt, re.S | re.M)
    return float(re.search(r'wpntxt_scale "([0-9.]+)"', m.group(1)).group(1))


def grid():
    """The model file's grid: (origin, step) per axis."""
    d = open(os.path.join(ROOT, "quakevr", "progs", MODEL + ".mdl"), "rb").read()
    sc = struct.unpack_from("<3f", d, 8)
    og = struct.unpack_from("<3f", d, 20)
    return og, sc


def outline(hw, hh, cw, ch):
    """A chamfered rectangle's 8 points (u, v), counter-clockwise from the top right corner's start; the cut cw along
    u, ch along v."""
    pts = []
    for su, sv, first_side in ((1, 1, True), (-1, 1, False), (-1, -1, True), (1, -1, False)):
        a = (su * hw, sv * (hh - ch))
        b = (su * (hw - cw), sv * hh)
        pts += [a, b] if first_side else [b, a]
    return pts


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    save = argv[argv.index("--save") + 1] if "--save" in argv else bpy.data.filepath
    dry = "--dry" in argv

    (ow, oh), orad, size, offset = read_style()
    if orad >= 0:
        raise SystemExit("housing_light: the entry's radius must be < 0 (a chamfer)")
    px = 0.1 * wpntxt_scale() * size * MPU
    og, sc = grid()
    sy, sz = sc[1], sc[2]

    def gx(k):
        return og[0] + sc[0] * k

    def gy(k):
        return og[1] + sy * k

    def gz(k):
        return og[2] + sz * k

    # The design in grid steps.
    k_face = round((FACE_X - og[0]) / sc[0])
    k_front = k_face - max(1, round(PROUD * 2 * oh * px / sc[0]))
    ky = round((FACE_Y - og[1]) / sy)
    kz = round(((FACE_Z[0] + FACE_Z[1]) * 0.5 - og[2]) / sz)
    wy, wz = round(ow * px / sy), round(oh * px / sz)
    cy, cz = round(-orad * px / sy), round(-orad * px / sz)
    h = 2 * wz * sz                                      # the snapped glass height
    fy, fz = round(FRAME * h / sy), round(FRAME * h / sz)
    ocy, ocz = round(cy + fy * (math.sqrt(2) - 1)), round(cz + fz * (math.sqrt(2) - 1))
    ry, rz = wy + fy // 2, round(RIVET_V * h / sz)
    rr = max(2, round(RIVET_R * h / sy))
    yc, zc = gy(ky), gz(kz)
    x_face, x_front = gx(k_face), gx(k_front)
    x_glass = x_front + GLASS_IN * h
    if gz(kz + wz + fz) > FACE_Z[1] + 1e-3 or gz(kz - wz - fz) < FACE_Z[0] - 1e-3:
        raise SystemExit("housing_light: the bezel (%d z steps tall) is taller than the end face" % (2 * (wz + fz)))

    # The table against the snapped window: the glass must be the opening's shape and sit in it.
    off_px = (abs(wy * sy - ow * px) / px, abs(wz * sz - oh * px) / px,
              abs((cy * sy + cz * sz) * 0.5 + orad * px) / px)
    want = Vector((x_glass, yc, zc))
    have = BASE_CENTRE + Vector((0.0, -offset[0], offset[1])) * MPU + Vector((-offset[2], 0.0, 0.0)) * MPU
    d = want - BASE_CENTRE
    match_off = (-d.y / MPU, d.z / MPU, -d.x / MPU)
    print("housing_light: font pixel %.5f; window %d x %d y/z steps (half %.4f x %.4f = %.2f x %.2f px), chamfer %d/%d "
          "steps (%.4f/%.4f), frame %d/%d steps (%.4f/%.4f), outer chamfer %d/%d, rivets at %d/%d steps (%.4f, %.4f) "
          "radius %d steps (%.4f/%.4f); glass height H %.4f: window %.3fH wide, chamfer %.3fH, frame %.3fH/%.3fH, "
          "proud %.3fH, rivet radius %.3fH at %.3fH/%.3fH"
          % (px, wy, wz, wy * sy, wz * sz, wy * sy / px, wz * sz / px, cy, cz, cy * sy, cz * sz, fy, fz, fy * sy,
             fz * sz, ocy, ocz, ry, rz, ry * sy, rz * sz, rr, rr * sy, rr * sz, h, 2 * wy * sy / h,
             (cy * sy + cz * sz) * 0.5 / h, fy * sy / h, fz * sz / h, (x_face - x_front) / h,
             rr * (sy + sz) * 0.5 / h, ry * sy / h, rz * sz / h))
    print("housing_light: x face %.4f, bezel front %.4f, glass %.4f; centre y %.4f z %.4f" % (x_face, x_front, x_glass,
                                                                                         yc, zc))
    if max(off_px) > 0.3 or (want - have).length > 0.01:
        raise SystemExit("housing_light: the table disagrees with the snapped design (off %.2f %.2f %.2f px, the glass "
                         "%.4f units off); a matching entry: .radius = %.2ff, .opening = {%.2ff, %.2ff}, "
                         ".offset = {%.4ff, %.4ff, %.4ff}"
                         % (off_px + ((want - have).length,) + (-(cy * sy + cz * sz) * 0.5 / px, wy * sy / px,
                                                                 wz * sz / px) + match_off))

    ob = bpy.data.objects[MODEL]
    if ob.mode != 'OBJECT':  # (the .blend was saved in edit mode: its mesh can't be written then)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='OBJECT')
    me = ob.data
    img = bpy.data.images.get(MODEL + "_skin0")
    sw, sh = img.size

    def uv_at(tx, ty):
        return (tx / sw, 1.0 - ty / sh)

    def place(ku, kv, x):
        """Grid steps right (u: -Y) and up from the centre."""
        return Vector((x, gy(ky - ku), gz(kz + kv)))

    verts, tris = [], []   # tris: (i, j, k, kind, centre, mode)

    def add(co):
        verts.append(co)
        return len(verts) - 1

    def ring(hw, hh, cw, ch, x):
        return [add(place(u, v, x)) for u, v in outline(hw, hh, cw, ch)]

    outer_back = ring(wy + fy, wz + fz, ocy, ocz, x_face)
    outer_front = ring(wy + fy, wz + fz, ocy, ocz, x_front)
    inner_front = ring(wy, wz, cy, cz, x_front)
    inner_back = ring(wy, wz, cy, cz, x_face)
    centre = place(0, 0, x_front)
    for a, b, kind, mode in ((outer_back, outer_front, "out", "outer"), (outer_front, inner_front, "back", "front"),
                             (inner_front, inner_back, "in", "inner"), (inner_back, outer_back, "fwd", "inner")):
        for i in range(8):
            j = (i + 1) % 8
            tris.append((a[i], a[j], b[j], kind, centre, mode))
            tris.append((a[i], b[j], b[i], kind, centre, mode))
    half = max(1, rr // 2)
    ring_pts = [(rr, -half), (rr, half), (half, rr), (-half, rr), (-rr, half), (-rr, -half), (-half, -rr), (half, -rr)]
    rivets = [(su * ry, sv * rz) for su in (-1, 1) for sv in (-1, 1)]
    for ru, rv in rivets:
        base = [add(place(ru + du, rv + dv, x_front)) for du, dv in ring_pts]
        tip = add(place(ru, rv, gx(k_front - 1)))
        rc = place(ru, rv, x_front)
        for i in range(8):
            tris.append((base[i], base[(i + 1) % 8], tip, "outback", rc, "rivet"))

    lo_u, hi_u = -(wy + fy) * sy, (wy + fy) * sy
    lo_v, hi_v = -(wz + fz) * sz, (wz + fz) * sz

    def tex(vi, mode, nrm):
        co = verts[vi]
        u, v = -(co.y - yc), co.z - zc
        if mode == "front":
            x0, y0, x1, y1 = FRONT_PATCH
            return x0 + (u - lo_u) / (hi_u - lo_u) * (x1 - x0), y0 + (hi_v - v) / (hi_v - lo_v) * (y1 - y0)
        if mode in ("outer", "inner"):
            along = (u - lo_u) / (hi_u - lo_u) if abs(nrm.z) >= abs(nrm.y) else (hi_v - v) / (hi_v - lo_v)
            rows = OUTER_ROWS if mode == "outer" else INNER_ROWS
            return (WALL_COLS[0] + (WALL_COLS[1] - WALL_COLS[0]) * along,
                    rows[0] if abs(co.x - x_front) < 1e-4 else rows[1])
        # rivet: the tip on the fitting's lit centre, the base ring round it
        for ru, rv in rivets:
            du, dv = u - ru * sy, v - rv * sz
            dist = math.hypot(du, dv)
            if dist < (rr + 1) * sy:
                if dist < 1e-5:
                    return RIVET_TEX[0], RIVET_TEX[1]
                return RIVET_TEX[0] + RIVET_TEX[2] * du / dist, RIVET_TEX[1] - RIVET_TEX[2] * dv / dist
        return RIVET_TEX[0], RIVET_TEX[1]

    def oriented(t):
        i, j, k, kind, c, _ = t
        p0, p1, p2 = verts[i], verts[j], verts[k]
        nrm = (p1 - p0).cross(p2 - p0)
        mid = (p0 + p1 + p2) / 3.0
        radial = Vector((0.0, mid.y - c.y, mid.z - c.z))
        if radial.length > 1e-6:
            radial.normalize()
        want_n = {"back": Vector((-1, 0, 0)), "fwd": Vector((1, 0, 0)), "out": radial, "in": -radial,
                  "outback": radial + Vector((-1, 0, 0))}[kind]
        return ((i, j, k) if nrm.dot(want_n) >= 0.0 else (i, k, j)), (nrm if nrm.dot(want_n) >= 0.0 else -nrm)

    print("housing_light: %d vertices, %d triangles" % (len(verts), len(tris)))
    if dry:
        print("housing_light: ok, --dry: nothing changed")
        return

    def apply(me):
        """Adds the housing or, when the mesh has it with this shape, moves it and maps it again. True: moved."""
        bm = bmesh.new()
        bm.from_mesh(me)
        lay_house = bm.verts.layers.int.get("qvr_housing") or bm.verts.layers.int.new("qvr_housing")
        lay_vert = bm.verts.layers.int.get("qvr_vert")
        lay_tri = bm.faces.layers.int.get("qvr_tri")
        lay_part = bm.faces.layers.int.get("qvr_part")
        lay_vid = bm.loops.layers.int.get("qvr_vid")
        lay_uv = bm.loops.layers.uv.active
        refs = [l for n, l in bm.verts.layers.float_vector.items() if n.startswith("qvr_ref")]
        shapes = list(bm.verts.layers.shape.values())
        old = [v for v in bm.verts if v[lay_house] == 1]
        reuse = len(old) == len(verts)
        if old and not reuse:
            bmesh.ops.delete(bm, geom=old, context='VERTS')
            old = []
        if reuse:
            old.sort(key=lambda v: v.index)
            for v, co in zip(old, verts):
                v.co = co  # (the frames follow frame 0: carry_frames and the export carry the move into them)
            oldset = set(old)
            faces = [f for f in bm.faces if all(v in oldset for v in f.verts)]
            if len(faces) != len(tris):
                raise SystemExit("housing_light: the old housing has %d faces, not %d" % (len(faces), len(tris)))
            lookup = {v: i for i, v in enumerate(old)}
            by_ids = {frozenset(t[:3]): t for t in tris}
            for f in faces:
                t = by_ids.get(frozenset(lookup[v] for v in f.verts))
                if t is None:
                    raise SystemExit("housing_light: the old housing's faces differ; delete it and run again")
                _, nrm = oriented(t)
                for loop in f.loops:
                    loop[lay_uv].uv = uv_at(*tex(lookup[loop.vert], t[5], nrm))
        else:
            bv = []
            for co in verts:
                v = bm.verts.new(co)
                v[lay_house] = 1
                if lay_vert is not None:
                    v[lay_vert] = -1
                for sl in shapes:
                    v[sl] = co
                for r in refs:
                    v[r] = co
                bv.append(v)
            for t in tris:
                (i, j, k), nrm = oriented(t)
                f = bm.faces.new((bv[i], bv[j], bv[k]))
                f.material_index = 0
                f.smooth = True
                if lay_tri is not None:
                    f[lay_tri] = -1
                if lay_part is not None:
                    f[lay_part] = 0
                for loop in f.loops:
                    if lay_vid is not None:
                        loop[lay_vid] = -1
                    loop[lay_uv].uv = uv_at(*tex(bv.index(loop.vert), t[5], nrm))
        bm.to_mesh(me)
        me.update()
        bm.free()
        return reuse

    sys.path.insert(0, os.path.join(HERE, "blender", "addons"))
    from quakevr_models import mdl_blender, source

    def export():
        try:
            written, _ = source.export(bpy.context, ROOT, None)
        except source.SourceError as e:
            raise SystemExit("housing_light: the export REFUSED:\n  " + str(e).replace("\n", "\n  "))
        return len(written)

    moved = apply(me)
    if not moved:
        export()  # (the housing's vertices are now the file's)
    # The frames: the housing carried along with the end face as the export does it (written into the shape keys, so
    # the .blend shows it in every frame).
    mdl_blender.carry_frames(ob)
    files = export()
    bpy.context.preferences.filepaths.save_version = 0  # (no .blend1 beside it)
    bpy.ops.wm.save_as_mainfile(filepath=save, compress=True)
    print("housing_light: ok, %d vertices, %d triangles (%s; opening %.2f x %.2f px, chamfer %.2f px); exported %d "
          "files, saved %s" % (len(verts), len(tris), "moved" if moved else "new", ow, oh, -orad, files, save))


if __name__ == "__main__":
    main()
