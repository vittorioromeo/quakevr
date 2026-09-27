# body_blender.py -- the VR body (quakevr/progs/vrbody.md5mesh, vrbody_lean, vrbody_brawny; their .md5anim; the
# skins vrbody_NN_00.tga) in Blender and back. The guide: docs/vr-port/MODELS_IN_BLENDER.md.
#
# The body's joints are the engine's (vr_avatar.cpp poses the bones from the player's tracked head and hands, with
# its own bind pose, and skins the mesh with the file's inverse bind matrices): the export keeps them as they are
# and refuses a moved or renamed bone. The mesh, its weights and texture coordinates and the skins are yours.
#
# The skins: 16 of them, shared by the three builds, armour * 4 + damage (vr_view.cpp picks one from the player's
# armour and health). You paint one (the plain one, vrbody_00_00, by default) and the export carries your changes
# into the other 15 under their armour plates and wounds, as the hand's export does under its blood.

import os

import bpy
import numpy as np
from mathutils import Matrix, Vector

from . import md5body as B
from . import qpal
from .mdl_blender import image_rgb, pack, unpack

KIND = "body"
A_VERT = "qvr_vert"  # POINT int: the Blender vertex's group at import (the file's vertices it stands for)
A_FV = "qvr_fv"      # CORNER int: the file's vertex this corner is
A_S, A_T = "qvr_s", "qvr_t"  # CORNER float: the file's texture coordinates, exactly (not a float2: that's a UV map)
SKINS = 16
BUILDS = (("vrbody_lean", "Lean"), ("vrbody", "Athletic"), ("vrbody_brawny", "Brawny"))


class BodyError(Exception):
    pass


def skin_path(folder, n):
    return os.path.join(folder, "vrbody_%02d_00.tga" % n)


# ----------------------------------------------------------------------------
# Import


