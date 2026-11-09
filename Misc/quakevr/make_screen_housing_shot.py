#!/usr/bin/env python3
# make_screen_housing_shot.py -- the shotgun's ammo screen housing, modelled on v_shot.blend (ROUND24.md, "Ammo screens
# as part of the gun"; his mockup: a bevelled metal bezel round the screen with four corner bolts on the receiver's
# end face, the glass set in).
#
# Usage, from the repository root (the options after the separator: [--dry] [--save other.blend]):
#   Git Bash:   "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup \
#                   Misc/quakevr/src_models/blend/v_shot/v_shot.blend \
#                   --python Misc/quakevr/make_screen_housing_shot.py -- --dry
#   PowerShell: & "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup `
#                   Misc\quakevr\src_models\blend\v_shot\v_shot.blend `
#                   --python Misc\quakevr\make_screen_housing_shot.py '--' --dry
#   (PowerShell eats a bare --: quote it. Leave out `-- --dry` to export and save.)
# Builds or moves the housing, exports the model as blend_export.py does (quakevr/progs, weapons.txt's barrel mask)
# and saves the .blend (in place, or as --save) as the export left it. Ends with "housing_shot: ok, ..." or
# "housing_shot: REFUSED: ..." (exit code 1). --dry: builds and checks it, exports and saves nothing.
# A new housing is built a grid step back and exported, then moved into place and exported again: new vertices follow
# the face in every frame and can't be held inside the box at the recoil's peak until the file has them (main's end).
# The three housing scripts work alike (housing_common.py; docs/vr-port/MODELS_IN_BLENDER.md, "Tweaking the ammo
# screen housings").
#
# One definition: the opening's half size and corner (`opening`, `radius`, in font pixels) are read from the "shot"
# entry of Quake/vr/vr_screenstyle.cpp, the table the engine shapes the glass by, so the bezel's opening and the glass
# are one shape. Change them there and run this again.
#
# Run again on a .blend that has the housing (its vertices carry the point attribute `qvr_housing`): with the same
# shape (as many vertices) its vertices are moved and re-mapped in place, so the model's file keeps its vertices;
# otherwise the old housing is deleted and built anew (its old vertices then stay in the .mdl unused: start from the
# .blend before the housing, in git history, to keep the file lean).
#
# The shape, in the plane of the end face's screen (u right = model -Y, v up = model +Z, the glass upright):
# - the bezel: a rounded frame round the opening, its front flat (`FRAME` wide), a 45-degree bevel outside it, its
#   walls going back into the gun; inside, the opening's walls go down one grid step to a dark back behind the glass;
# - four bolts beside it on the end face (the face is a trapezoid, wider at the top: the top pair further out).
# The model's X is coarse (the file's grid: 0.22 units a step), so every depth is a whole number of steps: the
# bezel's front DEPTH_STEPS in front of the end face's bottom edge (the face leans back 5.9 degrees: a step forward at
# its top, so at 1 step the front clears it at the bezel's top by 0.036), the bevel's foot and the opening's back 1
# step behind the front (at 1 step: on the bottom edge's step, behind the face over the opening: the face is the
# opening's back there and the dark back is hidden), the bolts' heads 1 step proud of their bases (at most flush with
# the front). The glass is upright (weapons.txt pitch) at the table's offset, between the bezel's front and the face
# at the glass's top edge (the script refuses otherwise); the opening's walls slope in from the lip to WALL_GRIP inside
# the glass at its depth, so no gap shows round it.
# Texture, all from the gun's own skin: the bezel's profile mapped across the shaded metal tube strip (x 104..146, rows
# 54..70) and its length along it, as if the tube were bent round the opening (darker at the bevel's foot, lit at the
# lip, dark down the opening's walls and on its back); the walls into the gun on the blue-grey steel plate (52..78 x
# 54..70); the bolts on the tube's dark rows at their base, lit on the head.

import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import housing_common as hc  # noqa: E402

STEM = "shot"
MODEL = "v_shot"

