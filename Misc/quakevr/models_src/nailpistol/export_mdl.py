# export_mdl.py -- writes the nail pistol's Quake MDLs from nailpistol.blend (README.md):
#
#   blender -b Misc/quakevr/models_src/nailpistol/nailpistol.blend -P Misc/quakevr/models_src/nailpistol/export_mdl.py
#           [-- <output progs folder>]      (default: the repository's quakevr/progs)
#
# Writes, from the scene's mesh objects (modifiers applied; one whose name starts with "_" is left out):
#   progs/v_nailpistol.mdl          every mesh but the Magazine, in the Body object's frame (its origin: the grip's hand
#                                   point; +X forward, +Y left, +Z up, as Quake's view weapons). Frame 0 "rest"; frame 1
#                                   "slideback": the Slide object moved back along its own -X by its "travel" property.
#   progs/v_nailpistol_mag.mdl      the Magazine object alone, in its own frame (its origin: the magazine's top; it goes
#                                   in and out along its -Z).
#   progs/vr_nailpistol_preview.mdl all of it, the magazine in place, scaled by the Body's "preview_scale" (the size a
#                                   weapon is drawn in the hand): to look at in the game (README.md, "In the game").
# The skin is the image of the Body's material (nailpistol_skin.png), each texel made the nearest of Quake's 224
# ordinary palette colours; that indexed skin is also saved as nailpistol_skin_pal.png. One skin for all three models.
# Triangles are written as Blender triangulates the faces; texel coordinates are the UVs' texels (u * width, (1 - v) *
# height, rounded down), flat shaded (each face's own normal).

import importlib.util
import math
import os
import struct
import sys
import zlib

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
QVR = os.path.normpath(os.path.join(HERE, "..", ".."))           # Misc/quakevr
REPO = os.path.normpath(os.path.join(QVR, "..", ".."))
sys.path.insert(0, QVR)
import mdlgen  # noqa: E402  (Misc/quakevr/mdlgen.py: the MDL writer the generated models share)


def load_qpal():
    path = os.path.join(QVR, "blender", "addons", "quakevr_models", "qpal.py")
    spec = importlib.util.spec_from_file_location("np_qpal", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


qpal = load_qpal()

BODY_MDL = "v_nailpistol.mdl"
MAG_MDL = "v_nailpistol_mag.mdl"
PREVIEW_MDL = "vr_nailpistol_preview.mdl"


def write_png_indexed(path, w, h, idx, palette):
    """An 8-bit palette PNG: idx (w * h palette indices, top row first)."""
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b"".join(b"\0" + bytes(idx[y * w:(y + 1) * w]) for y in range(h))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0))
    png += chunk(b"PLTE", b"".join(bytes(c) for c in palette)) + chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def skin_image(body):
    for slot in body.material_slots:
        if slot.material and slot.material.node_tree:
            for node in slot.material.node_tree.nodes:
                if node.type == "TEX_IMAGE" and node.image:
                    return node.image
    raise RuntimeError("the Body's material has no image texture")


def quantize(img):
    """The image as palette indices (top row first) and its size."""
    w, h = img.size
    px = list(img.pixels[:])
    ch = img.channels
    q = qpal.Quantizer()
    out = bytearray(w * h)
    for t in range(h):
        row = h - 1 - t  # Blender's rows go up from the bottom
        for s in range(w):
            i = (row * w + s) * ch
            # (the image's colours as stored: an sRGB PNG reads back as the bytes it holds)
            c = tuple(max(0, min(255, int(round(px[i + k] * 255.0)))) for k in range(3))
            out[t * w + s] = q.nearest(c, False)
    return out, w, h


def object_tris(ob, to_frame, w, h, depsgraph):
    """ob's triangles in a frame: [(3 positions, normal, 3 texels)], clockwise seen from outside (Quake's)."""
    ev = ob.evaluated_get(depsgraph)
    me = ev.to_mesh()
    me.calc_loop_triangles()
    uv = me.uv_layers.active
    if uv is None:
        raise RuntimeError("%s has no UV map" % ob.name)
    m = to_frame @ ob.matrix_world
    rot = m.to_3x3().normalized()
    out = []
    for lt in me.loop_triangles:
        ps = [m @ me.vertices[me.loops[li].vertex_index].co for li in lt.loops]
        n = (rot @ lt.normal).normalized()
        sts = []
        for li in lt.loops:
            u, v = uv.data[li].uv
            s = min(w - 1, max(0, int(math.floor(u * w))))
            t = min(h - 1, max(0, int(math.floor((1.0 - v) * h))))
            sts.append((s, t))
        # Blender's corners go counter-clockwise seen from outside; Quake's clockwise.
        out.append(((ps[0], ps[2], ps[1]), n, (sts[0], sts[2], sts[1])))
    ev.to_mesh_clear()
    return out


