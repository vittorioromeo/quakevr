# mdl_blender.py -- a Quake alias model (a weapon's v_*.mdl, the wrist gadget's vrgadget.mdl) in Blender and back,
# keeping what the engine relies on (docs/vr-port/MODELS_IN_BLENDER.md):
#
# - The header's scale and origin (the weapon Scale pivots about the origin; the anchors and ports are in model space),
#   the frames (count, names, groups), the skins (count, size, groups).
# - Every old vertex's index, and the old triangles in their places: vr_anchor.cpp's strip order, which the anchors
#   index, depends on them. An old vertex keeps its index whatever is done to it (deleted, it stays in the file
#   unused); new vertices and triangles go after the old ones, and a new triangle never uses an old vertex (it gets
#   a twin at the same place), so no old strip changes. An old triangle deleted or changed (other corners, other
#   winding) leaves its place: the export then checks every anchor.
# - Bytes: a vertex, a texel, a normal, a frame's bounds that didn't change are written as they were read, so an
#   unedited model comes back byte for byte.
#
# In Blender, the model's vertices that are at the same place in every frame are one vertex (the file splits them
# along the skin's seams and where the shading is flat), so edits don't tear. Frame 0 is the basis shape key and
# each other frame a shape key: an edit of frame 0 is carried into every frame by the part it is on (its rigid
# motion over the animation: recoil, the pump, spinning barrels), unless that frame's key was edited itself.
# Attributes (qvr_*) map Blender's vertices, corners and faces back to the file's.

import base64
import math
import os
import re
import zlib

import bpy
import bmesh
import numpy as np

from . import mdl
from . import qpal

KIND = "mdl"
A_VERT = "qvr_vert"   # POINT int: the vertex's group (the file's vertices it stands for)
A_VID = "qvr_vid"     # CORNER int: the file's vertex this corner is
A_TRI = "qvr_tri"     # FACE int: the file's triangle this face is
A_REF = "qvr_ref%d"   # POINT float3 per pose: the vertex's place in that pose as last read or written


class ExportError(Exception):
    pass


def pack(data):
    return base64.b64encode(zlib.compress(data, 9)).decode("ascii")


def unpack(s):
    return zlib.decompress(base64.b64decode(s))


def f32(x):
    return float(np.float32(x))


# ----------------------------------------------------------------------------
# Import


def skin_image_names(name, model):
    out = []
    for k, (g, _, ims) in enumerate(model.skins):
        for i in range(len(ims)):
            out.append("%s_skin%d" % (name, k) + ("" if g == 0 else "_%d" % i))
    return out


def set_image(img_name, w, h, indices):
    img = bpy.data.images.get(img_name)
    if img is not None and (img.size[0] != w or img.size[1] != h):
        bpy.data.images.remove(img)
        img = None
    if img is None:
        img = bpy.data.images.new(img_name, w, h, alpha=False)
    pal = np.array(qpal.PALETTE, np.float32) / 255.0
    idx = np.frombuffer(bytes(indices), np.uint8).reshape(h, w)[::-1]  # Blender's rows go bottom to top
    rgba = np.ones((h, w, 4), np.float32)
    rgba[:, :, :3] = pal[idx]
    img.pixels.foreach_set(rgba.ravel())
    img.update()
    img.pack()
    return img


def image_rgb(img):
    """The image's texels as (r, g, b) 0..255, rows top to bottom."""
    w, h = img.size
    flat = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(flat)
    px = np.clip(np.round(flat.reshape(h, w, 4)[::-1, :, :3] * 255.0), 0, 255).astype(np.int64)
    return [tuple(c) for c in px.reshape(-1, 3).tolist()]


def corner_st(model, tri, k):
    """The texel (s, t) corner k of triangle tri reads (onseam vertices on back-facing triangles half a skin on)."""
    ff = tri[0]
    v = tri[1 + k]
    onseam, s, t = model.st[v]
    if onseam and not ff:
        s += model.skin_size[0] // 2
    return s, t


def st_to_uv(model, s, t):
    w, h = model.skin_size
    return ((s + 0.5) / w, 1.0 - (t + 0.5) / h)


def uv_to_st(model, u, v):
    w, h = model.skin_size
    return int(math.floor(u * w)), int(math.floor((1.0 - v) * h))


def import_mdl(context, path):
    with open(path, "rb") as f:
        data = f.read()
    base = os.path.basename(path)
    model = mdl.Model(data, base)
    name = os.path.splitext(base)[0]
    ob = build_object(context, name, model)
    ob["qvr_kind"] = KIND
    ob["qvr_source"] = path
    ob["qvr_name"] = base  # the model the engine knows it as (its anchors: vr_weapons.inc's progs/<name>)
    return ob


