#!/usr/bin/env python3
# bake_normals.py -- bakes the normal maps of Quake VR's own models from their model files as they are now (round 21,
# "Baked normal maps" in docs/vr-port/ROUND21.md): the jointed hand, the body (its three builds share one map; the
# armoured torso has its own), the generated models (flashlight, wrist gadget and strap, shell, holster, pauldrons)
# and the view models (v_*.mdl):
#   quakevr/progs/hand_rig_00_00_norm.png                all four hand skins
#   quakevr/progs/vrbody_00_00_norm.png, _04_00_norm.png  the body's clothes (skins 0-3), its armours (4-15)
#   quakevr/progs/<model>.mdl_0_norm.png                 every skin of an alias model
# Tangent space as the engine reads it (green up the image, each axis of its frame of unit length), linear 8-bit RGB,
# blue a real z. The baking itself is in the models' Blender add-on (Misc/quakevr/blender/addons/quakevr_models:
# normalmaps.py, normalbake.py and the recipes normaldetail.py, normalbody.py, normaltiles.py), whose Bake Normal Map
# button runs the same code.
#
# Usage: python Misc/quakevr/bake_normals.py [model ...] [--progs folder] [--keep-edited | --force]
#   model: hand, body, or an .mdl's file name (vrflashlight.mdl, v_shot.mdl...); none: all of them.
#   After editing a model in Blender and exporting it, run it again for that model (or press Bake Normal Map).
# It needs numpy: with a Python that has none, it runs itself in Blender (headless, a throwaway profile):
#   blender --background --factory-startup --python Misc/quakevr/bake_normals.py -- [model ...]
#
# genguard.py keeps it from overwriting a map edited since it baked it (painted over in Blender or elsewhere): it
# stops, naming it, unless run with --keep-edited (bake the rest) or --force (overwrite it too).

import os
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
ADDONS = os.path.join(HERE, "blender", "addons")
BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"


def args():
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    return a


def in_blender(argv):
    """Runs this script in Blender, headless, with a throwaway profile (none of the user's add-ons or settings)."""
    blender = os.environ.get("BLENDER", BLENDER)
    prof = tempfile.mkdtemp(prefix="qvr_bake_")
    env = dict(os.environ, BLENDER_USER_RESOURCES=prof, BLENDER_USER_CONFIG=os.path.join(prof, "config"),
               BLENDER_USER_SCRIPTS=os.path.join(prof, "scripts"), BLENDER_USER_DATAFILES=os.path.join(prof, "data"),
               BLENDER_USER_EXTENSIONS=os.path.join(prof, "extensions"))
    cmd = [blender, "--background", "--factory-startup", "--python-exit-code", "1", "--python", os.path.abspath(__file__),
           "--"] + argv
    return subprocess.call(cmd, env=env)


def main():
    try:
        import numpy  # noqa: F401
    except ImportError:
        sys.exit(in_blender(args()))
    sys.path.insert(0, HERE)
    sys.path.insert(0, os.path.join(ADDONS, "quakevr_models"))
    sys.path.insert(0, ADDONS)
    a = args()
    force = "--force" in a
    keep = "--keep-edited" in a
    a = [x for x in a if x not in ("--force", "--keep-edited")]
    sys.argv[:] = [sys.argv[0]] + (["--force"] if force else []) + (["--keep-edited"] if keep else [])
    import genguard
    import normalmaps
    progs = os.path.join(ROOT, "quakevr", "progs")
    if "--progs" in a:
        i = a.index("--progs")
        progs = a[i + 1]
        del a[i:i + 2]
    targets = [normalmaps.target_of(x) for x in a] if a else normalmaps.all_targets(progs)
    targets = list(dict.fromkeys(targets))
    paths = [p for t in targets for p in normalmaps.outputs(progs, t)]
    guard = genguard.Guard("bake_normals.py", paths)
    import normalbake
    maps = {}  # all baked before any is written: a failure half way leaves the maps as they were
    for t in targets:
        t0 = time.time()
        maps.update(normalmaps.bake(progs, t))
        print("%s: baked in %.1f s" % (t, time.time() - t0))
    for path, rgb in maps.items():
        if path not in guard.kept:
            normalbake.write_png(path, rgb)
            print("  %d x %d -> %s" % (rgb.shape[1], rgb.shape[0], os.path.relpath(path, ROOT)))
    guard.finish()


if __name__ == "__main__":
    main()
