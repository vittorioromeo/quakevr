# blender_scene.py -- the logo splash's 3D layers, rendered by Blender (Cycles on the GPU) headless:
#
#   blender -b --factory-startup -P blender_scene.py -- --pass <fg|letters|stills|test> [--width 3840]
#           [--samples 96] [--frames a-b] [--pak <id1/pak0.pak>] [--out <dir>]
#
#   fg       the grunt idling, the thrown axe, the gibs of the burst          -> <out>/fg/fg_####.png
#   letters  "QUAKE VR" falling in, letter by letter                        -> <out>/letters/letters_####.png
#   stills   the letters at rest (each alone, all, all lit by fire) and "UNLEASHED" (plain and lit by fire)
#            -> <out>/stills/*.png
#   test     a few frames of everything at a low resolution, for looking at the scene -> <out>/test/
#
# The PNGs are RGBA with straight alpha (transparent film), 8 bits. Frame numbers are the timeline's (common.py).
# The grunt, the gibs and the head come from the player's own id1/pak0.pak (read in place: nothing is extracted); the
# axe and the brains are Quake VR's own models (quakevr/progs). Motion blur is Cycles' (half a frame shutter).

import math
import os
import struct
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "Misc", "quakevr", "blender", "addons", "quakevr_models"))
import common as C  # noqa: E402
import mdl  # noqa: E402

FONT = r"C:\Windows\Fonts\ANTQUAB.TTF"      # Book Antiqua Bold: the closest installed face to the logo's capitals


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    a = dict(axe="qvr", pas="test", width=3840, samples=96, frames=None, pak=r"C:\OHWorkspace\qvr-kit\qbase\id1\pak0.pak",
             out=C.WORK)
    i = 0
    while i < len(argv):
        k = argv[i]
        v = argv[i + 1] if i + 1 < len(argv) else None
        if k == "--pass":
            a["pas"] = v
        elif k == "--width":
            a["width"] = int(v)
        elif k == "--samples":
            a["samples"] = int(v)
        elif k == "--frames":
            lo, _, hi = v.partition("-")
            a["frames"] = (int(lo), int(hi or lo))
        elif k == "--pak":
            a["pak"] = v
        elif k == "--axe":                       # id (id1's v_axe.mdl) or qvr (Quake VR's)
            a["axe"] = v
        elif k == "--nomb":                      # test: no motion blur
            a["nomb"] = v == "1"
        elif k == "--out":
            a["out"] = v
        elif k == "--border":                    # test: render only x0,y0,x1,y1 (fractions, y up)
            a["border"] = [float(x) for x in v.split(",")]
        i += 2
    return a