def build_object(context, name, model):
    """The object for `model`, its vertices welded where they are at the same place in every pose."""
    poses = model.pose_bytes()
    group_of, groups = mdl.weld(model, poses)
    places = [[model.place(p[groups[g][0]]) for g in range(len(groups))] for p in poses]

    faces, face_tri, degenerate = [], [], []
    for ti, (ff, a, b, c) in enumerate(model.tris):
        ga, gb, gc = group_of[a], group_of[b], group_of[c]
        if ga == gb or gb == gc or ga == gc:
            degenerate.append(ti)  # covers nothing: kept in its place in the file, not in Blender
            continue
        faces.append((ga, gc, gb))  # clockwise (Quake) to counter-clockwise
        face_tri.append(ti)

    me = bpy.data.meshes.new(name)
    me.from_pydata(places[0], [], faces)
    me.update()
    if len(me.polygons) != len(faces):
        raise mdl.MdlError("%s: Blender kept %d of the %d triangles" % (model.name, len(me.polygons), len(faces)))
    ob = bpy.data.objects.new(name, me)
    context.collection.objects.link(ob)

    # Mapping attributes.
    av = me.attributes.new(A_VERT, 'INT', 'POINT')
    av.data.foreach_set("value", list(range(len(groups))))
    at = me.attributes.new(A_TRI, 'INT', 'FACE')
    at.data.foreach_set("value", face_tri)
    vids, uvs = [], []
    for ti in face_tri:
        tri = model.tris[ti]
        for k in (0, 2, 1):
            vids.append(tri[1 + k])
            uvs.extend(st_to_uv(model, *corner_st(model, tri, k)))
    ac = me.attributes.new(A_VID, 'INT', 'CORNER')
    ac.data.foreach_set("value", vids)
    uv = me.uv_layers.new(name="UVMap")
    uv.data.foreach_set("uv", uvs)
    for p, pl in enumerate(places):
        ar = me.attributes.new(A_REF % p, 'FLOAT_VECTOR', 'POINT')
        ar.data.foreach_set("vector", [c for q in pl for c in q])

    # Shading as the file has it: smooth where faces share a file vertex, sharp where they don't; UV seams where the
    # skin is split.
    me.shade_smooth()
    bm = bmesh.new()
    bm.from_mesh(me)
    vid_layer = bm.loops.layers.int.get(A_VID)
    uv_layer = bm.loops.layers.uv.active
    sharp = [False] * len(bm.edges)
    for e in bm.edges:
        lf = e.link_faces
        if len(lf) != 2:
            continue
        for v in e.verts:
            la = next(l for l in lf[0].loops if l.vert == v)
            lb = next(l for l in lf[1].loops if l.vert == v)
            if la[vid_layer] != lb[vid_layer]:
                sharp[e.index] = True
            if (la[uv_layer].uv - lb[uv_layer].uv).length > 1e-6:
                e.seam = True
    bm.to_mesh(me)
    bm.free()
    sa = me.attributes.get("sharp_edge") or me.attributes.new("sharp_edge", 'BOOLEAN', 'EDGE')
    sa.data.foreach_set("value", sharp)

    # Frames: frame 0 the basis, every other pose a shape key.
    names = model.pose_names()
    if len(poses) > 1:
        ob.shape_key_add(name=names[0] or "frame0", from_mix=False)
        for p in range(1, len(poses)):
            kb = ob.shape_key_add(name=names[p] or "frame%d" % p, from_mix=False)
            kb.data.foreach_set("co", [c for q in places[p] for c in q])
        me.shape_keys.use_relative = True
        animate_frames(context, ob, len(poses))

    # Skins.
    w, h = model.skin_size
    img_names = skin_image_names(name, model)
    i = 0
    for g, _, ims in model.skins:
        for im in ims:
            set_image(img_names[i], w, h, im)
            i += 1
    mat = bpy.data.materials.new(name)
    try:
        mat.use_nodes = True
    except AttributeError:
        pass
    nodes = mat.node_tree.nodes
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images[img_names[0]]
    tex.interpolation = 'Closest'
    bsdf = next(n for n in nodes if n.type == 'BSDF_PRINCIPLED')
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    me.materials.append(mat)

    set_reference(ob, model, group_of, degenerate)
    ob["qvr_images"] = img_names
    for o in context.selected_objects:
        o.select_set(False)
    context.view_layer.objects.active = ob
    ob.select_set(True)
    return ob


def set_reference(ob, model, group_of, degenerate):
    ob["qvr_mdl"] = pack(model.to_bytes())
    ob["qvr_groups"] = pack(np.asarray(group_of, np.int32).tobytes())
    ob["qvr_degenerate"] = list(degenerate)


def animate_frames(context, ob, n):
    """Scene frames 1..n show the model's poses (a key per pose at 1 on its frame, constant): play or scrub the
    timeline to see the animation. The basis is frame 1."""
    key = ob.data.shape_keys
    key.animation_data_create()
    act = bpy.data.actions.new(ob.name + "_frames")
    key.animation_data.action = act
    blocks = key.key_blocks
    for p in range(1, n):
        kb = blocks[p]
        for f in range(1, n + 1):
            kb.value = 1.0 if f == p + 1 else 0.0
            kb.keyframe_insert("value", frame=f)
        kb.value = 0.0
    try:
        fcurves = act.fcurves
    except AttributeError:  # Blender 5: layered actions
        fcurves = [fc for layer in act.layers for strip in layer.strips for bag in strip.channelbags
                   for fc in bag.fcurves]
    for fc in fcurves:
        for kp in fc.keyframe_points:
            kp.interpolation = 'CONSTANT'
    scene = context.scene
    scene.frame_start, scene.frame_end = 1, n
    scene.frame_set(1)


def reference(ob):
    model = mdl.Model(unpack(ob["qvr_mdl"]), ob.get("qvr_name", ob.name + ".mdl"))
    group_of = np.frombuffer(unpack(ob["qvr_groups"]), np.int32).tolist()
    return model, group_of, set(ob.get("qvr_degenerate", []))


# ----------------------------------------------------------------------------
# Carrying frame 0's edits into the other frames


def kabsch(A, B):
    """The rotation R that best takes the points A (centred) onto B (centred): B ~ R A."""
    A = A - A.mean(0)
    B = B - B.mean(0)
    H = A.T @ B
    U, _, Vt = np.linalg.svd(H)
    d = np.sign(np.linalg.det(Vt.T @ U.T)) or 1.0
    D = np.diag([1.0, 1.0, d])
    return Vt.T @ D @ U.T


