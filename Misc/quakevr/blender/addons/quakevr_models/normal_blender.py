# normal_blender.py -- the baked normal maps in Blender (docs/vr-port/MODELS_IN_BLENDER.md, HANDS_IN_BLENDER.md,
# "Normal maps"): shown on the imported models (their material's Normal input), baked again from the model files
# (normalmaps.py, as Misc/quakevr/bake_normals.py does), and baked from a high poly of your own with Cycles.
#
# A high poly: select it (or several), then the model (active), and Bake Normal Map. Cycles bakes the high poly's
# normals in the model's object space, a layer at a time where the model's triangles share texels (a layer's
# triangles never overlap in the skin), and normalbake.py writes them in the engine's own tangent frame (not
# Blender's: the engine builds its frame per pixel from the texture coordinates, and on a hard-edged low poly
# Blender's MikkTSpace tangents differ from it by degrees). The recipe's relief (knuckles, seams, stitches, knurling)
# is laid on it unless Add Details is off.

import os

import bpy
import numpy as np

from . import normalbake as nb
from . import normalmaps

HERE = os.path.dirname(os.path.abspath(__file__))
MISC = os.path.normpath(os.path.join(HERE, "..", "..", ".."))  # Misc/quakevr, when the add-on runs from the repository
NODE = "Quake VR normal map"


def genguard():
    """Misc/quakevr/genguard.py (the generators' guard), or None when the add-on was installed from a zip."""
    import sys
    if not os.path.exists(os.path.join(MISC, "genguard.py")):
        return None
    if MISC not in sys.path:
        sys.path.append(MISC)
    import genguard as g
    return g


def map_for(progs, target, skin=0):
    """The map file a model's skin reads (the body: its clothes' or its armour's), or None."""
    outs = normalmaps.outputs(progs, target)
    if target == normalmaps.BODY and skin >= 4:
        outs = outs[1:]
    return outs[0] if outs and os.path.exists(outs[0]) else None


def show(ob, path):
    """The map `path` on the object's material: an image node (non-colour) into a Normal Map node into the shader's
    Normal (Blender draws it in its own tangent frame, close to the engine's). Replaces the one shown before."""
    if not path or not ob.data.materials:
        return
    mat = ob.data.materials[0]
    if mat is None or mat.node_tree is None:
        return
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    for n in [n for n in nodes if n.get("qvr_normal")]:
        nodes.remove(n)
    bsdf = next((n for n in nodes if n.type == 'BSDF_PRINCIPLED'), None)
    if bsdf is None:
        return
    img = bpy.data.images.load(path, check_existing=True)
    img.reload()
    img.colorspace_settings.name = "Non-Color"
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = 'Linear'
    tex.label = NODE
    tex["qvr_normal"] = True
    nm = nodes.new("ShaderNodeNormalMap")
    nm.space = 'TANGENT'
    if ob.data.uv_layers:
        nm.uv_map = ob.data.uv_layers[0].name
    nm["qvr_normal"] = True
    tex.location = (bsdf.location.x - 600, bsdf.location.y - 300)
    nm.location = (bsdf.location.x - 300, bsdf.location.y - 300)
    links.new(tex.outputs["Color"], nm.inputs["Color"])
    links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])


def refresh(paths):
    """Images of the maps just written, loaded again."""
    for img in bpy.data.images:
        if img.filepath and os.path.normcase(os.path.abspath(bpy.path.abspath(img.filepath))) in {
                os.path.normcase(os.path.abspath(p)) for p in paths}:
            img.reload()


# ----------------------------------------------------------------------------
# A high poly, baked with Cycles


def layers(low, scale):
    """The model's triangles in layers whose texels don't overlap (each baked on its own), the biggest first."""
    r = nb.Raster(low, scale)
    order = np.argsort(-low.area)
    per_tri = {}
    srt = np.argsort(r.tri, kind="stable")
    tris = r.tri[srt]
    starts = np.flatnonzero(np.r_[True, tris[1:] != tris[:-1]])
    for a, b in zip(starts, np.r_[starts[1:], len(tris)]):
        per_tri[tris[a]] = r.pix[srt[a:b]]
    taken = []
    out = []
    for t in order:
        px = per_tri.get(t)
        if px is None:
            continue
        for i, used in enumerate(taken):
            if not used[px].any():
                used[px] = True
                out[i].append(t)
                break
        else:
            used = np.zeros(r.w * r.h, bool)
            used[px] = True
            taken.append(used)
            out.append([t])
    return out


