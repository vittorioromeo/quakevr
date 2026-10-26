# render_revolver.py -- renders the heavy revolver's exported MDLs (README.md, "Renders"), headless:
#
#   blender -b --factory-startup --python-exit-code 1 -P Misc/quakevr/models_src/revolver/render_revolver.py --
#           <out folder> [size] [view ...]
#
# Views (default: all): three (three-quarter, front-left from above, as the mockup's right half), side (the left side),
# right (the right side), open (swung out, from behind: the chambers and the shells), openfront (swung out, from the
# front), cocked (the hammer back, the left side), sights (from behind along the sight line, perspective), fp (first
# person: behind and above, as held), shells (the shell's four skins), scale (beside the shotgun and the nail pistol,
# each at its in-hand scale). Workbench, studio light, the skin's texels unfiltered; written as <out>/<view>.png.

import math
import os
import sys

import bpy
from mathutils import Euler, Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
PROGS = os.path.join(REPO, "quakevr", "progs")
sys.path.insert(0, os.path.join(REPO, "Misc", "quakevr", "blender", "addons"))
from quakevr_models import mdl_blender as MB  # noqa: E402

PREVIEW_SCALE = 0.45


def load(name, frame=0, scale=1.0, at=Vector()):
    ob = MB.import_mdl(bpy.context, os.path.join(PROGS, name))
    bpy.context.scene.frame_set(frame + 1)  # (the importer keys frame n of the MDL at scene frame n + 1)
    ob.scale = (scale, scale, scale)
    ob.location = at
    return ob


def set_skin(ob, path):
    img = bpy.data.images.load(path, check_existing=True)
    mat = ob.active_material.copy()
    for node in mat.node_tree.nodes:
        if node.type == "TEX_IMAGE":
            node.image = img
            node.interpolation = "Closest"
    ob.active_material = mat


def points(obs):
    bpy.context.view_layer.update()
    out = []
    for ob in obs:
        ev = ob.evaluated_get(bpy.context.evaluated_depsgraph_get())
        me = ev.to_mesh()
        out += [ob.matrix_world @ v.co for v in me.vertices]
        ev.to_mesh_clear()
    return out