class Frames:
    """Each Blender vertex's place in every pose, as the export writes it (see the top)."""

    def __init__(self, ob, model, group_of):
        me = ob.data
        nv = len(me.vertices)
        self.nposes = len(model.poses())
        ref_poses = model.pose_bytes()
        ngroups = max(group_of) + 1 if group_of else 0
        # Each group's reference places (from the file last read or written) and its piece (connected by triangles).
        self.ref = np.zeros((self.nposes, ngroups, 3))
        first = {}
        for v, g in enumerate(group_of):
            if g >= 0 and g not in first:
                first[g] = v
        for g, v in first.items():
            for p in range(self.nposes):
                self.ref[p, g] = model.place(ref_poses[p][v])
        parent = list(range(ngroups))

        def find(x):
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        for ff, a, b, c in model.tris:
            ga, gb, gc = group_of[a], group_of[b], group_of[c]
            if min(ga, gb, gc) < 0:
                continue
            parent[find(ga)] = find(gb)
            parent[find(gb)] = find(gc)
        piece_of = [find(g) for g in range(ngroups)]
        members = {}
        for g, r in enumerate(piece_of):
            members.setdefault(r, []).append(g)
        self.rot = {}  # (piece, pose) -> R
        for r, gs in members.items():
            P0 = self.ref[0, gs]
            for p in range(1, self.nposes):
                if len(gs) < 3:
                    self.rot[r, p] = np.eye(3)
                else:
                    self.rot[r, p] = kabsch(P0, self.ref[p, gs])
        self.piece_of = piece_of

        # Blender's vertices: the holder of each group (the first vertex with it), the rest new.
        attr = me.attributes.get(A_VERT)
        gid = np.full(nv, -1, np.int64)
        if attr is not None:
            vals = np.empty(nv, np.int32)
            attr.data.foreach_get("value", vals)
            gid[:] = vals
        self.holder = {}
        self.group = np.full(nv, -1, np.int64)
        for v in range(nv):
            g = int(gid[v])
            if 0 <= g < ngroups and g not in self.holder:
                self.holder[g] = v
                self.group[v] = g

        # Current places: the basis (the mesh) and each key.
        keys = me.shape_keys.key_blocks if me.shape_keys is not None else None
        if keys is not None and len(keys) != self.nposes:
            raise ExportError("the model has %d frames and the object %d shape keys: keep one per frame (don't add or "
                              "delete shape keys)" % (self.nposes, len(keys)))
        cur = np.zeros((self.nposes, nv, 3))
        buf = np.empty(nv * 3, np.float32)
        if keys is None:
            me.vertices.foreach_get("co", buf)
            cur[0] = buf.reshape(nv, 3)
        else:
            for p in range(self.nposes):
                keys[p].data.foreach_get("co", buf)
                cur[p] = buf.reshape(nv, 3)
            if keys[0].relative_key != keys[0] or any(k.relative_key != keys[0] for k in keys):
                raise ExportError("every shape key must be relative to the first (frame 0)")
        # The places last read or written (to tell a frame edited on its own from one that follows frame 0).
        last = np.full((self.nposes, nv, 3), np.nan)
        for p in range(self.nposes):
            a = me.attributes.get(A_REF % p)
            if a is not None and a.domain == 'POINT' and a.data_type == 'FLOAT_VECTOR':
                a.data.foreach_get("vector", buf)
                last[p] = buf.reshape(nv, 3)

        out = np.zeros((self.nposes, nv, 3))
        out[0] = cur[0]
        self.own_edits = [0] * self.nposes
        eps = 1e-5
        held = np.array([self.group[v] >= 0 for v in range(nv)])
        for v in range(nv):
            if not held[v]:
                continue
            g = self.group[v]
            d0 = cur[0, v] - self.ref[0, g]
            moved0 = np.abs(d0).max() > eps
            for p in range(1, self.nposes):
                k = cur[p, v]
                derived = False
                if not np.isnan(last[p, v]).any():
                    derived = (np.abs(k - last[p, v]).max() <= eps or
                               np.abs(k - (last[p, v] + cur[0, v] - last[0, v])).max() <= eps)
                else:
                    derived = (np.abs(k - self.ref[p, g]).max() <= eps or np.abs(k - (self.ref[p, g] + d0)).max() <= eps)
                if not derived:
                    out[p, v] = k
                    self.own_edits[p] += 1
                elif not moved0:
                    out[p, v] = self.ref[p, g]
                else:
                    out[p, v] = self.ref[p, g] + self.rot[piece_of[g], p] @ d0
        # New vertices ride the nearest held vertex of their own piece of the mesh (or the nearest overall).
        new = [v for v in range(nv) if not held[v]]
        self.new_count = len(new)
        if new and self.nposes > 1:
            comp = mesh_components(me)
            held_idx = np.nonzero(held)[0]
            if len(held_idx) == 0:
                for p in range(1, self.nposes):
                    out[p, new] = cur[0, new]
            else:
                by_comp = {}
                for v in held_idx:
                    by_comp.setdefault(comp[v], []).append(v)
                for v in new:
                    cands = by_comp.get(comp[v])
                    cands = np.array(cands) if cands else held_idx
                    u = cands[np.argmin(((cur[0, cands] - cur[0, v]) ** 2).sum(1))]
                    r = piece_of[self.group[u]]
                    off = cur[0, v] - cur[0, u]
                    for p in range(1, self.nposes):
                        out[p, v] = out[p, u] + self.rot[r, p] @ off
        self.places = out
        self.cur = cur


def mesh_components(me):
    parent = list(range(len(me.vertices)))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    ev = np.empty(len(me.edges) * 2, np.int32)
    me.edges.foreach_get("vertices", ev)
    for a, b in ev.reshape(-1, 2).tolist():
        parent[find(a)] = find(b)
    return [find(v) for v in range(len(me.vertices))]


