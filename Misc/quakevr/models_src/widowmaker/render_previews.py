# render_previews.py -- renders the exported Widowmaker (the MDL as the game reads it) for review, into renders/:
#
#   blender -b --factory-startup -P Misc/quakevr/models_src/widowmaker/render_previews.py [-- <view> ...]
#
# Views (all by default): three_quarter, first_person, side, side_out, front, back, sights, spin, compare, sheet.
# "spin" draws spin frame twin7 (the barrels turned 93 degrees, as they pass while firing); "compare" sets the
# three-quarter and first-person renders beside the mockup's halves (qvr-kit/scratch/widowmaker/mockup.png, when there).

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
MOCKUP = "C:/OHWorkspace/qvr-kit/scratch/widowmaker/mockup.png"
sys.path.insert(0, os.path.join(QVR, "blender", "addons"))
from quakevr_models import mdl_blender  # noqa: E402

sys.path.insert(0, HERE)
import build_widowmaker as B  # noqa: E402  (its constants only)


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
    bg.inputs["Color"].default_value = (0.022, 0.022, 0.024, 1.0)
    bg.inputs["Strength"].default_value = 1.0
    for name, rot, energy in (("key", (math.radians(45), 0, math.radians(-120)), 3.0),
                              ("fill", (math.radians(65), 0, math.radians(30)), 1.4),
                              ("rim", (math.radians(110), 0, math.radians(100)), 0.8)):
        ld = bpy.data.lights.new(name, "SUN")
        ld.energy = energy
        lo = bpy.data.objects.new(name, ld)
        lo.rotation_euler = rot
        sc.collection.objects.link(lo)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    return sc, cam


def load(name):
    ob = mdl_blender.import_mdl(bpy.context, os.path.join(PROGS, name))
    mat = ob.data.materials[0]
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Roughness"].default_value = 0.85
    for key in ("Specular IOR Level", "Specular"):
        if key in bsdf.inputs:
            bsdf.inputs[key].default_value = 0.15
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


def render(sc, path):
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("rendered", path)


