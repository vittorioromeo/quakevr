# texflow.py -- texturing a model's skin (docs/vr-port/MODELS_IN_BLENDER.md, "5. Texturing"), as fast as editing its
# mesh: the .blend is the source, saving it exports and the game reads the model again (autoexport.py, vr_model_watch).
#
# - Paint in Blender: one click sets up Texture Paint on the skin (nearest texels, Quake's palette as swatches, the
#   glowing colours as a separate palette); Palette Preview shows the skin as the export will put it on the palette;
#   UV Wires draws the UV layout over it. Saving the .blend writes the painted skin (its PNG beside a source .blend)
#   before the export reads it.
# - Edit Skin Externally: the skin's PNG (a source .blend's own <model>_skin<n>.png) opened in your paint program (the
#   add-on's Skin Editor preference, else Krita, Aseprite, GIMP or Photoshop found installed, else Windows' program for
#   PNGs), with its UV layout as <png>_uv.png beside it; the PNG is watched: saved there, Blender reads it, saves the
#   .blend, which exports. A skin painted in Blender meanwhile (not saved) is never replaced by itself: Load PNG Anyway.
#   The game's vr_edit_skin opens Blender on the model's source and starts this (edit_skin_from_game).
# - Helpers: Bake Starter Shading (ambient occlusion, cavities and worn edges baked from the mesh with Cycles, laid on
#   the skin along each texel's palette ramp, or a fresh grey base), Fill Material (the selected faces' texels filled
#   with metal, wood, leather, rust or cloth on a palette ramp), the UV layout as a PNG.
# The helpers leave the glowing texels and the flat area at texel (0, 0) (the engine floods it) alone unless asked.
# Every change of the skin's pixels is undone by Ctrl+Z (a step of its own: the pixels before and after kept here, the
# scene's qvr_skin_step says which are shown); painting strokes are Blender's own undo.

import glob
import os
import shutil
import subprocess
import tempfile
import time
import zlib

import bpy
import numpy as np
from bpy.app.handlers import persistent
from bpy.props import BoolProperty, EnumProperty, FloatProperty, IntProperty

from . import mdl_blender as MB
from . import qpal
from . import texgen as TG

VIEW_SUFFIX = ".qvr_view"   # the image the material shows for Palette Preview / UV Wires (never saved)
UV_COLOUR = (255, 0, 255)   # UV Wires' and the UV layout PNG's lines
HISTORY_STEPS = 40          # skin changes Ctrl+Z can take back
POLL_SECONDS = 0.5          # the external PNG's time asked this often
BACKUP_DIR = os.path.join(tempfile.gettempdir(), "quakevr_skin_backups")  # a PNG replaced by Blender's newer pixels

# The paint programs found when the Skin Editor preference is empty: (name, globs under Program Files and friends).
EDITORS = (
    ("Krita", ("Krita (x64)/bin/krita.exe", "Krita/bin/krita.exe")),
    ("Aseprite", ("Aseprite/Aseprite.exe", "Steam/steamapps/common/Aseprite/Aseprite.exe")),
    ("GIMP", ("GIMP 3/bin/gimp-3*.exe", "GIMP 2/bin/gimp-2*.exe")),
    ("Photoshop", ("Adobe/Adobe Photoshop */Photoshop.exe",)),
)

# Module state (not saved): the skin changes Ctrl+Z walks (step -> (image, before, after)); the PNGs watched; the
# shown view images' last inputs.
history = {"applied": 0, "steps": {}}
watches = {}   # normcase(png) -> {"image", "stamp", "pending", "conflict"}
views = {}     # skin image name -> crc of what the view image was made from
uv_cache = {}  # object name -> (key, edges (n, 2, 2))


# ----------------------------------------------------------------------------
# The model and its skins


def addon():
    import sys
    return sys.modules[__package__]


def the_model(context):
    return addon().active_model(context)


def skin_node(ob):
    for m in ob.data.materials:
        if m is not None and m.node_tree is not None:
            for n in m.node_tree.nodes:
                if n.type == 'TEX_IMAGE' and n.image is not None and not n.get("qvr_normal"):
                    return n
    return None


def node_skin(n):
    """The skin a material's image node shows (its view image stands for it)."""
    if n is None:
        return None
    s = n.get("qvr_skin")
    return bpy.data.images.get(s) if s else n.image


def skin_count(ob):
    return max(len(ob.get("qvr_images", [])), 1)


def skin_images(ob, make=True):
    """The model's skins in order. A skin a source .blend didn't keep (its other skins had no user, so the save left
    them out) is read from its PNG beside the .blend, else made from the model as built (make; not while drawing)."""
    names = list(ob.get("qvr_images", []))
    if not names:
        return [node_skin(skin_node(ob))]
    out = [bpy.data.images.get(n) for n in names]
    for k, n in enumerate(names):
        if out[k] is None and make:
            out[k] = restore_skin(ob, k, n)
    return out


def restore_skin(ob, k, name):
    here = os.path.dirname(bpy.data.filepath) if bpy.data.filepath else ""
    png = os.path.join(here, name + ".png") if here else ""
    if png and os.path.isfile(png):
        img = bpy.data.images.load(png)
        img.name = name
        img.filepath = bpy.path.relpath(png)
    else:
        try:
            model = MB.reference(ob)[0]
        except Exception:  # noqa: BLE001
            return None
        flat = [im for _, _, ims in model.skins for im in ims]
        if k >= len(flat):
            return None
        img = MB.set_image(name, model.skin_size[0], model.skin_size[1], flat[k])
    img.use_fake_user = True  # (kept by the save: no material shows it)
    return img