def carry_frames(ob):
    """Writes into the shape keys what the export would write (frame 0's edits carried along with each part)."""
    model, group_of, _ = reference(ob)
    fr = Frames(ob, model, group_of)
    keys = ob.data.shape_keys
    if keys is None:
        return 0
    for p in range(1, fr.nposes):
        keys.key_blocks[p].data.foreach_set("co", fr.places[p].astype(np.float32).ravel())
    set_refs(ob, fr.places)
    ob.data.update()
    return fr.nposes - 1


def set_refs(ob, places):
    me = ob.data
    for p in range(len(places)):
        a = me.attributes.get(A_REF % p)
        if a is None:
            a = me.attributes.new(A_REF % p, 'FLOAT_VECTOR', 'POINT')
        a.data.foreach_set("vector", places[p].astype(np.float32).ravel())


# ----------------------------------------------------------------------------
# Export


def triangulate(ob):
    me = ob.data
    if all(len(p.vertices) == 3 for p in me.polygons):
        return 0
    bm = bmesh.new()
    bm.from_mesh(me)
    faces = [f for f in bm.faces if len(f.verts) > 3]
    n = len(faces)
    bmesh.ops.triangulate(bm, faces=faces, quad_method='FIXED', ngon_method='EAR_CLIP')
    bm.to_mesh(me)
    bm.free()
    me.update()
    return n