def build_mesh(tri_sets, w, h):
    """mdlgen.Mesh of these triangles (vertices shared within a flat face), and per set the vertices it uses."""
    mesh = mdlgen.Mesh(w, h, {})
    index = {}
    used = []
    for tris in tri_sets:
        used.append(set())
        for ps, n, sts in tris:
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
                used[-1].update(corners)
    return mesh, used


def moved(mesh, moved_verts, delta):
    m = mdlgen.Mesh(mesh.skin_w, mesh.skin_h, {})
    m.tris = mesh.tris
    m.verts = [((p[0] + delta.x, p[1] + delta.y, p[2] + delta.z), n, st) if i in moved_verts else (p, n, st)
               for i, (p, n, st) in enumerate(mesh.verts)]
    return m


def export(out_dir=None, log=print):
    out_dir = out_dir or os.path.join(REPO, "quakevr", "progs")
    scene = bpy.context.scene
    objs = {ob.name: ob for ob in scene.objects}
    body, mag, slide = objs.get("Body"), objs.get("Magazine"), objs.get("Slide")
    if body is None or mag is None:
        raise RuntimeError("nailpistol.blend needs the objects Body and Magazine")
    depsgraph = bpy.context.evaluated_depsgraph_get()
    img = skin_image(body)
    skin, w, h = quantize(img)
    write_png_indexed(os.path.join(HERE, "nailpistol_skin_pal.png"), w, h, skin, qpal.PALETTE)

    to_body = body.matrix_world.inverted()
    parts = [ob for ob in scene.objects if ob.type == "MESH" and not ob.name.startswith("_") and ob is not mag
             and ob.parent is not mag]
    parts.sort(key=lambda ob: (ob is not body, ob.name))
    tri_sets = [object_tris(ob, to_body, w, h, depsgraph) for ob in parts]
    mesh, used = build_mesh(tri_sets, w, h)
    frames = []
    if slide is not None:
        moved_verts = used[parts.index(slide)]
        travel = float(slide.get("travel", 1.0))
        m = to_body @ slide.matrix_world
        delta = (m @ Vector((-travel, 0.0, 0.0))) - (m @ Vector((0.0, 0.0, 0.0)))
        frames.append(moved(mesh, moved_verts, delta))
    os.makedirs(out_dir, exist_ok=True)
    mdlgen.write_mdl(os.path.join(out_dir, BODY_MDL), mesh, [bytes(skin)], "rest", frames)
    log("%s: %d triangles, %d vertices, %d frames, skin %dx%d (%s)" % (
        BODY_MDL, len(mesh.tris), len(mesh.verts), 1 + len(frames), w, h, ", ".join(ob.name for ob in parts)))

    mag_tris = object_tris(mag, mag.matrix_world.inverted(), w, h, depsgraph)
    mag_mesh, _ = build_mesh([mag_tris], w, h)
    mdlgen.write_mdl(os.path.join(out_dir, MAG_MDL), mag_mesh, [bytes(skin)], "mag")
    log("%s: %d triangles, %d vertices" % (MAG_MDL, len(mag_mesh.tris), len(mag_mesh.verts)))
    mm = to_body @ mag.matrix_world
    loc, rot, _ = mm.decompose()
    e = rot.to_euler()
    log("Magazine in the Body's frame: origin %.3f %.3f %.3f, rotation %.2f %.2f %.2f degrees (x y z)" % (
        loc.x, loc.y, loc.z, math.degrees(e.x), math.degrees(e.y), math.degrees(e.z)))

    k = float(body.get("preview_scale", 0.45))
    scale = Matrix.Scale(k, 4)
    pre, _ = build_mesh([object_tris(ob, scale @ to_body, w, h, depsgraph) for ob in parts + [mag]], w, h)
    mdlgen.write_mdl(os.path.join(out_dir, PREVIEW_MDL), pre, [bytes(skin)], "preview")
    log("%s: %d triangles (scale %.2f)" % (PREVIEW_MDL, len(pre.tris), k))


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    export(argv[0] if argv else None)
