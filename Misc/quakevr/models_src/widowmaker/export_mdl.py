# export_mdl.py -- writes the Widowmaker's Quake MDLs from widowmaker.blend (README.md):
#
#   blender -b Misc/quakevr/models_src/widowmaker/widowmaker.blend -P Misc/quakevr/models_src/widowmaker/export_mdl.py
#           [-- <output progs folder>]      (default: the repository's quakevr/progs)
#
# Writes, from the scene's mesh objects (modifiers applied; one whose name starts with "_" is left out):
#   progs/v_widowmaker.mdl          every mesh, in the Body object's frame (its origin the model's: the super shotgun's
#                                   frame, as the Widowmaker made from AD's view model, so weapon slot 34's settings hold
#                                   it; +X forward, +Y left, +Z up), packed from that model's corner CORNER (the settings
#                                   scale about it). 36 frames: vr0..vr8 the gun at rest (the weapon animations' frame
#                                   numbers), then twin0..twin26 the Barrels object (and anything parented to it) turned
#                                   about the spin axis (y 0, z SPIN_Z, along x) by k * 360/27 degrees, clockwise seen from
#                                   behind: the Gatling spin (Quake/vr/vr_adweapons.cpp spinFrame; "twin": two barrels, a
#                                   shot each half turn).
#   progs/vr_widowmaker_preview.mdl the gun at rest scaled by the Body's "preview_scale" (its size in the hand), one frame:
#                                   to look at in the game (README.md, "In the game").
# The skin is the image of the Body's material (widowmaker_skin.png), each texel made the nearest of Quake's 224 ordinary
# palette colours (also saved as widowmaker_skin_pal.png). Triangles as Blender triangulates the faces; texel
# coordinates the UVs' texels (u * width, (1 - v) * height, rounded down); flat shaded (each face's own normal).

import importlib.util
import math
import os
import sys

import bpy
from mathutils import Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
QVR = os.path.normpath(os.path.join(HERE, "..", ".."))           # Misc/quakevr
REPO = os.path.normpath(os.path.join(QVR, "..", ".."))
sys.path.insert(0, QVR)
import mdlgen  # noqa: E402  (Misc/quakevr/mdlgen.py: the MDL writer the generated models share)