def export_mdl(context, ob, path, report, grow=False, allow_remap=False):
    """Writes the object as a Quake model at `path`. Returns (the report's lines, anchors moved, anchors renamed)."""
    if ob.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    if len([m for m in ob.modifiers if m.show_viewport]):
        report({'WARNING'}, "the object's modifiers are not exported: apply them first")
    tri_count = triangulate(ob)
    if tri_count:
        report({'INFO'}, "%d faces with more than 3 corners were split into triangles (the model is triangles)" %
               tri_count)
    old, group_of, degenerate = reference(ob)
    me = ob.data
    nv = len(me.vertices)
    fr = Frames(ob, old, group_of)
    P = fr.nposes
    old_poses = old.pose_bytes()
    sw, sh = old.skin_size
    scale = list(old.scale)
    origin = old.origin

    # Blender's corners -> file vertices.
    nl = len(me.loops)
    loop_vert = np.empty(nl, np.int32)
    me.loops.foreach_get("vertex_index", loop_vert)
    vid_attr = me.attributes.get(A_VID)
    loop_vid = np.full(nl, -1, np.int32)
    if vid_attr is not None and vid_attr.domain == 'CORNER':
        vid_attr.data.foreach_get("value", loop_vid)
    uv_layer = me.uv_layers.active
    if uv_layer is None:
        raise ExportError("the model has no UV map")
    uvs = np.empty(nl * 2, np.float32)
    uv_layer.data.foreach_get("uv", uvs)
    uvs = uvs.reshape(nl, 2)
    tri_attr = me.attributes.get(A_TRI)
    face_tri = np.full(len(me.polygons), -1, np.int32)
    if tri_attr is not None and tri_attr.domain == 'FACE':
        tri_attr.data.foreach_get("value", face_tri)

    old_nv, old_nt = old.num_verts, len(old.tris)
    vid_group = group_of

    def corner_st_of(li):
        return uv_to_st(old, float(uvs[li][0]), float(uvs[li][1]))

    # Each old vertex's Blender vertex: its group's (None: gone; the vertex stays in the file as it was, unused).
    vert_of_vid = [fr.holder.get(g) if g >= 0 else None for g in vid_group[:old_nv]]

    def holds(li):
        """The corner still is its file vertex: that vertex is at the corner's Blender vertex."""
        vid = int(loop_vid[li])
        return 0 <= vid < old_nv and vert_of_vid[vid] == int(loop_vert[li])

    def same_triangle(ti, ls):
        ff, a, b, c = old.tris[ti]
        got = [int(loop_vid[li]) for li in ls]  # Blender's order: a, c, b
        want = [a, c, b]
        return any(got == want[r:] + want[:r] for r in range(3))

    # Old triangles kept: a face that still is its triangle (the same file vertices, the same winding).
    kept = {}  # old triangle -> face
    for f in me.polygons:
        ti = int(face_tri[f.index])
        if not 0 <= ti < old_nt or ti in kept or ti in degenerate:
            continue
        ls = list(f.loop_indices)
        if len(ls) == 3 and all(holds(li) for li in ls) and same_triangle(ti, ls):
            kept[ti] = f.index
    # A face whose corners moved to other Blender vertices (extruded: its old corners stay, joined to it by new
    # sides) keeps its place too if the old vertices it names aren't used by any face kept: they move with it.
    used = {int(loop_vid[li]) for fi in kept.values() for li in me.polygons[fi].loop_indices}
    for f in me.polygons:
        ti = int(face_tri[f.index])
        if not 0 <= ti < old_nt or ti in kept or ti in degenerate:
            continue
        ls = list(f.loop_indices)
        if len(ls) != 3 or not same_triangle(ti, ls):
            continue
        movers = [li for li in ls if not holds(li)]
        if any(int(loop_vid[li]) in used for li in movers):
            continue
        for li in movers:
            vert_of_vid[int(loop_vid[li])] = int(loop_vert[li])
        used.update(int(loop_vid[li]) for li in ls)
        kept[ti] = f.index

    # The skin coordinates of the old vertices the kept triangles use: kept as they were, moved in place when every
    # corner agrees, and a triangle that disagrees with the rest leaves its place.
    half = sw // 2
    votes = {}  # vid -> {(front s, t) or (back s, t): count}
    for ti, fi in kept.items():
        ff = old.tris[ti][0]
        for li in me.polygons[fi].loop_indices:
            vid = int(loop_vid[li])
            s, t = corner_st_of(li)
            votes.setdefault(vid, {}).setdefault((ff, s, t), []).append(ti)
    new_st = {}
    moved_tris = set()
    for vid, opts in votes.items():
        onseam, s0, t0 = old.st[vid]
        orig = (onseam, s0, t0)

        def reads(on, s, t, ff):
            return (s + half, t) if (on and not ff) else (s, t)

        if all(reads(*orig, ff) == (cs, ct) for (ff, cs, ct) in opts):
            continue  # unchanged
        # The reading (onseam, s, t) most corners agree with (the old one first on a tie).
        cands = [orig]
        for (ff, cs, ct) in opts:
            cands += [(0, cs, ct), (1, cs, ct) if ff else (1, cs - half, ct)]
        best, best_n = None, -1
        for c in cands:
            n = sum(len(tis) for (ff, cs, ct), tis in opts.items() if reads(*c, ff) == (cs, ct))
            if n > best_n:
                best, best_n = c, n
        new_st[vid] = list(best)
        for (ff, cs, ct), tis in opts.items():
            if reads(*best, ff) != (cs, ct):
                moved_tris.update(tis)
    for ti in moved_tris:
        del kept[ti]

    # The file's vertices: the old ones (their places from their group's Blender vertex, or as they were if it's
    # gone), then the new ones: a (Blender vertex, texel) each, the twins of old vertices among them.
    st = [list(x) for x in old.st]
    for vid, v in new_st.items():
        if any(ti in kept for tis in votes[vid].values() for ti in tis):
            st[vid] = v
    new_verts = []  # (Blender vertex, s, t)
    new_key = {}
    twin_of = {}
    new_tris = []
    kept_faces = set(kept.values())
    for f in me.polygons:
        if f.index in kept_faces:
            continue
        ls = list(f.loop_indices)
        idx = []
        for li in ls:
            bv = int(loop_vert[li])
            s, t = corner_st_of(li)
            key = (bv, s, t)
            if key not in new_key:
                new_key[key] = old_nv + len(new_verts)
                new_verts.append(key)
                if holds(li):
                    vid = int(loop_vid[li])
                    o = st[vid]
                    if (o[1], o[2]) == (s, t) or (o[0] and (o[1] + half, o[2]) == (s, t)):
                        twin_of[new_key[key]] = vid
            idx.append(new_key[key])
        new_tris.append((1, idx[0], idx[2], idx[1]))  # counter-clockwise to clockwise (Quake)
    total_v = old_nv + len(new_verts)
    if total_v > 0x7FFF:
        raise ExportError("%d vertices: the engine takes 32767 at most" % total_v)

    # Places, per pose: bytes on the header's grid.
    places = np.zeros((P, total_v, 3))
    for vid in range(old_nv):
        bv = vert_of_vid[vid]
        if bv is None:
            for p in range(P):
                places[p, vid] = old.place(old_poses[p][vid])
        else:
            places[:, vid] = fr.places[:, bv]
    for i, (bv, s, t) in enumerate(new_verts):
        places[:, old_nv + i] = fr.places[:, bv]
    org = np.array(origin, np.float64)
    sc = np.array(scale, np.float64)
    q = np.round((places - org) / sc)
    grow_lines = []
    if (q < 0).any() or (q > 255).any():
        out = (q < 0).any(2).any(0) | (q > 255).any(2).any(0)
        if not grow:
            bad = sorted(int(b) for b in np.nonzero(out)[0])
            select_file_vertices(me, bad, vert_of_vid, new_verts, old_nv)
            hi = org + 255 * sc
            frames = [old.pose_names()[p] for p in range(P) if ((q[p] < 0) | (q[p] > 255)).any()]
            raise ExportError("%d vertices (selected) are outside the model's box in frames %s (x %.2f..%.2f, y %.2f..%.2f, "
                              "z %.2f..%.2f: a byte per axis on the header's grid). Keep them inside, or tick Grow the Box (the grid "
                              "gets coarser, every vertex moves a little; past the low side the origin moves too, which "
                              "the weapon's Scale turns about: the export then prints the offsets that keep it in "
                              "place)" % (len(bad), ", ".join(frames), org[0], hi[0], org[1], hi[1], org[2], hi[2]))
        low = (q < 0).any(axis=(0, 1))    # the axes past the box's low side,
        high = (q > 255).any(axis=(0, 1))  # and its high side: only these change
        lo = np.where(low, places.min(axis=(0, 1)) - 1e-4, org)
        hi = np.where(high, places.max(axis=(0, 1)) + 1e-4, org + 255 * sc)
        new_org, new_sc = org.copy(), sc.copy()
        for k in range(3):
            if low[k]:
                new_org[k] = f32(lo[k])
                while new_org[k] > lo[k]:
                    new_org[k] = float(np.nextafter(np.float32(new_org[k]), np.float32(-1e30)))
            need = (hi[k] - new_org[k]) / 255.0
            if (low[k] or high[k]) and need > sc[k]:
                new_sc[k] = f32(need * (1.0 + 1e-6))
        grow_lines = grow_report(old, ob, path, org, sc, new_org, new_sc)
        org, sc = new_org, new_sc
        origin, scale = tuple(float(x) for x in org), [float(x) for x in sc]
        q = np.round((places - org) / sc)
        for line in grow_lines:
            report({'WARNING'}, line)
    q = np.clip(q, 0, 255).astype(np.int64)
    same_grid = tuple(scale) == tuple(old.scale) and tuple(origin) == tuple(old.origin)
    # Unchanged old vertices keep their bytes exactly.
    old_b = np.array([[p[v][:3] for v in range(old_nv)] for p in old_poses], np.int64).reshape(P, old_nv, 3)
    if same_grid:
        same = np.abs(places[:, :old_nv] - (org + old_b * sc)).max(axis=2) <= np.abs(sc).min() * 1e-3
        q[:, :old_nv][same] = old_b[same]

    tris = [old.tris[ti] for ti in range(old_nt) if ti in kept or ti in degenerate] + new_tris
    tri_old_index = [ti for ti in range(old_nt) if ti in kept or ti in degenerate] + [-1] * len(new_tris)

    # Normals: an old vertex keeps its normal in a pose unless something round it changed there.
    changed = np.zeros((P, total_v), bool)
    changed[:, :old_nv] = (q[:, :old_nv] != old_b).any(axis=2)
    changed[:, old_nv:] = True
    tris_of = [[] for _ in range(total_v)]
    for i, t in enumerate(tris):
        for v in t[1:]:
            tris_of[v].append(i)
    old_tris_of = [[] for _ in range(old_nv)]
    for i, t in enumerate(old.tris):
        for v in t[1:]:
            old_tris_of[v].append(i)
    shares = {}  # a vertex -> the vertices sharing its normal (an old vertex and its twins)
    for n, o in twin_of.items():
        s = shares.setdefault(o, [o])
        s.append(n)
        shares[n] = s
    normals = np.zeros((P, total_v), np.int64)
    for p in range(P):
        for v in range(old_nv):
            normals[p, v] = old_poses[p][v][3]
    grid_pos = org + q * sc
    tri_arr = np.array([t[1:] for t in tris], np.int64).reshape(-1, 3)
    new_tri = np.array([oi < 0 for oi in tri_old_index], bool)
    same_set = [sorted(tri_old_index[ti] for ti in tris_of[v]) == sorted(old_tris_of[v]) for v in range(old_nv)]
    table = np.array(mdl.anorms())
    for p in range(P):
        a, b, c = grid_pos[p, tri_arr[:, 0]], grid_pos[p, tri_arr[:, 1]], grid_pos[p, tri_arr[:, 2]]
        fn = np.cross(c - a, b - a)  # clockwise seen from outside: outwards
        ln = np.linalg.norm(fn, axis=1)
        fn = fn / np.maximum(ln, 1e-12)[:, None]
        dirty = changed[p, tri_arr].any(axis=1) | new_tri if len(tri_arr) else new_tri
        for v in range(total_v):
            group = shares.get(v, [v])
            ts = [ti for g in group for ti in tris_of[g]]
            if v < old_nv and same_set[v] and len(group) == 1 and not changed[p, v] and not dirty[ts].any():
                continue  # nothing round it changed: its normal as it was
            if not ts:
                if v >= old_nv:
                    normals[p, v] = normals[p, twin_of[v]] if v in twin_of else 0
                continue
            n = fn[ts].sum(0)
            ln = np.linalg.norm(n)
            if ln > 1e-9:
                normals[p, v] = int(np.argmax(table @ (n / ln)))

    # The skins.
    img_names = list(ob.get("qvr_images", []))
    quant = qpal.Quantizer()
    skins = []
    i = 0
    changed_texels = 0
    for g, iv, ims in old.skins:
        out = []
        for im in ims:
            img = bpy.data.images.get(img_names[i]) if i < len(img_names) else None
            i += 1
            if img is None:
                out.append(im)
                continue
            if tuple(img.size) != (sw, sh):
                raise ExportError("the skin image %s is %dx%d; the model's skin is %dx%d (keep its size: the UVs are in "
                                  "its texels)" % (img.name, img.size[0], img.size[1], sw, sh))
            new = bytes(quant.indices(image_rgb(img), im))
            changed_texels += sum(1 for a, b in zip(im, new) if a != b)
            (c0, a0), (c1, a1) = corner_area(im, sw, sh), corner_area(new, sw, sh)
            if c0 != c1 or not a1 <= a0:
                report({'WARNING'}, "%s: the flat area at the skin's top-left corner grew or changed colour. The "
                                    "engine takes that area as background and paints over it with its neighbours' "
                                    "colours (Mod_FloodFillSkin), so what you painted there won't show: keep the "
                                    "corner texel's colour as it was and don't paint up to it in that colour" % img.name)
            out.append(new)
        skins.append([g, iv, out])

    # The new model.
    new = mdl.Model(old.to_bytes(), old.name)
    new.h[2:5] = scale
    new.h[5:8] = origin
    new.skins = skins
    new.st = st + [[0, s, t] for (_, s, t) in new_verts]
    new.tris = [tuple(t) for t in tris]
    pose_i = 0
    geometry_changed = False
    for fr_ in new.frames:
        subs = [fr_] if fr_[0] == "simple" else fr_[3]
        for sub in subs:
            hdr = bytearray(sub[1] if fr_[0] == "simple" else sub[0])
            vb = bytearray()
            for v in range(total_v):
                vb += bytes((int(q[pose_i, v, 0]), int(q[pose_i, v, 1]), int(q[pose_i, v, 2]), int(normals[pose_i, v])))
            old_vb = old.pose_verts(old.poses()[pose_i])
            if bytes(vb) != old_vb:
                geometry_changed = True
                arr = q[pose_i]
                hdr[0:3] = bytes(int(x) for x in arr.min(0))
                hdr[4:7] = bytes(int(x) for x in arr.max(0))
            if fr_[0] == "simple":
                fr_[1], fr_[2] = bytes(hdr), bytes(vb)
            else:
                sub[0], sub[1] = bytes(hdr), bytes(vb)
            pose_i += 1
        if fr_[0] == "group" and geometry_changed:
            bb = bytearray(fr_[1])
            bb[0:3] = bytes(min(s[0][k] for s in fr_[3]) for k in range(3))
            bb[4:7] = bytes(max(s[0][4 + k] for s in fr_[3]) for k in range(3))
            fr_[1] = bytes(bb)
    if geometry_changed or not same_grid:
        radius = float(np.linalg.norm(grid_pos, axis=2).max())
        new.h[8] = max(new.h[8], f32(radius))
    data = new.to_bytes()
    new = mdl.Model(data, old.name)  # read back: what the engine will read

    # The anchors, before and after: an anchor naming another vertex refuses the export unless remapping is asked.
    lines, moved, renamed = anchor_check(path, old, new, ob.get("qvr_source"))
    if renamed:
        remap = remap_lines(renamed)
        if not allow_remap:
            for line in lines + remap:
                print(line)
            raise ExportError("%d anchors would name other vertices (%s): old triangles were deleted or changed so "
                              "the strip order moved (see the report). Nothing was written. Undo that, or tick Remap "
                              "Anchors and set the new indices it prints" % (
                                  len(renamed), ", ".join("%s index %d" % (w, i) for w, i, *_ in renamed)))
        lines += remap
    with open(path, "wb") as f:
        f.write(data)
    report({'INFO'}, "wrote %s: %d vertices (%d new), %d triangles (%d kept in place, %d new), %d frames, %d texels "
           "changed" % (path, total_v, len(new_verts), len(tris), len(kept), len(new_tris), P, changed_texels))
    if fr.new_count:
        report({'INFO'}, "%d new vertices follow the part they're on in every frame" % fr.new_count)
    if any(fr.own_edits[1:]):
        report({'INFO'}, "frames edited on their own (kept as you left them): %s" % ", ".join(
            "%s (%d vertices)" % (new.pose_names()[p], n) for p, n in enumerate(fr.own_edits) if n))
    lines += holes_check(mdl.repo_root(path) or mdl.repo_root(ob.get("qvr_source") or path), path, old)
    for line in lines:
        print(line)
    # The object now stands for the file written: its attributes and references follow it.
    rebase(ob, new, kept, new_key, tri_old_index, vert_of_vid, grid_pos, degenerate)
    return lines, moved, renamed