def skin_image(ob, k):
    ims = skin_images(ob)
    return ims[k] if 0 <= k < len(ims) else None


def get_rgb(img):
    w, h = img.size
    flat = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(flat)
    return np.clip(np.round(flat.reshape(h, w, 4)[:, :, :3] * 255.0), 0, 255).astype(np.int32)


def set_rgb(img, rgb):
    h, w = rgb.shape[:2]
    rgba = np.ones((h, w, 4), np.float32)
    rgba[:, :, :3] = rgb / 255.0
    img.pixels.foreach_set(rgba.ravel())
    img.update()


def get_pixels(img):
    flat = np.empty(img.size[0] * img.size[1] * 4, np.float32)
    img.pixels.foreach_get(flat)
    return flat


def old_indices(ob, k, h, w):
    """The skin as last imported or built (the reference the export compares with), Blender's rows; None for the body."""
    if not ob.get("qvr_mdl"):
        return None
    try:
        model = MB.reference(ob)[0]
        flat = [im for _, _, ims in model.skins for im in ims]
        if k >= len(flat) or tuple(model.skin_size) != (w, h):
            return None
        return np.frombuffer(bytes(flat[k]), np.uint8).reshape(h, w)[::-1].astype(np.int32)
    except Exception:  # noqa: BLE001  (a preview without it: plain nearest colours)
        return None


def skin_file(img):
    """The PNG an external image is read from ("" when packed or generated)."""
    if img is None or img.packed_file is not None or img.source != 'FILE' or not img.filepath:
        return ""
    return os.path.abspath(bpy.path.abspath(img.filepath))


# ----------------------------------------------------------------------------
# Ctrl+Z for the skin's pixels


def record(context, img, before):
    """A change of img's pixels (before: get_pixels() first) made a step Ctrl+Z takes back. The operator's own undo
    push (bl_options UNDO) keeps the scene's qvr_skin_step; on_undo_redo shows the pixels of the step it says."""
    sc = context.scene
    n = int(sc.get("qvr_skin_step", 0)) + 1
    for s in [s for s in history["steps"] if s >= n or s <= n - HISTORY_STEPS]:
        del history["steps"][s]
    history["steps"][n] = (img.name, before, get_pixels(img))
    sc["qvr_skin_step"] = n
    history["applied"] = n


def show_pixels(name, flat):
    img = bpy.data.images.get(name)
    if img is not None and len(flat) == img.size[0] * img.size[1] * 4:
        img.pixels.foreach_set(flat)
        img.update()


@persistent
def on_undo_redo(*_args):
    sc = bpy.context.scene
    target = int(sc.get("qvr_skin_step", 0)) if sc is not None else 0
    while history["applied"] > target:
        s = history["steps"].get(history["applied"])
        if s:
            show_pixels(s[0], s[1])
        history["applied"] -= 1
    while history["applied"] < target:
        history["applied"] += 1
        s = history["steps"].get(history["applied"])
        if s:
            show_pixels(s[0], s[2])
    views.clear()


# ----------------------------------------------------------------------------
# Palettes and Texture Paint


def palettes():
    """Blender's swatches: "Quake" (the 224 ordinary colours) and "Quake Fullbright (glows)" (the 32 glowing ones)."""
    out = []
    for name, idx in (("Quake", range(qpal.FULLBRIGHT)), ("Quake Fullbright (glows)", range(qpal.FULLBRIGHT, 256))):
        pal = bpy.data.palettes.get(name)
        if pal is None or len(pal.colors) != len(idx):
            pal = pal or bpy.data.palettes.new(name)
            while len(pal.colors):
                pal.colors.remove(pal.colors[0])
            for i in idx:
                c = pal.colors.new()
                c.color = [v / 255.0 for v in qpal.PALETTE[i]]  # (swatches are display colours, as the skin's bytes)
        out.append(pal)
    return out


def set_paint_view(context):
    for area in (context.screen.areas if context.screen else ()):
        if area.type == 'VIEW_3D':
            sh = area.spaces.active.shading
            if sh.type not in ('MATERIAL', 'RENDERED'):
                sh.type = 'SOLID'
                sh.color_type = 'TEXTURE'


def show_skin(ob, img):
    """The material's image node on skin img (nearest texels)."""
    n = skin_node(ob)
    if n is None:
        return
    if n.get("qvr_skin"):
        del n["qvr_skin"]
    n.image = img
    n.interpolation = 'Closest'
    views.clear()


