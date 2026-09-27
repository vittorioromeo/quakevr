# Quake VR hand: imports and exports the jointed hand (quakevr/progs/hand_rig.md5mesh, .md5anim and the
# hand_rig_NN_00.lmp skins) with its armature, weights and skin, so it can be edited in Blender and reloaded in the
# game (vr_hand_reload). The author's guide: docs/vr-port/HANDS_IN_BLENDER.md. Files are read and written by
# md5hand.py (shared with Misc/quakevr/make_hand_rig.py).
#
# Blender 5.2. Install: Preferences > File Paths > Script Directories: add Misc/quakevr/blender, restart Blender,
# then Preferences > Add-ons: enable "Quake VR Hand".

bl_info = {
    "name": "Quake VR Hand",
    "author": "Quake VR",
    "version": (1, 0, 0),
    "blender": (5, 2, 0),
    "location": "File > Import/Export > Quake VR Hand (.md5mesh); 3D View > Sidebar > Quake VR",
    "description": "Edit Quake VR's jointed hand (hand_rig.md5mesh) with its bones, weights and skin",
    "category": "Import-Export",
}

import os

import bpy
from bpy.props import BoolProperty, StringProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper
from mathutils import Matrix, Vector

from . import md5hand as M

ARMATURE_NAME = "hand_rig"
MESH_NAME = "hand"
SKIN_NAME = "hand_rig_skin"
UV_EXACT = "qvr_st"  # per corner: the file's texture coordinates exactly (Blender's v is 1 - t)
DAMAGE_LEVELS = 4  # hand_rig_00_00.lmp clean .. hand_rig_03_00.lmp bloodiest


def skin_path(folder, level):
    return os.path.join(folder, "hand_rig_%02d_00.lmp" % level)


def find_hand(context):
    """The hand's armature and mesh objects (the ones the import made, or the selection's)."""
    arm = mesh = None
    cands = list(context.selected_objects) + [context.active_object] + list(context.scene.objects)
    for ob in cands:
        if ob is None:
            continue
        if arm is None and ob.type == 'ARMATURE' and (ob.get("qvr_hand") or ob.name.startswith(ARMATURE_NAME)):
            arm = ob
        if mesh is None and ob.type == 'MESH' and (ob.get("qvr_hand") or ob.name.startswith(MESH_NAME)):
            mesh = ob
    if mesh is not None and arm is None and mesh.parent is not None and mesh.parent.type == 'ARMATURE':
        arm = mesh.parent
    return arm, mesh


# ----------------------------------------------------------------------------
# Import


def bone_parent(j):
    """The parent a bone gets in Blender (for editing: each finger a chain; the file itself keeps them flat)."""
    name, kind, fi, idx, share = M.JOINTS[j]
    if kind == M.PALM:
        return None, False
    if kind == M.SEGMENT:
        return (0, False) if idx == 1 else (M.segment_joint(fi, idx - 1), True)
    return (0 if idx == 0 else M.segment_joint(fi, idx)), False  # a helper: after the segment before its joint