def load_flaregun_export():
    # (the flare gun's exporter: its palette, PNG and triangle helpers are shared, not copied)
    path = os.path.join(HERE, "..", "flaregun", "export_mdl.py")
    spec = importlib.util.spec_from_file_location("fg_export", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


fg = load_flaregun_export()
qpal = fg.qpal

GUN_MDL = "v_widowmaker.mdl"
PREVIEW_MDL = "vr_widowmaker_preview.mdl"
CORNER = (-15.5775, -4.6938, -4.2402)  # the super shotgun's model's scale_origin (v_shot2.mdl; vr_adweapons.cpp recipes)
SPIN_Z = 8.8                           # the spin axis: y 0, z SPIN_Z, along x (the muzzle anchor 44 0 8.8 on it)
STILL_FRAMES, SPIN_POSES = 9, 27
MARKERS = ("_Muzzle", "_SightRear", "_SightFront", "_HandAnchor", "_TwoHHandAnchor", "_ShellPort", "_EjectSlot")


def spun(mesh, spin_count, k):
    """mesh with its first spin_count vertices turned pose k about the spin axis (as vr_adweapons.cpp's posed)."""
    a = -math.radians(360.0 / SPIN_POSES * k)
    ca, sa = math.cos(a), math.sin(a)
    out = mdlgen.Mesh(mesh.skin_w, mesh.skin_h, {})
    out.tris = mesh.tris
    for i, (p, n, st) in enumerate(mesh.verts):
        if i < spin_count:
            y, z = p[1], p[2] - SPIN_Z
            p = (p[0], y * ca - z * sa, SPIN_Z + y * sa + z * ca)
            n = (n[0], n[1] * ca - n[2] * sa, n[1] * sa + n[2] * ca)
        out.verts.append((p, n, st))
    return out


def export(out_dir=None, log=print):
    out_dir = out_dir or os.path.join(REPO, "quakevr", "progs")
    scene = bpy.context.scene
    objs = {ob.name: ob for ob in scene.objects}
    body, barrels = objs.get("Body"), objs.get("Barrels")
    if body is None or barrels is None:
        raise RuntimeError("widowmaker.blend needs the objects Body and Barrels")
    depsgraph = bpy.context.evaluated_depsgraph_get()
    skin0, w, h = fg.quantize(fg.skin_image(body))
    fg.write_png_indexed(os.path.join(HERE, "widowmaker_skin_pal.png"), w, h, skin0, qpal.PALETTE)
    skins = [bytes(skin0)]

    to_body = body.matrix_world.inverted()

    def spins(ob):
        while ob is not None:
            if ob is barrels:
                return True
            ob = ob.parent
        return False

    meshes = [ob for ob in scene.objects if ob.type == "MESH" and not ob.name.startswith("_")]
    turning = sorted([ob for ob in meshes if spins(ob)], key=lambda ob: ob.name)
    still = sorted([ob for ob in meshes if not spins(ob)], key=lambda ob: (ob is not body, ob.name))
    # The turning vertices first (a shared vertex is never both: build_mesh keys by position, normal and texel, and a
    # turning part's faces are its own).
    spin_mesh = fg.build_mesh([fg.object_tris(ob, to_body, w, h, depsgraph) for ob in turning], w, h)
    spin_count = len(spin_mesh.verts)
    mesh = fg.build_mesh([fg.object_tris(ob, to_body, w, h, depsgraph) for ob in turning + still], w, h)
    assert all(mesh.verts[i][0] == spin_mesh.verts[i][0] for i in range(spin_count))
    frames = [mesh] * (STILL_FRAMES - 1) + [spun(mesh, spin_count, k) for k in range(SPIN_POSES)]
    names = ["vr%d" % i for i in range(STILL_FRAMES)] + ["twin%d" % k for k in range(SPIN_POSES)]
    os.makedirs(out_dir, exist_ok=True)
    mdlgen.write_mdl(os.path.join(out_dir, GUN_MDL), mesh, skins, "vr0", frames, names, CORNER)
    log("%s: %d triangles, %d vertices (%d turning), %d frames, skin %dx%d (%s; turning: %s)" % (
        GUN_MDL, len(mesh.tris), len(mesh.verts), spin_count, len(names), w, h, ", ".join(ob.name for ob in still),
        ", ".join(ob.name for ob in turning)))
    for ob in meshes:
        n = sum(len(p.vertices) - 2 for p in ob.data.polygons)
        log("  %s: %d triangles" % (ob.name, n))
    lo = [min(v[0][k] for v in mesh.verts) for k in range(3)]
    hi = [max(v[0][k] for v in mesh.verts) for k in range(3)]
    log("  box %.2f %.2f %.2f .. %.2f %.2f %.2f (packed from %.4f %.4f %.4f)" % (*lo, *hi, *CORNER))
    for name in MARKERS:
        m = objs.get(name)
        if m is not None:
            p = to_body @ m.matrix_world.translation
            log("%s in the Body's frame: %.3f %.3f %.3f" % (name, p.x, p.y, p.z))

    k = float(body.get("preview_scale", 0.48))
    pre = fg.build_mesh([fg.object_tris(ob, Matrix.Scale(k, 4) @ to_body, w, h, depsgraph) for ob in turning + still],
                        w, h)
    mdlgen.write_mdl(os.path.join(out_dir, PREVIEW_MDL), pre, [skins[0]], "preview")
    log("%s: %d triangles (scale %.2f)" % (PREVIEW_MDL, len(pre.tris), k))


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    export(argv[0] if argv else None)
