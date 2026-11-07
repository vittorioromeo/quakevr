# Quake VR: the one Blender add-on for every model of the game (until 2026-11 two: Quake VR Models and Quake VR Hand).
# It imports and exports Quake VR's body (vrbody*.md5mesh, .md5anim and the vrbody_NN_00.tga skins), the jointed hand
# (hand_rig.md5mesh: hand_blender.py), its weapons (v_*.mdl), its wrist gadget (vrgadget.mdl), the flashlight and every
# other model of quakevr/progs (.mdl) with their frames, skins and UVs, keeping what the engine relies on (the body's
# joints; the models' anchors, header, frames and vertex indices; checks.py: what the engine takes from the
# flashlight, the holster, the pauldrons, the shell...). One panel (3D View > Sidebar > Quake VR) finds what the open
# .blend edits and shows its tools. A model's source .blend, or a .blend holding the hand or the body, exports on save
# (autoexport.py; the game reads it again by itself); Show in Game asks the running game to draw it in front of you.
# Texturing (texflow.py, texgen.py): paint in Blender or in another program (its PNG watched), starter shading, fills.
# The workflow: docs/vr-port/BLENDER_WORKFLOW.md; the details: MODELS_IN_BLENDER.md, HANDS_IN_BLENDER.md.
#
# Blender 5.2. Install: Preferences > File Paths > Script Directories: add Misc/quakevr/blender, restart Blender,
# then Preferences > Add-ons: enable "Quake VR". (The package keeps its old name, quakevr_models, so an installed
# copy stays enabled with its preferences; the old hand add-on's entry is taken out by itself.)

bl_info = {
    "name": "Quake VR",
    "author": "Quake VR",
    "version": (2, 0, 0),
    "blender": (5, 2, 0),
    "location": "3D View > Sidebar > Quake VR; File > Import/Export > Quake VR Body / Hand / Model",
    "description": "Edit every Quake VR model (weapons, props, gear, monsters, the body, the jointed hand): "
                   "export on save, show it in the running game",
    "category": "Import-Export",
}

import os

import bpy
from bpy.props import BoolProperty, EnumProperty, StringProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper

from . import autoexport
from . import body_blender as BB
from . import checks
from . import hand_blender
from . import md5body
from . import md5source
from . import mdl
from . import mdl_blender as MB
from . import normal_blender as NB
from . import qpal
from . import texflow

REPORT_TEXT = "Quake VR export report"


def active_model(context, kind=None):
    """The imported model (the active or selected object, or the scene's): a mesh with qvr_kind."""
    cands = [context.active_object] + list(context.selected_objects) + list(context.scene.objects)
    for ob in cands:
        if ob is None:
            continue
        if ob.type == 'ARMATURE' and ob.get("qvr_body") in bpy.data.objects:
            ob = bpy.data.objects[ob["qvr_body"]]
        if ob.type == 'MESH' and ob.get("qvr_kind") in ((kind,) if kind else (MB.KIND, BB.KIND)):
            return ob
    return None


def write_report(lines):
    txt = bpy.data.texts.get(REPORT_TEXT) or bpy.data.texts.new(REPORT_TEXT)
    txt.clear()
    txt.write("\n".join(lines) + "\n")


def popup(context, title, lines, icon='ERROR'):
    def draw(self, _context):
        for line in lines[:24]:
            self.layout.label(text=line)
    if bpy.app.background:  # no window (blender --background): a popup would crash Blender; the report has it
        print(title)
        return
    context.window_manager.popup_menu(draw, title=title, icon=icon)


# ----------------------------------------------------------------------------
# Weapons and the gadget (.mdl)


