# make_ssg_muzzle.py -- the super shotgun's muzzle opened: its two bores modelled on v_shot2.blend (the author's
# feedback 2026-11-23: the muzzle read as a solid block; the nail pistol's barrel has a modelled bore).
#
# Usage, from the repository root (the options after the separator: [--dry] [--save other.blend]):
#   Git Bash:   "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup \
#                   Misc/quakevr/src_models/blend/v_shot2/v_shot2.blend \
#                   --python Misc/quakevr/make_ssg_muzzle.py -- --dry
#   PowerShell: & "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup `
#                   Misc\quakevr\src_models\blend\v_shot2\v_shot2.blend `
#                   --python Misc\quakevr\make_ssg_muzzle.py '--' --dry
#   (PowerShell eats a bare --: quote it. Leave out `-- --dry` to export and save.)
# Works as the screen housing scripts do (housing_common.py; docs/vr-port/MODELS_IN_BLENDER.md, "Opening the super
# shotgun's muzzle"): builds or moves the bores, paints their skin patch, exports the model as blend_export.py does
# (quakevr/progs, weapons.txt; the export runs make_ssg_open.py for the open halves and bakes the normal maps), marks
# the new triangles in the barrel mask as the cap they replace was, and saves the .blend. Ends with "muzzle_shot2: ok,
# ..." or "muzzle_shot2: REFUSED: ..." (exit code 1). Refuses if the UVs of any of the gun's other faces change.
#
# What it makes: the muzzle's flat cap (id's 7 triangles over the two barrels' outline, x 28.27-28.50) replaced by a
# face round two bores, each (make_ssg_open.py's breech bores' look):
# - a bright chamfer: a ring CHAMFER_W outside the bore on the face, one x step down to the bore;
# - the bore's tube (BORE_SIDES sides, radius BORE_R) DEPTH_STEPS x steps deep in all, its walls dark to black;
# - a black floor.
# The face keeps the cap's own texels (its outline's UVs; the new points mapped as the outline is); the chamfer, walls
# and floor map into a patch of free skin (PATCH), painted again on every run. The new vertices carry the point
# attribute `qvr_muzzle` (their place in the build, 1 up), the new faces the face attribute `qvr_muzzle_face`: run again
# with the same shape they're moved and re-mapped in place (the model's file keeps its vertices and triangles); with
# another shape they're built anew. The muzzle flash's collapsed vertices at the bores' middles are left alone.

import math
import os
import random
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import housing_common as hc  # noqa: E402
import make_ssg_open as so  # noqa: E402  (bridge, ear_clip, octagon; the breech bores' palette)
import mdlpolish as mpol  # noqa: E402  (the palette)

STEM = "shot2"
MODEL = "v_shot2"
VTAG = "qvr_muzzle"
FTAG = "qvr_muzzle_face"

CAP_X = 28.2         # the cap: faces with every corner in front of this, facing +x, bigger than CAP_AREA
CAP_AREA = 0.3       # (the sight post's front, 0.17, and the flash's collapsed triangles are smaller)
CAP_FACES = 7
BORE_R = 0.75        # the bores' radius
BORE_SIDES = 8
CHAMFER_W = 0.15     # the chamfer's ring on the face, outside the bore
DEPTH_STEPS = 9      # the bore's depth from the face in x steps (0.227): the chamfer one, the tube the rest
CENTRE_Z = None      # the bores' height: the outline's middle (None), or a model z

# The skin patch (texels from the top; free skin: no triangle maps there).
SKIN_W, SKIN_H = 512, 146
PATCH = (276, 0, 48, 16)        # x, y, w, h
WALL = (2, 1, 22, 15)           # within the patch: x0, y0, x1, y1 (the tube: round it across, the mouth at the top)
CHAMF = (24, 1, 44, 5)          # the chamfer ring: round it across, the face's edge at the top
FLOOR = (24, 7, 34, 15)         # the bore's floor
DARK = (5, 4, 1)                # palette: the walls' upper part (make_ssg_open.py's)
WALL_DARK = 0.4                 # the walls' share (from the mouth) painted DARK, the rest BORE