def grow_report(old, ob, path, org, sc, new_org, new_sc):
    """What growing the box does: the grid, and where the origin moved the weapon's Scale pivot, the offsets that keep
    the drawn weapon where it was (for each slot using the model, at its Scale)."""
    lines = ["the box grew: origin %.4f %.4f %.4f -> %.4f %.4f %.4f, scale %.5f %.5f %.5f -> %.5f %.5f %.5f; every vertex "
             "went onto the new grid, moving up to half a step along the axes that changed (%.4f units at most)" % (
                 tuple(org) + tuple(new_org) + tuple(sc) + tuple(new_sc) +
                 (max(float(b) / 2 for a, b, o, n in zip(sc, new_sc, org, new_org) if a != b or o != n),))]
    d = new_org - org
    if np.abs(d).max() > 0:
        root = mdl.repo_root(path) or mdl.repo_root(ob.get("qvr_source") or path)
        slots = mdl.weapon_slots(root, old.name)
        if slots:
            lines.append("the origin moved (%.4f %.4f %.4f): the weapon's Scale turns about it, so the drawn weapon "
                         "moves by that times (1 - Scale). To keep it in place, in the console (they're saved):" % tuple(d))
            for slot, ts in slots:
                for k, axis in enumerate("xyz"):
                    if d[k] != 0:
                        lines.append("  inc vr_wofs_%s_%02d %.4f        (slot %d at Scale %g; with another Scale: %.4f x "
                                     "(1 - Scale))" % (axis, slot + 1, -d[k] * (1 - ts), slot, ts, -d[k]))
    return lines


