# carve_mesh.py -- carves recesses (slots, grooves, vents) into a closed mesh with Blender's exact boolean, headless:
#   blender -b --factory-startup --python-exit-code 1 -P Misc/quakevr/blender/carve_mesh.py -- in.json out.json
# make_enemyguns.py writes in.json and reads out.json (its `carve`); never run by hand.
#
# in.json: {"verts": [[x, y, z]...], "faces": [[a, b, c]...] (counter-clockwise seen from outside),
#           "uvs": [[[u, v] x 3]...] (per face corner), "cuts": [cut...]}
# a cut:   {"box": [x0, x1, y0, y1, z0, z1], "inner": [[x, y, z]...] (the mesh's vertices moved `depth` in: the
#           recess's floor), "wall_uv": [u, v] (the box's faces: the recess's walls)}
# Each cut takes away (box - inner) from the mesh: inside the box the surface sinks to the inner mesh's (its floor keeps
# the skin's coordinates over it: the painted vent, now recessed), and the box's faces become the recess's walls.
# out.json: {"verts": [...], "faces": [[a, b, c]...] (counter-clockwise), "uvs": [...], "cut": [0 or 1 per face]}
# (1: a face of a recess: its walls and floor).

import json
import os
import sys

import bmesh
import bpy


def mesh_object(name, verts, faces, uvs, cut):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    assert len(me.polygons) == len(faces), '%s: Blender kept %d of %d faces' % (name, len(me.polygons), len(faces))
    uv = me.uv_layers.new(name='UVMap')
    uv.data.foreach_set('uv', [c for f in uvs for corner in f for c in corner])
    attr = me.attributes.new('qvr_cut', 'INT', 'FACE')
    attr.data.foreach_set('value', [cut] * len(faces))
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def box_object(name, box, uv):
    """The box, each face a fan of four triangles round its middle: a triangulation that is its own mirror image
    across any of the box's middle planes (the boolean splits the box's faces where its own diagonals cross the mesh,
    so a quad's single diagonal would leave the cut off its mirror image)."""
    x0, x1, y0, y1, z0, z1 = box
    v = [(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]
    # Counter-clockwise from outside.
    quads = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    f = []
    for q in quads:
        c = len(v)
        v.append(tuple(sum(v[i][k] for i in q) / 4.0 for k in range(3)))
        f += [(q[i], q[(i + 1) % 4], c) for i in range(4)]
    me = bpy.data.meshes.new(name)
    me.from_pydata(v, [], f)
    me.update()
    layer = me.uv_layers.new(name='UVMap')
    layer.data.foreach_set('uv', list(uv) * (3 * len(f)))
    attr = me.attributes.new('qvr_cut', 'INT', 'FACE')
    attr.data.foreach_set('value', [1] * len(f))
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def boolean(target, operand, op):
    """target `op` operand, applied: target's mesh replaced by the result."""
    mod = target.modifiers.new('carve', 'BOOLEAN')
    mod.operation = op
    mod.solver = os.environ.get('QVR_CARVE_SOLVER', 'EXACT')
    mod.object = operand
    mod.use_self = False
    mod.use_hole_tolerant = False
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(target.evaluated_get(dg))
    target.modifiers.remove(mod)
    old = target.data
    target.data = me
    bpy.data.meshes.remove(old)
    operand.hide_viewport = True
    operand.hide_render = True


def main():
    argv = sys.argv[sys.argv.index('--') + 1:]
    src = json.load(open(argv[0]))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    body = mesh_object('body', src['verts'], src['faces'], src['uvs'], 0)
    for i, cut in enumerate(src['cuts']):
        box = box_object('box%d' % i, cut['box'], cut['wall_uv'])
        inner = mesh_object('inner%d' % i, cut['inner'], src['faces'], src['uvs'], 1)
        boolean(box, inner, 'DIFFERENCE')
        boolean(body, box, 'DIFFERENCE')
    bm = bmesh.new()
    bm.from_mesh(body.data)
    # Points the boolean made twice (where a box's fan met an edge of the mesh along the middle plane) merged.
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges[:], dist=1e-6)
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
    uvl = bm.loops.layers.uv['UVMap']
    cutl = bm.faces.layers.int['qvr_cut']
    bm.verts.index_update()
    out = {'verts': [list(v.co) for v in bm.verts], 'faces': [], 'uvs': [], 'cut': []}
    for f in bm.faces:
        out['faces'].append([l.vert.index for l in f.loops])
        out['uvs'].append([list(l[uvl].uv) for l in f.loops])
        out['cut'].append(f[cutl])
    bm.free()
    json.dump(out, open(argv[1], 'w'))


main()
