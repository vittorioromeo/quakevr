# Quake VR models: imports and exports Quake VR's body (vrbody*.md5mesh, .md5anim and the vrbody_NN_00.tga skins),
# its weapons (v_*.mdl), its wrist gadget (vrgadget.mdl), the flashlight and every other model of quakevr/progs (.mdl)
# with their frames, skins and UVs, keeping what the engine relies on (the body's joints; the models' anchors, header,
# frames and vertex indices; checks.py: what the engine takes from the flashlight, the holster, the pauldrons, the
# shell...), so they can be edited in Blender and reloaded in the game (vr_model_reload). The author's guide: docs/vr-port/MODELS_IN_BLENDER.md. The hand
# has its own add-on (quakevr_hand).
#
# Blender 5.2. Install: Preferences > File Paths > Script Directories: add Misc/quakevr/blender, restart Blender,
# then Preferences > Add-ons: enable "Quake VR Models".

bl_info = {
    "name": "Quake VR Models",
    "author": "Quake VR",
    "version": (1, 0, 0),
    "blender": (5, 2, 0),
    "location": "File > Import/Export > Quake VR Body / Quake VR Model (.mdl); 3D View > Sidebar > Quake VR",
    "description": "Edit Quake VR's body, weapons, wrist gadget, flashlight and other models with their bones, "
                   "frames, skins and anchors",
    "category": "Import-Export",
}

import os

import bpy
from bpy.props import BoolProperty, EnumProperty, StringProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper

from . import body_blender as BB
from . import checks
from . import md5body
from . import mdl
from . import mdl_blender as MB
from . import qpal

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
    allow_remap: BoolProperty(name="Remap Anchors", default=False,
                              description="If deleted or changed triangles make an anchor index name another vertex, "
                                          "write the model anyway and print the new indices to set. Off: the export "
                                          "refuses")

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
            lines, moved, renamed = MB.export_mdl(context, ob, self.filepath, self.report, self.grow, self.allow_remap)
        except (MB.ExportError, mdl.MdlError, OSError) as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        write_report(["%s -> %s" % (ob.name, self.filepath)] + lines)
        holes = [l for l in lines if "HOLES" in l]
        model = os.path.basename(ob.get("qvr_source") or self.filepath)  # the model it was read as
        special = model.lower() in checks.CHECKS
        if moved or renamed or holes:
            what = ", ".join(sorted({m[0] for m in moved} | {r[0] for r in renamed}))
            if special and moved:
                msg = "CHECK: %s. The engine can't follow this from the model: the report says what to change" % what
            else:
                msg = "ANCHORS MOVED: %s. Your weapon poses hang off them: check them in the game" % what if (
                    moved or renamed) else "new holes in the model"
            self.report({'WARNING'}, msg + " (the report: Text Editor > %s)" % REPORT_TEXT)
            popup(context, "Quake VR: " + msg, [l for l in lines if l.strip().startswith(
                ("MOVED", "RENAMED", "OVER", "strip", "vr_wofs", "OPEN", "CRACK", "FLIPPED", "holes", "after",
                 "CHECK", "lens", "changed"))])
        elif special:
            lens = [l.strip() for l in lines if l.strip().startswith("lens ")]
            self.report({'INFO'}, "%sno new holes; in the game: vr_model_reload %s" % (
                "the engine reads its lens: %s; " % lens[0][len("lens "):].split(":")[0].strip() if lens else
                "checks passed, ", os.path.splitext(model)[0]))
        else:
            self.report({'INFO'}, "anchors unchanged, no new holes; in the game: vr_model_reload")
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

    def execute(self, context):
        try:
            if context.object is not None and context.object.mode != 'OBJECT':
                bpy.ops.object.mode_set(mode='OBJECT')
            BB.import_body(context, self.filepath, int(self.skin))
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

    def invoke(self, context, event):
        ob = active_model(context, BB.KIND)
        if ob is not None and ob.get("qvr_source"):
            self.filepath = ob["qvr_source"]
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
                if n.type == 'TEX_IMAGE' and n.image is not None:
                    return n.image
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


class QVR_PT_models(bpy.types.Panel):
    bl_label = "Quake VR Models"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Quake VR"

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)
        col.label(text="Weapons, gadget, flashlight... (.mdl)")
        col.operator(QVR_OT_import_mdl.bl_idname, text="Import Model", icon='IMPORT')
        col.operator(QVR_OT_export_mdl.bl_idname, text="Export Model", icon='EXPORT')
        col.operator(QVR_OT_carry_frames.bl_idname, icon='SHAPEKEY_DATA')
        col.separator()
        col.label(text="Body (.md5mesh)")
        col.operator(QVR_OT_import_body.bl_idname, text="Import Body", icon='IMPORT')
        col.operator(QVR_OT_export_body.bl_idname, text="Export Body", icon='EXPORT')
        col.operator(QVR_OT_apply_pose_body.bl_idname, icon='ARMATURE_DATA')
        col.operator(QVR_OT_select_unweighted.bl_idname, icon='GROUP_VERTEX')
        col.separator()
        col.label(text="Skin")
        col.operator(QVR_OT_save_skin.bl_idname, icon='IMAGE_DATA')
        col.operator(QVR_OT_reload_skin.bl_idname, icon='FILE_REFRESH')


def menu_import(self, context):
    self.layout.operator(QVR_OT_import_body.bl_idname, text="Quake VR Body (.md5mesh)")
    self.layout.operator(QVR_OT_import_mdl.bl_idname, text="Quake VR Model: Weapon, Gadget (.mdl)")


def menu_export(self, context):
    self.layout.operator(QVR_OT_export_body.bl_idname, text="Quake VR Body (.md5mesh)")
    self.layout.operator(QVR_OT_export_mdl.bl_idname, text="Quake VR Model: Weapon, Gadget (.mdl)")


classes = (QVR_OT_import_mdl, QVR_OT_export_mdl, QVR_OT_carry_frames, QVR_OT_import_body, QVR_OT_export_body,
           QVR_OT_apply_pose_body, QVR_OT_select_unweighted, QVR_OT_save_skin, QVR_OT_reload_skin, QVR_PT_models)


def register():
    for c in classes:
        bpy.utils.register_class(c)
    bpy.types.TOPBAR_MT_file_import.append(menu_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_export)
    bpy.types.TOPBAR_MT_file_import.remove(menu_import)
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
