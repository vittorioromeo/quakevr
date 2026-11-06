#!/usr/bin/env python3
# blend_export.py -- writes a weapon's shipped files from its source .blend (docs/vr-port/MODELS_IN_BLENDER.md, "A
# weapon's source .blend"): the model, the parts the engine draws in its place, their normal maps, and the data the
# game keeps for the model in quakevr/data/weapons.txt (an anchor or hotspot moved, the barrel mask). It checks
# first (frames, box, anchors on the surface, the sight line, the parts) and writes nothing if a check fails.
#
# Usage: blender -b Misc/quakevr/src_models/blend/v_shot/v_shot.blend --python Misc/quakevr/blend_export.py --
#            [--out folder] [--allow-moved] [--allow-sights]
#   --out: write there instead of quakevr/progs (weapons.txt is then left alone; the changes are listed).
#   --allow-moved: write even if the surface under an anchor moved (its empty not moved with it).
#   --allow-sights: write even if the sight line turned (line them up again in the game: vr_weaponstudio_align).
# Ends with "blend_export: ok, N files" or "blend_export: REFUSED: ..." (exit code 1).

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ADDONS = os.path.join(HERE, "blender", "addons")


def main():
    import bpy
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = None
    if "--out" in a:
        out = os.path.abspath(a[a.index("--out") + 1])
    sys.path.insert(0, ADDONS)
    from quakevr_models import source
    blend = bpy.data.filepath
    root = source.repo_root(blend) if blend else None
    if root is None:
        print("blend_export: REFUSED: open a source .blend inside the repository (blender -b <file.blend> --python ...)")
        sys.exit(1)
    print("blend_export: %s" % blend)
    try:
        written, changes = source.export(bpy.context, root, out, allow_moved="--allow-moved" in a,
                                         allow_sights="--allow-sights" in a)
    except source.SourceError as e:
        print("blend_export: REFUSED: nothing written:\n  " + str(e).replace("\n", "\n  "))
        sys.exit(1)
    for p in written:
        print("  wrote %s (%d bytes)" % (p, os.path.getsize(p)))
    print("blend_export: ok, %d files%s" % (len(written), ", weapons.txt changed" if changes and not out else ""))


if __name__ == "__main__":
    main()
