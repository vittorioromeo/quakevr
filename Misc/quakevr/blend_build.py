#!/usr/bin/env python3
# blend_build.py -- makes a weapon's source .blend from its shipped files, once (docs/vr-port/MODELS_IN_BLENDER.md, "A
# weapon's source .blend"; the add-on's source.py says what it holds). After that the .blend is the source:
# blend_export.py writes the shipped files from it.
#
# Usage: blender --background --factory-startup --python Misc/quakevr/blend_build.py -- v_shot.mdl [--out folder]
#   (default folder: Misc/quakevr/src_models/blend/<model>/). Runs itself in Blender when started with plain Python.

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
ADDONS = os.path.join(HERE, "blender", "addons")
BLENDER = os.environ.get("BLENDER", r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe")


# The skinned md5 models (MODELS_IN_BLENDER.md, sections 3 and 5): the jointed hand (its .blend makes the feminine hand
# from it on export, and its blood skins from the clean one) and the body (the athletic build, the other builds as its
# shape keys; the 16 skins from the plain one). The files each .blend's export writes, and so owns.
MD5 = {
    "hand_rig": ["hand_rig.md5mesh", "hand_rig.md5anim", "hand_rig.mdl", "hand_rig_f.md5mesh", "hand_rig_f.md5anim",
                 "hand_rig_f.mdl"] + ["hand_rig_%02d_00.lmp" % d for d in range(4)],
    "vrbody": ["vrbody%s.%s" % (b, e) for b in ("", "_lean", "_brawny", "_fem") for e in ("md5mesh", "md5anim", "mdl")]
              + ["vrbody_%02d_00.tga" % n for n in range(16)],
}


def build_md5(bpy, model, out):
    """Imports the shipped hand or body with the add-on and saves it as its source .blend (not exported: saved unedited,
    its export on save, md5source.py, writes the shipped files as they are)."""
    import addon_utils
    bpy.context.preferences.use_preferences_save = False  # (never the user's preferences)
    bpy.context.preferences.filepaths.save_version = 0  # (no .blend1 from the second save)
    addon_utils.enable("quakevr_models", default_set=True)
    from quakevr_models import body_blender, hand_blender
    prefs = bpy.context.preferences.addons["quakevr_models"].preferences
    prefs.export_on_save = False
    progs = os.path.join(ROOT, "quakevr", "progs")
    src = os.path.join(progs, model + ".md5mesh")
    out = out or os.path.join(HERE, "src_models", "blend", model)
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, model + ".blend")
    ctx = bpy.context
    for o in list(bpy.data.objects):  # (the startup scene's cube, camera and light)
        bpy.data.objects.remove(o)
    ctx.view_layer.update()
    ctx.view_layer.objects.active = None
    if model == "hand_rig":
        arm, ob = hand_blender.import_hand(ctx, src)
        holder = arm
    else:
        arm, ob = body_blender.import_body(ctx, src, 0, builds=True,
                                           report=lambda level, msg: print("blend_build: %s" % msg))
        holder = ob
    # Paths relative to the .blend (the same in every checkout): the file it was read from, the normal map shown.
    holder["qvr_source"] = "//" + os.path.relpath(src, out).replace("\\", "/")
    bpy.ops.wm.save_as_mainfile(filepath=path, compress=True)
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_mainfile(filepath=path, compress=True)
    print("blend_build: %s (%d bytes)" % (path, os.path.getsize(path)))
    sys.path.insert(0, HERE)
    import genguard
    genguard.own(path, [os.path.join(progs, f) for f in MD5[model]])
    print("blend_build: owns %s" % ", ".join(MD5[model]))


def main():
    try:
        import bpy
    except ImportError:
        sys.exit(subprocess.call([BLENDER, "--background", "--factory-startup", "--python-exit-code", "1", "--python",
                                  os.path.abspath(__file__), "--"] + sys.argv[1:]))
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = None
    if "--out" in a:
        i = a.index("--out")
        out = a[i + 1]
        del a[i:i + 2]
    if len(a) != 1:
        sys.exit("usage: blend_build.py <model.mdl> [--out folder]")
    name = a[0]
    sys.path.insert(0, ADDONS)
    if os.path.splitext(name)[0] in MD5:
        return build_md5(bpy, os.path.splitext(name)[0], out)
    from quakevr_models import source
    out = out or os.path.join(HERE, "src_models", "blend", os.path.splitext(name)[0])
    path = source.build(bpy.context, ROOT, name, out)
    print("blend_build: %s (%d bytes)" % (path, os.path.getsize(path)))
    # The files it now owns: no generator writes them again (genguard.py; Misc/quakevr/generated.json).
    sys.path.insert(0, HERE)
    import genguard
    files = source.owned_files(source.source_object())
    genguard.own(path, [os.path.join(ROOT, "quakevr", "progs", f) for f in files])
    print("blend_build: owns %s" % ", ".join(files))


if __name__ == "__main__":
    main()
