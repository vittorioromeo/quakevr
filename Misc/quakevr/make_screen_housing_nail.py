#!/usr/bin/env python3
# make_screen_housing_nail.py -- the nailgun's ammo screen housing, modelled on v_nail.blend (ROUND24.md, "Ammo screens
# as part of the gun"; his mockup kit/scratch/mockups/nail_housing.webp: the screen in a bevelled bezel on the receiver's
# rear, between the two hex barrels, standing on the receiver's top in front of the block with the red light).
#
# Usage: blender -b --factory-startup Misc/quakevr/src_models/blend/v_nail/v_nail.blend
#            --python Misc/quakevr/make_screen_housing_nail.py -- [--dry] [--save other.blend]
# Builds or moves the housing, exports the model as blend_export.py does (quakevr/progs) and saves the .blend (in place,
# or as --save) as the export left it. `--dry` builds it in memory and prints the numbers and checks only. Ends with
# "housing_nail: ok, ...".
#
# One definition: the window's half size and its chamfer (`opening`, `radius` < 0, in font pixels) and the glass's
# place (`offset`) are read from the "nail" entry of Quake/vr/vr_screenstyle.cpp, the table the engine shapes the glass
# by, and weapons.txt's slot 3 (wpntxt_av, wpntxt_x/y/z, wpntxt_scale). The bezel is built round that window; the run
# refuses when the glass would not sit in it (centred, GLASS_RECESS behind the bezel's front) and prints the offset that
# would. Change the table and run this again.
#
# Re-runnable: the housing's vertices carry the point attribute `qvr_housing`; with the same shape (as many vertices)
# they are moved and re-mapped in place, so the model's file keeps its vertices; otherwise the old housing is deleted and
# built anew (its old vertices then stay in the .mdl unused: start from the .blend before the housing, in git history,
# to keep the file lean).
#
# The build spec (his, in visible glass heights H): the window H x H, its corners cut 0.20 at 45 degrees; the bezel 0.18
# each side round it, standing 0.18 proud of the gun; the glass 0.08 behind the bezel's front; four round, slightly domed
# fasteners (radius 0.14), two each side of the window; no grille or plate. Materials from the gun's own skin: the
# bezel's front dark rusted metal, its bevels (outer edge, the window's lip) the worn orange-brown edge metal, the walls
# into the window dark, the fasteners the silver fittings with a bronze foot.
#
# The grid: v_nail's vertices are bytes, x 0.1562 a step (coarse), y 0.0465, z 0.0509. Every depth is a whole x step:
# the bezel's front 1 step (0.156) in front of the block's rear face (x 0.997), the window's walls a 45-degree bevel from
# the front down to the block's face (it is the window's back: no floor of its own), the glass halfway down them, the
# fasteners' heads 1 step proud of the front. Every y and z is snapped to the grid too, so the .blend is what the file
# holds; the run prints what the spec rounded to.
#
# The shape, in the plane of the block's rear face (u right = model -Y, v up = model +Z, seen from behind the gun): the
# front's outline (a chamfered rectangle, wide enough for the fasteners beside the window, its bottom a little under
# the receiver's top so it stands on it); an edge band one grid step wide round it and its walls back into the block
# (worn edge metal); the flat front; the window's lip bevelling down to the block's face.

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
STEM = "nail"
MODEL = "v_nail"
SLOT = 3
TAG = "housing_nail"

# Measured with vr_debug_screen_where (Debug > Reports > Ammo Screen on Model): a font pixel is 0.186 model units per
# unit of wpntxt_scale (0.0372 at 0.2, style size 1); weapons.txt's wpntxt_x/y/z move the glass along model +X/+Y/+Z,
# K_WPN model units a unit; the table's offset (right, up, out) along model -Y, +Z, -X, K_TAB model units a unit.
PX_PER_SCALE = 0.186
K_WPN = 1.786
K_TAB = 1.86

# The block the housing stands against (its rear face, model units) and the receiver's top under the housing.
X_FACE = 0.9968
BLOCK_Y = (-0.826, 0.893)
BLOCK_Z = (3.994, 5.367)
TOP_Z = 4.32

# The spec, in visible glass heights.
FRAME = 0.18
PROUD = 0.18
GLASS_RECESS = 0.08
CHAMFER = 0.20
FASTENER_R = 0.14
# The layout round it (glass heights): the fasteners' centres this far above and below the window's middle, their
# edge this far outside the bezel's frame, the front this far outside them; the front's top this many z steps under
# the block's top (the block shows above it, as in the mockup).
FASTENER_V = 0.27
FASTENER_GAP = 0.06
SIDE_MARGIN = 0.10
TOP_GAP_STEPS = 1
FASTENER_SIDES = 8
FASTENER_TOP = 0.55  # the head's top ring, a share of its radius (the dome's shoulder)

