# blend_md5_test.py -- Blender's side of blend_md5_test.sh (the jointed hand's and the body's source .blends,
# MODELS_IN_BLENDER.md sections 3 and 5). Run in Blender:
#   blender -b --factory-startup --python blend_md5_test.py -- feminine <work folder>
#       make_hand_feminine.derive on the shipped hand writes the shipped hand_rig_f files byte for byte (the hand's
#       .blend makes the feminine hand that way on export);
#   blender -b --factory-startup <copy of hand_rig.blend> --python blend_md5_test.py -- edit_hand
#   blender -b --factory-startup <copy of vrbody.blend> --python blend_md5_test.py -- edit_body
#       a small edit (a fingertip; the top of the head, on the basis shape key) saved with export on save on: the
#       add-on exports into the repository's quakevr/progs (the game's: it reads them again by itself) and the files
#       carry the edit (the hand: the feminine hand made again; the body: every build moved by as much).
# Prints PASS/FAIL per check and "blend_md5_<mode>: N passed, M failed".

import filecmp
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "blender", "addons"))
sys.path.insert(0, HERE)

import qvrtest  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SHIPPED = os.path.join(ROOT, "quakevr", "progs")
BUILDS = ("vrbody", "vrbody_lean", "vrbody_brawny", "vrbody_fem")


def feminine(t, work):
    import make_hand_feminine
    os.makedirs(work, exist_ok=True)
    outs = make_hand_feminine.derive(os.path.join(SHIPPED, "hand_rig.md5mesh"), work)
    same = [os.path.basename(o) for o in outs if filecmp.cmp(o, os.path.join(SHIPPED, os.path.basename(o)), False)]
    t.check(len(same) == 3, "the feminine hand made from the shipped hand is the shipped one byte for byte (%s)" %
            ", ".join(same))


def save_and_export(t):
    import quakevr_models
    from quakevr_models import autoexport
    quakevr_models.register()  # (its preferences are the defaults: export on save on)
    bpy.ops.wm.save_mainfile()
    res = autoexport.wait()
    ok = res is not None and res[0]
    t.check(ok, "the save exported: %s" % (" / ".join(res[1][:2]) if res else "no export"))
    if not ok and res:
        print("\n".join(res[1][-20:]))
    return ok


def edit_hand(t):
    from quakevr_models import md5hand as M
    ob = next(o for o in bpy.data.objects if o.type == 'MESH' and o.get("qvr_hand"))
    me = ob.data
    tip = max(range(len(me.vertices)), key=lambda i: me.vertices[i].co.x)  # a fingertip's end
    me.vertices[tip].co.x += 0.05
    me.update()
    progs = SHIPPED
    old = {f: open(os.path.join(progs, f), "rb").read() for f in ("hand_rig.md5mesh", "hand_rig_f.md5mesh",
                                                                   "hand_rig_00_00.lmp")}
    if not save_and_export(t):
        return
    new = {f: open(os.path.join(progs, f), "rb").read() for f in old}
    t.check(new["hand_rig.md5mesh"] != old["hand_rig.md5mesh"], "hand_rig.md5mesh carries the edit")
    t.check(new["hand_rig_f.md5mesh"] != old["hand_rig_f.md5mesh"], "the feminine hand was made again from it")
    t.check(new["hand_rig_00_00.lmp"] == old["hand_rig_00_00.lmp"], "the skins (unedited) as they were")
    m = M.parse_md5mesh(new["hand_rig.md5mesh"].decode(), "hand_rig.md5mesh")
    xs = max(M.rest_position(m["joints"], M.vertex_weights(m, v))[0] for v in range(len(m["verts"])))
    t.check(abs(xs - me.vertices[tip].co.x) < 1e-4, "the fingertip at its new place in the file (x %.4f)" % xs)


def edit_body(t):
    import bmesh
    from quakevr_models import md5body as B
    ob = next(o for o in bpy.data.objects if o.get("qvr_kind") == "body")
    keys = ob.data.shape_keys
    t.check(keys is not None and [k.name for k in keys.key_blocks] == list(BUILDS),
            "the builds are the body's shape keys (%s)" % ([k.name for k in keys.key_blocks] if keys else None))
    top = max(range(len(ob.data.vertices)), key=lambda i: ob.data.vertices[i].co.z)  # the top of the head
    before = {k.name: tuple(k.data[top].co) for k in keys.key_blocks}
    bpy.context.view_layer.objects.active = ob
    ob.active_shape_key_index = 0
    bpy.ops.object.mode_set(mode='EDIT')  # (an edit of the basis in Edit Mode: Blender carries it into the keys)
    bm = bmesh.from_edit_mesh(ob.data)
    bm.verts.ensure_lookup_table()
    bm.verts[top].co.z += 0.5
    bmesh.update_edit_mesh(ob.data)
    bpy.ops.object.mode_set(mode='OBJECT')
    after = {k.name: tuple(k.data[top].co) for k in keys.key_blocks}
    moved = [n for n in BUILDS if abs(after[n][2] - before[n][2] - 0.5) < 1e-5]
    t.check(len(moved) == 4, "the edit of the basis moved every build's vertex by as much (%s)" % ", ".join(moved))
    old = {n: open(os.path.join(SHIPPED, n + ".md5mesh")).read() for n in BUILDS}
    if not save_and_export(t):
        return
    for n in BUILDS:
        new = open(os.path.join(SHIPPED, n + ".md5mesh")).read()
        a, b = B.Mesh(old[n], n), B.Mesh(new, n)
        diff = [v for v in range(len(a.verts)) if a.rest(v) != b.rest(v)]
        dz = [b.rest(v)[2] - a.rest(v)[2] for v in diff]
        t.check(len(diff) >= 1 and all(abs(d - 0.5) < 1e-4 for d in dz) and len(b.verts) == len(a.verts),
                "%s: the top of the head 0.5 higher (%d file vertices moved), nothing else" % (n, len(diff)))


mode = sys.argv[sys.argv.index("--") + 1]
tally = qvrtest.Tally("blend_md5_" + mode)
if mode == "feminine":
    feminine(tally, os.path.abspath(sys.argv[sys.argv.index("--") + 2]))
elif mode == "edit_hand":
    edit_hand(tally)
else:
    edit_body(tally)
sys.exit(tally.verdict())
