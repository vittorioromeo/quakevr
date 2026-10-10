# render_views.py -- renders a Quake VR .mdl (its skin 0, frame 0, flat-lit as the engine's software look) from several
# angles, headless, for before/after checks of model edits:
#   blender -b --factory-startup --python-exit-code 1 -P Misc/quakevr/blender/render_views.py -- model.mdl out_prefix
#           [size] [view ...]   (model.mdl may be several joined by '+': a gun with its magazine and well)
# Writes out_prefix_<view>.png per view. Views: left, right, top, bottom, front, back, the three-quarter ones
# (fl: front-left from above, bl: back-left from below, br: back-right from below), and close-ups of a box given as
# zoom:x0,x1,y0,y1,z0,z1:dir (dir one of the views above; the camera frames the box). Model space: +x forward,
# +y left, +z up. Orthographic, Workbench (studio light, the skin's texels unfiltered).

import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "addons"))

from quakevr_models import mdl_blender as MB  # noqa: E402

DIRS = {
    "left": (0, 1, 0), "right": (0, -1, 0), "top": (0, 0, 1), "bottom": (0, 0, -1), "front": (1, 0, 0),
    "back": (-1, 0, 0), "fl": (0.6, 0.6, 0.5), "bl": (-0.6, 0.6, -0.5), "br": (-0.6, -0.6, -0.5),
    "fbl": (0.6, 0.6, -0.5), "fbr": (0.6, -0.6, -0.5),
}


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    path, prefix = argv[0], argv[1]
    size = int(argv[2]) if len(argv) > 2 else 512
    views = argv[3:] or ["left", "right", "top", "bottom", "fl", "bl"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    obs = [MB.import_mdl(bpy.context, one) for one in path.split("+")]  # (a+b: drawn together, e.g. a gun and its well)
    for ob in obs:
        if ob.data.shape_keys:
            for kb in ob.data.shape_keys.key_blocks[1:]:
                kb.value = 0.0
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    # QVR_RENDER_LIGHT=FLAT shows the skin's texels as they are (UV checks); STUDIO (default) the shape.
    sc.display.shading.light = os.environ.get("QVR_RENDER_LIGHT", "STUDIO")
    sc.display.shading.color_type = "TEXTURE"
    sc.display.shading.show_cavity = False
    sc.render.resolution_x = sc.render.resolution_y = size
    sc.render.film_transparent = False
    sc.world = bpy.data.worlds.new("w")
    sc.world.color = (0.25, 0.25, 0.28)
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.exposure = float(os.environ.get("QVR_RENDER_EXPOSURE", "1.0"))
    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam = bpy.data.objects.new("cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    pts = [ob.matrix_world @ v.co for ob in obs for v in ob.data.vertices]
    for view in views:
        box = None
        name = view
        if view.startswith("zoom:"):
            _, b, d = view.split(":")
            b = [float(x) for x in b.split(",")]
            box = (Vector((b[0], b[2], b[4])), Vector((b[1], b[3], b[5])))
            name = "zoom_%s_%d" % (d, abs(hash(view)) % 1000)
            view = d
        if box is None:
            lo = Vector((min(p[i] for p in pts) for i in range(3)))
            hi = Vector((max(p[i] for p in pts) for i in range(3)))
        else:
            lo, hi = box
        c = (lo + hi) / 2
        d = Vector(DIRS[view]).normalized()
        up = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
        right = d.cross(up).normalized()
        up2 = right.cross(d).normalized()
        corners = [Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
        ext = max(max(abs((q - c).dot(right)), abs((q - c).dot(up2))) for q in corners)
        cam_data.ortho_scale = 2.1 * ext
        cam.location = c + d * 200.0
        cam_data.clip_end = 1000.0
        rot = (-d).to_track_quat("-Z", "Y" if abs(d.z) < 0.9 else "X")
        cam.rotation_euler = rot.to_euler()
        sc.render.filepath = "%s_%s.png" % (prefix, name)
        bpy.ops.render.render(write_still=True)


main()