def shoot(sc, cam, objs, direction, path, ortho=True, margin=1.15, lens=50.0, target=None):
    lo, hi = bounds(objs)
    centre = target if target is not None else (lo + hi) / 2
    size = (hi - lo).length
    d = Vector(direction).normalized()
    cam.data.type = "ORTHO" if ortho else "PERSP"
    cam.data.lens = lens
    cam.data.clip_end = 1000.0
    if ortho:
        aim(cam, centre + d * size * 2, centre)
        inv = cam.matrix_world.inverted()
        cs = [inv @ Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
        w = max(c.x for c in cs) - min(c.x for c in cs)
        h = max(c.y for c in cs) - min(c.y for c in cs)
        aspect = sc.render.resolution_x / sc.render.resolution_y
        cam.data.ortho_scale = max(w, h * aspect) * margin
    else:
        dist = size * 0.5 * lens / 18.0 * margin
        aim(cam, centre + d * dist, centre)
    render(sc, path)


def pixels(p):
    im = bpy.data.images.load(p)
    return np.array(im.pixels[:], dtype=np.float32).reshape(im.size[1], im.size[0], im.channels)[::-1, :, :3]


def resize(a, h):
    w = int(round(a.shape[1] * h / a.shape[0]))
    ys = (np.arange(h) * a.shape[0] / h).astype(int)
    xs = (np.arange(w) * a.shape[1] / w).astype(int)
    return a[ys][:, xs]


def save(a, path):
    h, w = a.shape[:2]
    img = bpy.data.images.new("out", w, h, alpha=False)
    rgba = np.concatenate([a[::-1], np.ones((h, w, 1), dtype=np.float32)], axis=2)
    img.pixels = rgba.ravel()
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    print("rendered", path)


def compare(three_quarter, first_person, path):
    """The mockup's halves above, our first-person and three-quarter renders below, each 600 tall."""
    if not os.path.exists(MOCKUP):
        print("no mockup at", MOCKUP)
        return
    m = pixels(MOCKUP)
    half = m.shape[1] // 2
    top = np.concatenate([resize(m[:, :half], 600), resize(m[:, half:], 600)], axis=1)
    bot = np.concatenate([resize(pixels(first_person), 600), resize(pixels(three_quarter), 600)], axis=1)
    w = max(top.shape[1], bot.shape[1])
    pad = lambda a: np.concatenate([a, np.zeros((a.shape[0], w - a.shape[1], 3), dtype=np.float32)], axis=1)  # noqa
    save(np.concatenate([pad(top), pad(bot)], axis=0), path)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    views = set(argv) or {"three_quarter", "first_person", "side", "side_out", "front", "back", "sights", "spin",
                          "compare", "sheet"}
    os.makedirs(OUT, exist_ok=True)
    sc, cam = setup()
    out = lambda n: os.path.join(OUT, n + ".png")  # noqa: E731
    gun = load("v_widowmaker.mdl")
    if "three_quarter" in views or "compare" in views:
        sc.render.resolution_x, sc.render.resolution_y = 1000, 1000
        shoot(sc, cam, [gun], (-0.55, 0.5, 0.62), out("three_quarter"), ortho=False, margin=0.95, lens=85.0)
    if "first_person" in views or "compare" in views:
        # Behind and above the rear grip, a little to its left (the eye of the hand holding it at the hip).
        sc.render.resolution_x, sc.render.resolution_y = 800, 1000
        cam.data.type = "PERSP"
        cam.data.lens = 20.0
        aim(cam, Vector((-9.0, 7.5, 12.0)), Vector((30.0, -2.0, 7.0)))
        render(sc, out("first_person"))
    sc.render.resolution_x, sc.render.resolution_y = 1400, 600
    if "side" in views:
        shoot(sc, cam, [gun], (0, 1, 0), out("side"), margin=1.05)
    if "side_out" in views:
        shoot(sc, cam, [gun], (0, -1, 0), out("side_out"), margin=1.05)
    sc.render.resolution_x, sc.render.resolution_y = 800, 800
    if "front" in views:
        shoot(sc, cam, [gun], (1, 0, 0), out("front"), margin=1.3)
    if "back" in views:
        shoot(sc, cam, [gun], (-1, 0, 0), out("back"), margin=1.3)
    if "sights" in views:
        # The eye on the sight line behind the rear sight, looking down it at the front post.
        cam.data.type = "PERSP"
        cam.data.lens = 135.0
        aim(cam, Vector((B.SIGHT_REAR_X - 24.0, 0, B.SIGHT_Z + 0.12)), Vector((B.SIGHT_REAR_X + 40.0, 0, B.SIGHT_Z + 0.04)))
        render(sc, out("sights"))
    if "spin" in views:
        keys = gun.data.shape_keys
        if keys and "twin7" in keys.key_blocks:
            keys.key_blocks["twin7"].value = 1.0
            sc.render.resolution_x, sc.render.resolution_y = 1000, 1000
            shoot(sc, cam, [gun], (-0.55, 0.5, 0.62), out("spin"), ortho=False, margin=0.95, lens=85.0)
            keys.key_blocks["twin7"].value = 0.0
        else:
            print("no spin frame twin7")
    if "compare" in views:
        compare(out("three_quarter"), out("first_person"), out("compare_mockup"))
    if "sheet" in views:
        # The other views on one page (side, outer side; back, front, sights, spin), to look at once.
        names = [["side", "side_out"], ["back", "front", "sights", "spin"]]
        rows = []
        for row in names:
            ims = [resize(pixels(out(n)), 360 if len(row) == 2 else 300) for n in row if os.path.exists(out(n))]
            rows.append(np.concatenate(ims, axis=1))
        w = max(r.shape[1] for r in rows)
        pad = lambda a: np.concatenate([a, np.zeros((a.shape[0], w - a.shape[1], 3), dtype=np.float32)], axis=1)  # noqa
        save(np.concatenate([pad(r) for r in rows], axis=0), out("sheet"))


main()