def region_uv(reg, fx, fy):
    """A point in a patch region (fx across, fy down, 0-1) as Blender's UV (bottom-up)."""
    x0, y0, x1, y1 = reg
    s = PATCH[0] + x0 + 0.5 + (x1 - x0 - 1.0) * fx
    t = PATCH[1] + y0 + 0.5 + (y1 - y0 - 1.0) * fy
    return Vector((s / SKIN_W, 1.0 - t / SKIN_H))


class Muzzle(hc.Run):
    def say(self, msg):
        print("muzzle_%s: %s" % (self.stem, msg))

    def fail(self, msg):
        print("muzzle_%s: REFUSED: %s" % (self.stem, msg))
        sys.stdout.flush()
        sys.exit(1)

    def gun_uvs(self):
        """The UVs of the gun's faces this script doesn't own: not the bores' faces, not the cap they replace."""
        me = bpy.data.objects[self.model].data
        ftag = me.attributes.get(FTAG)
        uv = me.uv_layers.active.data
        faces = []
        for p in me.polygons:
            if (ftag is not None and ftag.data[p.index].value > 0) or is_cap(me, p):
                continue
            faces.append(tuple(sorted((me.loops[li].vertex_index, round(uv[li].uv[0], 6), round(uv[li].uv[1], 6))
                                      for li in p.loop_indices)))
        return sorted(faces)


def is_cap(me, p):
    return (p.area > CAP_AREA and p.normal.x > 0.9 and
            all(me.vertices[i].co.x > CAP_X for i in p.vertices))