def read_pak(path):
    with open(path, "rb") as f:
        data = f.read()
    ident, off, ln = struct.unpack_from("<4sii", data, 0)
    if ident != b"PACK":
        raise SystemExit("%s is not a pak" % path)
    out = {}
    for i in range(ln // 64):
        raw, pos, size = struct.unpack_from("<56sii", data, off + i * 64)
        out[raw.split(b"\0")[0].decode("latin-1").lower()] = data[pos:pos + size]
    return out


# ---------------------------------------------------------------------------------------------------------------
# Scene

def setup_scene(width, samples):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    prefs.compute_device_type = "OPTIX"
    prefs.refresh_devices()
    for d in prefs.devices:
        d.use = d.type == "OPTIX"
    sc.cycles.device = "GPU"
    sc.cycles.samples = samples
    sc.cycles.use_adaptive_sampling = True
    sc.cycles.use_denoising = True
    try:
        sc.cycles.denoiser = "OPTIX"
    except TypeError:
        pass
    sc.cycles.max_bounces = 6
    sc.cycles.transparent_max_bounces = 8
    sc.render.film_transparent = True
    sc.render.resolution_x = width
    sc.render.resolution_y = width * 9 // 16
    sc.render.resolution_percentage = 100
    sc.render.fps = C.FPS
    sc.render.use_motion_blur = True
    sc.render.motion_blur_shutter = 0.5
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    im = sc.render.image_settings
    im.file_format = "PNG"
    im.color_mode = "RGBA"
    im.color_depth = "8"
    im.compression = 30

    cam_data = bpy.data.cameras.new("cam")
    cam_data.lens = C.FOCAL_MM
    cam_data.sensor_width = C.SENSOR_MM
    cam_data.sensor_fit = "HORIZONTAL"
    cam_data.clip_start = 0.05
    cam = bpy.data.objects.new("cam", cam_data)
    sc.collection.objects.link(cam)
    cam.location = (0.0, -C.CAM_DIST, 0.0)
    cam.rotation_euler = (math.pi / 2, 0.0, 0.0)
    sc.camera = cam

    # World: a dim warm top and a dark red bottom (reflections for the metal; never seen: the film is transparent).
    w = bpy.data.worlds.new("world")
    sc.world = w
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    bg = nt.nodes.new("ShaderNodeBackground")
    out = nt.nodes.new("ShaderNodeOutputWorld")
    nt.links.new(tc.outputs["Generated"], sep.inputs[0])
    nt.links.new(sep.outputs["Z"], ramp.inputs[0])
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = (0.06, 0.004, 0.002, 1)
    ramp.color_ramp.elements[1].position = 0.75
    ramp.color_ramp.elements[1].color = (0.55, 0.48, 0.42, 1)
    nt.links.new(ramp.outputs[0], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 0.35
    nt.links.new(bg.outputs[0], out.inputs[0])
    return sc


def add_light(name, kind, loc, target, energy, color=(1, 1, 1), size=1.0):
    ld = bpy.data.lights.new(name, kind)
    ld.energy = energy
    ld.color = color
    if kind == "AREA":
        ld.size = size
    elif kind in ("POINT", "SPOT"):
        ld.shadow_soft_size = size
    ob = bpy.data.objects.new(name, ld)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = loc
    d = Vector(target) - Vector(loc)
    ob.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    return ob


def setup_lights():
    L = {}
    L["key"] = add_light("key", "AREA", (-4.5, -8.0, 6.0), (0, 0, 0), 2600, (1.0, 0.93, 0.85), 3.0)
    L["fill"] = add_light("fill", "AREA", (6.0, -7.0, 0.5), (0, 0, 0), 900, (0.75, 0.82, 1.0), 4.0)
    L["top"] = add_light("top", "AREA", (0.5, -1.6, 5.0), (0, -0.3, 0.2), 1000, (1.0, 0.95, 0.9), 2.0)
    L["rim"] = add_light("rim", "AREA", (0.0, 1.2, 3.5), (0, -1.0, 0.0), 1400, (1.0, 0.15, 0.08), 2.0)
    return L


# ---------------------------------------------------------------------------------------------------------------
# Quake models

def palette_from_pak(pak):
    raw = pak["gfx/palette.lmp"]
    return [(raw[i * 3] / 255.0, raw[i * 3 + 1] / 255.0, raw[i * 3 + 2] / 255.0) for i in range(256)]


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def skin_image(name, model, pal, png=None):
    if png and os.path.exists(png):
        img = bpy.data.images.load(png)
        img.name = name
        return img
    w, h = model.skin_size
    idx = model.skins[0][2][0]
    img = bpy.data.images.new(name, w, h, alpha=False)
    px = [0.0] * (w * h * 4)
    for y in range(h):
        row = (h - 1 - y) * w
        for x in range(w):
            r, g, b = pal[idx[y * w + x]]
            o = (row + x) * 4
            px[o], px[o + 1], px[o + 2], px[o + 3] = r, g, b, 1.0
    img.pixels = px
    img.pack()
    return img


def quake_material(name, img, rough=0.6, wet=False):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = "Closest"        # Quake's texels
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = rough
    if wet:  # gibs: wet flesh, a little darker
        mul = nt.nodes.new("ShaderNodeMix")
        mul.data_type = "RGBA"
        mul.blend_type = "MULTIPLY"
        mul.inputs["Factor"].default_value = 1.0
        mul.inputs["B"].default_value = (0.62, 0.5, 0.5, 1)
        nt.links.new(tex.outputs["Color"], mul.inputs["A"])
        nt.links.new(mul.outputs["Result"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.6
        try:
            bsdf.inputs["Coat Weight"].default_value = 0.15
            bsdf.inputs["Coat Roughness"].default_value = 0.2
        except KeyError:
            pass
    return m


def mdl_object(name, data, pal, scale, png=None, rough=0.6, wet=False, pose_names=None):
    """The model as a mesh (one vertex per file vertex, smooth shaded), its poses as shape keys (basis: the first),
    in metres, facing -y (the camera), its origin where the model's origin is."""
    model = mdl.Model(data, name)
    poses = model.pose_bytes()
    names = model.pose_names()
    want = [i for i, n in enumerate(names) if pose_names is None or n in pose_names] or [0]
    sw, sh = model.skin_size
    me = bpy.data.meshes.new(name)
    rot = Matrix.Rotation(-math.pi / 2, 3, "Z")        # Quake's +x (forward) to -y (the camera)

    def pose_coords(k):
        out = []
        for v in poses[k]:
            p = Vector(model.place(v)) * scale
            out.append(rot @ p)
        return out

    co = pose_coords(want[0])
    faces = [(t[3], t[2], t[1]) for t in model.tris]
    me.from_pydata([tuple(c) for c in co], [], faces)
    uv = me.uv_layers.new(name="uv")
    for poly, t in zip(me.polygons, model.tris):
        corners = (t[3], t[2], t[1])
        for li, vi in zip(poly.loop_indices, corners):
            onseam, s, tt = model.st[vi]
            if onseam and not t[0]:
                s += sw // 2
            uv.data[li].uv = ((s + 0.5) / sw, 1.0 - (tt + 0.5) / sh)
    for p in me.polygons:
        p.use_smooth = True
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    img = skin_image(name + "_skin", model, pal, png)
    ob.data.materials.append(quake_material(name + "_mat", img, rough, wet))
    if len(want) > 1:
        ob.shape_key_add(name="basis")
        for k in want:
            kb = ob.shape_key_add(name=names[k])
            for i, c in enumerate(pose_coords(k)):
                kb.data[i].co = c
    return ob


def fcurves_of(action):
    try:
        return list(action.fcurves)
    except AttributeError:
        return [fc for layer in action.layers for strip in layer.strips for bag in strip.channelbags
                for fc in bag.fcurves]


def set_interp(idblock, kind="LINEAR"):
    ad = idblock.animation_data
    if ad and ad.action:
        for fc in fcurves_of(ad.action):
            for kp in fc.keyframe_points:
                kp.interpolation = kind


def hide_key(ob, f, hidden):
    ob.hide_render = hidden
    ob.keyframe_insert("hide_render", frame=f)


# ---------------------------------------------------------------------------------------------------------------
# The grunt, the axe, the gibs

def build_grunt(pak, pal):
    stands = ["stand%d" % i for i in range(1, 9)]
    ob = mdl_object("grunt", pak["progs/soldier.mdl"], pal, C.GRUNT_SCALE, rough=0.7, pose_names=stands)
    # Feet on GRUNT_FEET_Z: the model's lowest point in the first pose.
    zmin = min(v.co.z for v in ob.data.vertices)
    ob.location = (0.0, C.GRUNT_Y, C.GRUNT_FEET_Z - zmin)
    ob.rotation_euler = (0, 0, math.radians(-8))          # a touch of three-quarter view
    keys = ob.data.shape_keys.key_blocks
    # Quake plays the stand loop at 10 frames a second; Ironwail lerps between poses: 6 of our frames a pose.
    for f in range(0, C.HIT + 2):
        ph = f / 6.0
        a = int(ph) % 8
        b = (a + 1) % 8
        t = ph - int(ph)
        for k in range(8):
            v = (1 - t if k == a else 0.0) + (t if k == b else 0.0)
            keys[k + 1].value = v
            keys[k + 1].keyframe_insert("value", frame=f)
    set_interp(ob.data.shape_keys)
    hide_key(ob, 0, False)
    hide_key(ob, C.HIT, True)              # gone in the frame the axe hits: gibs from here
    set_interp(ob, "CONSTANT")
    return ob


def build_axe(pak, pal, source):
    if source == "id":
        data = pak["progs/v_axe.mdl"]
    else:
        with open(os.path.join(REPO, "quakevr", "progs", "v_axe.mdl"), "rb") as f:
            data = f.read()
    ob = mdl_object("axe", data, pal, 0.026, rough=0.35, pose_names=["frame1"])
    # Centre the pivot near the head (an axe spins about its centre of mass, close to the blade).
    me = ob.data
    xs = [v.co for v in me.vertices]
    lo = Vector((min(c.x for c in xs), min(c.y for c in xs), min(c.z for c in xs)))
    hi = Vector((max(c.x for c in xs), max(c.y for c in xs), max(c.z for c in xs)))
    ext = hi - lo
    print("AXE EXTENT", tuple(round(e, 3) for e in ext), tuple(round(e, 3) for e in lo), tuple(round(e, 3) for e in hi))
    # The handle runs along the model's longest horizontal axis; after the import that's y (forward was x).
    # Quake VR's axe stands upright (the handle along z) with the blade reaching forward (-y): the pivot is between
    # the blade's centre and the middle (an axe spins about its centre of mass, near the head); turned a quarter about
    # z the blade's flat faces the camera, so the spin about y shows its whole profile.
    blade = [c for c in xs if c.y < lo.y + ext.y * 0.45] or xs
    bc = sum(blade, Vector()) / len(blade)
    piv = bc * 0.6 + (lo + hi) * 0.5 * 0.4
    lay = Matrix.Rotation(math.pi / 2, 4, "Z")
    me.transform(Matrix.Translation(-piv))
    me.transform(lay)
    ob.rotation_mode = "AXIS_ANGLE"
    for f in range(C.AXE_START - 2, C.AXE_OUT + 3):
        st = C.axe_state(f)
        if st is None:
            ob.location = (0, -50, 0)
            ob.rotation_axis_angle = (0, 0, 1, 0)
        else:
            p, ang = st
            ob.location = p
            ob.rotation_axis_angle = (ang, 0.0, 1.0, 0.0)
        ob.keyframe_insert("location", frame=f)
        ob.keyframe_insert("rotation_axis_angle", frame=f)
    set_interp(ob)
    hide_key(ob, 0, True)
    hide_key(ob, C.AXE_START - 1, False)
    hide_key(ob, C.AXE_OUT + 2, True)
    for fc in fcurves_of(ob.animation_data.action):
        if fc.data_path == "hide_render":
            for kp in fc.keyframe_points:
                kp.interpolation = "CONSTANT"
    return ob


def build_gibs(pak, pal):
    obs = []
    cache = {}
    for i, g in enumerate(C.GIBS):
        m = g["model"]
        if m.startswith("brain"):
            p = os.path.join(REPO, "quakevr", "progs", "gib_%s.mdl" % m)
            with open(p, "rb") as f:
                data = f.read()
            png = p + "_0.png"
        else:
            data = pak[m]
            png = None
        ob = mdl_object("gib%d" % i, data, pal, g["scale"], png=png, wet=True)
        me = ob.data
        c = sum((v.co for v in me.vertices), Vector()) / len(me.vertices)
        me.transform(Matrix.Translation(-c))
        ob.rotation_mode = "AXIS_ANGLE"
        for f in range(C.HIT - 1, C.GIB_END + 2):
            st = C.gib_state(g, max(f, C.HIT))
            p, ang = st
            ob.location = p
            ob.rotation_axis_angle = (ang,) + g["axis"]
            ob.keyframe_insert("location", frame=f)
            ob.keyframe_insert("rotation_axis_angle", frame=f)
        set_interp(ob)
        hide_key(ob, 0, True)
        hide_key(ob, C.HIT, False)
        for fc in fcurves_of(ob.animation_data.action):
            if fc.data_path == "hide_render":
                for kp in fc.keyframe_points:
                    kp.interpolation = "CONSTANT"
        obs.append(ob)
    return obs


# ---------------------------------------------------------------------------------------------------------------
# The letters

def node(nt, kind, **inputs):
    n = nt.nodes.new(kind)
    for k, v in inputs.items():
        n.inputs[k].default_value = v
    return n


def metal_material():
    """The logo's letters: pale chipped stone-metal, cracked, gritty, with blood in the cracks and smeared on."""
    m = bpy.data.materials.new("letter_metal")
    m.use_nodes = True
    nt = m.node_tree
    L = nt.links
    bsdf = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    L.new(tc.outputs["Object"], mp.inputs["Vector"])
    # Grit: fine fbm.
    grit = node(nt, "ShaderNodeTexNoise", Scale=38.0, Detail=12.0, Roughness=0.62)
    L.new(mp.outputs[0], grit.inputs["Vector"])
    # Mottling: big soft patches.
    mott = node(nt, "ShaderNodeTexNoise", Scale=4.0, Detail=6.0, Roughness=0.55)
    L.new(mp.outputs[0], mott.inputs["Vector"])
    # Cracks: Voronoi edges, warped.
    warp = node(nt, "ShaderNodeTexNoise", Scale=6.0, Detail=4.0)
    mixv = nt.nodes.new("ShaderNodeMix")
    mixv.data_type = "VECTOR"
    mixv.inputs["Factor"].default_value = 0.08
    L.new(mp.outputs[0], mixv.inputs["A"])
    L.new(warp.outputs["Color"], mixv.inputs["B"])
    vor = nt.nodes.new("ShaderNodeTexVoronoi")
    vor.feature = "DISTANCE_TO_EDGE"
    vor.inputs["Scale"].default_value = 3.2
    L.new(mixv.outputs["Result"], vor.inputs["Vector"])
    crack = nt.nodes.new("ShaderNodeMapRange")
    crack.inputs["From Min"].default_value = 0.0
    crack.inputs["From Max"].default_value = 0.035
    crack.inputs["To Min"].default_value = 1.0
    crack.inputs["To Max"].default_value = 0.0
    L.new(vor.outputs["Distance"], crack.inputs["Value"])
    # Blood: a thresholded noise, plus the cracks.
    bn = node(nt, "ShaderNodeTexNoise", Scale=2.6, Detail=8.0, Roughness=0.7)
    L.new(mp.outputs[0], bn.inputs["Vector"])
    bmask = nt.nodes.new("ShaderNodeMapRange")
    bmask.inputs["From Min"].default_value = 0.53
    bmask.inputs["From Max"].default_value = 0.6
    L.new(bn.outputs["Fac"], bmask.inputs["Value"])
    bl = nt.nodes.new("ShaderNodeMath")
    bl.operation = "MAXIMUM"
    L.new(bmask.outputs[0], bl.inputs[0])
    cr2 = nt.nodes.new("ShaderNodeMath")
    cr2.operation = "MULTIPLY"
    cr2.inputs[1].default_value = 0.85
    L.new(crack.outputs[0], cr2.inputs[0])
    L.new(cr2.outputs[0], bl.inputs[1])
    # Stone colour: mottled pale grey, darker grit.
    stone = nt.nodes.new("ShaderNodeValToRGB")
    stone.color_ramp.elements[0].position = 0.25
    stone.color_ramp.elements[0].color = (0.03, 0.028, 0.028, 1)
    stone.color_ramp.elements[1].position = 0.75
    stone.color_ramp.elements[1].color = (0.42, 0.4, 0.38, 1)
    mixg = nt.nodes.new("ShaderNodeMath")
    mixg.operation = "MULTIPLY_ADD"
    mixg.inputs[1].default_value = 0.9
    L.new(grit.outputs["Fac"], mixg.inputs[0])
    L.new(mott.outputs["Fac"], mixg.inputs[2])
    sub = nt.nodes.new("ShaderNodeMath")
    sub.operation = "SUBTRACT"
    sub.inputs[1].default_value = 0.45
    L.new(mixg.outputs[0], sub.inputs[0])
    L.new(sub.outputs[0], stone.inputs[0])
    blood_c = nt.nodes.new("ShaderNodeMix")
    blood_c.data_type = "RGBA"
    blood_c.inputs["B"].default_value = (0.07, 0.0, 0.0, 1)
    L.new(bl.outputs[0], blood_c.inputs["Factor"])
    L.new(stone.outputs["Color"], blood_c.inputs["A"])
    L.new(blood_c.outputs["Result"], bsdf.inputs["Base Color"])
    # Metal where stone, glossy dielectric where blood.
    met = nt.nodes.new("ShaderNodeMapRange")
    met.inputs["To Min"].default_value = 0.5
    met.inputs["To Max"].default_value = 0.0
    L.new(bl.outputs[0], met.inputs["Value"])
    L.new(met.outputs[0], bsdf.inputs["Metallic"])
    rgh = nt.nodes.new("ShaderNodeMapRange")
    rgh.inputs["To Min"].default_value = 0.55
    rgh.inputs["To Max"].default_value = 0.12
    L.new(bl.outputs[0], rgh.inputs["Value"])
    L.new(rgh.outputs[0], bsdf.inputs["Roughness"])
    # Bump: the grit and the cracks (cut in).
    hgt = nt.nodes.new("ShaderNodeMath")
    hgt.operation = "MULTIPLY_ADD"
    hgt.inputs[1].default_value = -0.6
    L.new(crack.outputs[0], hgt.inputs[0])
    L.new(grit.outputs["Fac"], hgt.inputs[2])
    bump = node(nt, "ShaderNodeBump", Strength=0.55, Distance=0.02)
    L.new(hgt.outputs[0], bump.inputs["Height"])
    L.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def red_material():
    """"UNLEASHED": the logo's glossy blood-red capitals."""
    m = bpy.data.materials.new("letter_red")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    nz = node(nt, "ShaderNodeTexNoise", Scale=9.0, Detail=8.0)
    nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = (0.2, 0.0, 0.0, 1)
    ramp.color_ramp.elements[1].position = 0.65
    ramp.color_ramp.elements[1].color = (0.5, 0.004, 0.002, 1)
    nt.links.new(nz.outputs["Fac"], ramp.inputs[0])
    nt.links.new(ramp.outputs[0], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.2
    try:
        bsdf.inputs["Coat Weight"].default_value = 0.7
        bsdf.inputs["Coat Roughness"].default_value = 0.05
        bsdf.inputs["Emission Color"].default_value = (1.0, 0.02, 0.0, 1)
        bsdf.inputs["Emission Strength"].default_value = 0.08
    except KeyError:
        pass
    bump = node(nt, "ShaderNodeBump", Strength=0.25, Distance=0.01)
    nt.links.new(nz.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def text_mesh(name, body, cap, track, depth, bevel):
    """`body` in the logo's face as one mesh: extruded and chamfered, cap height `cap`, standing on z = 0 facing the
    camera (-y), centred on x = 0."""
    font = bpy.data.fonts.load(FONT)
    cu = bpy.data.curves.new(name, "FONT")
    cu.body = body
    cu.font = font
    cu.size = 1.0
    cu.space_character = track
    cu.extrude = depth * 0.5
    cu.bevel_depth = bevel
    cu.bevel_resolution = 1
    cu.offset = -bevel * 0.35
    cu.align_x = "CENTER"
    ob = bpy.data.objects.new(name, cu)
    bpy.context.scene.collection.objects.link(ob)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    bpy.data.objects.remove(ob)
    # Text lies in XY with +z out of the face: stand it up (face to -y).
    me.transform(Matrix.Rotation(math.pi / 2, 4, "X"))
    zs = [v.co.z for v in me.vertices]
    h = max(zs) - min(zs)
    return me, h


def split_islands(me):
    """The mesh's connected parts as separate meshes, left to right."""
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)    # the caps and the sides share their rims
    bm.to_mesh(me)
    bm.verts.ensure_lookup_table()
    seen = set()
    parts = []
    for v in bm.verts:
        if v.index in seen:
            continue
        stack = [v]
        comp = []
        seen.add(v.index)
        while stack:
            u = stack.pop()
            comp.append(u.index)
            for e in u.link_edges:
                w = e.other_vert(u)
                if w.index not in seen:
                    seen.add(w.index)
                    stack.append(w)
        parts.append(comp)
    bm.free()
    out = []
    for comp in parts:
        cs = set(comp)
        bm = bmesh.new()
        bm.from_mesh(me)
        bm.verts.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.index not in cs], context="VERTS")
        m2 = bpy.data.meshes.new(me.name + "_part")
        bm.to_mesh(m2)
        bm.free()
        xs = [v.co.x for v in m2.vertices]
        out.append(((min(xs) + max(xs)) * 0.5, m2))
    out.sort(key=lambda t: t[0])
    # Pieces inside others' bounds (none expected for these capitals) stay separate.
    return [m for _, m in out]


def spike_mesh(cx, cap, depth):
    """The logo's Q: a chiselled nail through the ring's bottom, an arrowhead on top and a long tapering point."""
    prof = [  # (z in caps, half width in caps)
        (0.58, 0.0), (0.36, 0.14), (0.30, 0.06), (0.25, 0.075), (0.05, 0.1), (-0.03, 0.15), (-0.09, 0.085),
        (-0.4, 0.06), (-0.85, 0.0)]
    bm = bmesh.new()
    rings = []
    for z, hw in prof:
        z *= cap
        hw *= cap
        hd = depth * 0.5 + hw * 0.25
        if hw == 0:
            rings.append([bm.verts.new((cx, -depth * 0.25, z))])
        else:
            rings.append([bm.verts.new((cx - hw, 0.0, z)), bm.verts.new((cx, -hd, z)), bm.verts.new((cx + hw, 0.0, z)),
                          bm.verts.new((cx, hd * 0.6, z))])
    for a, b in zip(rings, rings[1:]):
        if len(a) == 1:
            for k in range(4):
                bm.faces.new((a[0], b[(k + 1) % 4], b[k]))
        elif len(b) == 1:
            for k in range(4):
                bm.faces.new((a[k], a[(k + 1) % 4], b[0]))
        else:
            for k in range(4):
                bm.faces.new((a[k], a[(k + 1) % 4], b[(k + 1) % 4], b[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("spike")
    bm.to_mesh(me)
    bm.free()
    return me


def build_letters():
    """The seven letters of "QUAKE VR", each an object resting on the wall (its back face on y = 0), origin at its
    centre. Returns [(object, rest location)]."""
    me, h = text_mesh("title", "OUAKE VR", 1.0, C.TITLE_TRACK, 0.0, 0.0)
    # Measure the cap height on the flat glyphs, then build the real (extruded, chamfered) text at that scale.
    zs = sorted(v.co.z for v in me.vertices)
    cap_unit = zs[-1] - 0.0                          # O overshoots a little; good enough for scaling
    s = C.TITLE_CAP / max(cap_unit, 1e-6)
    bpy.data.meshes.remove(me)
    me, _ = text_mesh("title", "OUAKE VR", 1.0, C.TITLE_TRACK, C.LETTER_DEPTH / s, 0.022 / s)
    me.transform(Matrix.Scale(s, 4))
    parts = split_islands(me)
    if len(parts) != 7:
        print("WARNING: %d letter parts (want 7)" % len(parts))
    mat = metal_material()
    out = []
    for i, pm in enumerate(parts[:7]):
        xs = [v.co.x for v in pm.vertices]
        ys = [v.co.y for v in pm.vertices]
        zs = [v.co.z for v in pm.vertices]
        cx = (min(xs) + max(xs)) * 0.5
        if i == 0:  # the Q: the O and its nail
            sp = spike_mesh(cx, C.TITLE_CAP, C.LETTER_DEPTH)
            bm = bmesh.new()
            bm.from_mesh(pm)
            bm.from_mesh(sp)
            bm.to_mesh(pm)
            bm.free()
            zs = [v.co.z for v in pm.vertices]
        c = Vector((cx, max(ys), (min(zs) + max(zs)) * 0.5))      # back face on the wall
        pm.transform(Matrix.Translation(-c))
        for p in pm.polygons:
            p.use_smooth = False
        ob = bpy.data.objects.new("letter%d" % i, pm)
        bpy.context.scene.collection.objects.link(ob)
        pm.materials.append(mat)
        rest = Vector((c.x, 0.0, C.TITLE_Z + c.z))
        ob.location = rest
        out.append((ob, rest))
    return out


def build_unleashed():
    me, _ = text_mesh("sub", "UNLEASHED", 1.0, C.SUB_TRACK, 0.0, 0.0)
    zs = [v.co.z for v in me.vertices]
    s = C.SUB_CAP / (max(zs) - min(zs))
    bpy.data.meshes.remove(me)
    me, _ = text_mesh("sub", "UNLEASHED", 1.0, C.SUB_TRACK, 0.05 / s, 0.012 / s)
    me.transform(Matrix.Scale(s, 4))
    ys = [v.co.y for v in me.vertices]
    me.transform(Matrix.Translation((0.0, -max(ys), 0.0)))
    me.materials.append(red_material())
    ob = bpy.data.objects.new("unleashed", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = (0.0, 0.0, C.SUB_Z)
    return ob


def animate_letters(letters, lo, hi):
    for i, (ob, rest) in enumerate(letters):
        ob.rotation_mode = "XYZ"
        for f in range(lo, hi + 1):
            h = C.letter_height(i, f)
            if h is None:
                ob.location = (rest.x, -40.0, rest.z)
                ob.rotation_euler = (0, 0, 0)
            else:
                rx, rz = C.letter_tilt(i, f)
                ob.location = (rest.x, -h, rest.z)
                ob.rotation_euler = (rx, 0.0, rz)
            ob.keyframe_insert("location", frame=f)
            ob.keyframe_insert("rotation_euler", frame=f)
        set_interp(ob)
        land = C.LAND[i]
        hide_key(ob, lo, True)
        hide_key(ob, land - C.FALL, False)
        for fc in fcurves_of(ob.animation_data.action):
            if fc.data_path == "hide_render":
                for kp in fc.keyframe_points:
                    kp.interpolation = "CONSTANT"


# ---------------------------------------------------------------------------------------------------------------

def render_frames(sc, folder, prefix, lo, hi):
    os.makedirs(folder, exist_ok=True)
    for f in range(lo, hi + 1):
        sc.frame_set(f)
        sc.render.filepath = os.path.join(folder, "%s_%04d.png" % (prefix, f))
        bpy.ops.render.render(write_still=True)
        print("RENDERED", sc.render.filepath, flush=True)


def render_still(sc, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("RENDERED", path, flush=True)


def fire_lights(L, on):
    """The finale's light: the key dimmed, hot orange from below and the sides."""
    if on:
        L["key"].data.energy = 2400
        L["fill"].data.energy = 0
        L["rim"].data.energy = 0
        for name, loc, e in (("fire1", (-2.5, -1.6, -1.6), 2600), ("fire2", (2.5, -1.6, -1.4), 2600),
                             ("fire3", (0.0, -2.2, -2.2), 3000)):
            add_light(name, "POINT", loc, (0, 0, 0), e, (1.0, 0.42, 0.08), 0.6)


def main():
    a = parse_args()
    t0 = __import__("time").time()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = setup_scene(a["width"], a["samples"])
    L = setup_lights()
    pak = read_pak(a["pak"])
    pal = palette_from_pak(pak)
    out = os.path.abspath(a["out"])
    pas = a["pas"]

    if pas in ("fg", "test"):
        grunt = build_grunt(pak, pal)
        axe = build_axe(pak, pal, a["axe"])
        gibs = build_gibs(pak, pal)
    if pas in ("letters", "stills", "test"):
        letters = build_letters()
        lo, hi = C.LAND[0] - C.FALL - 1, C.LETTERS_END
        if pas != "stills":
            animate_letters(letters, 0, hi)
    if pas in ("stills", "test"):
        sub = build_unleashed()

    if pas == "fg":
        lo, hi = a["frames"] or (0, C.GIB_END)
        render_frames(sc, os.path.join(out, "fg"), "fg", lo, hi)
    elif pas == "letters":
        lo, hi = a["frames"] or (C.LAND[0] - C.FALL, C.LETTERS_END)
        render_frames(sc, os.path.join(out, "letters"), "letters", lo, hi)
    elif pas == "stills":
        sc.render.use_motion_blur = False
        d = os.path.join(out, "stills")
        sc.frame_set(C.LETTERS_END)
        for ob, _ in letters:
            ob.hide_render = False
        sub.hide_render = True
        render_still(sc, os.path.join(d, "letters_rest.png"))
        for ob, _ in letters:
            ob.hide_render = True
        sub.hide_render = False
        render_still(sc, os.path.join(d, "unleashed.png"))
        sub.hide_render = True
        keep = sc.cycles.samples
        sc.cycles.samples = 16
        for i, (ob, _) in enumerate(letters):
            for j, (o2, _) in enumerate(letters):
                o2.hide_render = j != i
            render_still(sc, os.path.join(d, "rest_%d.png" % i))
        sc.cycles.samples = keep
        fire_lights(L, True)
        for ob, _ in letters:
            ob.hide_render = False
        render_still(sc, os.path.join(d, "letters_rest_fire.png"))
        for ob, _ in letters:
            ob.hide_render = True
        sub.hide_render = False
        render_still(sc, os.path.join(d, "unleashed_fire.png"))
    elif pas == "test":
        if a.get("nomb"):
            sc.render.use_motion_blur = False
        if a.get("border"):
            sc.render.use_border = True
            sc.render.use_crop_to_border = True
            (sc.render.border_min_x, sc.render.border_min_y, sc.render.border_max_x,
             sc.render.border_max_y) = a["border"]
        frames = range(a["frames"][0], a["frames"][1] + 1, max(1, (a["frames"][1] - a["frames"][0]) // 5)) \
            if a["frames"] else [30, C.HIT - 6, C.HIT + 4, C.HIT + 14, C.LAND[0] - 5, C.LETTERS_END]
        for f in frames:
            sc.frame_set(f)
            sub.hide_render = f < C.LETTERS_END
            render_still(sc, os.path.join(out, "test", "test_%04d.png" % f))
    print("BLENDER DONE %s in %.1f s" % (pas, __import__("time").time() - t0), flush=True)


main()
