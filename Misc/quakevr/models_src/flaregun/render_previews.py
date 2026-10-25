# render_previews.py -- renders the exported flare gun (the MDLs as the game reads them) for review, into renders/:
#
#   blender -b --factory-startup -P Misc/quakevr/models_src/flaregun/render_previews.py [-- <view> ...]
#
# Views (all by default): three_quarter, side, front, back, sights, skins, flare, scale, compare. "scale" puts it beside
# the nail pistol and the shotgun, each at its in-hand scale, their hand points on one line; "compare" sets the
# three-quarter render beside the mockup's (qvr-kit/scratch/flaregun/mockup.webp, when it is there).

import math
import os
import sys

import bpy
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
QVR = os.path.normpath(os.path.join(HERE, "..", ".."))
REPO = os.path.normpath(os.path.join(QVR, "..", ".."))
PROGS = os.path.join(REPO, "quakevr", "progs")
OUT = os.path.join(HERE, "renders")
MOCKUP = "C:/OHWorkspace/qvr-kit/scratch/flaregun/mockup.webp"
sys.path.insert(0, os.path.join(QVR, "blender", "addons"))
from quakevr_models import mdl_blender  # noqa: E402

sys.path.insert(0, HERE)
import build_flaregun as B  # noqa: E402  (its constants only)

# The sight line in v_flaregun.mdl's frame (export_mdl.py prints it).
SIGHT_Z, SIGHT_REAR_X = B.SIGHT_Z - B.HAND.z, B.SIGHT_REAR_X - B.HAND.x


def setup():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            sc.render.engine = eng
            break
        except TypeError:
            pass
    sc.view_settings.view_transform = "Standard"
    sc.render.resolution_x, sc.render.resolution_y = 1000, 800
    sc.render.film_transparent = False
    world = bpy.data.worlds.new("w")
    sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.045, 0.045, 0.05, 1.0)
    bg.inputs["Strength"].default_value = 1.0
    for name, rot, energy in (("key", (math.radians(40), 0, math.radians(150)), 4.0),
                              ("fill", (math.radians(70), 0, math.radians(-40)), 1.1),
                              ("rim", (math.radians(110), 0, math.radians(60)), 0.9)):
        ld = bpy.data.lights.new(name, "SUN")
        ld.energy = energy
        lo = bpy.data.objects.new(name, ld)
        lo.rotation_euler = rot
        sc.collection.objects.link(lo)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    return sc, cam


def load(name, scale=1.0, offset=Vector(), skin=0):
    ob = mdl_blender.import_mdl(bpy.context, os.path.join(PROGS, name))
    ob.matrix_world = Matrix.Translation(offset) @ Matrix.Scale(scale, 4)
    mat = ob.data.materials[0]
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Roughness"].default_value = 0.85
    for key in ("Specular IOR Level", "Specular"):
        if key in bsdf.inputs:
            bsdf.inputs[key].default_value = 0.15
    if skin:
        m2 = mat.copy()
        tex = next(n for n in m2.node_tree.nodes if n.type == "TEX_IMAGE")
        tex.image = bpy.data.images[ob["qvr_images"][skin]]
        ob.data = ob.data.copy()
        ob.data.materials[0] = m2
    return ob