# Skin places (texels, top-left origin; v_nail_skin0 is 512 x 210). The front: the brown rusted metal at 300..324 x
# 163..177, no black texel in it (centre, texels per model unit). The edge band and the walls: the worn edge rim along the top of the framed panels (x 150..245, row 80-81,
# darker from row 82); the window's lip from that rim (at the front) to the panel's dark inside (at the block's face).
# The fasteners: a bronze-brown foot, a silver head lit at its top.
FRONT = (312.0, 170.0, 14.0)
RIM_SPAN = (151.0, 244.0)
ROW_RIM, ROW_RIM_IN, ROW_WALL_BACK, ROW_DARK = 80.3, 81.7, 83.5, 94.0  # (the lip's back a step inside the block)
FASTENER_FOOT = (404.0, 179.0)
FASTENER_HEAD = (246.0, 133.0)
FASTENER_TIP = (246.0, 128.0)


def read_style():
    """The "nail" entry's opening (half width, half height), radius, size and offset (right, up, out)."""
    src = open(os.path.join(ROOT, "Quake", "vr", "vr_screenstyle.cpp"), encoding="utf-8").read()
    m = re.search(r'\{"%s",(.*?)\n\s*(?:\{"|\};)' % STEM, src, re.S)  # (up to the next entry or the table's end)
    if not m:
        raise SystemExit("%s: no \"%s\" entry in vr_screenstyle.cpp" % (TAG, STEM))
    body = m.group(1)
    num = r"(-?[0-9.]+)f?"
    op = re.search(r"\.opening\s*=\s*\{\s*%s\s*,\s*%s\s*\}" % (num, num), body)
    off = re.search(r"\.offset\s*=\s*\{\s*%s\s*,\s*%s\s*,\s*%s\s*\}" % (num, num, num), body)
    rad = re.search(r"\.radius\s*=\s*" + num, body)
    size = re.search(r"\.size\s*=\s*" + num, body)
    if not (op and rad and off and size):
        raise SystemExit("%s: the \"%s\" entry needs .radius, .size, .offset and .opening" % (TAG, STEM))
    return ((float(op.group(1)), float(op.group(2))), float(rad.group(1)), float(size.group(1)),
            tuple(float(off.group(i)) for i in (1, 2, 3)))


def read_weapon():
    """weapons.txt's slot: wpntxt_av, wpntxt_x/y/z, wpntxt_scale and the pitch/yaw/roll (all 0: the glass upright)."""
    txt = open(os.path.join(ROOT, "quakevr", "data", "weapons.txt"), encoding="utf-8").read()
    m = re.search(r"^weapon %d\s*\{(.*?)^\}" % SLOT, txt, re.S | re.M)
    if not m:
        raise SystemExit("%s: no weapon %d in weapons.txt" % (TAG, SLOT))

    def val(key):
        k = re.search(r'^\s*%s "([^"]*)"' % key, m.group(1), re.M)
        if not k:
            raise SystemExit("%s: weapon %d has no %s" % (TAG, SLOT, key))
        return k.group(1)

    av = Vector([float(a) for a in val("wpntxt_av").split()])
    off = Vector((float(val("wpntxt_x")), float(val("wpntxt_y")), float(val("wpntxt_z"))))
    turned = [float(val("wpntxt_" + k)) for k in ("pitch", "yaw", "roll")]
    if any(abs(a) > 1e-3 for a in turned):
        raise SystemExit("%s: wpntxt_pitch/yaw/roll must be 0 (the glass upright, facing -X): %s" % (TAG, turned))
    return av, off, float(val("wpntxt_scale"))


def grid():
    """The model file's grid: origin and step per axis."""
    d = open(os.path.join(ROOT, "quakevr", "progs", MODEL + ".mdl"), "rb").read()
    return Vector(struct.unpack_from("<3f", d, 20)), Vector(struct.unpack_from("<3f", d, 8))