class QVR_OT_paint_skin(bpy.types.Operator):
    """Paint the skin in Blender: Texture Paint on it, nearest texels, Quake's palette as swatches (Fullbright Swatches
    for the glowing colours). Ctrl+S writes it and exports; the export puts every changed texel on the palette"""
    bl_idname = "quakevr_models.paint_skin"
    bl_label = "Paint in Blender"
    bl_options = {'REGISTER'}

    def execute(self, context):
        ob = the_model(context)
        k = context.scene.qvr_skin_index
        img = skin_image(ob, k) if ob is not None else None
        if img is None:
            self.report({'ERROR'}, "no skin %d" % k)
            return {'CANCELLED'}
        if context.object is not None and context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        for o in context.selected_objects:
            o.select_set(False)
        ob.select_set(True)
        context.view_layer.objects.active = ob
        show_skin(ob, img)
        refresh_view(ob)
        bpy.ops.object.mode_set(mode='TEXTURE_PAINT')
        ip = context.tool_settings.image_paint
        ip.mode = 'IMAGE'
        ip.canvas = img
        ordinary, _ = palettes()
        ip.palette = ordinary
        try:
            ip.seam_bleed = 1
        except (AttributeError, TypeError):
            pass
        br = getattr(ip, "brush", None)
        for attr, v in (("strength", 1.0), ("blend", 'MIX'), ("curve_distance_falloff_preset", 'CONSTANT'),
                        ("curve_preset", 'CONSTANT')):
            try:
                setattr(br, attr, v)
            except (AttributeError, TypeError, ValueError):
                pass
        set_paint_view(context)
        self.report({'INFO'}, "painting %s: Ctrl+S exports it (the export puts it on Quake's palette)" % img.name)
        return {'FINISHED'}


class QVR_OT_use_palette(bpy.types.Operator):
    """Paint with Quake's ordinary colours, or with the glowing ones (they glow in the dark: sights, screens, lights;
    a texel glows only if painted exactly in one)"""
    bl_idname = "quakevr_models.use_palette"
    bl_label = "Quake Swatches"
    bl_options = {'REGISTER'}
    fullbright: BoolProperty(name="Fullbright", default=False)

    def execute(self, context):
        context.tool_settings.image_paint.palette = palettes()[1 if self.fullbright else 0]
        return {'FINISHED'}


# ----------------------------------------------------------------------------
# Palette Preview and UV Wires: a view image the material shows in the skin's place (never saved)


def uv_edges(ob):
    """The UV layout's edges, (n, 2, 2) in 0..1 (cached until the UVs change)."""
    me = ob.data
    if not me.uv_layers:
        return np.zeros((0, 2, 2))
    uv = np.empty(len(me.loops) * 2, np.float32)
    me.uv_layers.active.data.foreach_get("uv", uv)
    key = (len(me.loops), zlib.crc32(uv.tobytes()))
    c = uv_cache.get(ob.name)
    if c and c[0] == key:
        return c[1]
    uv = uv.reshape(-1, 2)
    starts = np.empty(len(me.polygons), np.int32)
    totals = np.empty(len(me.polygons), np.int32)
    me.polygons.foreach_get("loop_start", starts)
    me.polygons.foreach_get("loop_total", totals)
    a, b = [], []
    for s, t in zip(starts.tolist(), totals.tolist()):
        for i in range(t):
            a.append(s + i)
            b.append(s + (i + 1) % t)
    edges = np.stack([uv[a], uv[b]], 1) if a else np.zeros((0, 2, 2))
    uv_cache[ob.name] = (key, edges)
    return edges


def refresh_view(ob, force=False):
    """The material shows the skin, or (Palette Preview, UV Wires on) its view image made anew if the skin changed."""
    sc = bpy.context.scene
    n = skin_node(ob)
    skin = node_skin(n)
    if n is None or skin is None:
        return
    preview, wires = sc.qvr_palette_preview, sc.qvr_uv_wires
    if not preview and not wires:
        if n.get("qvr_skin"):
            n.image = skin
            del n["qvr_skin"]
        return
    px = get_pixels(skin)
    crc = zlib.crc32(px.tobytes()) ^ (preview * 1) ^ (wires * 2)
    view = bpy.data.images.get(skin.name + VIEW_SUFFIX)
    if not force and view is not None and views.get(skin.name) == crc and n.image == view:
        return
    w, h = skin.size
    rgb = np.clip(np.round(px.reshape(h, w, 4)[:, :, :3] * 255.0), 0, 255).astype(np.int32)
    if preview:
        ims = skin_images(ob, make=False)
        k = ims.index(skin) if skin in ims else 0
        rgb = TG.PAL[TG.quantize(rgb, old_indices(ob, k, h, w))]
    if wires:
        rgb[TG.raster_edges(uv_edges(ob), w, h)] = UV_COLOUR
    if view is None or tuple(view.size) != (w, h):
        if view is not None:
            bpy.data.images.remove(view)
        view = bpy.data.images.new(skin.name + VIEW_SUFFIX, w, h, alpha=False)
        view["qvr_view"] = True
    set_rgb(view, rgb)
    n.image = view
    n["qvr_skin"] = skin.name
    n.interpolation = 'Closest'
    views[skin.name] = crc


def view_timer():
    sc = bpy.context.scene
    if sc is None or not (sc.qvr_palette_preview or sc.qvr_uv_wires):
        return None
    ob = the_model(bpy.context)
    if ob is not None:
        refresh_view(ob)
    return 0.3


def on_view_toggle(self, context):
    ob = the_model(context)
    if ob is not None:
        refresh_view(ob, force=True)
    if (self.qvr_palette_preview or self.qvr_uv_wires) and not bpy.app.background and \
            not bpy.app.timers.is_registered(view_timer):
        bpy.app.timers.register(view_timer, first_interval=0.3)


def on_skin_index(self, context):
    ob = the_model(context)
    img = skin_image(ob, self.qvr_skin_index) if ob is not None else None
    if img is not None:
        show_skin(ob, img)
        refresh_view(ob, force=True)
        if context.object is ob and ob.mode == 'TEXTURE_PAINT':
            context.tool_settings.image_paint.canvas = img