def low_object(low, tris, matrix, name, size):
    """A throwaway object of some of the model's triangles, drawn as the engine draws them (its normals), with an image
    to bake into."""
    T = low.T[tris][:, ::-1]  # the engine's triangles are clockwise
    me = bpy.data.meshes.new(name)
    me.from_pydata(low.P.tolist(), [], T.tolist())
    me.update()
    uv = me.uv_layers.new(name="UVMap")
    c = T.ravel()
    uv.data.foreach_set("uv", np.stack([low.UV[c, 0] / low.W, 1.0 - low.UV[c, 1] / low.H], 1).ravel())
    me.shade_smooth()
    me.normals_split_custom_set(low.N[c].tolist())
    ob = bpy.data.objects.new(name, me)
    ob.matrix_world = matrix
    bpy.context.scene.collection.objects.link(ob)
    mat = bpy.data.materials.new(name)
    me.materials.append(mat)
    img = bpy.data.images.new(name, size[0], size[1], float_buffer=True)
    img.colorspace_settings.name = "Non-Color"
    node = mat.node_tree.nodes.new("ShaderNodeTexImage")
    node.image = img
    mat.node_tree.nodes.active = node
    return ob, img


def cycles_base(context, model_ob, highs, report=print):
    """base(low, raster) for normalmaps.bake: the high polys' normals at each entry, baked by Cycles in the model's
    object space (a layer at a time); where a ray finds nothing, the model's own normal."""

    def base(low, r):
        scene = context.scene
        saved = (scene.render.engine, [o for o in context.selected_objects], context.view_layer.objects.active)
        scene.render.engine = 'CYCLES'
        scene.cycles.samples = 4
        size = np.ptp(low.P, axis=0).max()
        out = r.n.copy()
        made = []
        try:
            for i, tris in enumerate(layers(low, max(1, r.scale // 2))):
                ob, img = low_object(low, tris, model_ob.matrix_world, "qvr_bake_low_%d" % i, (r.w, r.h))
                made.append((ob, img))
                for o in context.selected_objects:
                    o.select_set(False)
                for h in highs:
                    h.select_set(True)
                ob.select_set(True)
                context.view_layer.objects.active = ob
                bpy.ops.object.bake(type='NORMAL', normal_space='OBJECT', use_selected_to_active=True,
                                    cage_extrusion=0.02 * size, max_ray_distance=0.06 * size, margin=0)
                px = np.array(img.pixels[:], np.float32).reshape(r.h, r.w, 4)[::-1, :, :3].reshape(-1, 3) * 2 - 1
                m = np.isin(r.tri, tris)
                v = px[r.pix[m]]
                good = np.linalg.norm(v, axis=1) > 0.5
                idx = np.flatnonzero(m)[good]
                out[idx] = nb.normalize(v[good])
            report("baked the high poly in %d layers" % len(made))
        finally:
            for ob, img in made:
                me, mats = ob.data, list(ob.data.materials)
                bpy.data.objects.remove(ob)
                bpy.data.meshes.remove(me)
                for m in mats:
                    bpy.data.materials.remove(m)
                bpy.data.images.remove(img)
            scene.render.engine = saved[0]
            for o in saved[1]:
                o.select_set(True)
            context.view_layer.objects.active = saved[2]
        return out

    return base


# ----------------------------------------------------------------------------
# Baking, guarded


def bake(context, model_ob, path, highs=(), details=True, overwrite=False, report=print):
    """Bakes the maps of the model file `path` (as exported) and writes them, unless one was edited since it was baked
    (painted over) and `overwrite` is off. Returns the paths written, or None if it refused."""
    progs = os.path.dirname(os.path.abspath(path))
    target = normalmaps.target_of(path)
    outs = normalmaps.outputs(progs, target)
    g = genguard()
    if g is not None:
        edited = g.edited(outs)
    else:  # no manifest to tell a baked map from a painted one: only overwrite when asked
        edited = [p for p in outs if os.path.exists(p)]
    if edited and not overwrite:
        report("not baked: %s %s edited since it was baked (or can't be told apart from an edited one); tick "
               "Overwrite Edited Map to replace it" % (", ".join(os.path.basename(p) for p in edited),
                                                        "was" if len(edited) == 1 else "were"))
        return None
    base = cycles_base(context, model_ob, list(highs), report) if highs else None
    maps = normalmaps.bake(progs, target, base=base, details=details)
    for p, rgb in maps.items():
        nb.write_png(p, rgb)
    # a bake of the recipe is recorded as the script's (bake_normals.py bakes it again after the next edit); a high
    # poly's is not, so that the script counts it as edited and leaves it alone
    if g is not None and not highs:
        manifest = g.load()
        for p in maps:
            r = g.rel(p)
            if r:
                manifest[r] = {"sha256": g.sha256(p), "by": "bake_normals.py"}
        g.save(manifest)
    refresh(list(maps))
    return list(maps)
