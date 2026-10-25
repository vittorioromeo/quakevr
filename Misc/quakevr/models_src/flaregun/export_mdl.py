# export_mdl.py -- writes the flare gun's Quake MDLs from flaregun.blend (README.md):
#
#   blender -b Misc/quakevr/models_src/flaregun/flaregun.blend -P Misc/quakevr/models_src/flaregun/export_mdl.py
#           [-- <output progs folder>]      (default: the repository's quakevr/progs)
#
# Writes, from the scene's mesh objects (modifiers applied; one whose name starts with "_" is left out):
#   progs/v_flaregun.mdl          every mesh but the Flare, in the Body object's frame (its origin: the grip's hand point;
#                                 +X forward, +Y left, +Z up, as Quake's view weapons). One frame "rest"; three skins:
#                                 0 loaded, 1 the top barrel fired (the left cartridge's cap gone, the top muzzle sooted),
#                                 2 both fired.
#   progs/vr_flare.mdl            the Flare object alone (the projectile), in its own frame (its middle; its cap along +X).
#   progs/vr_flaregun_preview.mdl the gun scaled by the Body's "preview_scale" (the size a weapon is drawn in the hand),
#                                 skin 0: to look at in the game (README.md, "In the game").
# Skin 0 is the image of the Body's material (flaregun_skin.png), each texel made the nearest of Quake's 224 ordinary
# palette colours (that indexed skin is also saved as flaregun_skin_pal.png). Skins 1 and 2 are skin 0 with the overlay
# images flaregun_skin_fired1.png and flaregun_skin_fired2.png on top (their opaque texels; the rest transparent), so a
# repaint of skin 0 carries over. Triangles are written as Blender triangulates the faces; texel coordinates are the UVs'
# texels (u * width, (1 - v) * height, rounded down), flat shaded (each face's own normal).

import importlib.util
import math
import os
import struct
import sys
import zlib

import bpy
from mathutils import Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
QVR = os.path.normpath(os.path.join(HERE, "..", ".."))           # Misc/quakevr
REPO = os.path.normpath(os.path.join(QVR, "..", ".."))
sys.path.insert(0, QVR)
import mdlgen  # noqa: E402  (Misc/quakevr/mdlgen.py: the MDL writer the generated models share)


def load_qpal():
    path = os.path.join(QVR, "blender", "addons", "quakevr_models", "qpal.py")
    spec = importlib.util.spec_from_file_location("fg_qpal", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


qpal = load_qpal()

GUN_MDL = "v_flaregun.mdl"
FLARE_MDL = "vr_flare.mdl"
PREVIEW_MDL = "vr_flaregun_preview.mdl"
OVERLAYS = ("flaregun_skin_fired1", "flaregun_skin_fired2")
MARKERS = ("_MuzzleTop", "_MuzzleBottom", "_SightRear", "_SightFront")


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


def overlay_image(name):
    img = bpy.data.images.get(name) or bpy.data.images.get(name + ".png")
    if img is None:
        path = os.path.join(HERE, name + ".png")
        if not os.path.exists(path):
            return None
        img = bpy.data.images.load(path)
    return img


def quantize(img, base=None):
    """The image as palette indices (top row first) and its size; with base (indices), an overlay: its transparent
    texels are base's."""
    w, h = img.size
    px = list(img.pixels[:])
    ch = img.channels
    q = qpal.Quantizer()
    out = bytearray(w * h)
    for t in range(h):
        row = h - 1 - t  # Blender's rows go up from the bottom
        for s in range(w):
            i = (row * w + s) * ch
            if base is not None and ch == 4 and px[i + 3] < 0.5:
                out[t * w + s] = base[t * w + s]
                continue
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
    """mdlgen.Mesh of these triangles (vertices shared within a flat face)."""
    mesh = mdlgen.Mesh(w, h, {})
    index = {}
    for tris in tri_sets:
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
    return mesh


def export(out_dir=None, log=print):
    out_dir = out_dir or os.path.join(REPO, "quakevr", "progs")
    scene = bpy.context.scene
    objs = {ob.name: ob for ob in scene.objects}
    body, flare = objs.get("Body"), objs.get("Flare")
    if body is None:
        raise RuntimeError("flaregun.blend needs the object Body")
    depsgraph = bpy.context.evaluated_depsgraph_get()
    skin0, w, h = quantize(skin_image(body))
    write_png_indexed(os.path.join(HERE, "flaregun_skin_pal.png"), w, h, skin0, qpal.PALETTE)
    skins = [bytes(skin0)]
    for name in OVERLAYS:
        img = overlay_image(name)
        if img is None:
            log("%s.png: missing, that skin left out" % name)
            continue
        if tuple(img.size) != (w, h):
            raise RuntimeError("%s is %dx%d, the skin %dx%d" % (name, img.size[0], img.size[1], w, h))
        sk, _, _ = quantize(img, skin0)
        skins.append(bytes(sk))

    to_body = body.matrix_world.inverted()
    parts = [ob for ob in scene.objects if ob.type == "MESH" and not ob.name.startswith("_") and ob is not flare
             and ob.parent is not flare]
    parts.sort(key=lambda ob: (ob is not body, ob.name))
    mesh = build_mesh([object_tris(ob, to_body, w, h, depsgraph) for ob in parts], w, h)
    os.makedirs(out_dir, exist_ok=True)
    mdlgen.write_mdl(os.path.join(out_dir, GUN_MDL), mesh, skins, "rest")
    log("%s: %d triangles, %d vertices, %d skins %dx%d (%s)" % (
        GUN_MDL, len(mesh.tris), len(mesh.verts), len(skins), w, h, ", ".join(ob.name for ob in parts)))
    for name in MARKERS:
        m = objs.get(name)
        if m is not None:
            p = to_body @ m.matrix_world.translation
            log("%s in the Body's frame: %.3f %.3f %.3f" % (name, p.x, p.y, p.z))

    if flare is not None:
        fm = build_mesh([object_tris(flare, flare.matrix_world.inverted(), w, h, depsgraph)], w, h)
        mdlgen.write_mdl(os.path.join(out_dir, FLARE_MDL), fm, [skins[0]], "flare")
        log("%s: %d triangles, %d vertices" % (FLARE_MDL, len(fm.tris), len(fm.verts)))

    k = float(body.get("preview_scale", 0.45))
    pre = build_mesh([object_tris(ob, Matrix.Scale(k, 4) @ to_body, w, h, depsgraph) for ob in parts], w, h)
    mdlgen.write_mdl(os.path.join(out_dir, PREVIEW_MDL), pre, [skins[0]], "preview")
    log("%s: %d triangles (scale %.2f)" % (PREVIEW_MDL, len(pre.tris), k))


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    export(argv[0] if argv else None)