def outline(hw, hh, cu, cv):
    """A chamfered rectangle's 8 points (corner cuts cu along u, cv along v), counter-clockwise from the right side's
    top: the same count and order for every size, so rings join point to point."""
    return [(hw, hh - cv), (hw - cu, hh), (-hw + cu, hh), (-hw, hh - cv),
            (-hw, -hh + cv), (-hw + cu, -hh), (hw - cu, -hh), (hw, -hh + cv)]


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    dry = "--dry" in argv
    save = argv[argv.index("--save") + 1] if "--save" in argv else bpy.data.filepath

    (op_w, op_h), radius, size, offset = read_style()
    if radius >= 0.0:
        raise SystemExit("%s: the \"%s\" entry's radius must be < 0 (a chamfer), not %g" % (TAG, STEM, radius))
    av, wofs, wscale = read_weapon()
    org, stp = grid()
    px = PX_PER_SCALE * wscale * size

    def sy(y):
        return org.y + round((y - org.y) / stp.y) * stp.y

    def sz(z):
        return org.z + round((z - org.z) / stp.z) * stp.z

    def gx(k):
        return org.x + stp.x * k

    k_face = round((X_FACE - org.x) / stp.x)
    x_face = gx(k_face)        # the block's rear face: the window's back
    x_front = gx(k_face - 1)   # the bezel's front, one step proud
    x_head = gx(k_face - 2)    # the fasteners' heads, one step proud of the front
    x_in = gx(k_face + 1)      # the walls end inside the block

    # The window, snapped: its centre on the block's middle (y) and in the front's middle (z); the front stands on the
    # receiver's top (its bottom the grid line just under it) and its top TOP_GAP_STEPS under the block's.
    cy = sy((BLOCK_Y[0] + BLOCK_Y[1]) * 0.5)
    if abs(cy - (BLOCK_Y[0] + BLOCK_Y[1]) * 0.5) > stp.y * 0.26:  # (on a half step: the edges land on whole ones)
        cy = cy + stp.y * 0.5 if cy < (BLOCK_Y[0] + BLOCK_Y[1]) * 0.5 else cy - stp.y * 0.5
    k_bot = math.floor((TOP_Z - org.z) / stp.z)
    k_top = round((BLOCK_Z[1] - org.z) / stp.z) - TOP_GAP_STEPS
    z_bot, z_top = org.z + k_bot * stp.z, org.z + k_top * stp.z
    cz = (z_bot + z_top) * 0.5
    hw = sy(cy + op_w * px) - cy           # the window's half width and height (model units, on the grid)
    hh = sz(cz + op_h * px) - cz
    H = 2.0 * hh                           # a visible glass height
    cut = -radius * px
    cu, cv = sy(cy + cut) - cy, sz(cz + cut) - cz
    recess = GLASS_RECESS * H

    # The front's outline: the fasteners beside the window, past the bezel's frame.
    fr = FASTENER_R * H  # (its points are snapped one by one)
    fu = sy(cy + hw + FRAME * H + FASTENER_GAP * H + FASTENER_R * H) - cy
    fv = sz(cz + FASTENER_V * H) - cz
    ow = sy(cy + fu + fr + SIDE_MARGIN * H) - cy
    oh = (z_top - z_bot) * 0.5
    if cy + ow > BLOCK_Y[1] + 1e-3 or cy - ow < BLOCK_Y[0] - 1e-3:
        raise SystemExit("%s: the front (half width %.3f) is wider than the block" % (TAG, ow))
    co_u, co_v = sy(cy + cu + (oh - hh) * 0.4142) - cy, sz(cz + cv + (oh - hh) * 0.4142) - cz
    band_u, band_v = stp.y, stp.z

    def ring_lip(d):
        """The window grown by d (shrunk if negative), its cut grown by d * 0.41, snapped."""
        return outline(sy(cy + hw + d) - cy, sz(cz + hh + d) - cz,
                       max(sy(cy + cu + d * 0.4142) - cy, stp.y), max(sz(cz + cv + d * 0.4142) - cz, stp.z))

    # The lip: from the front (the window grown by the recess) down 45 degrees through the block's face (the window's
    # back) to a step inside the block, where its open end is hidden; at the glass's depth the walls are on the window's
    # edge (snapped: within half a grid step; the glass tucks under them).
    lip = ring_lip(recess)
    back = ring_lip(recess - (x_in - x_front))
    rings = {
        "inner": (outline(ow, oh, co_u, co_v), x_in),
        "outer": (outline(ow, oh, co_u, co_v), x_front),
        "band": (outline(ow - band_u, oh - band_v, max(co_u - band_u * 0.4142, stp.y), max(co_v - band_v * 0.4142,
                                                                                            stp.z)), x_front),
        "lip": (lip, x_front),
        "back": (back, x_in),
    }

    ob = bpy.data.objects[MODEL]
    me = ob.data
    img = bpy.data.images.get(MODEL + "_skin0")
    sw, sh = img.size

    def uv_at(tx, ty):
        return (tx / sw, 1.0 - ty / sh)

    def place(u, v, x):
        return Vector((x, cy - u, cz + v))

    verts = []   # positions
    tris = []    # (i, j, k, expected normal kind, centre, uv mode)
    ring_of = {}  # vertex -> (ring name, place along it)

    def add(co):
        verts.append(Vector((co.x, sy(co.y), sz(co.z))))  # (the file's bytes: the fasteners' circles too)
        return len(verts) - 1

    ring_idx = {}
    for name, (pts, x) in rings.items():
        ring_idx[name] = [add(place(u, v, x)) for u, v in pts]
        for i, vi in enumerate(ring_idx[name]):
            ring_of[vi] = (name, i)
    m = 8
    centre = place(0.0, 0.0, x_front)
    for a_name, b_name, kind, mode in (("inner", "outer", "out", "rim"), ("outer", "band", "back", "rim"),
                                       ("band", "lip", "back", "front"), ("lip", "back", "inback", "lip")):
        a, b = ring_idx[a_name], ring_idx[b_name]
        for i in range(m):
            j = (i + 1) % m
            tris.append((a[i], a[j], b[j], kind, centre, mode))
            tris.append((a[i], b[j], b[i], kind, centre, mode))
    # The lip's end inside the block, closed (no crack for the holes check; nothing sees it).
    c_end = add(place(0.0, 0.0, x_in))
    k = ring_idx["back"]
    for i in range(m):
        tris.append((k[i], k[(i + 1) % m], c_end, "back", centre, "dark"))
    fasteners = [(su * fu, sv * fv) for su in (-1.0, 1.0) for sv in (1.0, -1.0)]
    for bu, bv in fasteners:
        base = [add(place(bu + fr * math.cos(2 * math.pi * (i + 0.5) / FASTENER_SIDES),
                          bv + fr * math.sin(2 * math.pi * (i + 0.5) / FASTENER_SIDES), x_front))
                for i in range(FASTENER_SIDES)]
        top = [add(place(bu + FASTENER_TOP * fr * math.cos(2 * math.pi * (i + 0.5) / FASTENER_SIDES),
                         bv + FASTENER_TOP * fr * math.sin(2 * math.pi * (i + 0.5) / FASTENER_SIDES), x_head))
               for i in range(FASTENER_SIDES)]
        tip = add(place(bu, bv, x_head))
        bc = place(bu, bv, x_head)
        for i in range(FASTENER_SIDES):
            j = (i + 1) % FASTENER_SIDES
            tris.append((base[i], base[j], top[j], "outback", bc, "fastener"))
            tris.append((base[i], top[j], top[i], "outback", bc, "fastener"))
            tris.append((top[i], top[j], tip, "back", bc, "fastener"))

    def wraps(ids):
        return any(vi in ring_of and ring_of[vi][1] == m - 1 for vi in ids)

    def perimeter_share(name, i, wrap):
        """How far round its ring a ring point is (0..1), by length (rings with different corner cuts line up)."""
        pts = rings[name][0]
        lens = [math.dist(pts[k], pts[(k + 1) % m]) for k in range(m)]
        if wrap and i == 0:
            i = m
        return sum(lens[:i]) / sum(lens)

    def tex_of(vi, mode, wrap=False):
        co = verts[vi]
        u, v = -(co.y - cy), co.z - cz
        if mode == "front":
            return FRONT[0] + u * FRONT[2], FRONT[1] - v * FRONT[2]
        if mode == "dark":
            return (RIM_SPAN[0] + RIM_SPAN[1]) * 0.5 + u * 8.0, ROW_DARK - v * 8.0
        if mode in ("rim", "lip"):
            name, i = ring_of[vi]
            col = RIM_SPAN[0] + (RIM_SPAN[1] - RIM_SPAN[0]) * perimeter_share(name, i, wrap)
            row = {"inner": ROW_WALL_BACK, "outer": ROW_RIM, "band": ROW_RIM_IN,
                   "lip": ROW_RIM, "back": ROW_DARK}[name]
            return col, row
        for bu, bv in fasteners:  # fastener: foot ring bronze, head ring and tip silver
            du, dv = u - bu, v - bv
            rr = math.hypot(du, dv)
            if rr < fr + 1e-3:
                if rr < 1e-4:
                    return FASTENER_TIP
                if abs(co.x - x_front) < 1e-4:
                    return FASTENER_FOOT[0] + 2.0 * du / rr, FASTENER_FOOT[1] + 2.0 * dv / rr
                return FASTENER_HEAD[0] + 1.5 * du / rr, FASTENER_HEAD[1] - 1.5 * dv / rr
        return FASTENER_TIP

    def oriented(t):
        i, j, k, kind, c, _ = t
        p0, p1, p2 = verts[i], verts[j], verts[k]
        nrm = (p1 - p0).cross(p2 - p0)
        mid = (p0 + p1 + p2) / 3.0
        radial = Vector((0.0, mid.y - c.y, mid.z - c.z))
        if radial.length > 1e-6:
            radial.normalize()
        back_dir = Vector((-1, 0, 0))
        want = {"back": back_dir, "out": radial, "inback": back_dir - radial, "outback": radial + back_dir}[kind]
        return (i, j, k) if nrm.dot(want) >= 0.0 else (i, k, j)

    def apply(me):
        """Adds the housing or, when the mesh has one of this shape, moves it into place and maps it again."""
        bm = bmesh.new()
        bm.from_mesh(me)
        lay_house = bm.verts.layers.int.get("qvr_housing") or bm.verts.layers.int.new("qvr_housing")
        lay_vert = bm.verts.layers.int.get("qvr_vert")
        lay_tri = bm.faces.layers.int.get("qvr_tri")
        lay_vid = bm.loops.layers.int.get("qvr_vid")
        lay_uv = bm.loops.layers.uv.active
        shapes = list(bm.verts.layers.shape.values())
        old = [v for v in bm.verts if v[lay_house] == 1]
        reuse = len(old) == len(verts)
        if old and not reuse:
            print("%s: the old housing has %d vertices, not %d: deleted, built anew" % (TAG, len(old), len(verts)))
            bmesh.ops.delete(bm, geom=old, context='VERTS')
            old = []
        if reuse:
            old.sort(key=lambda v: v.index)
            for v, co in zip(old, verts):
                v.co = co  # (the frames follow frame 0: the export carries the move into them)
            oldset = set(old)
            faces = [f for f in bm.faces if all(v in oldset for v in f.verts)]
            if len(faces) != len(tris):
                raise SystemExit("%s: the old housing has %d faces, not %d" % (TAG, len(faces), len(tris)))
            lookup = {v: i for i, v in enumerate(old)}
            mode_of = {frozenset(t[:3]): t[5] for t in tris}
            for f in faces:
                ids = [lookup[v] for v in f.verts]
                mode = mode_of.get(frozenset(ids), "front")
                for loop in f.loops:
                    loop[lay_uv].uv = uv_at(*tex_of(lookup[loop.vert], mode, wraps(ids)))
        else:
            bv = []
            for co in verts:
                v = bm.verts.new(co)
                v[lay_house] = 1
                if lay_vert is not None:
                    v[lay_vert] = -1
                for sl in shapes:
                    v[sl] = co
                bv.append(v)
            for t in tris:
                i, j, k = oriented(t)
                f = bm.faces.new((bv[i], bv[j], bv[k]))
                f.material_index = 0
                f.smooth = True
                if lay_tri is not None:
                    f[lay_tri] = -1
                for loop in f.loops:
                    if lay_vid is not None:
                        loop[lay_vid] = -1
                    loop[lay_uv].uv = uv_at(*tex_of(bv.index(loop.vert), t[5], wraps(t[:3])))
        bm.to_mesh(me)
        me.update()
        bm.free()
        return reuse

    # The glass: where weapons.txt and the table's offset put it, against the window's centre at the glass's depth.
    glass = av + wofs * K_WPN + Vector((-offset[2], -offset[0], offset[1])) * K_TAB
    want = Vector((x_front + recess, cy, cz))
    miss = glass - want
    need = (-(want.y - (av + wofs * K_WPN).y) / K_TAB, (want.z - (av + wofs * K_WPN).z) / K_TAB,
            -(want.x - (av + wofs * K_WPN).x) / K_TAB)
    lines = [
        "grid steps x %.4f y %.4f z %.4f; H (the visible glass height) %.4f model units" % (stp.x, stp.y, stp.z, H),
        "window %.4f x %.4f (spec 1.00 x 1.00 H: %.3f x %.3f), corner cut %.4f / %.4f (spec 0.20: %.3f / %.3f)"
        % (2 * hw, 2 * hh, 2 * hw / H, 2 * hh / H, cu, cv, cu / H, cv / H),
        "bezel proud %.4f (spec 0.18: %.3f), glass %.4f behind its front (spec 0.08: %.3f), frame to the edge "
        "%.3f top / %.3f bottom over the receiver's top / %.3f sides (spec 0.18 at least)"
        % (x_face - x_front, (x_face - x_front) / H, recess, recess / H, (oh - hh) / H,
           (oh - hh - (TOP_Z - z_bot)) / H, (fu - fr - hw) / H),
        "fasteners radius %.4f (spec 0.14: %.3f) at u +-%.4f, v +-%.4f; head %.4f proud" % (fr, fr / H, fu, fv,
                                                                                         x_front - x_head),
        "front %.4f x %.4f, x %.4f (front) / %.4f (window's back, the block's face) / %.4f (heads)"
        % (2 * ow, 2 * oh, x_front, x_face, x_head),
        "glass centre %.4f %.4f %.4f (want %.4f %.4f %.4f)" % (*glass, *want),
    ]
    for l in lines:
        print("%s: %s" % (TAG, l))
    if miss.length > 0.01:
        msg = "%s: the glass is %.3f off the window: the table's offset should be {%.4ff, %.4ff, %.4ff}" % (
            TAG, miss.length, *need)
        if not dry:
            raise SystemExit(msg)
        print(msg)

    # No fullbright texel under the housing's UVs (the skin's own colours, exactly palette fullbrights glow).
    sys.path.insert(0, os.path.join(HERE, "blender", "addons"))
    from quakevr_models import mdl_blender, qpal, source
    pix = list(img.pixels)
    glow = 0
    for t in tris:
        tx = [tex_of(i, t[5], wraps(t[:3])) for i in t[:3]]
        for x in range(int(min(p[0] for p in tx)), int(max(p[0] for p in tx)) + 1):
            for y in range(int(min(p[1] for p in tx)), int(max(p[1] for p in tx)) + 1):
                o = ((sh - 1 - y) * sw + x) * 4
                rgb = tuple(int(round(c * 255)) for c in pix[o:o + 3])
                glow += rgb in qpal.EXACT_FULLBRIGHT
    if glow:
        raise SystemExit("%s: %d fullbright texels under the housing's UVs" % (TAG, glow))

    moved = apply(me)
    if dry:
        print("%s: --dry, %d vertices, %d triangles; not exported or saved" % (TAG, len(verts), len(tris)))
        return

    def export():
        try:
            written, _ = source.export(bpy.context, ROOT, None)
        except source.SourceError as e:
            raise SystemExit("%s: the export REFUSED:\n  %s" % (TAG, str(e).replace("\n", "\n  ")))
        return len(written)

    files = export()
    mdl_blender.carry_frames(ob)  # (the .blend's frames as the file has them)

    # Every frame: the housing moves as the block's rear face does (the export carries a new vertex with its nearest
    # old one).
    hs = me.attributes["qvr_housing"]
    flags = [0] * len(me.vertices)
    hs.data.foreach_get("value", flags)
    mine = [i for i, f in enumerate(flags) if f]
    corners = [v.index for v in me.vertices if not flags[v.index] and abs(v.co.x - X_FACE) < 0.01
               and BLOCK_Z[0] - 0.01 < v.co.z < BLOCK_Z[1] + 0.01 and BLOCK_Y[0] - 0.01 < v.co.y < BLOCK_Y[1] + 0.01]
    worst = 0.0
    keys = me.shape_keys.key_blocks if me.shape_keys else []
    for kb in keys[1:]:
        ref = [kb.data[i].co - me.vertices[i].co for i in corners]
        for i in mine:
            d = kb.data[i].co - me.vertices[i].co
            worst = max(worst, min((d - r).length for r in ref))
    bpy.context.preferences.filepaths.save_version = 0  # (no .blend1 beside it)
    bpy.ops.wm.save_as_mainfile(filepath=save, compress=True)
    print("%s: ok, %d vertices, %d triangles (%s; opening %.2f x %.2f px, radius %.2f; in %d frames the housing "
          "follows the block's face within %.4f); exported %d files, saved %s"
          % (TAG, len(verts), len(tris), "moved in place" if moved else "built", op_w, op_h, radius, len(keys) - 1,
             worst, files, save))


if __name__ == "__main__":
    main()