# ----------------------------------------------------------------------------
# Saving: the skins written before the export reads them, the view image out of the saved file


def model_objects():
    return [o for o in bpy.data.objects if o.type == 'MESH' and o.get("qvr_kind")]


@persistent
def on_save_pre(*_args):
    for ob in model_objects():
        n = skin_node(ob)
        if n is not None and n.get("qvr_skin"):
            n.image = node_skin(n)  # (the file keeps the skin on its material; put back after the save)
            n["qvr_view_back"] = True
        for img in skin_images(ob, make=False):
            if img is None or not img.is_dirty:
                continue
            if img.packed_file is not None:
                img.pack()
            elif skin_file(img):
                img.save()
                note_written(skin_file(img))


@persistent
def on_save_post(*_args):
    for ob in model_objects():
        n = skin_node(ob)
        if n is not None and n.get("qvr_view_back"):
            del n["qvr_view_back"]
            views.clear()
            refresh_view(ob, force=True)


@persistent
def on_load_post(*_args):
    history["steps"].clear()
    sc = bpy.context.scene
    history["applied"] = int(sc.get("qvr_skin_step", 0)) if sc is not None else 0
    watches.clear()
    views.clear()
    uv_cache.clear()


# ----------------------------------------------------------------------------
# Edit Skin Externally


def prefs():
    a = bpy.context.preferences.addons.get(__package__)
    return a.preferences if a else None


def detect_editors():
    """[(name, exe)] of the paint programs installed (nothing installed by us)."""
    roots = [os.environ.get(v) for v in ("ProgramFiles", "ProgramFiles(x86)", "ProgramW6432")]
    la = os.environ.get("LOCALAPPDATA")
    roots += [os.path.join(la, "Programs")] if la else []
    found = []
    for name, pats in EDITORS:
        for r in [r for r in dict.fromkeys(roots) if r]:
            hits = sorted(glob.glob(os.path.join(r, *p.split("/"))) for p in pats)
            hit = next((h[-1] for h in hits if h), None)
            if hit:
                found.append((name, hit))
                break
    return found


def editor():
    """The paint program: the Skin Editor preference, else the first one found, else "" (Windows' program for PNGs)."""
    p = prefs()
    exe = bpy.path.abspath(p.skin_editor).strip() if p and p.skin_editor else ""
    if exe and os.path.isfile(exe):
        return exe
    found = detect_editors()
    return found[0][1] if found else ""


def open_in_editor(paths):
    exe = editor()
    if exe:
        subprocess.Popen([exe] + list(paths), close_fds=True)
        return exe
    if hasattr(os, "startfile"):
        os.startfile(paths[0])  # noqa: S606
        return "the program Windows opens PNGs with"
    subprocess.Popen(["xdg-open", paths[0]])
    return "xdg-open"


def stamp(path):
    try:
        st = os.stat(path)
        return (st.st_mtime_ns, st.st_size)
    except OSError:
        return None


def note_written(path):
    """We wrote the PNG ourselves: not a change to read back."""
    w = watches.get(os.path.normcase(path))
    if w is not None:
        w["stamp"] = stamp(path)
        w["pending"] = None


def backup(path):
    if not os.path.isfile(path):
        return None
    os.makedirs(BACKUP_DIR, exist_ok=True)
    dst = os.path.join(BACKUP_DIR, "%s.%s.png" % (os.path.splitext(os.path.basename(path))[0],
                                                  time.strftime("%Y%m%d-%H%M%S")))
    shutil.copyfile(path, dst)
    return dst


def uv_layout_png(ob, img, path):
    """The UV layout as a PNG the skin's size: lines on transparent, to lay over the skin as a layer."""
    w, h = img.size
    m = TG.raster_edges(uv_edges(ob), w, h)
    rgba = np.zeros((h, w, 4), np.float32)
    rgba[m, :3] = np.array(UV_COLOUR) / 255.0
    rgba[m, 3] = 1.0
    tmp = bpy.data.images.new("qvr_uv_layout_tmp", w, h, alpha=True)
    try:
        tmp.pixels.foreach_set(rgba.ravel())
        tmp.filepath_raw = path
        tmp.file_format = 'PNG'
        tmp.save()
    finally:
        bpy.data.images.remove(tmp)
    return path


def ensure_skin_file(img, k):
    """The skin's PNG (a packed skin written beside the saved .blend and read from there from now on, as Save Skin PNG
    did); its newest pixels: Blender's, if painted and not saved (the PNG before them kept in BACKUP_DIR)."""
    path = skin_file(img)
    if not path:
        blend = bpy.data.filepath
        if not blend:
            raise RuntimeError("save the .blend first: the skin's PNG goes beside it")
        path = os.path.join(os.path.dirname(blend), "%s_skin%d.png" % (os.path.splitext(os.path.basename(blend))[0], k))
        if os.path.exists(path):
            backup(path)
        img.filepath_raw = path
        img.file_format = 'PNG'
        img.save()
        if img.packed_file is not None:
            img.unpack(method='REMOVE')
        img.filepath = path
        img.reload()
        return path
    if img.is_dirty:
        backup(path)
        img.save()
    return path