def import_hand(context, path):
    with open(path) as f:
        mesh = M.parse_md5mesh(f.read(), os.path.basename(path))
    names = [j[0] for j in mesh["joints"]]
    if names != M.JOINT_NAMES:
        missing = [n for n in M.JOINT_NAMES if n not in names]
        raise M.Md5Error("not Quake VR's hand: its joints are not the hand's%s" % (
            " (missing: %s)" % ", ".join(missing) if missing else ""))
    joints = mesh["joints"]
    rests = [M.rest_position(joints, M.vertex_weights(mesh, v)) for v in range(len(mesh["verts"]))]

    # Blender's vertices: the file's, joined where the skin's seams split them (same place, same weights).
    key_to_index, vmap, positions, weights = {}, [], [], []
    for v, rest in enumerate(rests):
        w = tuple((j, b) for j, b, _ in M.vertex_weights(mesh, v))
        key = (rest, w)
        if key not in key_to_index:
            key_to_index[key] = len(positions)
            positions.append(rest)
            weights.append(w)
        vmap.append(key_to_index[key])
    faces = [(vmap[a], vmap[c], vmap[b]) for a, b, c in mesh["tris"]]  # clockwise (Quake) to counter-clockwise
    corners_st = [[mesh["verts"][a][:2], mesh["verts"][c][:2], mesh["verts"][b][:2]] for a, b, c in mesh["tris"]]

    # The armature.
    arm_data = bpy.data.armatures.new(ARMATURE_NAME)
    arm = bpy.data.objects.new(ARMATURE_NAME, arm_data)
    arm["qvr_hand"] = 1
    arm["qvr_source"] = path
    context.collection.objects.link(arm)
    arm.show_in_front = True
    mesh_data = bpy.data.meshes.new(MESH_NAME)
    ob = bpy.data.objects.new(MESH_NAME, mesh_data)
    ob["qvr_hand"] = 1
    context.collection.objects.link(ob)

    for o in context.view_layer.objects:
        o.select_set(False)
    context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    helpers = arm_data.collections.new("helpers")
    main = arm_data.collections.new("joints")
    ebs = []
    for name, parent, pos, quat in joints:
        eb = arm_data.edit_bones.new(name)
        eb.head = Vector(pos)
        ebs.append(eb)
    heads = [Vector(p) for _, _, p, _ in joints]
    for j, (name, kind, fi, idx, share) in enumerate(M.JOINTS):
        eb = ebs[j]
        if kind == M.PALM:
            eb.tail = eb.head + Vector((4.0, 0.0, 0.0))
        elif kind == M.SEGMENT:
            if idx < 3:
                eb.tail = heads[M.segment_joint(fi, idx + 1)]
            else:
                d = (heads[j] - heads[j - 1]).normalized()
                reach = max([(Vector(positions[i]) - heads[j]).dot(d) for i, w in enumerate(weights)
                             if max(w, key=lambda x: x[1])[0] == j] + [0.8])
                eb.tail = heads[j] + d * reach
        else:
            seg = M.segment_joint(fi, idx + 1)
            nxt = heads[seg + 1] if idx < 2 else heads[seg] + (heads[seg] - heads[seg - 1])
            eb.tail = eb.head + (nxt - heads[seg]).normalized() * 0.5
        (helpers if kind == M.PART else main).assign(eb)
    for j in range(len(M.JOINTS)):
        parent, connect = bone_parent(j)
        if parent is not None:
            ebs[j].parent = ebs[parent]
            ebs[j].use_connect = connect
    bpy.ops.object.mode_set(mode='OBJECT')
    helpers.is_visible = False

    # The mesh.
    mesh_data.from_pydata([tuple(p) for p in positions], [], faces)
    uv = mesh_data.uv_layers.new(name="UVMap")
    exact = mesh_data.attributes.new(UV_EXACT, 'FLOAT2', 'CORNER')
    li = 0
    for poly, sts in zip(mesh_data.polygons, corners_st):
        for k, loop in enumerate(poly.loop_indices):
            s, t = sts[k]
            uv.data[loop].uv = (s, 1.0 - t)
            exact.data[loop].vector = (s, t)
    mesh_data.validate(clean_customdata=False)
    groups = [ob.vertex_groups.new(name=n) for n in M.JOINT_NAMES]
    for i, w in enumerate(weights):
        for j, b in w:
            groups[j].add([i], b, 'REPLACE')
    ob.parent = arm
    mod = ob.modifiers.new("Armature", 'ARMATURE')
    mod.object = arm

    # The skin.
    folder = os.path.dirname(path)
    if os.path.exists(skin_path(folder, 0)):
        img = load_skin_image(skin_path(folder, 0))
        mat = bpy.data.materials.new("hand_rig")
        try:
            mat.use_nodes = True
        except AttributeError:
            pass
        nodes = mat.node_tree.nodes
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = img
        tex.interpolation = 'Closest'
        bsdf = next(n for n in nodes if n.type == 'BSDF_PRINCIPLED')
        mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        mesh_data.materials.append(mat)
    context.view_layer.objects.active = ob
    ob.select_set(True)
    return arm, ob


def load_skin_image(lmp):
    w, h, pix = M.read_lmp(lmp)
    pal = M.palette()
    img = bpy.data.images.get(SKIN_NAME)
    if img is not None and (img.size[0] != w or img.size[1] != h):
        bpy.data.images.remove(img)
        img = None
    if img is None:
        img = bpy.data.images.new(SKIN_NAME, w, h, alpha=False)
    flat = [0.0] * (w * h * 4)
    for y in range(h):
        row = pix[(h - 1 - y) * w:(h - y) * w]  # Blender's rows go bottom to top
        o = y * w * 4
        for x, i in enumerate(row):
            r, g, b = pal[i]
            flat[o + 4 * x:o + 4 * x + 4] = (r / 255.0, g / 255.0, b / 255.0, 1.0)
    img.pixels.foreach_set(flat)
    img.update()
    img["qvr_lmp"] = lmp
    img.pack()
    return img


# ----------------------------------------------------------------------------
# Export


class HandError(Exception):
    pass