# Where the screen is (vr_debug_screen_where, Debug > Reports > Ammo Screen on Model): a font pixel is 0.07102 model
# units at weapons.txt's wpntxt_scale 0.35. The housing's centre on the end face (model Y, Z) and the end face's
# bottom edge on the grid (x 1.3966, z 1.414).
PX = 0.07102
CENTRE_Y = -0.03
CENTRE_Z = 2.48
DEPTH_STEPS = 1   # the bezel's front this many grid steps (0.22 units) in front of the end face's bottom edge (2: the
#                   first housing's, twice as deep); the bevel and the opening go one step back from it. Then set the
#                   table's out as this script's REFUSED line says (it sits the glass between the front and the face)
FRAME = 0.10      # the bezel's flat front, wide (model units)
BEVEL = 0.12      # its outer bevel, wide (and one grid step deep)
LIP = 0.2         # the opening's front edge outside the glass (font pixels)
WALL_GRIP = 0.25  # the glass this far inside the opening's walls at its depth (font pixels): the walls slope to it
CORNER_PTS = 4    # points on each rounded corner
# The end face (measured on the .blend): its bottom edge on step k_face at z 1.414, its top edge a step forward at
# z 3.546 (it leans back 5.9 degrees). The glass: weapons.txt's place (model x 0.862) moved by the table's out (2.029
# model units a world unit; vr_debug_screen_where), drawn 0.04 of a character (0.32 px) further in (vr_text3d.cpp's gap).
FACE_Z = (1.414, 3.546)
GLASS0_X = 0.862
MODEL_PER_UNIT = 2.029
GLASS_GAP = 0.32 * PX
CLEAR = 0.01      # the least room (model units) between the glass or the bezel's front and the end face behind it
BOLT_SIDES = 8
# The bolts: (u, v) from the centre, the head's radius at its base and top.
BOLTS = [(-1.38, -0.68), (1.38, -0.68), (-1.50, 0.67), (1.50, 0.67)]
BOLT_R = (0.18, 0.135)
# Skin places (texels). The walls into the gun: the steel plate (its middle, texels per model unit). The bezel: the
# shaded metal tube strip (x 104..148, rows 52..70: dark at its edges, lit in the middle), its front on the lit rows
# (centre column, texels per unit, middle row, rows per unit), the bevel on the brightest row, the opening's lip
# bright going down to the dark row inside. The bolts on the same strip: dark base, lit head.
STEEL = (65.0, 62.0, 7.0)
TUBE_SPAN = (104.0, 146.0)
PROFILE_ROWS = (64.0, 54.5, 58.5, 61.5, 70.5)  # each ring's row: the walls' back, the bevel's foot, the front's
#                                                 outer edge, the opening's lip, inside the opening (and its back)
BOLT_X = 120.0
BOLT_ROWS = (52.0, 58.0, 61.0)  # base ring, top ring, top centre


def outline(hw, hh, r, n):
    """A rounded (r > 0) or chamfered (r < 0) rectangle's points, n per corner, counter-clockwise from the top right
    corner's start: the same count and order for every size, so rings join point to point."""
    pts = []
    corners = [(1, 1, 0.0), (-1, 1, 90.0), (-1, -1, 180.0), (1, -1, 270.0)]
    rr = abs(r)
    for sx, sy, a0 in corners:
        if r >= 0:
            cx, cy = sx * (hw - rr), sy * (hh - rr)
            for i in range(n):
                a = math.radians(a0 + 90.0 * i / (n - 1))
                pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
        else:
            a = (sx * hw, sy * (hh - rr)) if a0 in (0.0, 180.0) else (sx * (hw - rr), sy * hh)
            b = (sx * (hw - rr), sy * hh) if a0 in (0.0, 180.0) else (sx * hw, sy * (hh - rr))
            for i in range(n):
                t = i / (n - 1)
                pts.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
    return pts


def offset_outline(hw, hh, r, d, n):
    """The outline grown by d (shrunk if negative): a rounded one keeps its centres, a chamfer's cut grows by d * 0.41."""
    if r >= 0:
        return outline(hw + d, hh + d, max(r + d, 0.0), n)
    return outline(hw + d, hh + d, min(r - d * 0.4142, -1e-4), n)