def watch(img, path):
    watches[os.path.normcase(path)] = {"image": img.name, "path": path, "stamp": stamp(path), "pending": None,
                                       "conflict": False}
    if not bpy.app.background and not bpy.app.timers.is_registered(watch_timer):
        bpy.app.timers.register(watch_timer, first_interval=POLL_SECONDS, persistent=False)


def save_blend():
    if not bpy.data.filepath:
        return False
    if bpy.app.background:
        bpy.ops.wm.save_mainfile()
        return True
    wm = bpy.context.window_manager
    win = bpy.context.window or (wm.windows[0] if wm.windows else None)
    if win is None:
        return False
    with bpy.context.temp_override(window=win):
        bpy.ops.wm.save_mainfile()
    return True


def load_png(w, force=False):
    """The watched PNG read into its image (unless painted in Blender meanwhile: then only with force), a Ctrl+Z
    step; the .blend saved (which exports). A message, or None when nothing changed."""
    img = bpy.data.images.get(w["image"])
    if img is None:
        return "%s: its image is gone" % os.path.basename(w["path"])
    if img.is_dirty and not force:
        w["conflict"] = True
        return ("%s changed, but the skin was painted in Blender since the last save: not read (Load PNG Anyway "
                "replaces Blender's strokes; Ctrl+S writes them over the PNG)" % os.path.basename(w["path"]))
    before = get_pixels(img)
    img.reload()
    w["conflict"] = False
    w["stamp"] = stamp(w["path"])
    w["pending"] = None
    after = get_pixels(img)
    if len(after) == len(before) and np.array_equal(after, before):
        return None
    record(bpy.context, img, before)
    if not bpy.app.background:
        try:
            bpy.ops.ed.undo_push(message="Skin from %s" % os.path.basename(w["path"]))
        except RuntimeError:
            pass
    views.clear()
    saved = save_blend()
    return "%s read%s" % (os.path.basename(w["path"]), "; .blend saved (exports)" if saved else "")


def poll_watches():
    """One look at the watched PNGs: a change read once it has stood still for a poll (a program still writing it is
    not read half-way). The messages."""
    out = []
    for w in list(watches.values()):
        s = stamp(w["path"])
        if s is None or s == w["stamp"] or w["conflict"]:
            continue
        if w["pending"] != s:
            w["pending"] = s
            continue
        msg = load_png(w)
        if msg:
            out.append(msg)
    return out


def watch_timer():
    if not watches:
        return None
    for msg in poll_watches():
        print("Quake VR: " + msg)
        from . import autoexport
        autoexport.show('WARNING' if "not read" in msg else 'INFO', "Quake VR: " + msg)
    return POLL_SECONDS


class QVR_OT_edit_skin_external(bpy.types.Operator):
    """Open the skin's PNG in your paint program (Skin Editor in the add-on's preferences; else Krita, Aseprite, GIMP
    or Photoshop if installed), its UV layout beside it as <png>_uv.png. Saving it there brings it back: Blender reads
    it, saves the .blend, which exports; the game reads the model again"""
    bl_idname = "quakevr_models.edit_skin_external"
    bl_label = "Edit Skin Externally"
    bl_options = {'REGISTER'}
    skin: IntProperty(name="Skin", default=-1, min=-1, description="The skin (-1: the panel's)")
    open_editor: BoolProperty(name="Open the Editor", default=True, options={'SKIP_SAVE'})

    def execute(self, context):
        ob = the_model(context)
        k = context.scene.qvr_skin_index if self.skin < 0 else self.skin
        img = skin_image(ob, k) if ob is not None else None
        if img is None:
            self.report({'ERROR'}, "no skin %d" % k)
            return {'CANCELLED'}
        try:
            path = ensure_skin_file(img, k)
            files = [path]
            p = prefs()
            if p is None or p.uv_layout_png:
                files.append(uv_layout_png(ob, img, os.path.splitext(path)[0] + "_uv.png"))
        except (RuntimeError, OSError) as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        watch(img, path)
        who = open_in_editor(files[:1]) if self.open_editor else "nothing (open_editor off)"
        self.report({'INFO'}, "opened %s in %s; watching it: save it there and the game gets it" % (
            os.path.basename(path), os.path.basename(who)))
        return {'FINISHED'}


class QVR_OT_load_png_anyway(bpy.types.Operator):
    """Read the watched PNG into the skin although the skin was painted in Blender since the last save (those strokes
    replaced; Ctrl+Z brings them back)"""
    bl_idname = "quakevr_models.load_png_anyway"
    bl_label = "Load PNG Anyway"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        msgs = [load_png(w, force=True) for w in watches.values() if w["conflict"]]
        self.report({'INFO'}, "; ".join(m for m in msgs if m) or "nothing to read")
        return {'FINISHED'}


class QVR_OT_stop_watching(bpy.types.Operator):
    """Stop watching the skins' PNGs"""
    bl_idname = "quakevr_models.stop_watching"
    bl_label = "Stop Watching"
    bl_options = {'REGISTER'}

    def execute(self, context):
        watches.clear()
        return {'FINISHED'}


