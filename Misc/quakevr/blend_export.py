#!/usr/bin/env python3
# blend_export.py -- writes a weapon's shipped files from its source .blend (docs/vr-port/MODELS_IN_BLENDER.md, "A
# weapon's source .blend"): the model, the parts the engine draws in its place, their normal maps, and the data the
# game keeps for the model in quakevr/data/weapons.txt (an anchor or hotspot moved, the barrel mask). It checks
# first (frames, box, anchors on the surface, the sight line, the parts) and writes nothing if a check fails.
# A .blend holding the jointed hand or the body (imported, then saved) writes their md5 files and skins
# (blender/addons/quakevr_models/md5source.py).
#
# Usage: blender -b Misc/quakevr/src_models/blend/v_shot/v_shot.blend --python Misc/quakevr/blend_export.py --
#            [--out folder] [--allow-moved] [--allow-sights] [--grow]
#   --out: write there instead of quakevr/progs (weapons.txt is then left alone; the changes are listed).
#   --grow: vertices past the model's box make it bigger instead of refusing (as the model's Grow the Box, kept in the
#     .blend: the add-on's Quake VR panel, under Export on Save). The grid starts from the file it replaces (so once
#     grown it stays so); an origin that moved gets the weapon's offsets that keep it in place in weapons.txt, listed
#     on "  box: " lines.
#   --allow-moved: write even if the surface under an anchor moved (its empty not moved with it).
#   --allow-sights: write even if the sight line turned (line them up again in the game: vr_weaponstudio_align).
# Ends with "blend_export: ok, N files" or "blend_export: REFUSED: ..." (exit code 1).

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ADDONS = os.path.join(HERE, "blender", "addons")


def export_handmade(script, out):
    """Runs the hand-made source's export_mdl.py (-- <out>) in this Blender, on the open .blend."""
    import runpy
    os.makedirs(out, exist_ok=True)
    before = {f: os.path.getmtime(os.path.join(out, f)) for f in os.listdir(out)}
    argv = sys.argv
    sys.argv = [script, "--", out]
    try:
        runpy.run_path(script, run_name="__main__")
    except SystemExit as e:
        if e.code not in (0, None):
            print("blend_export: REFUSED: %s stopped (%s)" % (os.path.basename(script), e.code))
            sys.exit(1)
    except Exception as e:  # noqa: BLE001
        print("blend_export: REFUSED: %s failed: %s" % (os.path.basename(script), e))
        sys.exit(1)
    finally:
        sys.argv = argv
    written = [f for f in sorted(os.listdir(out)) if os.path.getmtime(os.path.join(out, f)) != before.get(f)]
    for f in written:
        print("  wrote %s (%d bytes)" % (os.path.join(out, f), os.path.getsize(os.path.join(out, f))))
    print("blend_export: ok, %d files (%s)" % (len(written), os.path.basename(script)))


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
    hand = source.handmade_script(blend)
    if hand:  # a hand-made source (models_src/<gun>/): its own export_mdl.py writes its files
        export_handmade(hand, out or os.path.join(root, "quakevr", "progs"))
        return
    from quakevr_models import md5source
    kind, _ = md5source.find(bpy.context)
    if kind is not None:  # the jointed hand or the body, imported and saved (md5source.py)
        def report(level, msg):
            print("  %s: %s" % (next(iter(level)).lower(), msg))
        try:
            written = md5source.export(bpy.context, blend, out, report)
        except md5source.Md5SourceError as e:
            print("blend_export: REFUSED: nothing written:\n  " + str(e).replace("\n", "\n  "))
            sys.exit(1)
        for p in written:
            print("  wrote %s (%d bytes)" % (p, os.path.getsize(p)))
        print("blend_export: ok, %d files (the %s)" % (len(written), kind))
        return
    try:
        written, changes = source.export(bpy.context, root, out, allow_moved="--allow-moved" in a,
                                         allow_sights="--allow-sights" in a, grow="--grow" in a)
    except source.SourceError as e:
        print("blend_export: REFUSED: nothing written:\n  " + str(e).replace("\n", "\n  "))
        sys.exit(1)
    for p in written:
        print("  wrote %s (%d bytes)" % (p, os.path.getsize(p)))
    print("blend_export: ok, %d files%s" % (len(written), ", weapons.txt changed" if changes and not out else ""))


if __name__ == "__main__":
    main()