def export_hand(context, path, write_skin=True, report=print):
    arm, ob = find_hand(context)
    if arm is None or ob is None:
        raise HandError("no hand to export: import hand_rig.md5mesh first (File > Import > Quake VR Hand)")
    if ob.mode == 'EDIT' or arm.mode == 'EDIT':
        bpy.ops.object.mode_set(mode='OBJECT')
    if len([m for m in ob.modifiers if m.type != 'ARMATURE' and m.show_viewport]):
        report({'WARNING'}, "the hand's modifiers other than the armature are not exported: apply them first")

    # The bones: the hand's 33, by name.
    bones = arm.data.bones
    names = set(b.name for b in bones)
    missing = [n for n in M.JOINT_NAMES if n not in names]
    extra = sorted(names - set(M.JOINT_NAMES))
    if missing or extra:
        raise HandError("the bones must be the hand's %d, unrenamed.%s%s" % (
            len(M.JOINT_NAMES), " Missing: %s." % ", ".join(missing) if missing else "",
            " Not the hand's: %s." % ", ".join(extra) if extra else ""))
    heads = {b.name: tuple(b.head_local) for b in bones}  # armature space, as the file's
    joints = []
    for j, (name, kind, fi, idx, share) in enumerate(M.JOINTS):
        pj = M.pivot_joint(j)
        pos = (0.0, 0.0, 0.0) if pj is None else heads[M.JOINT_NAMES[pj]]
        joints.append((name, -1 if j == 0 else 0, pos))

    # The mesh, in the armature's space.
    me = ob.data
    to_arm = arm.matrix_world.inverted() @ ob.matrix_world
    if to_arm == Matrix.Identity(4):
        places = [tuple(v.co) for v in me.vertices]
    else:
        places = [tuple(to_arm @ v.co) for v in me.vertices]

    group_joint = {g.index: M.JOINT_INDEX.get(g.name) for g in ob.vertex_groups}
    infl, unweighted, trimmed, normalized = [], [], 0, 0
    for v in me.vertices:
        w = [(group_joint[g.group], g.weight) for g in v.groups if group_joint.get(g.group) is not None and g.weight > 0.0]
        if not w:
            unweighted.append(v.index)
            infl.append(w)
            continue
        if len(w) > 4:
            keep = sorted(range(len(w)), key=lambda i: -w[i][1])[:4]
            w = [w[i] for i in sorted(keep)]
            trimmed += 1
        total = sum(b for _, b in w)
        if abs(total - 1.0) > 1e-4:
            w = [(j, b / total) for j, b in w]
            normalized += 1
        infl.append(w)
    if unweighted:
        for v in me.vertices:
            v.select = v.index in set(unweighted)
        raise HandError("%d vertices are weighted to no bone (they are selected now: give them weights, e.g. Weight "
                        "Paint or Object Data > Vertex Groups > Assign): %s" % (
                            len(unweighted), ", ".join(str(i) for i in unweighted[:20])))
    if trimmed:
        report({'INFO'}, "%d vertices had more than 4 weights: kept the 4 largest" % trimmed)
    if normalized:
        report({'INFO'}, "%d vertices' weights didn't add up to 1: normalized" % normalized)

    # Triangles (a face with more corners split as Blender does), and a file vertex per (vertex, texture place).
    me.calc_loop_triangles()
    uv = me.uv_layers.active
    if uv is None:
        raise HandError("the hand has no UV map")
    exact = me.attributes.get(UV_EXACT)
    verts, key_to_index, tris = [], {}, []
    for lt in me.loop_triangles:
        t = []
        for loop in lt.loops:
            u, v = uv.data[loop].uv
            st = (M.f32(u), M.f32(1.0 - v))
            if exact is not None:
                s0, t0 = exact.data[loop].vector
                if M.f32(u) == M.f32(s0) and M.f32(v) == M.f32(1.0 - t0):
                    st = (s0, t0)  # unedited: the file's exactly
            vi = me.loops[loop].vertex_index
            key = (vi, st)
            if key not in key_to_index:
                key_to_index[key] = len(verts)
                verts.append((st, infl[vi], places[vi]))
            t.append(key_to_index[key])
        tris.append((t[0], t[2], t[1]))  # counter-clockwise to clockwise (Quake)
    if len(tris) > 65535 or len(verts) > 65535:
        raise HandError("too many triangles or vertices for the engine (65535 at most)")

    anim = os.path.splitext(path)[0] + ".md5anim"
    cmd = "Blender %s, quakevr_hand" % bpy.app.version_string
    M.write_md5mesh(path, joints, verts, tris, "hand_rig", cmd)
    M.write_md5anim(anim, joints, [p for _, _, p in verts], cmd)
    report({'INFO'}, "wrote %s and %s: %d vertices, %d triangles" % (path, os.path.basename(anim), len(verts), len(tris)))
    if write_skin:
        export_skin(os.path.dirname(path), report)