class QVR_OT_write_uv_layout(bpy.types.Operator):
    """Write the UV layout as a PNG beside the skin's (<skin>_uv.png: lines on transparent), to paint over as a layer"""
    bl_idname = "quakevr_models.write_uv_layout"
    bl_label = "Write UV Layout PNG"
    bl_options = {'REGISTER'}

    def execute(self, context):
        ob = the_model(context)
        k = context.scene.qvr_skin_index
        img = skin_image(ob, k) if ob is not None else None
        if img is None:
            self.report({'ERROR'}, "no skin %d" % k)
            return {'CANCELLED'}
        base = skin_file(img)
        if not base:
            if not bpy.data.filepath:
                self.report({'ERROR'}, "save the .blend first: the PNG goes beside it")
                return {'CANCELLED'}
            base = os.path.join(os.path.dirname(bpy.data.filepath), img.name + ".png")
        path = uv_layout_png(ob, img, os.path.splitext(base)[0] + "_uv.png")
        self.report({'INFO'}, "wrote %s" % path)
        return {'FINISHED'}


def edit_skin_from_game(skin=-1):
    """The game's vr_edit_skin (blender <source.blend> --python-expr): Edit Skin Externally once the window is up."""
    def run():
        sc = bpy.context.scene
        if skin >= 0 and sc is not None:
            sc.qvr_skin_index = skin
        wm = bpy.context.window_manager
        win = wm.windows[0] if wm.windows else None
        try:
            if win is None:
                bpy.ops.quakevr_models.edit_skin_external(skin=skin)
            else:
                area = next((a for a in win.screen.areas if a.type == 'VIEW_3D'), None)
                with bpy.context.temp_override(window=win, area=area):
                    bpy.ops.quakevr_models.edit_skin_external(skin=skin)
        except RuntimeError as e:
            print("Quake VR: couldn't edit the skin: %s" % e)
        return None
    if bpy.app.background:
        run()
    else:
        bpy.app.timers.register(run, first_interval=1.0)


# ----------------------------------------------------------------------------
# Helpers: masks, the starter shading, the material fills


def selected_faces_mask(ob, w, h, every=False):
    """The texels of the selected faces' UVs (every: all the faces') ((h, w) bool), or None when no face is
    selected."""
    if ob.mode == 'EDIT':
        ob.update_from_editmode()
    me = ob.data
    if not me.uv_layers:
        return None
    me.calc_loop_triangles()
    sel = np.ones(len(me.polygons), bool)
    if not every:
        me.polygons.foreach_get("select", sel)
    if not sel.any():
        return None
    tri_loops = np.empty(len(me.loop_triangles) * 3, np.int32)
    tri_poly = np.empty(len(me.loop_triangles), np.int32)
    me.loop_triangles.foreach_get("loops", tri_loops)
    me.loop_triangles.foreach_get("polygon_index", tri_poly)
    uv = np.empty(len(me.loops) * 2, np.float32)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    tris = uv[tri_loops.reshape(-1, 3)[sel[tri_poly]]]
    return TG.raster_triangles(tris, w, h)


def keep_mask(rgb, keep_fullbright, keep_corner):
    return TG.protected(rgb, keep_fullbright, keep_corner)


def bake_maps(context, ob, w, h, samples, ao_distance, edge_distance):
    """(ao, cavity, edge) (h, w) 0..1 baked with Cycles from a copy of the model (only its own surface): ambient
    occlusion (1 open), cavities (1 in a crease) and convex edges (1 on a sharp outer edge)."""
    sc = context.scene
    saved = {"engine": sc.render.engine, "active": context.view_layer.objects.active,
             "selected": list(context.selected_objects)}
    cy = getattr(sc, "cycles", None)
    saved_cy = (cy.samples, cy.device) if cy is not None else None
    tmp = ob.copy()
    tmp.data = ob.data.copy()
    for k in list(tmp.keys()):
        del tmp[k]
    context.collection.objects.link(tmp)
    mat = bpy.data.materials.new("qvr_bake_tmp")
    imgs = []
    try:
        mat.use_nodes = True
        nt = mat.node_tree
        nodes, links = nt.nodes, nt.links
        for n in list(nodes):
            nodes.remove(n)
        out = nodes.new("ShaderNodeOutputMaterial")
        em = nodes.new("ShaderNodeEmission")
        links.new(em.outputs[0], out.inputs["Surface"])
        aos = []
        for dist, inside in ((ao_distance, False), (edge_distance, False), (edge_distance, True)):
            a = nodes.new("ShaderNodeAmbientOcclusion")
            a.inputs["Distance"].default_value = dist
            a.samples = 16
            a.inside = inside
            a.only_local = True
            aos.append(a)
        target = nodes.new("ShaderNodeTexImage")
        nodes.active = target
        tmp.data.materials.clear()
        tmp.data.materials.append(mat)
        sc.render.engine = 'CYCLES'
        if cy is not None:
            cy.samples = samples
            cy.device = 'CPU'
        bpy.ops.object.mode_set(mode='OBJECT') if context.object and context.object.mode != 'OBJECT' else None
        for o in context.selected_objects:
            o.select_set(False)
        tmp.select_set(True)
        context.view_layer.objects.active = tmp
        res = []
        for a in aos:
            img = bpy.data.images.new("qvr_bake_tmp", w, h, alpha=False, float_buffer=True)
            img.colorspace_settings.name = "Non-Color"
            imgs.append(img)
            target.image = img
            links.new(a.outputs["AO"], em.inputs["Color"])
            bpy.ops.object.bake(type='EMIT', margin=2, use_clear=True)
            px = get_pixels(img).reshape(h, w, 4)[:, :, 0]
            res.append(np.clip(px, 0.0, 1.0))
        ao, small, inside = res
        return ao, 1.0 - small, 1.0 - inside
    finally:
        bpy.data.objects.remove(tmp)
        for img in imgs:
            bpy.data.images.remove(img)
        bpy.data.materials.remove(mat)
        sc.render.engine = saved["engine"]
        if cy is not None and saved_cy is not None:
            cy.samples, cy.device = saved_cy
        for o in saved["selected"]:
            if o.name in bpy.data.objects:
                o.select_set(True)
        context.view_layer.objects.active = saved["active"]