def main(h):
    ob = bpy.data.objects[MODEL]
    me = ob.data
    sc, org = (Vector(v) for v in h.grid)

    def snap(p):
        return Vector(tuple(org[i] + round((p[i] - org[i]) / sc[i]) * sc[i] for i in range(3)))

    bm = bmesh.new()
    bm.from_mesh(me)
    bm.normal_update()
    vlay =bm.verts.layers.int.get(VTAG) or bm.verts.layers.int.new(VTAG)
    flay = bm.faces.layers.int.get(FTAG) or bm.faces.layers.int.new(FTAG)
    uvl = bm.loops.layers.uv.active
    mine_f = [f for f in bm.faces if f[flay] > 0]
    if mine_f:
        region = mine_f
    else:
        region = [f for f in bm.faces if f.calc_area() > CAP_AREA and f.normal.x > 0.9 and
                  all(v.co.x > CAP_X for v in f.verts)]
        if len(region) != CAP_FACES:
            h.fail("the muzzle's cap: %d faces found, not %d" % (len(region), CAP_FACES))
    # The outline: the gun's vertices round the region, with their UVs there (the cap's texels).
    out_uv = {}
    for f in region:
        for loop in f.loops:
            if loop.vert[vlay] == 0:
                out_uv.setdefault(loop.vert, Vector(loop[uvl].uv))
    outline = list(out_uv)
    if len(outline) != 9:
        h.fail("the muzzle's outline has %d vertices, not 9" % len(outline))
    cy0 = sum(v.co.y for v in outline) / 9
    cz0 = sum(v.co.z for v in outline) / 9
    outline.sort(key=lambda v: math.atan2(v.co.z - cz0, v.co.y - cy0))  # (counter-clockwise in y, z)
    oyz = [(v.co.y, v.co.z) for v in outline]
    if so.area2(oyz) <= 0:
        h.fail("the muzzle's outline isn't star-shaped round its middle")
    # The face's x at (y, z): the outline filled (its own triangles), barycentric.
    otris = so.ear_clip(oyz)

    def face_x(y, z):
        best = None
        for a, b, c in otris:
            (ya, za), (yb, zb), (yc, zc) = oyz[a], oyz[b], oyz[c]
            d = (yb - ya) * (zc - za) - (zb - za) * (yc - ya)
            w1 = ((yb - y) * (zc - z) - (zb - z) * (yc - y)) / d
            w2 = ((yc - y) * (za - z) - (zc - z) * (ya - y)) / d
            w3 = 1.0 - w1 - w2
            if best is None or min(w1, w2, w3) > best[0]:
                best = (min(w1, w2, w3), w1 * outline[a].co.x + w2 * outline[b].co.x + w3 * outline[c].co.x)
        return best[1]
    # The face's UVs: the outline's, as an affine map of (y, z) (least squares over its 9 points).
    A = np.array([[y, z, 1.0] for y, z in oyz])
    U = np.array([tuple(out_uv[v]) for v in outline])
    affine = np.linalg.lstsq(A, U, rcond=None)[0]

    def face_uv(y, z):
        return Vector(tuple(np.array([y, z, 1.0]) @ affine))

    # The bores: each barrel's middle (the outline's y extent either side of its middle; z its middle).
    ys = [p[0] for p in oyz]
    zs = [p[1] for p in oyz]
    notch = min((p for p in oyz if p[1] > cz0), key=lambda p: abs(p[0] - cy0))  # (the V between the barrels' tops)
    cz = CENTRE_Z if CENTRE_Z is not None else (min(zs) + max(zs)) / 2
    centres = [((min(ys) + notch[0]) / 2, cz), ((notch[0] + max(ys)) / 2, cz)]
    xstep = sc.x
    co, tris = [], []  # the build's vertices (positions) and triangles (i, j, k, uvs, want); i < 0: outline[-1 - i]

    def add(p):
        co.append(snap(p))
        return len(co) - 1

    def pos(i):
        return outline[-1 - i].co if i < 0 else co[i]

    rings = []
    for cyb, czb in centres:
        mouth2 = so.octagon(cyb, czb, BORE_R + CHAMFER_W, BORE_R + CHAMFER_W, BORE_SIDES)
        bore2 = so.octagon(cyb, czb, BORE_R, BORE_R, BORE_SIDES)
        mouth = [add(Vector((face_x(y, z), y, z))) for y, z in mouth2]
        x_in = min(co[i].x for i in mouth) - xstep
        inner = [add(Vector((x_in, y, z))) for y, z in bore2]
        bottom = [add(Vector((x_in - (DEPTH_STEPS - 1) * xstep, y, z))) for y, z in bore2]
        rings.append((cyb, czb, mouth, inner, bottom))
        for i in mouth:
            d = math.hypot(co[i].y - cyb, co[i].z - czb)
            if not so.inside_poly(oyz, co[i].y, co[i].z) or so.edge_distance(oyz, co[i].y, co[i].z) < 0.08:
                h.fail("a bore's chamfer reaches the muzzle's edge (%.2f %.2f, r %.2f)" % (co[i].y, co[i].z, d))
    # The face: the outline round the two chamfers' outer rings.
    holes = [[(co[i].y, co[i].z) for i in r[2]] for r in rings]
    poly = so.bridge(oyz, holes)
    at = {}
    for k, v in enumerate(outline):
        at[(round(v.co.y, 5), round(v.co.z, 5))] = -1 - k
    for r in rings:
        for i in r[2]:
            at[(round(co[i].y, 5), round(co[i].z, 5))] = i
    ids = [at[(round(p[0], 5), round(p[1], 5))] for p in poly]
    for a, b, c in so.ear_clip(poly):
        vs = (ids[a], ids[b], ids[c])
        tris.append((vs, tuple(face_uv(pos(i).y, pos(i).z) if i >= 0 else Vector(out_uv[outline[-1 - i]])
                               for i in vs), Vector((1.0, 0.0, 0.0))))
    face_tris = len(tris)
    for cyb, czb, mouth, inner, bottom in rings:
        n = BORE_SIDES
        for k in range(n):
            j = (k + 1) % n
            mid = (co[inner[k]] + co[inner[j]]) * 0.5
            inward = Vector((0.0, cyb - mid.y, czb - mid.z)).normalized()
            fa, fb = k / n, (k + 1) / n
            # The chamfer: the face's ring down to the bore's mouth (out of the gun and in to the axis).
            quad(tris, (mouth[k], mouth[j], inner[j], inner[k]),
                 (region_uv(CHAMF, fa, 0), region_uv(CHAMF, fb, 0), region_uv(CHAMF, fb, 1), region_uv(CHAMF, fa, 1)),
                 Vector((1.0, 0.0, 0.0)) + inward, co)
            # The tube's wall, facing in.
            quad(tris, (inner[k], inner[j], bottom[j], bottom[k]),
                 (region_uv(WALL, fa, 0), region_uv(WALL, fb, 0), region_uv(WALL, fb, 1), region_uv(WALL, fa, 1)),
                 inward, co)
        # The floor, facing out of the bore.
        r2 = [(co[i].y, co[i].z) for i in bottom]
        for a, b, c in so.ear_clip(r2):
            vs = (bottom[a], bottom[b], bottom[c])
            tris.append((vs, tuple(region_uv(FLOOR, 0.5 + 0.5 * (co[i].y - cyb) / BORE_R,
                                             0.5 - 0.5 * (co[i].z - czb) / BORE_R) for i in vs),
                         Vector((1.0, 0.0, 0.0))))
    bad = 0
    built = []
    for vs, uvs, want in tris:
        p = [pos(i) for i in vs]
        nrm = (p[1] - p[0]).cross(p[2] - p[0])
        if nrm.dot(want) < 0:
            vs, uvs = (vs[0], vs[2], vs[1]), (uvs[0], uvs[2], uvs[1])
        if nrm.length < 1e-6 or abs(nrm.normalized().dot(want.normalized())) < 0.05:
            bad += 1
            print("muzzle: a bad triangle at %.2f %.2f %.2f" % tuple((p[0] + p[1] + p[2]) / 3))
        built.append((vs, uvs))
    if bad:
        h.fail("%d bad triangles (too thin, or standing across their side)" % bad)

    moved = put(h, bm, me, vlay, flay, uvl, region, outline, co, built)
    bm.free()
    ob["qvr_muzzle"] = "ssg muzzle bores (make_ssg_muzzle.py)"

    image = bpy.data.images[MODEL + "_skin0"]
    image.pixels[0]  # (loads it)
    painted = paint(image)

    def after_export():
        mark_mask(h)
        if painted:
            image.save()
            print("  wrote %s" % bpy.path.abspath(image.filepath))

    print("muzzle: bores at y %.3f, %.3f, z %.3f; r %.2f, chamfer %.2f, %d x steps deep (%.2f)" % (
        centres[0][0], centres[1][0], cz, BORE_R, CHAMFER_W, DEPTH_STEPS, DEPTH_STEPS * xstep))
    h.finish("%d vertices, %d triangles (face %d, bores %d), %s; skin patch %s" % (
        len(co), len(built), face_tris, len(built) - face_tris, "moved in place" if moved else "built",
        "painted" if painted else "unchanged"), after_export=after_export)