def image_rgb_rows(img):
    """The image's texels as (r, g, b) 0..255, rows top to bottom."""
    w, h = img.size
    flat = [0.0] * (w * h * 4)
    img.pixels.foreach_get(flat)
    rows = []
    for y in range(h - 1, -1, -1):
        o = y * w * 4
        for x in range(w):
            p = flat[o + 4 * x:o + 4 * x + 3]
            rows.append(tuple(max(0, min(255, int(round(c * 255.0)))) for c in p))
    return rows


def export_skin(folder, report=print):
    img = bpy.data.images.get(SKIN_NAME)
    if img is None:
        report({'WARNING'}, "no skin image (%s): the skins were left as they are" % SKIN_NAME)
        return
    clean = skin_path(folder, 0)
    if not os.path.exists(clean):
        raise HandError("%s is missing: the skin is written over the existing skins (their size and blood)" % clean)
    w, h, old = M.read_lmp(clean)
    if tuple(img.size) != (w, h):
        raise HandError("the skin image is %dx%d; the hand's skin is %dx%d (keep its size: the UVs are laid out on it)"
                        % (img.size[0], img.size[1], w, h))
    q = M.Quantizer()
    new = bytes(q.indices(image_rgb_rows(img), old))
    changed = sum(1 for a, b in zip(old, new) if a != b)
    olds = [M.read_lmp(skin_path(folder, d))[2] if os.path.exists(skin_path(folder, d)) else None
            for d in range(DAMAGE_LEVELS)]
    M.write_lmp(clean, w, h, new)
    for d in range(1, DAMAGE_LEVELS):
        if olds[d] is not None and len(olds[d]) == len(old):
            M.write_lmp(skin_path(folder, d), w, h, M.carry_damage(old, olds[d], new))
    report({'INFO'}, "skin: %d texels changed, written to the clean skin and under the blood of the %d damage skins"
           % (changed, DAMAGE_LEVELS - 1))
    for ext in (".png", ".tga", ".jpg", ".pcx"):
        for d in range(DAMAGE_LEVELS):
            p = os.path.splitext(skin_path(folder, d))[0] + ext
            if os.path.exists(p):
                report({'WARNING'}, "%s is drawn instead of the .lmp (the engine prefers it): remove it" % p)


# ----------------------------------------------------------------------------
# Operators


class QVR_OT_import_hand(bpy.types.Operator, ImportHelper):
    """Import Quake VR's jointed hand (hand_rig.md5mesh) with its bones, weights and skin"""
    bl_idname = "import_scene.quakevr_hand"
    bl_label = "Import Quake VR Hand"
    bl_options = {'REGISTER', 'UNDO'}
    filename_ext = ".md5mesh"
    filter_glob: StringProperty(default="*.md5mesh", options={'HIDDEN'})

    def execute(self, context):
        try:
            import_hand(context, self.filepath)
        except (M.Md5Error, OSError) as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        return {'FINISHED'}


class QVR_OT_export_hand(bpy.types.Operator, ExportHelper):
    """Export Quake VR's jointed hand (hand_rig.md5mesh, .md5anim; the skins): then vr_hand_reload in the game"""
    bl_idname = "export_scene.quakevr_hand"
    bl_label = "Export Quake VR Hand"
    filename_ext = ".md5mesh"
    filter_glob: StringProperty(default="*.md5mesh", options={'HIDDEN'})
    write_skin: BoolProperty(name="Skin", default=True,
                             description="Write the skin image into hand_rig_00_00.lmp (and under the blood of the "
                                         "damage skins 01..03)")

    def invoke(self, context, event):
        arm, _ = find_hand(context)
        if arm is not None and arm.get("qvr_source"):
            self.filepath = arm["qvr_source"]
        return ExportHelper.invoke(self, context, event)

    def execute(self, context):
        try:
            export_hand(context, self.filepath, self.write_skin, self.report)
        except (HandError, M.Md5Error, OSError, ValueError) as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        return {'FINISHED'}


class QVR_OT_apply_pose(bpy.types.Operator):
    """Make the hand's pose (bones moved, turned or scaled in Pose Mode) its rest shape: the mesh as the pose deforms
    it and the bones where the pose puts them"""
    bl_idname = "quakevr_hand.apply_pose"
    bl_label = "Apply Pose as Rest (Mesh Too)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        arm, ob = find_hand(context)
        if arm is None or ob is None:
            self.report({'ERROR'}, "no hand")
            return {'CANCELLED'}
        apply_pose(context, arm, ob)
        return {'FINISHED'}