class QVR_OT_bake_shading(bpy.types.Operator):
    """Bake starter shading from the mesh onto the skin: ambient occlusion and cavities darken, worn outer edges
    lighten, each texel along its own palette ramp (Overlay), or on a fresh grey base. The glowing texels and the
    flat area at texel (0, 0) are left alone unless asked. Ctrl+Z takes it back"""
    bl_idname = "quakevr_models.bake_shading"
    bl_label = "Bake Starter Shading"
    bl_options = {'REGISTER', 'UNDO'}

    mode: EnumProperty(name="Mode", default='OVERLAY', items=(
        ('OVERLAY', "Overlay", "Shade the skin's own colours (each along its palette ramp)"),
        ('BASE', "Fresh Base", "Replace the colours with a grey base, shaded")))
    ao: FloatProperty(name="Occlusion", default=0.6, min=0.0, max=1.0, description="How dark occluded texels go")
    cavity: FloatProperty(name="Cavities", default=0.5, min=0.0, max=1.0, description="How dark creases go")
    wear: FloatProperty(name="Edge Wear", default=0.35, min=0.0, max=1.0, description="How light worn outer edges go")
    ao_distance: FloatProperty(name="Occlusion Reach", default=0.0, min=0.0,
                               description="How far occlusion looks (units; 0: a quarter of the model's size)")
    edge_distance: FloatProperty(name="Edge Width", default=0.0, min=0.0,
                                 description="How wide edges and cavities are (units; 0: 2% of the model's size)")
    samples: IntProperty(name="Samples", default=16, min=1, max=256)
    selected_only: BoolProperty(name="Selected Faces Only", default=False)
    keep_fullbright: BoolProperty(name="Keep Glowing Texels", default=True)
    keep_corner: BoolProperty(name="Keep Texel (0, 0)'s Area", default=True,
                              description="The engine floods the flat area at the skin's top-left corner")

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        ob = the_model(context)
        k = context.scene.qvr_skin_index
        img = skin_image(ob, k) if ob is not None else None
        if img is None:
            self.report({'ERROR'}, "no skin %d" % k)
            return {'CANCELLED'}
        w, h = img.size
        size = max(ob.dimensions) / max(max(ob.scale), 1e-6) or 1.0
        rgb = get_rgb(img)
        sel = selected_faces_mask(ob, w, h, every=not self.selected_only)  # (not the texels no face uses)
        if sel is None:
            self.report({'ERROR'}, "no face selected (Selected Faces Only)" if self.selected_only else "no UVs")
            return {'CANCELLED'}
        mask = sel & ~keep_mask(rgb, self.keep_fullbright, self.keep_corner)
        mode = ob.mode
        try:
            ao, cav, edge = bake_maps(context, ob, w, h, self.samples, self.ao_distance or 0.25 * size,
                                      self.edge_distance or 0.02 * size)
        except RuntimeError as e:
            self.report({'ERROR'}, "bake failed: %s" % e)
            return {'CANCELLED'}
        finally:
            if mode != 'OBJECT' and context.view_layer.objects.active is ob:
                bpy.ops.object.mode_set(mode=mode)
        before = get_pixels(img)
        idx = TG.nearest_ordinary(rgb)  # (a glowing texel shaded when asked: along its nearest ordinary colour's ramp)
        if self.mode == 'BASE':
            idx = np.full(idx.shape, int(TG.pick_in_ramp(0, 0.75)), np.int32)
        factor = (1.0 - self.ao * (1.0 - ao)) * (1.0 - self.cavity * cav)
        shaded = TG.shade_in_ramp(idx[mask], factor[mask], self.wear * 0.4 * edge[mask])
        out = rgb.copy()
        out[mask] = TG.PAL[shaded]
        set_rgb(img, out)
        record(context, img, before)
        views.clear()
        refresh_view(ob, force=True)
        self.report({'INFO'}, "shading baked on %s (%d texels); Ctrl+S exports it" % (img.name, int(mask.sum())))
        return {'FINISHED'}


def ramp_items(self, context):
    return [('AUTO', "Material's Own", "The material's usual ramp")] + \
           [(str(i), n, "Palette row %d (indices %d..%d)" % (i, i * 16, i * 16 + 15)) for i, n in enumerate(TG.RAMP_NAMES)]