def bounds(objs):
    pts = [ob.matrix_world @ Vector(c) for ob in objs for c in ob.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def aim(cam, eye, target, up=Vector((0, 0, 1))):
    f = (target - eye).normalized()
    r = f.cross(up).normalized()
    u = r.cross(f)
    cam.matrix_world = Matrix((r, u, -f)).transposed().to_4x4()
    cam.location = eye


def shoot(sc, cam, objs, direction, path, ortho=True, margin=1.15, lens=50.0, target=None, up=Vector((0, 0, 1))):
    lo, hi = bounds(objs)
    centre = target if target is not None else (lo + hi) / 2
    size = (hi - lo).length
    d = Vector(direction).normalized()
    cam.data.type = "ORTHO" if ortho else "PERSP"
    cam.data.lens = lens
    cam.data.clip_end = 1000.0
    if ortho:
        aim(cam, centre + d * size * 2, centre, up)
        # Fit the bounds' corners, as the camera sees them.
        inv = cam.matrix_world.inverted()
        cs = [inv @ Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
        w = max(c.x for c in cs) - min(c.x for c in cs)
        h = max(c.y for c in cs) - min(c.y for c in cs)
        aspect = sc.render.resolution_x / sc.render.resolution_y
        cam.data.ortho_scale = max(w, h * aspect) * margin
    else:
        dist = size * 0.5 * lens / 18.0 * margin
        aim(cam, centre + d * dist, centre, up)
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("rendered", path)


def clear():
    for ob in list(bpy.data.objects):
        if ob.type == "MESH":
            bpy.data.objects.remove(ob)


def compare(three_quarter, path):
    """The mockup's right half beside the three-quarter render, both 800 tall."""
    if not os.path.exists(MOCKUP):
        print("no mockup at", MOCKUP)
        return
    def pixels(p):
        im = bpy.data.images.load(p)
        a = np.array(im.pixels[:], dtype=np.float32).reshape(im.size[1], im.size[0], im.channels)[:, :, :3]
        return a
    m = pixels(MOCKUP)
    m = m[:, m.shape[1] // 2:]
    r = pixels(three_quarter)
    def resize(a, h):
        w = int(round(a.shape[1] * h / a.shape[0]))
        ys = (np.arange(h) * a.shape[0] / h).astype(int)
        xs = (np.arange(w) * a.shape[1] / w).astype(int)
        return a[ys][:, xs]
    m, r = resize(m, 800), resize(r, 800)
    out = np.concatenate([m, r], axis=1)
    h, w = out.shape[:2]
    img = bpy.data.images.new("compare", w, h, alpha=False)
    rgba = np.concatenate([out, np.ones((h, w, 1), dtype=np.float32)], axis=2)
    img.pixels = rgba.ravel()
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    print("rendered", path)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    views = set(argv) or {"three_quarter", "side", "front", "back", "sights", "skins", "flare", "scale", "compare"}
    os.makedirs(OUT, exist_ok=True)
    sc, cam = setup()
    out = lambda n: os.path.join(OUT, n + ".png")  # noqa: E731
    gun = load("v_flaregun.mdl")
    if "three_quarter" in views or "compare" in views:
        sc.render.resolution_x, sc.render.resolution_y = 1000, 1000
        shoot(sc, cam, [gun], (0.62, 0.72, 0.22), out("three_quarter"), ortho=False, margin=1.15)
        sc.render.resolution_x, sc.render.resolution_y = 1000, 800
    if "side" in views:
        shoot(sc, cam, [gun], (0, 1, 0), out("side"))
    if "front" in views:
        sc.render.resolution_x, sc.render.resolution_y = 800, 800
        shoot(sc, cam, [gun], (1, 0, 0), out("front"))
    if "back" in views:
        sc.render.resolution_x, sc.render.resolution_y = 800, 800
        shoot(sc, cam, [gun], (-1, 0, 0), out("back"))
    if "sights" in views:
        # The eye on the sight line, behind the rear notch, looking down it at the front blade.
        sc.render.resolution_x, sc.render.resolution_y = 800, 800
        cam.data.type = "PERSP"
        cam.data.lens = 135.0
        aim(cam, Vector((SIGHT_REAR_X - 12.0, 0, SIGHT_Z + 0.05)), Vector((SIGHT_REAR_X + 20.0, 0, SIGHT_Z + 0.05)))
        sc.render.filepath = out("sights")
        bpy.ops.render.render(write_still=True)
        print("rendered", out("sights"))
    sc.render.resolution_x, sc.render.resolution_y = 1000, 800
    if "skins" in views:
        guns = [gun] + [load("v_flaregun.mdl", 1.0, Vector((0, -4.2 * k, 0)), k) for k in (1, 2)]
        lo, hi = bounds(guns)
        sc.render.resolution_x, sc.render.resolution_y = 1200, 600
        shoot(sc, cam, guns, (0.75, -0.25, 0.6), out("skins"), ortho=True, margin=1.0,
              target=Vector(((lo.x + hi.x) / 2 + 3.0, (lo.y + hi.y) / 2, 6.0)))
        for g in guns[1:]:
            bpy.data.objects.remove(g)
        sc.render.resolution_x, sc.render.resolution_y = 1000, 800
    if "flare" in views:
        gun.hide_render = True
        fl = load("vr_flare.mdl")
        sc.render.resolution_x, sc.render.resolution_y = 800, 500
        shoot(sc, cam, [fl], (0.6, 0.7, 0.4), out("flare"), ortho=True, margin=1.3)
        bpy.data.objects.remove(fl)
        gun.hide_render = False
        sc.render.resolution_x, sc.render.resolution_y = 1000, 800
    if "scale" in views:
        # Each at its in-hand scale (vr_weapons.inc: v_shot 0.44, the nail pistol 0.45, this 0.45), hand points on
        # one line (v_shot's hand anchor 2.0562 0.0247 1.2804).
        gun.matrix_world = Matrix.Scale(0.45, 4)
        pistol = load("v_nailpistol.mdl", 0.45, Vector((8.0, 0, 0)))
        shot = load("v_shot.mdl", 0.44, Vector((17.0, 0, 0)) - Vector((2.0562, 0.0247, 1.2804)) * 0.44)
        sc.render.resolution_x, sc.render.resolution_y = 1400, 600
        shoot(sc, cam, [gun, pistol, shot], (0, 1, 0), out("scale_vs_nailpistol_shotgun"), margin=1.08)
    if "compare" in views:
        compare(out("three_quarter"), out("compare_mockup"))


main()
