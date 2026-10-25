# export_mdl.py -- writes the heavy revolver's Quake MDLs from revolver.blend (README.md):
#
#   blender -b Misc/quakevr/models_src/revolver/revolver.blend -P Misc/quakevr/models_src/revolver/export_mdl.py
#           [-- <output progs folder>]      (default: the repository's quakevr/progs)
#
# Writes, from the scene's mesh objects (modifiers applied; one whose name starts with "_" is left out):
#   progs/v_revolver.mdl         the whole gun in the Body object's frame (its origin: the grip's hand point; +X forward,
#                                +Y left, +Z up, as Quake's view weapons). Frames: 0 rest, 1 the hammer cocked, 2 the
#                                trigger pulled (the hammer down), 3 the cylinder swung out on its crane, 4 swung out
#                                with the ejector rod pushed.
#   progs/v_revolver_frame.mdl   the gun without the crane, the cylinder and the rod (for those drawn as models of their
#                                own, below): frames 0 rest, 1 cocked, 2 trigger pulled.
#   progs/v_revolver_crane.mdl   the Crane in its own frame (its origin its pivot; it swings about its X).
#   progs/v_revolver_cyl.mdl     the Cylinder and the EjectorRod in the Cylinder's frame (its origin on its axis at its
#                                rear face; it spins about its X): frames 0 rest, 1 the rod pushed (ejecting).
#   progs/v_revolver_shell.mdl   the Shell in its own frame (its origin the rim's back face's centre, the hull along
#                                +X), in the gun's units; skins: 0 red, 1 the blue slug, 2 red spent, 3 blue spent.
#   progs/vr_revolver_preview.mdl all of it, six shells chambered (red and blue in turn), scaled by the Body's
#                                "preview_scale": frames 0 rest, 1 swung out, 2 cocked (to look at in the game).
# The gun's skin is the image of the Body's material (revolver_skin.png), each texel made the nearest of Quake's 224
# ordinary palette colours (that indexed skin is saved as revolver_skin_pal.png); the shell's skins are the files the
# Shell's "skins" property names. Triangles are written as Blender triangulates the faces; texel coordinates are the
# UVs' texels, flat shaded (each face's own normal). It prints where the parts sit in the Body's frame (for the code).

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
    spec = importlib.util.spec_from_file_location("rv_qpal", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


qpal = load_qpal()

GUN_PARTS = ("Body", "Crane", "Cylinder", "EjectorRod", "Hammer", "Trigger")
REST = {}
COCKED = {"cock": 1.0}
PULLED = {"pull": 1.0}
SWUNG = {"swing": 1.0}
EJECT = {"swing": 1.0, "eject": 1.0}


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


def skin_image(ob):
    for slot in ob.material_slots:
        if slot.material and slot.material.node_tree:
            for node in slot.material.node_tree.nodes:
                if node.type == "TEX_IMAGE" and node.image:
                    return node.image
    raise RuntimeError("%s's material has no image texture" % ob.name)


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


def local_tris(ob, w, h, depsgraph):
    """ob's triangles in its own coordinates: [(3 positions, normal, 3 texels)], clockwise seen from outside."""
    ev = ob.evaluated_get(depsgraph)
    me = ev.to_mesh()
    me.calc_loop_triangles()
    uv = me.uv_layers.active
    if uv is None:
        raise RuntimeError("%s has no UV map" % ob.name)
    out = []
    for lt in me.loop_triangles:
        ps = [me.vertices[me.loops[li].vertex_index].co.copy() for li in lt.loops]
        sts = []
        for li in lt.loops:
            u, v = uv.data[li].uv
            sts.append((min(w - 1, max(0, int(math.floor(u * w)))), min(h - 1, max(0, int(math.floor((1 - v) * h))))))
        # Blender's corners go counter-clockwise seen from outside; Quake's clockwise.
        out.append(((ps[0], ps[2], ps[1]), lt.normal.copy(), (sts[0], sts[2], sts[1])))
    ev.to_mesh_clear()
    return out


def posed(objs, pose):
    """Every object's world matrix with the pose's parts moved (fractions of their properties), then put back."""
    crane, hammer, trigger, rod = (objs.get(n) for n in ("Crane", "Hammer", "Trigger", "EjectorRod"))
    saved = [(ob, ob.rotation_euler.copy(), ob.location.copy()) for ob in objs.values()]
    if crane is not None:
        crane.rotation_euler.x += math.radians(float(crane.get("swing", -95.0))) * pose.get("swing", 0.0)
    if hammer is not None:
        hammer.rotation_euler.y += math.radians(float(hammer.get("cock", -38.0))) * pose.get("cock", 0.0)
    if trigger is not None:
        trigger.rotation_euler.y += math.radians(float(trigger.get("pull", 22.0))) * pose.get("pull", 0.0)
    if rod is not None:
        rod.location = rod.location + rod.matrix_basis.to_3x3() @ Vector((-float(rod.get("travel", 1.0)), 0, 0)) * \
            pose.get("eject", 0.0)
    bpy.context.view_layer.update()
    out = {name: ob.matrix_world.copy() for name, ob in objs.items()}
    for ob, rot, loc in saved:
        ob.rotation_euler = rot
        ob.location = loc
    bpy.context.view_layer.update()
    return out


class Source:
    """Triangles to draw: an object's, placed by place(world matrices of a pose), its texels moved by st_offset."""
    def __init__(self, tris, place, st_offset=(0, 0)):
        self.tris, self.place, self.st_offset = tris, place, st_offset


def build(sources, poses, objs, to_frame, w, h):
    """mdlgen.Mesh per pose (the same vertices and triangles, moved): vertices shared within a flat face."""
    worlds = [posed(objs, p) for p in poses]
    mats = [[to_frame(wm) @ s.place(wm) for s in sources] for wm in worlds]
    meshes = [mdlgen.Mesh(w, h, {}) for _ in poses]
    index = {}
    for si, s in enumerate(sources):
        for ps, n, sts in s.tris:
            m0 = mats[0][si]
            w0 = [m0 @ p for p in ps]
            if (w0[1] - w0[0]).cross(w0[2] - w0[0]).length <= 1e-6:
                continue
            corners = []
            for p, st in zip(ps, sts):
                st = (st[0] + s.st_offset[0], st[1] + s.st_offset[1])
                key = (si, round(p.x, 4), round(p.y, 4), round(p.z, 4), round(n.x, 3), round(n.y, 3), round(n.z, 3),
                       st)
                k = index.get(key)
                if k is None:
                    k = index[key] = len(meshes[0].verts)
                    for fi, mesh in enumerate(meshes):
                        m = mats[fi][si]
                        q = m @ p
                        nn = (m.to_3x3().normalized() @ n).normalized()
                        mesh.verts.append(((q.x, q.y, q.z), (nn.x, nn.y, nn.z), st))
                corners.append(k)
            if len(set(corners)) == 3:
                for mesh in meshes:
                    mesh.tris.append(tuple(corners))
    return meshes


def write(out_dir, name, meshes, skins, label, log):
    mdlgen.write_mdl(os.path.join(out_dir, name), meshes[0], skins, label, meshes[1:])
    log("%s: %d triangles, %d vertices, %d frames, %d skins %dx%d" % (
        name, len(meshes[0].tris), len(meshes[0].verts), len(meshes), len(skins), meshes[0].skin_w,
        meshes[0].skin_h))


def chamber_matrix(cyl, i, proud):
    """Chamber i's shell (its rim's back face) in the Cylinder's frame: chamber 0 on top, then round to the left."""
    pitch = float(cyl.get("pitch", 1.45))
    a = math.pi / 2 + i * math.pi / 3
    return Matrix.Translation(Vector((-proud, pitch * math.cos(a), pitch * math.sin(a))))


def export(out_dir=None, log=print):
    out_dir = out_dir or os.path.join(REPO, "quakevr", "progs")
    os.makedirs(out_dir, exist_ok=True)
    scene = bpy.context.scene
    objs = {ob.name: ob for ob in scene.objects if ob.type == "MESH" and not ob.name.startswith("_")}
    missing = [n for n in GUN_PARTS + ("Shell",) if n not in objs]
    if missing:
        raise RuntimeError("revolver.blend needs the objects %s" % ", ".join(missing))
    body, cyl, shell = objs["Body"], objs["Cylinder"], objs["Shell"]
    depsgraph = bpy.context.evaluated_depsgraph_get()
    skin, w, h = quantize(skin_image(body))
    write_png_indexed(os.path.join(HERE, "revolver_skin_pal.png"), w, h, skin, qpal.PALETTE)
    shell_skins = []
    for fn in str(shell.get("skins", "")).split() or [None]:
        img = skin_image(shell) if fn is None else bpy.data.images.load(os.path.join(HERE, fn), check_existing=True)
        shell_skins.append(quantize(img))
    sw, sh = shell_skins[0][1], shell_skins[0][2]

    tris = {n: local_tris(objs[n], w, h, depsgraph) for n in GUN_PARTS}
    # (any other mesh but the Shell is drawn with the Body: a part added by hand)
    extra = [n for n in objs if n not in GUN_PARTS and n != "Shell"]
    for n in extra:
        tris[n] = local_tris(objs[n], w, h, depsgraph)
    shell_tris = local_tris(shell, sw, sh, depsgraph)

    def src(n):
        return Source(tris[n], lambda wm, n=n: wm[n])

    to_body = lambda wm: wm["Body"].inverted()  # noqa: E731
    gun = [src(n) for n in GUN_PARTS + tuple(extra)]
    write(out_dir, "v_revolver.mdl", build(gun, [REST, COCKED, PULLED, SWUNG, EJECT], objs, to_body, w, h),
          [bytes(skin)], "revolver", log)
    frame = [src(n) for n in ("Body", "Hammer", "Trigger") + tuple(extra)]
    write(out_dir, "v_revolver_frame.mdl", build(frame, [REST, COCKED, PULLED], objs, to_body, w, h), [bytes(skin)],
          "frame", log)
    write(out_dir, "v_revolver_crane.mdl", build([src("Crane")], [REST], objs, lambda wm: wm["Crane"].inverted(), w,
                                                 h), [bytes(skin)], "crane", log)
    write(out_dir, "v_revolver_cyl.mdl", build([src("Cylinder"), src("EjectorRod")], [REST, {"eject": 1.0}], objs,
                                               lambda wm: wm["Cylinder"].inverted(), w, h), [bytes(skin)], "cyl", log)
    write(out_dir, "v_revolver_shell.mdl", build([Source(shell_tris, lambda wm: wm["Shell"])], [REST], objs,
                                                 lambda wm: wm["Shell"].inverted(), sw, sh),
          [bytes(s[0]) for s in shell_skins], "shell", log)

    # The preview: the gun's skin with the red and the blue shell's below it.
    k = float(body.get("preview_scale", 0.45))
    ph = h + sh
    pskin = bytearray(skin) + bytearray(w * sh)
    for v, (s_idx, _, _) in enumerate(shell_skins[:2]):
        for t in range(sh):
            for s in range(sw):
                if v * sw + s < w:
                    pskin[(h + t) * w + v * sw + s] = s_idx[t * sw + s]
    proud = float(shell.get("proud", 0.1))
    chambered = [Source(shell_tris, lambda wm, i=i: wm["Cylinder"] @ chamber_matrix(cyl, i, proud),
                        ((i % 2) * sw, h)) for i in range(6)]
    to_pre = lambda wm: Matrix.Scale(k, 4) @ wm["Body"].inverted()  # noqa: E731
    write(out_dir, "vr_revolver_preview.mdl", build(gun + chambered, [REST, SWUNG, COCKED], objs, to_pre, w, ph),
          [bytes(pskin)], "preview", log)

    # Where the parts sit in the Body's frame (for code that draws them as models of their own).
    rest = posed(objs, REST)
    inv = rest["Body"].inverted()
    for n in ("Crane", "Cylinder", "EjectorRod", "Hammer", "Trigger"):
        loc = (inv @ rest[n]).to_translation()
        log("%s origin in the Body's frame: %.3f %.3f %.3f" % (n, loc.x, loc.y, loc.z))
    loc = (rest["Crane"].inverted() @ rest["Cylinder"]).to_translation()
    log("Cylinder origin in the Crane's frame: %.3f %.3f %.3f" % (loc.x, loc.y, loc.z))
    for name in ("front_sight", "rear_notch"):
        if name in body:
            p = Vector(body[name])
            log("%s (Body frame): %.3f %.3f %.3f" % (name, p.x, p.y, p.z))


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    export(argv[0] if argv else None)