class QVR_OT_fill_material(bpy.types.Operator):
    """Fill the selected faces' texels with a material in Quake's palette (metal, wood, leather, rust, cloth: a
    procedural pattern on a palette ramp, nearest texels). The glowing texels and texel (0, 0)'s area are left alone
    unless asked. Ctrl+Z takes it back"""
    bl_idname = "quakevr_models.fill_material"
    bl_label = "Fill Material"
    bl_options = {'REGISTER', 'UNDO'}

    material: EnumProperty(name="Material", default='METAL',
                           items=[(k, v[0], v[1]) for k, v in TG.MATERIALS.items()])
    ramp: EnumProperty(name="Ramp", items=ramp_items)
    scale: FloatProperty(name="Scale", default=1.0, min=0.2, max=8.0, description="The pattern's size, in texels")
    brightness: FloatProperty(name="Brightness", default=0.0, min=-0.5, max=0.5)
    seed: IntProperty(name="Seed", default=1, min=0)
    keep_fullbright: BoolProperty(name="Keep Glowing Texels", default=True)
    keep_corner: BoolProperty(name="Keep Texel (0, 0)'s Area", default=True)

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        ob = the_model(context)
        k = context.scene.qvr_skin_index
        img = skin_image(ob, k) if ob is not None else None
        if img is None:
            self.report({'ERROR'}, "no skin %d" % k)
            return {'CANCELLED'}
        w, h = img.size
        sel = selected_faces_mask(ob, w, h)
        if sel is None:
            self.report({'ERROR'}, "select the faces to fill (Edit Mode, face select)")
            return {'CANCELLED'}
        rgb = get_rgb(img)
        mask = sel & ~keep_mask(rgb, self.keep_fullbright, self.keep_corner)
        v, second = TG.pattern(self.material, h, w, self.scale, self.seed)
        v = np.clip(v + self.brightness, 0.0, 1.0)
        row = TG.MATERIALS[self.material][2] if self.ramp == 'AUTO' else int(self.ramp)
        idx = TG.pick_in_ramp(row, v)
        if second is not None:
            idx = np.where(second, TG.pick_in_ramp(0, v), idx)
        before = get_pixels(img)
        out = rgb.copy()
        out[mask] = TG.PAL[idx[mask]]
        set_rgb(img, out)
        record(context, img, before)
        views.clear()
        refresh_view(ob, force=True)
        self.report({'INFO'}, "%s filled on %s (%d texels); Ctrl+S exports it" % (
            TG.MATERIALS[self.material][0], img.name, int(mask.sum())))
        return {'FINISHED'}


# ----------------------------------------------------------------------------
# The panel's box


def draw_panel(layout, context, ob):
    sc = context.scene
    box = layout.box()
    box.label(text="Texturing", icon='BRUSH_DATA')
    n = skin_count(ob)
    if n > 1:
        box.prop(sc, "qvr_skin_index", text="Skin (of %d)" % n)
    col = box.column(align=True)
    col.operator(QVR_OT_paint_skin.bl_idname, icon='TPAINT_HLT')
    if context.object is ob and ob.mode == 'TEXTURE_PAINT':
        row = col.row(align=True)
        row.operator(QVR_OT_use_palette.bl_idname, text="Swatches").fullbright = False
        row.operator(QVR_OT_use_palette.bl_idname, text="Fullbright", icon='LIGHT').fullbright = True
    row = col.row(align=True)
    row.prop(sc, "qvr_palette_preview", toggle=True)
    row.prop(sc, "qvr_uv_wires", toggle=True)
    col = box.column(align=True)
    col.operator(QVR_OT_edit_skin_external.bl_idname, icon='WINDOW')
    if watches:
        names = ", ".join(os.path.basename(w["path"]) for w in watches.values())
        col.label(text="Watching %s" % names, icon='HIDE_OFF')
        if any(w["conflict"] for w in watches.values()):
            col.operator(QVR_OT_load_png_anyway.bl_idname, icon='ERROR')
        col.operator(QVR_OT_stop_watching.bl_idname, icon='X')
    col.operator(QVR_OT_write_uv_layout.bl_idname, icon='UV')
    col = box.column(align=True)
    col.operator(QVR_OT_bake_shading.bl_idname, icon='SHADING_RENDERED')
    col.operator(QVR_OT_fill_material.bl_idname, icon='TEXTURE')


classes = (QVR_OT_paint_skin, QVR_OT_use_palette, QVR_OT_edit_skin_external, QVR_OT_load_png_anyway,
           QVR_OT_stop_watching, QVR_OT_write_uv_layout, QVR_OT_bake_shading, QVR_OT_fill_material)

HANDLERS = (("save_pre", on_save_pre), ("save_post", on_save_post), ("load_post", on_load_post),
            ("undo_post", on_undo_redo), ("redo_post", on_undo_redo))


def register():
    for c in classes:
        bpy.utils.register_class(c)
    S = bpy.types.Scene
    S.qvr_skin_index = IntProperty(name="Skin", default=0, min=0, max=63, update=on_skin_index,
                                   description="The skin Paint, Edit Externally and the helpers work on")
    S.qvr_palette_preview = BoolProperty(name="Palette Preview", default=False, update=on_view_toggle,
                                         description="Show the skin as the export will put it on Quake's palette")
    S.qvr_uv_wires = BoolProperty(name="UV Wires", default=False, update=on_view_toggle,
                                  description="Draw the UV layout over the skin on the model")
    for name, fn in HANDLERS:
        lst = getattr(bpy.app.handlers, name)
        if fn not in lst:
            lst.append(fn)


def unregister():
    for name, fn in HANDLERS:
        lst = getattr(bpy.app.handlers, name)
        if fn in lst:
            lst.remove(fn)
    for p in ("qvr_skin_index", "qvr_palette_preview", "qvr_uv_wires"):
        if hasattr(bpy.types.Scene, p):
            delattr(bpy.types.Scene, p)
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