def remap_lines(renamed):
    out = ["anchor remap: the anchors below name other vertices now. Set them (the console, then they're saved) and, to "
           "ship them, in Quake/vr/vr_weapons.inc (vr_shells.cpp for the shell port):"]
    for what, idx, ov, nv, now in renamed:
        m = re.search(r"\((vr_wofs_\w+)\)", what)
        if now is None:
            out.append("  %s: vertex %d is in no triangle now: pick another with vr_anchor_nearest" % (what, ov))
        elif m:
            out.append("  %s %d        (was %d)" % (m.group(1), now, idx))
        else:
            out.append("  %s: %d (was %d)" % (what, now, idx))
    return out


def corner_area(skin, w, h):
    """The texels the engine's Mod_FloodFillSkin repaints: the area of texel (0, 0)'s colour joined to it."""
    c = skin[0]
    seen = {0}
    stack = [0]
    while stack:
        i = stack.pop()
        x, y = i % w, i // w
        for j in ((i - 1) if x > 0 else -1, (i + 1) if x < w - 1 else -1, (i - w) if y > 0 else -1,
                  (i + w) if y < h - 1 else -1):
            if j >= 0 and j not in seen and skin[j] == c:
                seen.add(j)
                stack.append(j)
    return c, seen


def select_file_vertices(me, bad, vert_of_vid, new_verts, old_nv):
    sel = set()
    for b in bad:
        bv = vert_of_vid[b] if b < old_nv else new_verts[b - old_nv][0]
        if bv is not None:
            sel.add(bv)
    for v in me.vertices:
        v.select = v.index in sel