def camera(sc):
    cam_data = bpy.data.cameras.new("cam")
    cam = bpy.data.objects.new("cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam_data.clip_start, cam_data.clip_end = 0.05, 2000.0
    return cam


def aim(cam, obs, d, roll=0.0, ortho=True, margin=1.12):
    """Looks along -d at obs' middle, framing them; roll (degrees) about the view."""
    pts = points(obs)
    lo = Vector((min(p[i] for p in pts) for i in range(3)))
    hi = Vector((max(p[i] for p in pts) for i in range(3)))
    c = (lo + hi) / 2
    d = Vector(d).normalized()
    q = (-d).to_track_quat("-Z", "Y" if abs(d.z) < 0.9 else "X")
    q = q @ Euler((0, 0, math.radians(roll))).to_quaternion()
    m = q.to_matrix()
    right, up = m.col[0], m.col[1]
    aspect = bpy.context.scene.render.resolution_x / bpy.context.scene.render.resolution_y  # (>= 1 here)
    ext = max(max(abs((p - c).dot(right)) for p in pts) / aspect, max(abs((p - c).dot(up)) for p in pts))
    ext_w = ext * aspect
    cam.rotation_euler = q.to_euler()
    if ortho:
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = 2.0 * ext_w * margin
        cam.location = c + d * 300.0
    else:
        cam.data.type = "PERSP"
        cam.data.lens = 50.0
        dist = ext * margin / math.tan(cam.data.angle / 2)
        cam.location = c + d * dist


def body_props():
    """The Body's custom properties in revolver.blend (the sight line)."""
    with bpy.data.libraries.load(os.path.join(HERE, "revolver.blend")) as (src, dst):
        dst.objects = ["Body"]
    body = dst.objects[0]
    return {k: body[k] for k in body.keys()}


def clear():
    for ob in list(bpy.data.objects):
        if ob.type == "MESH":
            bpy.data.objects.remove(ob)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    out = os.path.abspath(argv[0])
    size = int(argv[1]) if len(argv) > 1 else 768
    views = argv[2:] or ["three", "side", "right", "open", "openfront", "cocked", "sights", "fp", "shells", "scale"]
    os.makedirs(out, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "TEXTURE"
    sc.display.shading.show_cavity = False
    sc.render.resolution_x = sc.render.resolution_y = size
    sc.render.film_transparent = False
    sc.world = bpy.data.worlds.new("w")
    sc.world.color = (0.021, 0.021, 0.023)
    sc.view_settings.view_transform = "Standard"
    cam = camera(sc)
    for view in views:
        clear()
        sc.render.resolution_x = size
        if view in ("three", "side", "right", "open", "openfront", "cocked", "fp", "sights"):
            frame = {"open": 1, "openfront": 1, "cocked": 2}.get(view, 0)
            gun = load("vr_revolver_preview.mdl", frame)
            if view == "three":
                aim(cam, [gun], (0.55, 0.8, 0.18), roll=24.0, ortho=False, margin=1.25)

            elif view in ("side", "cocked"):
                aim(cam, [gun], (0, 1, 0))
            elif view == "right":
                aim(cam, [gun], (0, -1, 0))
            elif view == "open":
                aim(cam, [gun], (-0.6, 0.62, 0.42), roll=-10.0, ortho=False, margin=1.4)
            elif view == "openfront":
                aim(cam, [gun], (0.55, 0.75, 0.3), roll=-15.0, ortho=False, margin=1.4)
            elif view == "fp":
                # Behind the hand and above, a little to the left: about where the eye is when holding it.
                aim(cam, [gun], (-0.88, 0.32, 0.3), roll=0.0, ortho=False, margin=1.0)

            else:  # sights: the eye on the sight line, 25 cm behind the rear notch (in units at the preview's scale)
                props = body_props()
                notch = Vector(props["rear_notch"]) * float(props.get("preview_scale", PREVIEW_SCALE))
                cam.data.type = "PERSP"
                cam.data.lens = 150.0

                cam.location = notch + Vector((-25.0 / 3.81, 0.0, 0.0))
                cam.rotation_euler = Vector((1, 0, 0)).to_track_quat("-Z", "Y").to_euler()
        elif view == "shells":
            obs = []
            names = ["revolver_shell.png", "revolver_shell_slug.png", "revolver_shell_spent.png",
                     "revolver_shell_slug_spent.png"]
            for i, n in enumerate(names):
                ob = load("v_revolver_shell.mdl", 0, 1.0, Vector((0, -i * 1.8, 0)))
                set_skin(ob, os.path.join(HERE, n))
                obs.append(ob)
            # one standing on its head, its crimp up, one showing its head
            st = load("v_revolver_shell.mdl", 0, 1.0, Vector((1.0, 2.2, -1.2)))
            st.rotation_euler = (0, -math.pi / 2, 0)
            hd = load("v_revolver_shell.mdl", 0, 1.0, Vector((-1.6, 2.2, 0.6)))
            hd.rotation_euler = (0, math.pi / 2, 0)
            set_skin(hd, os.path.join(HERE, names[1]))
            obs += [st, hd]
            aim(cam, obs, (0.35, 0.8, 0.55), ortho=False, margin=1.4)
        elif view == "scale":
            sc.render.resolution_x = int(size * 1.5)
            # Each at its in-hand scale (vr_wofs_scale), stacked top to bottom, their origins in a vertical line.
            obs = [load("v_revolver.mdl", 0, 0.45), load("v_nailpistol.mdl", 0, 0.45), load("v_shot.mdl", 0, 0.44)]
            z = 0.0
            for ob in obs:
                pts = points([ob])
                ob.location.z = z - max(p.z for p in pts)
                z = z - (max(p.z for p in pts) - min(p.z for p in pts)) - 1.0
            aim(cam, obs, (0, 1, 0), margin=1.05)

        sc.render.filepath = os.path.join(out, view + ".png")
        bpy.ops.render.render(write_still=True)


if __name__ == "__main__":  # (other scripts import its helpers)
    main()