def apply_pose(context, arm, ob):
    if context.object is not None and context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    deps = context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(deps)
    em = ev.to_mesh()
    if len(em.vertices) != len(ob.data.vertices):
        ev.to_mesh_clear()
        raise HandError("the hand's modifiers change its vertices: apply them first")
    co = [0.0] * (3 * len(em.vertices))
    em.vertices.foreach_get("co", co)
    ev.to_mesh_clear()
    ob.data.vertices.foreach_set("co", co)
    ob.data.update()
    with context.temp_override(active_object=arm, object=arm, selected_objects=[arm], selected_editable_objects=[arm]):
        bpy.ops.object.mode_set(mode='POSE')
        bpy.ops.pose.armature_apply(selected=False)
        bpy.ops.object.mode_set(mode='OBJECT')


class QVR_OT_select_unweighted(bpy.types.Operator):
    """Select the hand's vertices weighted to no bone (Edit Mode)"""
    bl_idname = "quakevr_hand.select_unweighted"
    bl_label = "Select Unweighted Vertices"

    def execute(self, context):
        arm, ob = find_hand(context)
        if ob is None:
            return {'CANCELLED'}
        bpy.ops.object.mode_set(mode='OBJECT')
        known = {g.index for g in ob.vertex_groups if g.name in M.JOINT_INDEX}
        n = 0
        for v in ob.data.vertices:
            v.select = not any(g.group in known and g.weight > 0.0 for g in v.groups)
            n += v.select
        context.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='EDIT')
        self.report({'INFO'}, "%d unweighted vertices" % n)
        return {'FINISHED'}


class QVR_OT_save_skin(bpy.types.Operator, ExportHelper):
    """Save the hand's skin as a PNG to paint elsewhere (then Reload Skin PNG)"""
    bl_idname = "quakevr_hand.save_skin"
    bl_label = "Save Skin PNG"
    filename_ext = ".png"
    filter_glob: StringProperty(default="*.png", options={'HIDDEN'})

    def execute(self, context):
        img = bpy.data.images.get(SKIN_NAME)
        if img is None:
            self.report({'ERROR'}, "no skin image")
            return {'CANCELLED'}
        save_skin_png(img, self.filepath)
        return {'FINISHED'}


def save_skin_png(img, path):
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()
    img["qvr_png"] = path


class QVR_OT_reload_skin(bpy.types.Operator):
    """Read the skin PNG again (after painting it elsewhere)"""
    bl_idname = "quakevr_hand.reload_skin"
    bl_label = "Reload Skin PNG"

    def execute(self, context):
        img = bpy.data.images.get(SKIN_NAME)
        if img is None or not img.get("qvr_png"):
            self.report({'ERROR'}, "save the skin as a PNG first (Save Skin PNG)")
            return {'CANCELLED'}
        reload_skin_png(img)
        return {'FINISHED'}


def reload_skin_png(img):
    img.filepath = img["qvr_png"]
    if img.packed_file is not None:
        img.unpack(method='REMOVE')
        img.filepath = img["qvr_png"]
    img.reload()


class QVR_PT_hand(bpy.types.Panel):
    bl_label = "Quake VR Hand"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Quake VR"

    def draw(self, context):
        col = self.layout.column(align=True)
        col.operator(QVR_OT_import_hand.bl_idname, text="Import Hand", icon='IMPORT')
        col.operator(QVR_OT_export_hand.bl_idname, text="Export Hand", icon='EXPORT')
        col.separator()
        col.operator(QVR_OT_apply_pose.bl_idname, icon='ARMATURE_DATA')
        col.operator(QVR_OT_select_unweighted.bl_idname, icon='GROUP_VERTEX')
        col.separator()
        col.operator(QVR_OT_save_skin.bl_idname, icon='IMAGE_DATA')
        col.operator(QVR_OT_reload_skin.bl_idname, icon='FILE_REFRESH')


def menu_import(self, context):
    self.layout.operator(QVR_OT_import_hand.bl_idname, text="Quake VR Hand (.md5mesh)")


def menu_export(self, context):
    self.layout.operator(QVR_OT_export_hand.bl_idname, text="Quake VR Hand (.md5mesh)")


classes = (QVR_OT_import_hand, QVR_OT_export_hand, QVR_OT_apply_pose, QVR_OT_select_unweighted, QVR_OT_save_skin,
           QVR_OT_reload_skin, QVR_PT_hand)


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
