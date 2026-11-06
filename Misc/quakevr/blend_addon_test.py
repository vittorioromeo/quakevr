# blend_addon_test.py -- the one Quake VR add-on (blender/addons/quakevr_models), headless: it registers whole (the
# hand's tools in it, one panel), takes the old hand add-on's entry out of the preferences, finds what a .blend edits,
# and a .blend holding the jointed hand or the body exports on save (md5source.py) into a folder of copies, the files
# as shipped but their commandline line; Show in Game writes the game's request.
#
# Usage (blend_addon_test.sh runs it): blender -b --factory-startup --python Misc/quakevr/blend_addon_test.py --
#            hand|body <work folder inside the repository, holding progs/ with copies of the model's files>
# Ends with "blend_addon_<kind>: N passed, M failed" (exit code 1 on a failure).

import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "blender", "addons"))
sys.path.insert(0, HERE)

import qvrtest  # noqa: E402

MODELS = {"hand": "hand_rig", "body": "vrbody"}


def strip_cmd(path):
    with open(path, "rb") as f:
        return [l for l in f.read().split(b"\n") if not l.startswith(b"commandline")]


def main(t, kind, work):
    import addon_utils
    prefs = bpy.context.preferences
    prefs.use_preferences_save = False  # (never the user's preferences)
    old = prefs.addons.new()
    old.module = "quakevr_hand"  # the old hand add-on's entry, as an older setup has it
    addon_utils.enable("quakevr_models", default_set=True)
    import quakevr_models as Q
    t.check(hasattr(bpy.types, "QUAKEVR_HAND_OT_apply_pose") and hasattr(bpy.types, "QVR_PT_quakevr")
            and not hasattr(bpy.types, "QVR_PT_hand") and not hasattr(bpy.types, "QVR_PT_models"),
            "one add-on: the hand's operators registered with the models', one panel")
    Q.retire_old_addons()
    t.check(prefs.addons.get("quakevr_hand") is None and prefs.addons.get("quakevr_models") is not None,
            "the old hand add-on's entry is gone, Quake VR's kept")

    name = MODELS[kind]
    root = os.path.dirname(os.path.dirname(HERE))
    shipped = os.path.join(root, "quakevr", "progs", name + ".md5mesh")
    out = os.path.join(work, "progs")
    Q.import_for_edit(shipped)
    k, ob, label = Q.detect(bpy.context)
    t.check(k == kind, "detected: %s (%s)" % (k, label))
    p = prefs.addons["quakevr_models"].preferences
    p.progs_dir = out
    blend = os.path.join(work, name + ".blend")
    before = {f: os.stat(os.path.join(out, f)).st_mtime_ns for f in os.listdir(out)}
    bpy.ops.wm.save_as_mainfile(filepath=blend)
    res = Q.autoexport.wait()
    if not t.check(res is not None and res[0], "the save exported: %s" % (res[1][:3] if res else "no export")):
        print(open(Q.autoexport.log_path()).read()[-3000:])
        return
    newer = sorted(f for f in os.listdir(out) if os.stat(os.path.join(out, f)).st_mtime_ns != before.get(f))
    t.check(name + ".md5mesh" in newer and name + ".md5anim" in newer, "written: %s" % ", ".join(newer))
    same = strip_cmd(os.path.join(out, name + ".md5mesh")) == strip_cmd(shipped)
    t.check(same, "%s.md5mesh as shipped but its commandline line" % name)
    progs = os.path.dirname(shipped)
    differ = [f for f in newer if not f.endswith(".md5mesh") and
              open(os.path.join(out, f), "rb").read() != open(os.path.join(progs, f), "rb").read()]
    t.check(not differ, "saved unedited, the other files as shipped (differ: %s)" % (", ".join(differ) or "none"))
    req = Q.write_request(["show %s" % Q.game_models(bpy.context)[0]])
    text = open(req).read() if req else ""
    t.check(req == os.path.join(work, Q.REQUEST_FILE) and text == "show %s\n" % name,
            "Show in Game's request: %r in %s" % (text, req))


kind = sys.argv[sys.argv.index("--") + 1]
tally = qvrtest.Tally("blend_addon_" + kind)
main(tally, kind, os.path.abspath(sys.argv[sys.argv.index("--") + 2]))
sys.exit(tally.verdict())