def quad(tris, vs, uvs, want, co):
    """Two triangles over a quad (its corners in order round it), split along the shorter diagonal."""
    a, b, c, d = vs
    if (co[a] - co[c]).length <= (co[b] - co[d]).length:
        tris.append(((a, b, c), (uvs[0], uvs[1], uvs[2]), want))
        tris.append(((a, c, d), (uvs[0], uvs[2], uvs[3]), want))
    else:
        tris.append(((a, b, d), (uvs[0], uvs[1], uvs[3]), want))
        tris.append(((b, c, d), (uvs[1], uvs[2], uvs[3]), want))


def put(h, bm, me, vlay, flay, uvl, region, outline, co, built):
    """Into the mesh: the build's vertices (the old ones moved in place when there are as many) and faces (an old one
    kept, re-mapped, when the build has it, wound alike: its triangle in the file stays); the cap's faces gone."""
    old = sorted((v for v in bm.verts if v[vlay] > 0), key=lambda v: (v[vlay], v.index))
    lay_vert = bm.verts.layers.int.get("qvr_vert")
    lay_tri = bm.faces.layers.int.get("qvr_tri")
    lay_vid = bm.loops.layers.int.get("qvr_vid")
    shapes = list(bm.verts.layers.shape.values())
    moved = len(old) == len(co)
    h.moved_verts = 0
    if moved:
        sc, org = h.grid
        for v, p in zip(old, co):
            on = [org[i] + round((p[i] - org[i]) / sc[i]) * sc[i] for i in range(3)]
            if max(abs(v.co[i] - on[i]) for i in range(3)) > 1e-4:
                v.co = p
                h.moved_verts += 1
        bv = old
    else:
        bmesh.ops.delete(bm, geom=[f for f in region if f.is_valid], context='FACES_ONLY')
        if old:
            bmesh.ops.delete(bm, geom=old, context='VERTS')
        region = []
        bv = []
        for p in co:
            v = bm.verts.new(p)
            if lay_vert is not None:
                v[lay_vert] = -1
            for sl in shapes:
                v[sl] = p
            bv.append(v)
    for i, v in enumerate(bv):
        v[vlay] = i + 1

    def vert(i):
        return outline[-1 - i] if i < 0 else bv[i]
    want = {frozenset(vert(i) for i in vs): (vs, uvs) for vs, uvs in built}
    kept = 0
    for f in [f for f in region if f.is_valid]:
        key = frozenset(f.verts)
        t = want.get(key)
        if t is not None and len(f.verts) == 3:
            order = [vert(i) for i in t[0]]
            fv = list(f.verts)
            if fv[(fv.index(order[0]) + 1) % 3] == order[1]:
                for loop in f.loops:
                    loop[uvl].uv = t[1][order.index(loop.vert)]
                f[flay] = 1
                del want[key]
                kept += 1
                continue
        bm.faces.remove(f)
    for vs, uvs in want.values():
        f = bm.faces.new([vert(i) for i in vs])
        f.material_index = 0
        f.smooth = False
        f[flay] = 1
        if lay_tri is not None:
            f[lay_tri] = -1
        for loop, uv in zip(f.loops, uvs):
            loop[uvl].uv = uv
            if lay_vid is not None:
                loop[lay_vid] = -1
    # (the cap's inner edges, left loose)
    for e in [e for e in bm.edges if not e.link_faces and all(v in set(outline) for v in e.verts)]:
        bm.edges.remove(e)
    bm.to_mesh(me)
    me.update()
    h.kept, h.made = kept, len(built) - kept
    return moved