def import_body(context, path, skin=0):
    with open(path) as f:
        text = f.read()
    mesh = B.Mesh(text, os.path.basename(path))
    anim_path = os.path.splitext(path)[0] + ".md5anim"
    anim_text = open(anim_path).read() if os.path.exists(anim_path) else None
    if anim_text is None:
        raise BodyError("%s is missing: the body is the .md5mesh and its .md5anim" % anim_path)
    name = os.path.splitext(os.path.basename(path))[0]
    nj = len(mesh.joints)

    # Blender's vertices: the file's, joined where the skin's seams split them (same place, same weights).
    rests = [mesh.rest(v) for v in range(len(mesh.verts))]
    key_index, vmap, positions, weights = {}, [], [], []
    for v, rest in enumerate(rests):
        w = tuple((j, B.f32(b)) for j, b, _, _ in mesh.vertex_weights(v))
        key = (rest, w)
        if key not in key_index:
            key_index[key] = len(positions)
            positions.append(rest)
            weights.append(w)
        vmap.append(key_index[key])
    faces = [(vmap[a], vmap[c], vmap[b]) for a, b, c in mesh.tris]  # clockwise (Quake) to counter-clockwise

    # The armature: a bone per joint, its head at the joint, along the joint's +x, its Z the joint's +z (the hint).
    arm_data = bpy.data.armatures.new(name + "_rig")
    arm = bpy.data.objects.new(name + "_rig", arm_data)
    arm["qvr_kind"] = KIND + "_rig"
    arm.show_in_front = True
    context.collection.objects.link(arm)
    for o in context.selected_objects:
        o.select_set(False)
    context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    children = {}
    for j, (_, parent, _, _) in enumerate(mesh.joints):
        children.setdefault(parent, []).append(j)
    ebs = []
    for j, (jname, parent, pos, quat) in enumerate(mesh.joints):
        eb = arm_data.edit_bones.new(jname)
        R = mesh.rot[j]
        x = Vector((R[0][0], R[1][0], R[2][0]))
        z = Vector((R[0][2], R[1][2], R[2][2]))
        head = Vector(pos)
        length = 3.0
        for c in children.get(j, []):
            d = (Vector(mesh.joints[c][2]) - head).dot(x)
            if d > 0.5 and not mesh.joints[c][0].startswith(("foretwist", "wrist_")):
                length = d
                break
        if jname.startswith(("foretwist", "wrist_")):
            length = 1.5
        eb.head = head
        eb.tail = head + x * length
        eb.align_roll(z)
        ebs.append(eb)
    for j, (_, parent, _, _) in enumerate(mesh.joints):
        if parent >= 0:
            ebs[j].parent = ebs[parent]
    bpy.ops.object.mode_set(mode='OBJECT')
    helpers = arm_data.collections.new("forearm twist")
    main = arm_data.collections.new("body")
    for b in arm_data.bones:
        (helpers if b.name.startswith(("foretwist", "wrist_")) else main).assign(b)

    # The mesh.
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(p) for p in positions], [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    context.collection.objects.link(ob)
    av = me.attributes.new(A_VERT, 'INT', 'POINT')
    av.data.foreach_set("value", list(range(len(positions))))
    fvs, sts, uvs = [], [], []
    for a, b, c in mesh.tris:
        for v in (a, c, b):
            s, t = mesh.verts[v][0]
            fvs.append(v)
            sts += [s, t]
            uvs += [s, 1.0 - t]
    me.attributes.new(A_FV, 'INT', 'CORNER').data.foreach_set("value", fvs)
    me.uv_layers.new(name="UVMap").data.foreach_set("uv", uvs)
    me.attributes.new(A_S, 'FLOAT', 'CORNER').data.foreach_set("value", sts[0::2])
    me.attributes.new(A_T, 'FLOAT', 'CORNER').data.foreach_set("value", sts[1::2])
    me.shade_smooth()
    groups = [ob.vertex_groups.new(name=j[0]) for j in mesh.joints]
    for i, w in enumerate(weights):
        for j, b in w:
            groups[j].add([i], b, 'REPLACE')
    ob.parent = arm
    mod = ob.modifiers.new("Armature", 'ARMATURE')
    mod.object = arm

    ob["qvr_kind"] = KIND
    ob["qvr_source"] = path
    ob["qvr_md5mesh"] = pack(text.encode())
    ob["qvr_md5anim"] = pack(anim_text.encode())
    arm["qvr_body"] = ob.name

    folder = os.path.dirname(path)
    if os.path.exists(skin_path(folder, skin)):
        img = load_skin(skin_path(folder, skin), "vrbody_skin")
        mat = bpy.data.materials.new("vrbody")
        try:
            mat.use_nodes = True
        except AttributeError:
            pass
        tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
        tex.image = img
        tex.interpolation = 'Closest'
        bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        me.materials.append(mat)
        ob["qvr_skin"] = skin
    context.view_layer.objects.active = ob
    ob.select_set(True)
    return arm, ob


def load_skin(path, img_name):
    header, w, h, rgb, alpha, top = qpal.read_tga(path)
    img = bpy.data.images.get(img_name)
    if img is not None and (img.size[0] != w or img.size[1] != h):
        bpy.data.images.remove(img)
        img = None
    if img is None:
        img = bpy.data.images.new(img_name, w, h, alpha=False)
    px = np.ones((h, w, 4), np.float32)
    px[:, :, :3] = (np.array(rgb, np.float32).reshape(h, w, 3) / 255.0)[::-1]
    img.pixels.foreach_set(px.ravel())
    img.update()
    img.pack()
    img["qvr_tga"] = path
    return img


def find_body(context):
    cands = [context.active_object] + list(context.selected_objects) + list(context.scene.objects)
    for ob in cands:
        if ob is not None and ob.type == 'MESH' and ob.get("qvr_kind") == KIND:
            return ob
        if ob is not None and ob.type == 'ARMATURE' and ob.get("qvr_body") in bpy.data.objects:
            return bpy.data.objects[ob["qvr_body"]]
    return None


def armature_of(ob):
    if ob.parent is not None and ob.parent.type == 'ARMATURE':
        return ob.parent
    for m in ob.modifiers:
        if m.type == 'ARMATURE' and m.object is not None:
            return m.object
    return None


# ----------------------------------------------------------------------------
# Export


def export_body(context, ob, path, report, write_skin=True, carry=True, quantize=True, builds=False):
    if context.object is not None and context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    src_text = unpack(ob["qvr_md5mesh"]).decode()
    src = B.Mesh(src_text, os.path.basename(path))
    anim_text = unpack(ob["qvr_md5anim"]).decode()
    arm = armature_of(ob)
    if arm is None:
        raise BodyError("the body has no armature: import it again (File > Import > Quake VR Body)")
    if len([m for m in ob.modifiers if m.type != 'ARMATURE' and m.show_viewport]):
        report({'WARNING'}, "the body's modifiers other than the armature are not exported: apply them first")

    # The joints: the engine's, unrenamed and unmoved.
    names = [j[0] for j in src.joints]
    have = {b.name: b for b in arm.data.bones}
    missing = [n for n in names if n not in have]
    extra = sorted(set(have) - set(names))
    if missing or extra:
        raise BodyError("the bones must be the body's %d, unrenamed (the engine finds them by name).%s%s" % (
            len(names), " Missing: %s." % ", ".join(missing) if missing else "",
            " Not the body's: %s." % ", ".join(extra) if extra else ""))
    moved = [n for n, (_, _, p, _) in zip(names, src.joints) if (Vector(have[n].head_local) - Vector(p)).length > 1e-3]
    if moved:
        raise BodyError("bones moved: %s. The body's joints are the engine's (vr_avatar.cpp's bind pose: it places them "
                        "from your tracked head and hands), so they stay where they are. Move them back (import the "
                        "body again, or undo). To change the body's shape, move its vertices, or pose the bones and "
                        "click Apply Pose to Mesh" % ", ".join(moved))
    if any(pb.matrix_basis != Matrix.Identity(4) for pb in arm.pose.bones):
        report({'WARNING'}, "the armature is posed: the export writes the mesh at rest, without the pose (Apply Pose "
                            "to Mesh keeps it)")

    me = ob.data
    to_arm = arm.matrix_world.inverted() @ ob.matrix_world
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    if to_arm != Matrix.Identity(4):
        M = np.array(to_arm)
        co = (co.astype(np.float64) @ M[:3, :3].T + M[:3, 3]).astype(np.float32)
    places = [tuple(float(c) for c in p) for p in co]

    # Weights.
    jidx = {n: i for i, n in enumerate(names)}
    group_joint = {g.index: jidx.get(g.name) for g in ob.vertex_groups}
    infl, unweighted, trimmed, normalized = [], [], 0, 0
    for v in me.vertices:
        w = [(group_joint[g.group], g.weight) for g in v.groups if group_joint.get(g.group) is not None and g.weight > 0]
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
        raise BodyError("%d vertices are weighted to no bone (selected now: give them weights in Weight Paint, or Object "
                        "Data > Vertex Groups > Assign): %s" % (len(unweighted), ", ".join(map(str, unweighted[:20]))))
    if trimmed:
        report({'INFO'}, "%d vertices had more than 4 weights: kept the 4 largest" % trimmed)
    if normalized:
        report({'INFO'}, "%d vertices' weights didn't add up to 1: normalized" % normalized)

    # Which Blender vertex still is which group of the file's vertices (the first one with it: duplicates are new).
    nv = len(me.vertices)
    gid = np.full(nv, -1, np.int32)
    a = me.attributes.get(A_VERT)
    if a is not None and a.domain == 'POINT':
        a.data.foreach_get("value", gid)
    holder = {}
    for v in range(nv):
        if gid[v] >= 0 and gid[v] not in holder:
            holder[int(gid[v])] = v
    fv_group = {}  # the file's vertex -> its group, as imported: from the corners that still are it
    nl = len(me.loops)
    loop_vert = np.empty(nl, np.int32)
    me.loops.foreach_get("vertex_index", loop_vert)
    loop_fv = np.full(nl, -1, np.int32)
    a = me.attributes.get(A_FV)
    if a is not None and a.domain == 'CORNER':
        a.data.foreach_get("value", loop_fv)
    exact = read_exact(me)
    uv = uv_map(me)
    if uv is None:
        raise BodyError("the body has no UV map")
    uvs = np.empty(nl * 2, np.float32)
    uv.data.foreach_get("uv", uvs)
    uvs = uvs.reshape(nl, 2)

    # The file's rest places and weights, to tell an unedited vertex.
    rests = [src.rest(v) for v in range(len(src.verts))]
    src_w = [[(j, b) for j, b, _, _ in src.vertex_weights(v)] for v in range(len(src.verts))]
    src_st = [(B.f32(s), B.f32(t)) for (s, t), _, _, _ in src.verts]

    def corner_st(li):
        u, v = float(uvs[li][0]), float(uvs[li][1])
        st = (B.f32(u), B.f32(1.0 - v))
        s0, t0 = float(exact[li][0]), float(exact[li][1])
        if unedited(u, v, s0, t0):
            st = (s0, t0)  # the file's exactly
        return st

    def is_file_vertex(li, st):
        fv = int(loop_fv[li])
        if not 0 <= fv < len(src.verts):
            return None
        g = gid[loop_vert[li]]
        if holder.get(int(g)) != int(loop_vert[li]):
            return None
        if src_st[fv] != st:
            return None
        return fv

    me.calc_loop_triangles()
    keys, key_of, tris = [], {}, []
    for lt in me.loop_triangles:
        t = []
        for li in lt.loops:
            st = corner_st(li)
            bv = int(loop_vert[li])
            fv = is_file_vertex(li, st)
            key = ("f", fv) if fv is not None else ("n", bv, st)
            if key not in key_of:
                key_of[key] = len(keys)
                keys.append((key, bv, st))
            t.append(key_of[key])
        tris.append(t)
    # The order: the file's vertices still there in their order, then the new ones.
    order = sorted(range(len(keys)), key=lambda i: (0, keys[i][0][1]) if keys[i][0][0] == "f" else (1, i))
    new_index = {old: new for new, old in enumerate(order)}
    tris = [(new_index[a], new_index[c], new_index[b]) for a, b, c in tris]  # counter-clockwise to clockwise
    if len(tris) > 65535 or len(keys) > 65535:
        raise BodyError("too many triangles or vertices for the engine (65535 at most)")

    out_verts, unchanged, rest_out, reweighted = [], 0, [], []
    for i in order:
        key, bv, st = keys[i]
        place = places[bv]
        w = infl[bv]
        fv = key[1] if key[0] == "f" else None
        same = (fv is not None and place == rests[fv] and len(w) == len(src_w[fv]) and
                all(j == j0 and abs(b - b0) < 1e-6 for (j, b), (j0, b0) in zip(w, src_w[fv])))
        if same:
            ws = [(j, b, o, t) for j, b, o, t in src.vertex_weights(fv)]
            out_verts.append((st, src.verts[fv][1], ws))
            unchanged += 1
        else:
            ws = [(j, b, src.offset_for(j, place), None) for j, b in w]
            st_text = src.verts[fv][1] if fv is not None else None
            out_verts.append((st, st_text, ws))
        rest_out.append(place)
        reweighted.append(fv is None or len(w) != len(src_w[fv]) or
                          any(j != j0 or abs(b - b0) >= 1e-6 for (j, b), (j0, b0) in zip(w, src_w[fv])))
    untouched = unchanged == len(src.verts) == len(out_verts) and [tuple(t) for t in tris] == [tuple(t) for t in src.tris]
    cmd = src.commandline if untouched else "Blender %s, quakevr_models" % bpy.app.version_string
    anim = os.path.splitext(path)[0] + ".md5anim"
    B.write_md5mesh(path, src, out_verts, tris, cmd)
    bounds = None
    if not untouched:
        bounds = ([min(p[k] for p in rest_out) for k in range(3)], [max(p[k] for p in rest_out) for k in range(3)])
    B.write_md5anim(anim, anim_text, bounds, cmd)
    report({'INFO'}, "wrote %s and %s: %d vertices (%d as they were), %d triangles" % (
        path, os.path.basename(anim), len(out_verts), unchanged, len(tris)))
    # What was written is what the next export compares with.
    with open(path) as f:
        ob["qvr_md5mesh"] = pack(f.read().encode())
    with open(anim) as f:
        ob["qvr_md5anim"] = pack(f.read().encode())
    if builds:
        same_topology = (len(out_verts) == len(src.verts) and all(keys[i][0] == ("f", n) for n, i in enumerate(order))
                         and [tuple(t) for t in tris] == [tuple(t) for t in src.tris])
        if not same_topology:
            report({'WARNING'}, "the other builds were left as they are: carrying edits into them needs the same "
                                "vertices, triangles and texture coordinates (you added, deleted or split some)")
        else:
            carry_to_builds(path, src, out_verts, rest_out, rests, reweighted, report)
    rebase(ob, loop_vert, keys, order, tris)
    if write_skin:
        export_skin(ob, os.path.dirname(path), report, carry, quantize)


def carry_to_builds(path, src, out_verts, rest_out, rests, reweighted, report):
    """The other builds (vrbody_lean, vrbody, vrbody_brawny) get the same edits: each vertex moved by the same amount
    (the joints are the same in every build), and the new weights where they changed."""
    folder = os.path.dirname(path)
    this = os.path.splitext(os.path.basename(path))[0]
    for name, _ in BUILDS:
        other = os.path.join(folder, name + ".md5mesh")
        if name == this or not os.path.exists(other):
            continue
        with open(other) as f:
            om = B.Mesh(f.read(), name + ".md5mesh")
        if len(om.verts) != len(src.verts) or [tuple(t) for t in om.tris] != [tuple(t) for t in src.tris]:
            report({'WARNING'}, "%s isn't the same mesh as %s (vertices, triangles): left as it is" % (name, this))
            continue
        verts, rest, changed = [], [], 0
        for i in range(len(om.verts)):
            d = tuple(rest_out[i][k] - rests[i][k] for k in range(3))
            if d == (0.0, 0.0, 0.0) and not reweighted[i]:
                verts.append((om.verts[i][0], om.verts[i][1], list(om.vertex_weights(i))))
                rest.append(om.rest(i))
                continue
            place = tuple(B.f32(om.rest(i)[k] + d[k]) for k in range(3))
            w = [(j, b) for j, b, _, _ in out_verts[i][2]] if reweighted[i] else [(j, b) for j, b, _, _ in
                                                                                   om.vertex_weights(i)]
            verts.append((om.verts[i][0], om.verts[i][1], [(j, b, om.offset_for(j, place), None) for j, b in w]))
            rest.append(place)
            changed += 1
        if not changed:
            continue
        cmd = "Blender %s, quakevr_models (%s's edits)" % (bpy.app.version_string, this)
        B.write_md5mesh(other, om, verts, om.tris, cmd)
        anim = os.path.splitext(other)[0] + ".md5anim"
        with open(anim) as f:
            anim_text = f.read()
        B.write_md5anim(anim, anim_text, ([min(p[k] for p in rest) for k in range(3)],
                                          [max(p[k] for p in rest) for k in range(3)]), cmd)
        report({'INFO'}, "%s: the same edits (%d vertices)" % (name, changed))


def unedited(u, v, s0, t0):
    """The UV (u, v) still is the file's (s0, t0) (v = 1 - t, rounded to a float: within a rounding)."""
    return s0 == s0 and abs(u - s0) <= 1e-7 and abs(v - (1.0 - t0)) <= 2e-7


def uv_map(me):
    return me.uv_layers.get("UVMap") or me.uv_layers.active


def read_exact(me):
    """Each corner's texture coordinates as the file had them (NaN where unknown)."""
    nl = len(me.loops)
    out = np.full((nl, 2), np.nan, np.float32)
    for k, name in enumerate((A_S, A_T)):
        a = me.attributes.get(name)
        if a is not None and a.domain == 'CORNER' and a.data_type == 'FLOAT':
            col = np.empty(nl, np.float32)
            a.data.foreach_get("value", col)
            out[:, k] = col
    return out


def rebase(ob, loop_vert, keys, order, tris):
    """After an export, the corners name the file's new vertices and every Blender vertex is its own group."""
    me = ob.data
    new_index = {old: new for new, old in enumerate(order)}
    # Each corner's file vertex: the loop triangles' corners, by (Blender vertex, st).
    by = {}
    for i, (key, bv, st) in enumerate(keys):
        by[(bv, st)] = new_index[i]
    nl = len(me.loops)
    fv = np.full(nl, -1, np.int32)
    exact = read_exact(me)
    uvs = np.empty(nl * 2, np.float32)
    uv_map(me).data.foreach_get("uv", uvs)
    for li in range(nl):
        u, v = float(uvs[2 * li]), float(uvs[2 * li + 1])
        s0, t0 = float(exact[li][0]), float(exact[li][1])
        st = (s0, t0) if unedited(u, v, s0, t0) else (B.f32(u), B.f32(1.0 - v))
        fv[li] = by.get((int(loop_vert[li]), st), -1)
        exact[li] = st
    (me.attributes.get(A_FV) or me.attributes.new(A_FV, 'INT', 'CORNER')).data.foreach_set("value", fv)
    for k, name in enumerate((A_S, A_T)):
        (me.attributes.get(name) or me.attributes.new(name, 'FLOAT', 'CORNER')).data.foreach_set(
            "value", exact[:, k].copy())
    (me.attributes.get(A_VERT) or me.attributes.new(A_VERT, 'INT', 'POINT')).data.foreach_set(
        "value", list(range(len(me.vertices))))


def export_skin(ob, folder, report, carry=True, quantize=True):
    img = None
    for m in ob.data.materials:
        if m is not None and m.node_tree is not None:
            for n in m.node_tree.nodes:
                if n.type == 'TEX_IMAGE' and n.image is not None:
                    img = n.image
    if img is None:
        img = bpy.data.images.get("vrbody_skin")
    if img is None:
        report({'WARNING'}, "no skin image: the skins were left as they are")
        return
    which = int(ob.get("qvr_skin", 0))
    target = skin_path(folder, which)
    if not os.path.exists(target):
        raise BodyError("%s is missing: the skin is written over the existing one (its size and format)" % target)
    header, w, h, old, alpha, top = qpal.read_tga(target)
    if tuple(img.size) != (w, h):
        raise BodyError("the skin image is %dx%d; the body's skin is %dx%d (keep its size: the UVs are laid out on it)" %
                        (img.size[0], img.size[1], w, h))
    rgb = image_rgb(img)
    if quantize:
        q = qpal.Quantizer()
        idx_old = old_indices(old)
        new_idx = q.indices(rgb, idx_old)
        rgb = [qpal.PALETTE[i] for i in new_idx]
    changed = sum(1 for a, b in zip(old, rgb) if a != b)
    olds = {}
    if carry and which == 0:
        for n in range(1, SKINS):
            p = skin_path(folder, n)
            if os.path.exists(p):
                olds[n] = qpal.read_tga(p)
    qpal.write_tga(target, header, w, h, rgb, alpha, top)
    for n, (hd, w2, h2, var, al, tp) in olds.items():
        if (w2, h2) != (w, h):
            report({'WARNING'}, "%s is %dx%d, not %dx%d: left as it is" % (skin_path(folder, n), w2, h2, w, h))
            continue
        out = [v if v != o else nw for o, v, nw in zip(old, var, rgb)]
        qpal.write_tga(skin_path(folder, n), hd, w2, h2, out, al, tp)
    report({'INFO'}, "skin %s: %d texels changed%s%s" % (
        os.path.basename(target), changed, " (in Quake's palette)" if quantize else " (full colour)",
        ", carried into the %d other skins under their armour and wounds" % len(olds) if olds else ""))


def old_indices(rgb):
    """The palette index of each texel whose colour is exactly one (the rest None)."""
    lookup = {}
    for i, c in enumerate(qpal.PALETTE):
        lookup.setdefault(c, i)
    return [lookup.get(c) for c in rgb]


def apply_pose_to_mesh(context, ob):
    """The mesh as the armature's pose deforms it becomes its rest shape; the bones go back to rest (they are the
    engine's)."""
    arm = armature_of(ob)
    if context.object is not None and context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    if arm is None or all(pb.matrix_basis == Matrix.Identity(4) for pb in arm.pose.bones):
        return 0  # not posed: nothing to apply (and the rest shape stays exactly as it is)
    deps = context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(deps)
    em = ev.to_mesh()
    if len(em.vertices) != len(ob.data.vertices):
        ev.to_mesh_clear()
        raise BodyError("the body's modifiers change its vertices: apply them first")
    co = np.empty(len(em.vertices) * 3, np.float32)
    em.vertices.foreach_get("co", co)
    ev.to_mesh_clear()
    rest = np.empty(len(co), np.float32)
    ob.data.vertices.foreach_get("co", rest)
    # Only the vertices the pose moved: the others keep their rest places exactly (not the armature's rounding).
    moved = np.abs(co - rest).reshape(-1, 3).max(axis=1) > 1e-5
    rest.reshape(-1, 3)[moved] = co.reshape(-1, 3)[moved]
    ob.data.vertices.foreach_set("co", rest)
    ob.data.update()
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    return int(moved.sum())
