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