def main(h):
    (ow, oh), orad = h.style["opening"], h.style["radius"]
    if ow <= 0 or oh <= 0:
        h.fail("the \"%s\" entry needs .opening" % STEM)
    (step, _, _), (ox, _, _) = hc.mdl_grid(MODEL)

    def gx(k):
        """The model file's X grid: x of step k."""
        return ox + step * k

    # The grid steps the housing stands on (the end face's bottom edge is x 1.3966: step k_face).
    k_face = round((1.3966 - gx(0)) / step)
    if DEPTH_STEPS < 1:
        h.fail("DEPTH_STEPS must be 1 or more")
    x_face = gx(k_face)                     # the bolts' bases, the end face's bottom edge
    x_front = gx(k_face - DEPTH_STEPS)      # the bezel's front
    x_mid = x_front + step                  # the bevel's foot, the opening's back (behind the face at 1 step)
    x_bolt = max(x_front, gx(k_face - 1))   # the bolts' heads: a step proud of the face, at most flush with the front
    x_back = gx(k_face + 1)                 # the bezel's walls end inside the gun

    def face_x(z):
        """The end face's x at height z (it leans back: a step forward at its top)."""
        return x_face - step * (z - FACE_Z[0]) / (FACE_Z[1] - FACE_Z[0])

    # The glass: upright, its top edge nearest the leaning face. It has to sit behind the bezel's front and in front
    # of the face; the opening's walls slope in from the lip to WALL_GRIP inside it at its depth.
    (ow, oh) = h.style["opening"]
    x_glass = GLASS0_X - h.style["offset"][2] * MODEL_PER_UNIT + GLASS_GAP
    x_room = min(face_x(CENTRE_Z + oh * PX), x_mid)
    if not x_front + CLEAR <= x_glass <= x_room - CLEAR:
        want = (x_front + x_room) * 0.5
        h.fail("the glass (x %.3f) is not between the bezel's front (%.3f) and the end face (%.3f): set the \"%s\" "
               "entry's offset out to %.4ff" % (x_glass, x_front, x_room, STEM,
                                               -(want - GLASS_GAP - GLASS0_X) / MODEL_PER_UNIT))
    t_glass = (x_glass - x_front) / (x_mid - x_front)
    wall_in = (LIP + WALL_GRIP) / t_glass - LIP
    if wall_in >= min(ow, oh):
        h.fail("the glass (x %.3f) too near the bezel's front for its walls: move it in" % x_glass)

    ob = bpy.data.objects[MODEL]
    me = ob.data
    img = bpy.data.images.get(MODEL + "_skin0")
    sw, sh = img.size

    def uv_at(tx, ty):
        return (tx / sw, 1.0 - ty / sh)

    def place(u, v, x):
        return Vector((x, CENTRE_Y - u, CENTRE_Z + v))

    # Rings of the bezel (u, v outlines), outside in.
    gw, gh, gr = ow * PX, oh * PX, orad * PX
    n = CORNER_PTS
    lip = offset_outline(gw, gh, gr, LIP * PX, n)
    wall = offset_outline(gw, gh, gr, -wall_in * PX, n)
    front_out = offset_outline(gw, gh, gr, LIP * PX + FRAME, n)
    foot = offset_outline(gw, gh, gr, LIP * PX + FRAME + BEVEL, n)
    top_clear = face_x(CENTRE_Z + gh + LIP * PX + FRAME) - x_front
    if top_clear < CLEAR:
        h.fail("the bezel's front is not in front of the end face at its top (%.3f): DEPTH_STEPS too small" % top_clear)
    rings = [(foot, x_back), (foot, x_mid), (front_out, x_front), (lip, x_front), (wall, x_mid)]
    # (normals expected: walls out, bevel out and back, front back, the opening's walls in)
    strips = ["out", "outback", "back", "in"]

    # The housing's vertices and triangles, built in a list first (positions, UV per corner).
    verts = []   # (co, (tx, ty) or None)
    tris = []    # (i, j, k, expected normal kind, centre for radial kinds, uv mode)

    def add(co):
        verts.append(co)
        return len(verts) - 1

    ring_idx = []
    ring_of = {}  # vertex -> (ring, place along it)
    for r, (pts, x) in enumerate(rings):
        ring_idx.append([add(place(u, v, x)) for u, v in pts])
        for i, vi in enumerate(ring_idx[-1]):
            ring_of[vi] = (r, i)
    m = len(foot)
    centre = place(0.0, 0.0, x_mid)
    for s in range(4):
        a, b = ring_idx[s], ring_idx[s + 1]
        uvk = "steel" if s == 0 else "ring"
        for i in range(m):
            j = (i + 1) % m
            tris.append((a[i], a[j], b[j], strips[s], centre, uvk))
            tris.append((a[i], b[j], b[i], strips[s], centre, uvk))
    # The opening's back: a fan behind the glass.
    c_back = add(place(0.0, 0.0, x_mid))
    w = ring_idx[4]
    for i in range(m):
        tris.append((w[i], w[(i + 1) % m], c_back, "back", centre, "dark"))
    # The bolts.
    for bu, bv in BOLTS:
        base = [add(place(bu + BOLT_R[0] * math.cos(2 * math.pi * i / BOLT_SIDES),
                          bv + BOLT_R[0] * math.sin(2 * math.pi * i / BOLT_SIDES), x_face)) for i in range(BOLT_SIDES)]
        top = [add(place(bu + BOLT_R[1] * math.cos(2 * math.pi * i / BOLT_SIDES),
                         bv + BOLT_R[1] * math.sin(2 * math.pi * i / BOLT_SIDES), x_bolt)) for i in range(BOLT_SIDES)]
        tip = add(place(bu, bv, x_bolt))
        bc = place(bu, bv, x_bolt)
        for i in range(BOLT_SIDES):
            j = (i + 1) % BOLT_SIDES
            tris.append((base[i], base[j], top[j], "out", bc, "bolt"))
            tris.append((base[i], top[j], top[i], "out", bc, "bolt"))
            tris.append((top[i], top[j], tip, "back", bc, "bolt"))

    def wraps(ids):
        """Whether a face spans the rings' seam (between place m-1 and 0)."""
        return any(vi in ring_of and ring_of[vi][1] == m - 1 for vi in ids)

    def tex_of(vi, mode, wrap=False):
        """A corner's texel. `wrap`: the face spans the rings' seam (place 0 taken as m)."""
        co = verts[vi]
        u, v = -(co.y - CENTRE_Y), co.z - CENTRE_Z
        if mode == "steel":
            return STEEL[0] + u * STEEL[2], STEEL[1] - v * STEEL[2]
        if mode == "ring":
            # The bezel's profile across the tube's rows (as if the tube were bent round the opening: its lit band
            # along the lip), its length along the tube's columns.
            r, i = ring_of[vi]
            if wrap and i == 0:
                i = m
            return TUBE_SPAN[0] + (TUBE_SPAN[1] - TUBE_SPAN[0]) * i / m, PROFILE_ROWS[r]
        if mode == "dark":
            return (TUBE_SPAN[0] + TUBE_SPAN[1]) * 0.5 + u * 8.0, PROFILE_ROWS[-1]
        # bolt: by ring (base, top, centre), around by angle
        for bi, (bu, bv) in enumerate(BOLTS):
            du, dv = u - bu, v - bv
            rr = math.hypot(du, dv)
            if rr < BOLT_R[0] + 1e-3:
                if rr < 1e-4:
                    return BOLT_X, BOLT_ROWS[2]
                row = BOLT_ROWS[0] if abs(co.x - x_face) < 1e-4 else BOLT_ROWS[1]
                return BOLT_X + 3.0 * du / rr, row
        return BOLT_X, BOLT_ROWS[2]

    def oriented(t):
        i, j, k, kind, c, _ = t
        p0, p1, p2 = verts[i], verts[j], verts[k]
        nrm = (p1 - p0).cross(p2 - p0)
        mid = (p0 + p1 + p2) / 3.0
        radial = Vector((0.0, mid.y - c.y, mid.z - c.z))
        if radial.length > 1e-6:
            radial.normalize()
        want = {"back": Vector((-1, 0, 0)), "out": radial, "in": -radial,
                "outback": radial + Vector((-1, 0, 0))}[kind]
        return (i, j, k) if nrm.dot(want) >= 0.0 else (i, k, j)

    # The triangles, wound and mapped.
    built = []
    for t in tris:
        ijk = oriented(t)
        wrap = wraps(t[:3])
        built.append(ijk + (tuple(uv_at(*tex_of(vi, t[5], wrap)) for vi in ijk),))

    hs = me.attributes.get(hc.TAG)
    old = 0
    if hs is not None:
        flags = [0] * len(me.vertices)
        hs.data.foreach_get("value", flags)
        old = sum(1 for f in flags if f > 0)
    if old != len(verts):
        # New: built a grid step back, inside the box in every frame: the export carries new vertices along with the
        # face and doesn't read their own frames, so they can't be held in the box at the recoil's peak until the
        # file has them (exported, then moved into place below).
        sunk = Vector((step, 0.0, 0.0))
        h.apply(me, [co + sunk for co in verts], built, smooth=True)
        h.export()  # (the housing's vertices are now the file's)
    h.apply(me, verts, built, smooth=True)

    # The frames: the housing carried along with the end face as the export does it, then kept inside the model's box:
    # at the recoil's peak (shot3) the receiver's top back corner is on the box's low X, and the bezel's front, standing
    # in front of it, would be past it (0.1..0.18 units). Those places are held on the box's edge in that frame (a frame
    # edited on its own: the export keeps it) rather than growing the box (every vertex of the gun moved).
    _, mdl_blender = hc.addon()
    mdl_blender.carry_frames(ob)
    hs = me.attributes[hc.TAG]
    flags = [0] * len(me.vertices)
    hs.data.foreach_get("value", flags)
    low_x = gx(0) + 1e-3
    held = 0
    for kb in me.shape_keys.key_blocks[1:]:
        for i, f in enumerate(flags):
            if f and kb.data[i].co.x < low_x:
                kb.data[i].co.x = low_x
                held += 1
    me.update()
    h.finish("%d vertices, %d triangles (opening %.1f x %.1f px, radius %.1f; depth %d: x %.3f / %.3f / %.3f, the "
             "glass at %.3f (%.3f behind the front, %.3f before the face), the bezel's top %.3f before the face, walls "
             "in %.2f px; %d frame places held in the box)" % (
                 len(verts), len(tris), ow, oh, orad, DEPTH_STEPS, x_front, x_mid, x_face, x_glass, x_glass - x_front,
                 x_room - x_glass, top_clear, wall_in, held))


if __name__ == "__main__":
    hc.Run(STEM, MODEL).main(main)
