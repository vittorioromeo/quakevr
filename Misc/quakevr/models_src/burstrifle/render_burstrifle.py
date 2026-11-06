# render_burstrifle.py -- renders the burst rifle's source .blend (Misc/quakevr/src_models/blend/v_gruntgun/) at the two
# angles of Vittorio's reference (first person, from behind the gun's left; a turntable three-quarter view of its left
# side), its skin's texels unfiltered, and puts each beside the reference's half when one is given:
#
#   blender -b Misc/quakevr/src_models/blend/v_gruntgun/v_gruntgun.blend -P Misc/quakevr/models_src/burstrifle/render_burstrifle.py -- <out.png> [reference image]
#
# The reference is 3:1 per half (top: first person, bottom: turntable), as his was (1536 x 1024).

import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

W, H = 1536, 512
VIEWS = {  # camera place, where it looks, lens (mm)
    "first person": (Vector((-34.0, 9.5, 12.5)), Vector((60.0, -1.0, 5.0)), 15.0),
    "turntable": (Vector((30.0, 88.0, 46.0)), Vector((4.0, 0.0, 3.5)), 52.0),
}


def setup():
    sc = bpy.context.scene
    ob = [o for o in sc.objects if o.type == 'MESH'][0]
    if ob.data.shape_keys:
        ob.active_shape_key_index = 0
    img = [i for i in bpy.data.images if "skin" in i.name][0]
    mat = bpy.data.materials.new("preview")
    if bpy.app.version < (5, 0, 0):
        mat.use_nodes = True
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = "Closest"
    mat.node_tree.nodes.active = tex
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    for e in sc.objects:
        if e.type == 'EMPTY':
            e.hide_render = True
    sc.render.engine = "BLENDER_WORKBENCH"
    sh = sc.display.shading
    sh.light = "STUDIO"
    sh.color_type = "TEXTURE"
    sh.show_specular_highlight = False
    sh.show_cavity = False
    sc.world = sc.world or bpy.data.worlds.new("w")
    sc.world.color = (0.035, 0.036, 0.04)
    sc.render.resolution_x, sc.render.resolution_y = W, H
    sc.render.film_transparent = False
    sc.view_settings.view_transform = "Standard"
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    return sc, cam


def render(sc, cam, view, path):
    at, look, lens = VIEWS[view]
    cam.location = at
    cam.rotation_euler = (look - at).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = lens
    cam.data.clip_start = 0.5
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


def pixels(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    a = np.array(img.pixels[:], np.float32).reshape(h, w, 4)[::-1]  # top row first
    bpy.data.images.remove(img)
    return a


def half(a):
    h, w = a.shape[0] // 2 * 2, a.shape[1] // 2 * 2
    a = a[:h, :w]
    return 0.25 * (a[0::2, 0::2] + a[1::2, 0::2] + a[0::2, 1::2] + a[1::2, 1::2])


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(argv[0])
    ref = argv[1] if len(argv) > 1 else None
    sc, cam = setup()
    base = os.path.splitext(out)[0]
    shots = []
    for i, view in enumerate(VIEWS):
        p = "%s_%d.png" % (base, i)
        render(sc, cam, view, p)
        shots.append(pixels(p))
    cells = []
    if ref:
        r = pixels(ref)
        rh = r.shape[0] // 2
        for i, s in enumerate(shots):
            cells.append(np.concatenate([half(r[i * rh:(i + 1) * rh, :W]), half(s)], axis=1))
    else:
        cells = [half(s) for s in shots]
    grid = np.concatenate(cells, axis=0)
    h, w = grid.shape[:2]
    img = bpy.data.images.new("compare", w, h, alpha=True)
    img.pixels.foreach_set(np.ascontiguousarray(grid[::-1]).ravel())
    img.filepath_raw = out
    img.file_format = "PNG"
    img.save()
    print("render_burstrifle: %s (%dx%d)" % (out, w, h))


main()