def paint(image):
    """Paints the patch (the chamfer bright, the walls dark to black, the floor black). Returns whether it changed."""
    w, hgt = image.size
    pal = mpol.palette() / 255.0
    rng = random.Random(2311)
    before = image.pixels[:]
    px = list(before)

    def put_px(s, t, idx):
        i = 4 * ((hgt - 1 - t) * w + s)
        px[i:i + 4] = [float(c) for c in pal[idx]] + [1.0]

    def inside(reg, x, y):
        return reg[0] <= x < reg[2] and reg[1] <= y < reg[3]
    for y in range(PATCH[3]):
        for x in range(PATCH[2]):
            if inside(CHAMF, x, y):
                idx = rng.choice(so.CHAMFER)
            elif inside(WALL, x, y):
                f = (y - WALL[1] + 0.5) / (WALL[3] - WALL[1])
                idx = rng.choice(DARK) if f < WALL_DARK else rng.choice(so.BORE)
            else:
                idx = rng.choice(so.BORE)
            put_px(PATCH[0] + x, PATCH[1] + y, idx)
    image.pixels[:] = px
    after = image.pixels[:]
    return any(abs(a - b) > 1e-6 for a, b in zip(before, after))


def mark_mask(h):
    """The new triangles take the barrel mask's mark as the cap they replace had (the export leaves new ones out)."""
    if h.dry:
        return
    source, _ = hc.addon()
    me = bpy.data.objects[MODEL].data
    ftag, tri = me.attributes.get(FTAG), me.attributes.get("qvr_tri")
    if ftag is None or tri is None:
        h.fail("after the export the faces lost %s or qvr_tri" % FTAG)
    ids = [tri.data[p.index].value for p in me.polygons if ftag.data[p.index].value > 0]
    if any(i < 0 for i in ids):
        h.fail("after the export %d of the bores' faces have no triangle in the file" % sum(i < 0 for i in ids))
    changes = {}
    for slot, vals in source.model_slots(hc.ROOT, MODEL + ".mdl").items():
        mask = vals.get("fx_gfx_mask", "0")
        if ":" not in mask:
            continue
        bits = source.mask_decode(mask)
        if max(ids) >= len(bits):
            h.fail("slot %d's barrel mask has %d triangles, the bores reach %d" % (slot, len(bits), max(ids)))
        for i in ids:
            bits[i] = True
        nm = source.mask_encode(bits)
        if nm != mask:
            changes[(slot, "fx_gfx_mask")] = nm
    if changes:
        source.write_weapons_txt(hc.ROOT, changes)
        print("  barrel mask: the bores' %d triangles marked (weapons.txt)" % len(ids))


Muzzle(STEM, MODEL).main(main)