def anchor_check(path, old, new, source=None):
    """(lines, moved, renamed) for the model about to be written (the repository's anchors: the one it is written
    in, or else the one it was read from)."""
    base = old.name
    root = mdl.repo_root(path) or (mdl.repo_root(source) if source else None)
    if mdl.is_gadget(base):
        l, m = mdl.gadget_report(old, new)
        return ["anchors (the wrist gadget's screen, vr_gadget.cpp):"] + ["  " + x for x in l], m, []
    lines = []
    anchors = mdl.weapon_anchors(root, base)
    l, m, renamed = mdl.anchor_report(old, new, anchors)
    if root is None:
        lines.append("anchors: vr_weapons.inc not found (the model isn't in the repository's quakevr/progs): only the "
                     "strip order is checked")
    elif not anchors:
        lines.append("anchors: no slot in vr_weapons.inc uses progs/%s" % base)
    lines += ["anchors (vr_weapons.inc's defaults, vr_shells.cpp):"] + ["  " + x for x in l]
    return lines, m, renamed


def holes_check(root, path, old):
    """check_mdl_holes.py on the model written and on the one before it: new holes are reported."""
    if root is None:
        return ["holes: check_mdl_holes.py not found (not in the repository)"]
    import importlib.util
    import tempfile
    spec = importlib.util.spec_from_file_location("check_mdl_holes", os.path.join(root, "Misc", "quakevr",
                                                                                  "check_mdl_holes.py"))
    ch = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(ch)
    after = ch.Report(path)
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, os.path.basename(path))
        with open(p, "wb") as f:
            f.write(old.to_bytes())
        before = ch.Report(p)

    def summary(r):
        return "open loops %d (%d see-through), cracks %d (%d wide), flipped %d (%d in sight), inverted %d" % (
            len(r.loops), len(r.visible_loops), len(r.cracks), len(r.wide_cracks), len(r.flipped),
            len(r.visible_flipped), len(r.inverted))

    out = ["holes (check_mdl_holes.py): before: %s" % summary(before), "                              after:  %s%s" % (
        summary(after), "" if after.ok() else "  HOLES: see-through gaps, wide cracks or flipped faces")]
    if after.ok() or not before.ok():
        return out
    for pts, c, hidden in after.loops:
        if not hidden:
            out.append("    OPEN loop of %d edges at (%.1f, %.1f, %.1f)" % ((len(pts),) + tuple(c)))
    for c, w in after.wide_cracks:
        out.append("    CRACK at (%.1f, %.1f, %.1f), %.3f wide" % (tuple(c) + (w,)))
    for p, hidden in after.flipped:
        if not hidden:
            out.append("    FLIPPED edge at (%.1f, %.1f, %.1f)" % tuple(p))
    return out


def rebase(ob, new, kept, new_key, tri_old_index, vert_of_vid, grid_pos, degenerate):
    """After an export, the object stands for the file written: the corners name its vertices, the faces its
    triangles, each Blender vertex is one group, and the shape keys and references hold the places written (on the
    grid)."""
    me = ob.data
    nv = len(me.vertices)
    P = grid_pos.shape[0]
    # The Blender vertex of every file vertex (-1: unused, kept as it was).
    group_of = [-1] * new.num_verts
    for vid, bv in enumerate(vert_of_vid):
        if bv is not None:
            group_of[vid] = bv
    for (bv, s, t), vid in new_key.items():
        group_of[vid] = bv
    # Each face's triangle and each corner's vertex in the new file.
    new_index = {}
    for i, oi in enumerate(tri_old_index):
        if oi >= 0:
            new_index[("old", oi)] = i
    first_new = sum(1 for oi in tri_old_index if oi >= 0)
    ft = []
    lv = np.empty(len(me.loops), np.int32)
    if me.attributes.get(A_VID) is not None:
        me.attributes[A_VID].data.foreach_get("value", lv)
    else:
        lv.fill(-1)
    kept_face = {fi: ti for ti, fi in kept.items()}
    k = first_new
    for f in me.polygons:
        if f.index in kept_face:
            ft.append(new_index[("old", kept_face[f.index])])
        else:
            ft.append(k)
            tri = new.tris[k]
            order = [tri[1], tri[3], tri[2]]  # Blender's a, c, b
            for li, vid in zip(f.loop_indices, order):
                lv[li] = vid
            k += 1
    a = me.attributes.get(A_TRI) or me.attributes.new(A_TRI, 'INT', 'FACE')
    a.data.foreach_set("value", ft)
    a = me.attributes.get(A_VID) or me.attributes.new(A_VID, 'INT', 'CORNER')
    a.data.foreach_set("value", lv)
    a = me.attributes.get(A_VERT) or me.attributes.new(A_VERT, 'INT', 'POINT')
    a.data.foreach_set("value", list(range(nv)))
    # Places: the grid's.
    places = np.zeros((P, nv, 3))
    have = np.zeros(nv, bool)
    for vid, bv in enumerate(group_of):
        if bv >= 0 and not have[bv]:
            places[:, bv] = grid_pos[:, vid]
            have[bv] = True
    keys = me.shape_keys
    buf = np.empty(nv * 3, np.float32)
    for p in range(P):
        if keys is not None:
            keys.key_blocks[p].data.foreach_get("co", buf)
        else:
            me.vertices.foreach_get("co", buf)
        cur = buf.reshape(nv, 3).astype(np.float64)
        places[p, ~have] = cur[~have]
    if keys is not None:
        # The basis last: Blender carries a basis change into the other keys only in Edit Mode, not here.
        for p in range(P):
            keys.key_blocks[p].data.foreach_set("co", places[p].astype(np.float32).ravel())
    me.vertices.foreach_set("co", places[0].astype(np.float32).ravel())
    set_refs(ob, places)
    me.update()
    set_reference(ob, new, group_of, [new_index[("old", ti)] for ti in degenerate if ("old", ti) in new_index])