class QVR_OT_import_mdl(bpy.types.Operator, ImportHelper):
    """Import a Quake VR model (.mdl: a weapon, the wrist gadget, the flashlight, the holster...) with its frames,
    skins and UVs"""
    bl_idname = "import_scene.quakevr_mdl"
    bl_label = "Import Quake VR Model"
    bl_options = {'REGISTER', 'UNDO'}
    filename_ext = ".mdl"
    filter_glob: StringProperty(default="*.mdl", options={'HIDDEN'})

    def execute(self, context):
        try:
            if context.object is not None and context.object.mode != 'OBJECT':
                bpy.ops.object.mode_set(mode='OBJECT')
            ob = MB.import_mdl(context, self.filepath)
            NB.show(ob, NB.map_for(os.path.dirname(self.filepath), os.path.basename(self.filepath)))
        except (mdl.MdlError, OSError) as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        self.report({'INFO'}, "%s: %d vertices, %d frames" % (ob.name, len(ob.data.vertices),
                                                              len(ob.data.shape_keys.key_blocks) if ob.data.shape_keys
                                                              else 1))
        return {'FINISHED'}


class QVR_OT_export_mdl(bpy.types.Operator, ExportHelper):
    """Export the model (keeps its header, frames, vertex order and anchors; checks them, what the engine takes from
    it and the holes): then vr_model_reload in the game"""
    bl_idname = "export_scene.quakevr_mdl"
    bl_label = "Export Quake VR Model"
    bl_options = {'REGISTER', 'UNDO'}
    filename_ext = ".mdl"
    filter_glob: StringProperty(default="*.mdl", options={'HIDDEN'})
    grow: BoolProperty(name="Grow the Box", default=False,
                       description="If vertices are past the model's box (its byte grid), make the grid coarser so "
                                   "they fit (every vertex moves a little, the anchors too). Off: the export refuses "
                                   "and selects them")
    bake_normals: BoolProperty(name="Bake Normal Map", default=True,
                               description="Bake its normal map again from the exported model (<model>.mdl_0_norm.png"
                                           ": its bevels, seams and tiles' relief; bake_normals.py). A map painted or "
                                           "baked from a high poly since is left alone")

    def invoke(self, context, event):
        ob = active_model(context, MB.KIND)
        if ob is not None and ob.get("qvr_source"):
            self.filepath = ob["qvr_source"]
        return ExportHelper.invoke(self, context, event)

    def execute(self, context):
        ob = active_model(context, MB.KIND)
        if ob is None:
            self.report({'ERROR'}, "no model to export: import one first (File > Import > Quake VR Model)")
            return {'CANCELLED'}
        try:
            lines, moved = MB.export_mdl(context, ob, self.filepath, self.report, self.grow)
        except (MB.ExportError, mdl.MdlError, OSError) as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        write_report(["%s -> %s" % (ob.name, self.filepath)] + lines)
        if self.bake_normals:
            bake_after_export(self, context, ob, self.filepath)
        holes = [l for l in lines if "HOLES" in l]
        model = os.path.basename(ob.get("qvr_source") or self.filepath)  # the model it was read as
        special = model.lower() in checks.CHECKS
        if moved or holes:
            what = ", ".join(sorted({m[0] for m in moved}))
            if special and moved:
                msg = "CHECK: %s. The engine can't follow this from the model: the report says what to change" % what
            else:
                msg = "ANCHORS MOVED: %s. Your weapon poses hang off them: check them in the game" % what if (
                    moved) else "new holes in the model"
            self.report({'WARNING'}, msg + " (the report: Text Editor > %s)" % REPORT_TEXT)
            popup(context, "Quake VR: " + msg, [l for l in lines if l.strip().startswith(
                ("MOVED", "OVER", "vr_wofs", "OPEN", "CRACK", "FLIPPED", "holes", "after",
                 "CHECK", "lens", "changed"))])
        elif special:
            lens = [l.strip() for l in lines if l.strip().startswith("lens ")]
            self.report({'INFO'}, "%sno new holes; in the game: vr_model_reload %s" % (
                "the engine reads its lens: %s; " % lens[0][len("lens "):].split(":")[0].strip() if lens else
                "checks passed, ", os.path.splitext(model)[0]))
        else:
            self.report({'INFO'}, "anchors in place, no new holes; in the game: vr_model_reload")
        return {'FINISHED'}


