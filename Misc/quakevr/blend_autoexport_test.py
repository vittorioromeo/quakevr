# blend_autoexport_test.py -- the Blender half of blend_autoexport_test.sh: the Quake VR Models add-on's export on save
# (autoexport.py) on a copy of a source .blend, headless, as Ctrl+S runs it.
#
# Usage: blender -b --factory-startup <copy of v_shot.blend inside the repository> --python
#            Misc/quakevr/blend_autoexport_test.py -- ok|refuse
#   ok: saved as it is: the export runs and writes the game's files (byte for byte the shipped ones, new times).
#   refuse: a frame's shape key renamed (the frames check fails), saved: the export refuses and writes nothing (the
#           game's files and weapons.txt keep their times).
# Ends with "blend_autoexport_save: N passed, M failed" (exit code 1 on a failure).

import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "blender", "addons"))
sys.path.insert(0, HERE)

import qvrtest  # noqa: E402


def main(t):
    mode = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "ok"
    import quakevr_models
    from quakevr_models import autoexport, source
    quakevr_models.register()  # (not through Preferences: its preferences are the defaults, export on save on)
    blend = bpy.data.filepath
    root = source.repo_root(blend)
    ob = autoexport.source_model()
    if not t.check(root is not None and ob is not None, "the copy is a source .blend inside the repository (%s)" % blend):
        return
    progs = os.path.join(root, "quakevr", "progs")
    name = ob["qvr_name"]
    watched = [os.path.join(progs, f) for f in (name, name + "_0_norm.png")] + \
              [os.path.join(progs, p) for p in source.SPECS.get(name, {}).get("parts", ())] + \
              [os.path.join(root, "quakevr", "data", "weapons.txt")]
    watched = [p for p in watched[:-1] if os.path.exists(p)] + watched[-1:]  # (a model without a normal map)
    before = {p: os.stat(p).st_mtime_ns for p in watched}
    if mode == "refuse":
        kb = ob.data.shape_keys.key_blocks[1]
        kb.name = kb.name + "x"
    bpy.ops.wm.save_mainfile()
    res = autoexport.wait()
    if not t.check(res is not None, "the save started an export"):
        return
    ok, lines = res
    print("\n".join("  " + l for l in lines[:6]))
    after = {p: os.stat(p).st_mtime_ns for p in watched}
    if mode == "refuse":
        t.check(not ok and any("frames:" in l for l in lines), "a renamed frame: the export refused")
        same = [p for p in watched if before[p] == after[p]]
        t.check(len(same) == len(watched), "nothing written on the refusal: %d of %d files untouched" % (
            len(same), len(watched)))
    else:
        t.check(ok, "the save exported")
        newer = [os.path.basename(p) for p in watched[:-1] if after[p] != before[p]]
        t.check(len(newer) == len(watched) - 1, "the game's files written: %s" % ", ".join(newer))


tally = qvrtest.Tally("blend_autoexport_save")
main(tally)
sys.exit(tally.verdict())