class QVR_OT_carry_frames(bpy.types.Operator):
    """Carry frame 0's edits into the other frames (shape keys) as the export does: each edited vertex follows its
    part's motion (recoil, the pump, spinning barrels). Frames you edited on their own are kept"""
    bl_idname = "quakevr_models.carry_frames"
    bl_label = "Carry Frame 0 Edits into Frames"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ob = active_model(context, MB.KIND)
        if ob is None:
            return {'CANCELLED'}
        if ob.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        try:
            n = MB.carry_frames(ob)
        except (MB.ExportError, mdl.MdlError) as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        self.report({'INFO'}, "%d frames updated" % n)
        return {'FINISHED'}


# ----------------------------------------------------------------------------
# The body


def skin_items(self, context):
    names = ["plain", "scratched", "wounded", "badly wounded"]
    armour = ["", "green armour, ", "yellow armour, ", "red armour, "]
    return [(str(n), "vrbody_%02d_00: %s%s" % (n, armour[n // 4], names[n % 4]), "") for n in range(BB.SKINS)]


class QVR_OT_import_body(bpy.types.Operator, ImportHelper):
    """Import Quake VR's body (vrbody.md5mesh, vrbody_lean or vrbody_brawny) with its bones, weights and skin"""
    bl_idname = "import_scene.quakevr_body"
    bl_label = "Import Quake VR Body"
    bl_options = {'REGISTER', 'UNDO'}
    filename_ext = ".md5mesh"
    filter_glob: StringProperty(default="vrbody*.md5mesh", options={'HIDDEN'})
    skin: EnumProperty(name="Skin", items=skin_items,
                       description="The skin to paint. The plain one (00) carries your changes into the other 15 on "
                                   "export; another is written alone")
    builds: BoolProperty(name="Every Build (Shape Keys)", default=True,
                         description="The other builds beside it (vrbody_lean, vrbody, vrbody_brawny, vrbody_fem) as "
                                     "shape keys: edits to the basis carry into them, and the export writes them all")

    def execute(self, context):
        try:
            if context.object is not None and context.object.mode != 'OBJECT':
                bpy.ops.object.mode_set(mode='OBJECT')
            BB.import_body(context, self.filepath, int(self.skin), self.builds, self.report)
            ob = active_model(context, BB.KIND)
            if ob is not None:
                NB.show(ob, NB.map_for(os.path.dirname(self.filepath), "body", int(self.skin)))
        except (md5body.Md5Error, BB.BodyError, qpal.TgaError, OSError) as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        return {'FINISHED'}


class QVR_OT_export_body(bpy.types.Operator, ExportHelper):
    """Export the body (.md5mesh, .md5anim; the skins): then vr_model_reload in the game"""
    bl_idname = "export_scene.quakevr_body"
    bl_label = "Export Quake VR Body"
    bl_options = {'REGISTER', 'UNDO'}
    filename_ext = ".md5mesh"
    filter_glob: StringProperty(default="vrbody*.md5mesh", options={'HIDDEN'})
    write_skin: BoolProperty(name="Skin", default=True, description="Write the skin image into its .tga")
    carry: BoolProperty(name="Into the Other Skins", default=True,
                        description="Painting the plain skin (00): carry the changes into the other 15 (armour, "
                                    "wounds), under their plates and blood")
    builds: BoolProperty(name="The Other Builds Too", default=False,
                         description="Make the same edits (each vertex moved as much, the same new weights) in the "
                                     "other two builds (vrbody_lean, vrbody, vrbody_brawny: vr_body_build). Needs the "
                                     "same vertices, triangles and UVs")
    quantize: BoolProperty(name="Quake's Palette", default=True,
                           description="Put every changed texel on Quake's palette (as the body's skins are painted). "
                                       "Off: full colour (the engine takes it; the Quake look doesn't)")
    bake_normals: BoolProperty(name="Bake Normal Maps", default=True,
                               description="Bake the body's normal maps again from the exported mesh (vrbody_00_00_norm"
                                           ".png, vrbody_04_00_norm.png; bake_normals.py). A map painted or baked "
                                           "from a high poly since is left alone")

    def invoke(self, context, event):
        ob = active_model(context, BB.KIND)
        if ob is not None and ob.get("qvr_source"):
            self.filepath = bpy.path.abspath(ob["qvr_source"])
        return ExportHelper.invoke(self, context, event)

    def execute(self, context):
        ob = active_model(context, BB.KIND)
        if ob is None:
            self.report({'ERROR'}, "no body to export: import one first (File > Import > Quake VR Body)")
            return {'CANCELLED'}
        try:
            BB.export_body(context, ob, self.filepath, self.report, self.write_skin, self.carry, self.quantize,
                           self.builds)
        except (BB.BodyError, md5body.Md5Error, qpal.TgaError, OSError, ValueError) as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        if self.bake_normals:
            bake_after_export(self, context, ob, self.filepath)
        return {'FINISHED'}


class QVR_OT_apply_pose_body(bpy.types.Operator):
    """Make the body's pose (bones moved, turned or scaled in Pose Mode) the mesh's rest shape; the bones go back to
    rest (they are the engine's)"""
    bl_idname = "quakevr_models.apply_pose_body"
    bl_label = "Apply Pose to Mesh"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ob = active_model(context, BB.KIND)
        if ob is None:
            return {'CANCELLED'}
        try:
            n = BB.apply_pose_to_mesh(context, ob)
        except BB.BodyError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        self.report({'INFO'}, "%d vertices took the pose's shape; the bones are back at rest" % n if n else
                    "the armature isn't posed: nothing to apply")
        return {'FINISHED'}


class QVR_OT_select_unweighted(bpy.types.Operator):
    """Select the body's vertices weighted to no bone (Edit Mode)"""
    bl_idname = "quakevr_models.select_unweighted"
    bl_label = "Select Unweighted Vertices"

    def execute(self, context):
        ob = active_model(context, BB.KIND)
        if ob is None:
            return {'CANCELLED'}
        bpy.ops.object.mode_set(mode='OBJECT')
        known = {g.index for g in ob.vertex_groups}
        n = 0
        for v in ob.data.vertices:
            v.select = not any(g.group in known and g.weight > 0.0 for g in v.groups)
            n += v.select
        context.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='EDIT')
        self.report({'INFO'}, "%d unweighted vertices" % n)
        return {'FINISHED'}


# ----------------------------------------------------------------------------
# Skins as PNGs, to paint elsewhere


def model_image(ob):
    for m in ob.data.materials:
        if m is not None and m.node_tree is not None:
            for n in m.node_tree.nodes:
                if n.type == 'TEX_IMAGE' and n.image is not None and not n.get("qvr_normal"):
                    return texflow.node_skin(n)  # (the skin, not the normal map shown: normal_blender.show, nor
                    # Palette Preview's view image: texflow.refresh_view)
    return None


class QVR_OT_save_skin(bpy.types.Operator, ExportHelper):
    """Save the model's skin as a PNG to paint in another editor (then Reload Skin PNG)"""
    bl_idname = "quakevr_models.save_skin"
    bl_label = "Save Skin PNG"
    filename_ext = ".png"
    filter_glob: StringProperty(default="*.png", options={'HIDDEN'})

    def execute(self, context):
        ob = active_model(context)
        img = model_image(ob) if ob is not None else None
        if img is None:
            self.report({'ERROR'}, "no skin image")
            return {'CANCELLED'}
        img.filepath_raw = self.filepath
        img.file_format = 'PNG'
        img.save()
        img["qvr_png"] = self.filepath
        return {'FINISHED'}


class QVR_OT_reload_skin(bpy.types.Operator):
    """Read the skin PNG again (after painting it elsewhere)"""
    bl_idname = "quakevr_models.reload_skin"
    bl_label = "Reload Skin PNG"

    def execute(self, context):
        ob = active_model(context)
        img = model_image(ob) if ob is not None else None
        if img is None or not img.get("qvr_png"):
            self.report({'ERROR'}, "save the skin as a PNG first (Save Skin PNG)")
            return {'CANCELLED'}
        img.filepath = img["qvr_png"]
        if img.packed_file is not None:
            img.unpack(method='REMOVE')  # the PNG is the image now
            img.filepath = img["qvr_png"]
        img.reload()
        return {'FINISHED'}


def bake_after_export(op, context, ob, path):
    """The normal maps baked again after an export (the export stands whatever happens here)."""
    try:
        done = NB.bake(context, ob, path, report=lambda m: op.report({'WARNING'}, m))
    except (OSError, ValueError, mdl.MdlError) as e:
        op.report({'WARNING'}, "normal map not baked: %s" % e)
        return
    if done:
        op.report({'INFO'}, "normal map baked: %s" % ", ".join(os.path.basename(p) for p in done))


class QVR_OT_bake_normal_map(bpy.types.Operator):
    """Bake the model's normal map from its model file as last exported (export your edits first): its bevels, seams,
    stitches and tiles' relief, as Misc/quakevr/bake_normals.py does. With other meshes selected too (the model
    active), their shape is baked from them with Cycles (a high poly), the relief laid on it"""
    bl_idname = "quakevr_models.bake_normal_map"
    bl_label = "Bake Normal Map"
    bl_options = {'REGISTER'}
    details: BoolProperty(name="Add Details", default=True,
                          description="Lay the model's relief (seams, stitches, knurling, tiles, muscles) on the shape")
    overwrite: BoolProperty(name="Overwrite Edited Map", default=False,
                            description="Replace a map painted or baked from a high poly since the last bake")

    def execute(self, context):
        ob = active_model(context)
        path = bpy.path.abspath(ob.get("qvr_source")) if ob is not None and ob.get("qvr_source") else None
        if not path or not os.path.exists(path):
            self.report({'ERROR'}, "no exported model to bake: import one (or export it first)")
            return {'CANCELLED'}
        highs = [o for o in context.selected_objects if o is not ob and o.type == 'MESH']
        try:
            done = NB.bake(context, ob, path, highs, self.details, self.overwrite,
                           report=lambda m: self.report({'WARNING'}, m))
        except (OSError, ValueError, RuntimeError, mdl.MdlError) as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        if not done:
            return {'CANCELLED'}
        NB.show(ob, done[0])
        self.report({'INFO'}, "baked %s%s; in the game: vr_model_reload" % (
            ", ".join(os.path.basename(p) for p in done), " from %d high poly object(s)" % len(highs) if highs else ""))
        return {'FINISHED'}


# ----------------------------------------------------------------------------
# The game: Show in Game (a request file the game reads), the source .blend of an imported model, and what the game's
# vr_edit_in_blender asks of Blender (edit_from_game).

REQUEST_FILE = "blender_request.txt"  # in the game folder (quakevr/): read by the engine's vr_blender.cpp


def detect(context):
    """What this .blend edits: (kind, object, label). kind: 'source' (a model's source .blend: export on save),
    'hand', 'body' (imported jointed hand or body: export on save too), 'mdl' (an imported .mdl: its source .blend
    owns its files), or None."""
    ob = autoexport.source_model()
    if ob is not None:
        return 'source', ob, "Source of %s" % ob.get("qvr_name", ob.name)
    blend = bpy.data.filepath
    from . import source as S
    if blend and S.handmade_script(blend):
        return 'source', None, "Hand-made source: %s" % os.path.basename(blend)
    kind, ob = md5source.find(context)
    if kind is not None:
        return kind, ob, "The %s: %s" % ("jointed hand" if kind == md5source.HAND else "body",
                                         md5source.model_name(kind, ob))
    ob = active_model(context, MB.KIND)
    if ob is not None:
        return 'mdl', ob, "Imported: %s" % os.path.basename(ob.get("qvr_source") or ob.name)
    return None, None, "No Quake VR model here"


def repo_root_of(path):
    from . import source as S
    return S.repo_root(path) if path else None


def owned_source(model_file):
    """The source .blend that writes quakevr/progs/<model_file> (generated.json's "source"), or None."""
    import json
    root = repo_root_of(model_file) or repo_root_of(bpy.data.filepath)
    if root is None:
        return None
    try:
        with open(os.path.join(root, "Misc", "quakevr", "generated.json")) as f:
            entry = json.load(f).get("quakevr/progs/" + os.path.basename(model_file).lower())
    except (OSError, ValueError):
        return None
    src = entry.get("source") if entry else None
    return os.path.join(root, *src.split("/")) if src else None


def game_models(context):
    """The game's names (vr_model_reload's) of what this .blend edits."""
    kind, ob, _ = detect(context)
    if kind == 'source' and ob is not None:
        return [os.path.splitext(ob.get("qvr_name", ob.name))[0]]
    if kind == 'source':  # a hand-made source: its main model (source.HANDMADE_FILES)
        from . import source as S
        d = os.path.basename(os.path.dirname(os.path.abspath(bpy.data.filepath)))
        return [os.path.splitext(m)[0] for m in S.HANDMADE_FILES.get(d, ())][:1]
    if kind in (md5source.HAND, md5source.BODY):
        return [md5source.model_name(kind, ob)]
    if kind == 'mdl':
        return [os.path.splitext(os.path.basename(ob.get("qvr_source") or ob.name))[0]]
    return []


def game_folder():
    """The game folder the request goes to: the Game Folder preference's parent (it names progs), else the
    repository's quakevr."""
    out = autoexport.out_dir()
    if out:
        return os.path.dirname(out.rstrip("\\/"))
    root = repo_root_of(bpy.data.filepath)
    return os.path.join(root, "quakevr") if root else None


def write_request(lines):
    """Writes the request the game reads within half a second (vr_blender_link: show <model> / reload <model>):
    the file's path, or None (no game folder)."""
    folder = game_folder()
    if folder is None or not os.path.isdir(folder):
        return None
    path = os.path.join(folder, REQUEST_FILE)
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return path


class QVR_OT_show_in_game(bpy.types.Operator):
    """Show the model in the running game: it is read again and drawn in front of you, turning (vr_model_show),
    until you press it again for another model or the game's Debug > Tests > Hide the Model from Blender. Save first:
    the game shows the model as last exported"""
    bl_idname = "quakevr_models.show_in_game"
    bl_label = "Show in Game"

    def execute(self, context):
        names = game_models(context)
        if not names:
            self.report({'ERROR'}, "no Quake VR model in this .blend")
            return {'CANCELLED'}
        path = write_request(["show %s" % names[0]])
        if path is None:
            self.report({'ERROR'}, "no game folder: save the .blend inside the repository, or set the Game Folder")
            return {'CANCELLED'}
        self.report({'INFO'}, "asked the game to show %s (%s)" % (names[0], path))
        return {'FINISHED'}


class QVR_OT_open_source(bpy.types.Operator):
    """Open the model's source .blend (Misc/quakevr/src_models/blend/...): edit that one, its save exports to the game
    (an imported model's .blend doesn't: the source owns the files)"""
    bl_idname = "quakevr_models.open_source"
    bl_label = "Open Its Source .blend"

    def execute(self, context):
        ob = active_model(context, MB.KIND)
        src = owned_source(ob.get("qvr_source") or "") if ob is not None else None
        if not src or not os.path.isfile(src):
            self.report({'ERROR'}, "no source .blend recorded for this model (generated.json)")
            return {'CANCELLED'}
        bpy.ops.wm.open_mainfile(filepath=src)
        return {'FINISHED'}


def import_for_edit(path):
    """Imports a game file (the jointed hand, the body, a .mdl without a source .blend) into the open scene."""
    name = os.path.basename(path).lower()
    if name.endswith(".md5mesh") and name.startswith("hand_rig"):
        bpy.ops.import_scene.quakevr_hand(filepath=path)
    elif name.endswith(".md5mesh"):
        bpy.ops.import_scene.quakevr_body(filepath=path)
    else:
        bpy.ops.import_scene.quakevr_mdl(filepath=path)
    print("Quake VR: imported %s: save it as a .blend inside the repository and Ctrl+S exports it" % path)


def edit_from_game(path):
    """The game's vr_edit_in_blender for a model without a source .blend (blender --python-expr): imports it once the
    window is up."""
    if bpy.app.background:
        import_for_edit(path)
        return

    def run():
        wm = bpy.context.window_manager
        win = wm.windows[0] if wm.windows else None
        area = next((a for a in win.screen.areas if a.type == 'VIEW_3D'), None) if win else None
        region = next((r for r in area.regions if r.type == 'WINDOW'), None) if area else None
        try:
            if win is None:
                import_for_edit(path)
            else:
                with bpy.context.temp_override(window=win, area=area, region=region):
                    import_for_edit(path)
        except RuntimeError as e:
            print("Quake VR: couldn't import %s: %s" % (path, e))
        return None
    bpy.app.timers.register(run, first_interval=0.5)


edit_skin_from_game = texflow.edit_skin_from_game  # (the game's vr_edit_skin: q.edit_skin_from_game(<skin>))


class QVR_PT_quakevr(bpy.types.Panel):
    """The one Quake VR panel: what this .blend edits, found by itself, and its tools."""
    bl_label = "Quake VR"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Quake VR"

    def draw(self, context):
        layout = self.layout
        kind, ob, label = detect(context)
        layout.label(text=label, icon='OBJECT_DATA' if kind else 'QUESTION')
        if kind in ('source', md5source.HAND, md5source.BODY):
            autoexport.draw_panel(layout)
        if kind is not None:
            layout.operator(QVR_OT_show_in_game.bl_idname, icon='PLAY')
        col = layout.column(align=True)
        if kind == 'mdl':
            col.label(text="Its source .blend writes its files:", icon='INFO')
            col.operator(QVR_OT_open_source.bl_idname, icon='FILE_BLEND')
            col.operator(QVR_OT_export_mdl.bl_idname, text="Export Model Anyway", icon='EXPORT')
        if kind in ('source', 'mdl'):
            col.operator(QVR_OT_carry_frames.bl_idname, icon='SHAPEKEY_DATA')
        elif kind == md5source.BODY:
            col.operator(QVR_OT_export_body.bl_idname, text="Export Body Now", icon='EXPORT')
            col.operator(QVR_OT_apply_pose_body.bl_idname, icon='ARMATURE_DATA')
            col.operator(QVR_OT_select_unweighted.bl_idname, icon='GROUP_VERTEX')
        elif kind == md5source.HAND:
            hand_blender.draw_tools(col)
        if kind in ('source', 'mdl', md5source.BODY):
            col.separator()
            col.operator(QVR_OT_bake_normal_map.bl_idname, icon='NORMALS_FACE')
            if ob is not None:
                texflow.draw_panel(layout, context, ob)  # (Save/Reload Skin PNG: Edit Skin Externally does both)
        box = layout.box()
        box.label(text="Import (then save inside the repository)")
        row = box.row(align=True)
        row.operator(QVR_OT_import_mdl.bl_idname, text="Model", icon='IMPORT')
        row.operator(QVR_OT_import_body.bl_idname, text="Body", icon='IMPORT')
        row.operator(hand_blender.QVR_OT_import_hand.bl_idname, text="Hand", icon='IMPORT')


def menu_import(self, context):
    self.layout.operator(QVR_OT_import_body.bl_idname, text="Quake VR Body (.md5mesh)")
    self.layout.operator(hand_blender.QVR_OT_import_hand.bl_idname, text="Quake VR Hand (.md5mesh)")
    self.layout.operator(QVR_OT_import_mdl.bl_idname, text="Quake VR Model: Weapon, Gadget (.mdl)")


def menu_export(self, context):
    self.layout.operator(QVR_OT_export_body.bl_idname, text="Quake VR Body (.md5mesh)")
    self.layout.operator(hand_blender.QVR_OT_export_hand.bl_idname, text="Quake VR Hand (.md5mesh)")
    self.layout.operator(QVR_OT_export_mdl.bl_idname, text="Quake VR Model: Weapon, Gadget (.mdl)")


class QVR_AP_models(bpy.types.AddonPreferences):
    bl_idname = __package__

    export_on_save: BoolProperty(
        name="Export on Save", default=True,
        description="Saving a model's source .blend (Misc/quakevr/src_models/blend), or a .blend holding the "
                    "jointed hand or the body, exports it to the game, as blend_export.py does, in a background "
                    "Blender: nothing is written if a check fails. The game reads the changed models again by itself "
                    "(vr_model_watch)")
    progs_dir: StringProperty(
        name="Game Folder", subtype='DIR_PATH', default="",
        description="Where the export on save writes the game's files. Empty: the repository's quakevr/progs (and "
                    "its data/weapons.txt). Another folder: only the model files (weapons.txt left alone)")

    skin_editor: StringProperty(
        name="Skin Editor", subtype='FILE_PATH', default="",
        description="The paint program Edit Skin Externally opens the skin's PNG in (its .exe). Empty: Krita, "
                    "Aseprite, GIMP or Photoshop if installed (in that order), else the program Windows opens PNGs "
                    "with")
    uv_layout_png: BoolProperty(
        name="UV Layout PNG", default=True,
        description="Edit Skin Externally also writes the UV layout beside the skin (<skin>_uv.png: lines on "
                    "transparent), to lay over it as a layer")

    def draw(self, context):
        col = self.layout.column()
        col.prop(self, "export_on_save")
        col.prop(self, "progs_dir")
        col.separator()
        col.prop(self, "skin_editor")
        if not self.skin_editor:
            found = texflow.detect_editors()
            col.label(text="Found: %s" % (", ".join(n for n, _ in found) if found else
                                          "none (Windows' program for PNGs)"), icon='INFO')
        col.prop(self, "uv_layout_png")
        col.label(text="Palettes for paint programs: %s" % os.path.join(os.path.dirname(__file__), "palettes"),
                  icon='COLOR')


OLD_ADDONS = ("quakevr_hand",)  # merged into this one (2026-11): their entries taken out of the preferences


def retire_old_addons():
    """The hand's old add-on (quakevr_hand, now hand_blender.py here) disabled and out of the preferences, so Blender
    doesn't look for it at each start. Run from a timer (not while add-ons are being enabled)."""
    import addon_utils
    prefs = bpy.context.preferences
    gone = []
    for name in OLD_ADDONS:
        if prefs.addons.get(name) is not None:
            try:
                addon_utils.disable(name, default_set=True)
            except Exception:  # noqa: BLE001  (its folder is gone: only the entry is left)
                pass
            if prefs.addons.get(name) is not None:
                prefs.addons.remove(prefs.addons[name])
            gone.append(name)
    if gone:
        print("Quake VR: the old add-on %s is part of Quake VR now: taken out of the preferences (kept so when they "
              "are saved: Auto-Save Preferences does it on quitting)" % ", ".join(gone))
        prefs.is_dirty = True
    return None


classes = (QVR_AP_models, QVR_OT_import_mdl, QVR_OT_export_mdl, QVR_OT_carry_frames, QVR_OT_import_body,
           QVR_OT_export_body, QVR_OT_apply_pose_body, QVR_OT_select_unweighted, QVR_OT_save_skin,
           QVR_OT_reload_skin, QVR_OT_bake_normal_map, QVR_OT_show_in_game, QVR_OT_open_source,
           QVR_PT_quakevr) + hand_blender.classes


def register():
    for c in classes:
        bpy.utils.register_class(c)
    autoexport.register()
    texflow.register()
    bpy.types.TOPBAR_MT_file_import.append(menu_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_export)
    if not bpy.app.background:
        bpy.app.timers.register(retire_old_addons, first_interval=1.0)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_export)
    bpy.types.TOPBAR_MT_file_import.remove(menu_import)
    texflow.unregister()
    autoexport.unregister()
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
