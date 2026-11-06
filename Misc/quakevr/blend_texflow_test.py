# blend_texflow_test.py -- the Blender half of blend_texflow_test.sh: the add-on's texturing (texflow.py, texgen.py;
# MODELS_IN_BLENDER.md, "5. Texturing") on a copy of a model's source .blend, headless.
#
# Usage: blender -b --factory-startup <copy of <model>.blend inside the repository> --python
#            Misc/quakevr/blend_texflow_test.py -- <out folder> [skin]
# Runs: the palette files; the quantizer (the export's and the preview's agree; a fullbright painted exactly glows);
# Paint in Blender; Palette Preview and UV Wires (the view image never saved in the skin's place); Fill Material and
# Bake Starter Shading (texels changed only where asked, glowing texels and texel (0, 0)'s area kept, every texel a
# palette colour; Ctrl+Z's pixels); the UV layout PNG; Edit Skin Externally (the PNG changed outside read back, the
# .blend saved, exported: the game's .mdl has the new texels, a painted fullbright glowing; a skin painted in Blender
# meanwhile not replaced). Writes <out>/<model>_fill.png and _bake.png (before | after) to look at.
# Ends with "blend_texflow: N passed, M failed" (exit code 1 on a failure).

import os
import sys

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "blender", "addons"))
sys.path.insert(0, HERE)

import qvrtest  # noqa: E402


def pair_png(path, a, b):
    """before | after side by side (Blender's rows), 2x."""
    h, w = a.shape[:2]
    rgb = np.concatenate([a, np.zeros((h, 4, 3), np.int32) + 255, b], 1).repeat(2, 0).repeat(2, 1)
    H, W = rgb.shape[:2]
    rgba = np.ones((H, W, 4), np.float32)
    rgba[:, :, :3] = rgb / 255.0
    img = bpy.data.images.new("pair", W, H, alpha=False)
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()
    bpy.data.images.remove(img)


def select_half(ob):
    """The faces whose centre is past the model's middle along its longest axis: selected."""
    me = ob.data
    c = np.empty(len(me.polygons) * 3, np.float32)
    me.polygons.foreach_get("center", c)
    c = c.reshape(-1, 3)
    ax = int(np.argmax(c.max(0) - c.min(0)))
    sel = c[:, ax] > np.median(c[:, ax])
    me.polygons.foreach_set("select", sel)
    return int(sel.sum())


def palette_only(rgb, TG):
    return bool((TG.exact_index(rgb) >= 0).all())


def main(t):
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = args[0] if args else os.path.join(os.path.dirname(bpy.data.filepath), "out")
    skin = int(args[1]) if len(args) > 1 else 0
    os.makedirs(out, exist_ok=True)
    import quakevr_models
    from quakevr_models import autoexport, mdl, qpal, source, texflow as TF, texgen as TG
    quakevr_models.register()
    ctx = bpy.context
    ob = autoexport.source_model()
    root = source.repo_root(bpy.data.filepath)
    if not t.check(ob is not None and root is not None, "a source .blend inside the repository"):
        return
    name = ob["qvr_name"]
    ims = TF.skin_images(ob)
    img = ims[skin]
    w, h = img.size
    print("blend_texflow: %s: %d skin(s), %dx%d, skin %d (%s)" % (name, len(ims), w, h, skin, img.name))
    ctx.scene.qvr_skin_index = skin

    # Palette files.
    files = TG.write_palette_files(os.path.join(out, "palettes"))
    with open(os.path.join(out, "palettes", "quake.gpl")) as f:
        gpl = f.read().splitlines()
    cols = [l for l in gpl if l[:1].isdigit() or l[:1] == " "]
    t.check(len(files) == 5 and len(cols) == 256 and sum("FULLBRIGHT" in l for l in cols) == 32,
            "palette files: %d written, quake.gpl 256 colours, 32 marked FULLBRIGHT" % len(files))

    # Quantizers: the export's (qpal) and the preview's (texgen) agree; an exact fullbright glows, a near one doesn't.
    rgb0 = TF.get_rgb(img)
    rng = np.random.default_rng(3)
    test = rgb0.copy()
    pick = rng.random(test.shape[:2]) < 0.05
    test[pick] = rng.integers(0, 256, (int(pick.sum()), 3))
    fb = qpal.PALETTE[251]
    test[0, :8] = fb
    test[1, :8] = (fb[0], fb[1] + 3, fb[2])  # (251 is pure red: 3 more green)
    old = TF.old_indices(ob, skin, h, w)
    a = TG.quantize(test, old)
    top = test[::-1].reshape(-1, 3)
    b = qpal.Quantizer().indices([tuple(c) for c in top.tolist()], old[::-1].reshape(-1).tolist() if old is not None
                                 else None)
    b = np.frombuffer(bytes(b), np.uint8).reshape(h, w)[::-1]
    t.check(np.array_equal(a, b), "the preview's quantizer agrees with the export's (%d texels differ)" %
            int((a != b).sum()))
    print("  painted: %s, 3 off: %s, old: %s" % (a[0, :8].tolist(), a[1, :8].tolist(),
                                                 old[:2, :8].tolist() if old is not None else None))
    t.check((a[0, :8] == 251).all() and (a[1, :8] < qpal.FULLBRIGHT).all(),
            "a fullbright colour painted exactly glows, one 3 off stays ordinary")

    # Paint in Blender.
    try:
        r = bpy.ops.quakevr_models.paint_skin()
        ip = ctx.tool_settings.image_paint
        t.check(r == {'FINISHED'} and ob.mode == 'TEXTURE_PAINT' and ip.canvas == img and ip.palette is not None
                and len(ip.palette.colors) == 224 and TF.skin_node(ob).interpolation == 'Closest',
                "Paint in Blender: Texture Paint on %s, nearest texels, 224 swatches" % img.name)
        pc = ip.palette.colors[100].color
        print("  swatch 100: %s (palette %s)" % (tuple(round(v * 255) for v in pc), qpal.PALETTE[100]))
        t.check(tuple(round(v * 255) for v in pc) == qpal.PALETTE[100], "a swatch is the palette's colour")
        bpy.ops.object.mode_set(mode='OBJECT')
    except RuntimeError as e:
        t.check(False, "Paint in Blender: %s" % e)

    # Palette Preview and UV Wires.
    ctx.scene.qvr_palette_preview = True
    ctx.scene.qvr_uv_wires = True
    n = TF.skin_node(ob)
    view_on = n.image is not None and n.image.name.endswith(TF.VIEW_SUFFIX) and quakevr_models.model_image(ob) == img
    vrgb = TF.get_rgb(n.image) if view_on else None
    wires = int((vrgb == TF.UV_COLOUR).all(2).sum()) if vrgb is not None else 0
    t.check(view_on and wires > 0 and TF.skin_image(ob, skin) == img,
            "Palette Preview + UV Wires: the material shows the view image (%d wire texels), the skin stays the skin"
            % wires)
    TF.on_save_pre()
    t.check(n.image == img, "saving: the material's image put back to the skin")
    TF.on_save_post()
    t.check(n.image.name.endswith(TF.VIEW_SUFFIX), "after the save: the view image shown again")
    ctx.scene.qvr_palette_preview = False
    ctx.scene.qvr_uv_wires = False
    t.check(n.image == img, "both off: the material shows the skin")

    # Fill Material on half the faces.
    nsel = select_half(ob)
    keep = TG.protected(rgb0)
    sel = TF.selected_faces_mask(ob, w, h)
    step0 = int(ctx.scene.get("qvr_skin_step", 0))
    r = bpy.ops.quakevr_models.fill_material('EXEC_DEFAULT', material='WOOD')
    rgb1 = TF.get_rgb(img)
    ch = (rgb1 != rgb0).any(2)
    t.check(r == {'FINISHED'} and ch.sum() > 0 and not (ch & ~sel).any() and not (ch & keep).any()
            and palette_only(rgb1, TG),
            "Fill Material (wood) on %d faces: %d texels changed, all in the faces' UVs, none glowing or in texel "
            "(0, 0)'s area, every texel a palette colour" % (nsel, int(ch.sum())))
    pair_png(os.path.join(out, "%s_fill.png" % os.path.splitext(name)[0]), rgb0, rgb1)
    for kind in ("METAL", "LEATHER", "RUST", "CLOTH"):
        v, _ = TG.pattern(kind, h, w)
        t.check(0.0 <= v.min() and v.max() <= 1.0 and v.std() > 0.02, "pattern %s: varied values in 0..1" % kind)

    # Ctrl+Z's pixels (the handler as Blender's undo calls it: the scene's step back, then forward).
    ctx.scene["qvr_skin_step"] = step0
    TF.on_undo_redo()
    t.check(np.array_equal(TF.get_rgb(img), rgb0), "undo: the skin before the fill")
    ctx.scene["qvr_skin_step"] = step0 + 1
    TF.on_undo_redo()
    t.check(np.array_equal(TF.get_rgb(img), rgb1), "redo: the fill again")

    # Bake Starter Shading (overlay).
    r = bpy.ops.quakevr_models.bake_shading('EXEC_DEFAULT', samples=8)
    rgb2 = TF.get_rgb(img)
    ch2 = (rgb2 != rgb1).any(2)
    keep1 = TG.protected(rgb1)
    darker = float((TG.LUM[TG.exact_index(rgb2)] < TG.LUM[TG.exact_index(rgb1)]).mean())
    used = TF.selected_faces_mask(ob, w, h, every=True)
    t.check(not (ch2 & ~used).any(), "the bake left the texels no face uses alone")
    t.check(r == {'FINISHED'} and ch2.sum() > 0 and not (ch2 & keep1).any() and palette_only(rgb2, TG)
            and len(bpy.data.objects) == len([o for o in bpy.data.objects if not o.name.startswith(ob.name + ".")]),
            "Bake Starter Shading: %d texels shaded (%.0f%% darker), none glowing or in the corner area, every texel "
            "a palette colour, no bake object left" % (int(ch2.sum()), 100 * darker))
    pair_png(os.path.join(out, "%s_bake.png" % os.path.splitext(name)[0]), rgb1, rgb2)

    # The UV layout PNG.
    r = bpy.ops.quakevr_models.write_uv_layout()
    uvp = os.path.splitext(TF.skin_file(img))[0] + "_uv.png"
    t.check(r == {'FINISHED'} and os.path.isfile(uvp), "Write UV Layout PNG: %s" % os.path.basename(uvp))

    # Edit Skin Externally: Blender's pixels (painted, not saved) written to the PNG first, then watched.
    progs = os.path.join(root, "quakevr", "progs")
    mdl_path = os.path.join(progs, name)
    r = bpy.ops.quakevr_models.edit_skin_external(skin=skin, open_editor=False)
    path = TF.skin_file(img)
    t.check(r == {'FINISHED'} and len(TF.watches) == 1 and not img.is_dirty,
            "Edit Skin Externally: %s written (the fill and bake in it) and watched" % os.path.basename(path))
    # Another program saves the PNG: a block painted in a glowing colour, another in an ordinary one.
    ext = TF.get_rgb(img)
    ext[h - 40:h - 30, w // 2:w // 2 + 10] = qpal.PALETTE[251]
    ext[h - 40:h - 30, w // 2 + 10:w // 2 + 20] = (200, 10, 10)
    tmp = bpy.data.images.new("ext", w, h, alpha=False)
    rgba = np.ones((h, w, 4), np.float32)
    rgba[:, :, :3] = ext / 255.0
    tmp.pixels.foreach_set(rgba.ravel())
    tmp.filepath_raw = path
    tmp.file_format = 'PNG'
    tmp.save()
    bpy.data.images.remove(tmp)
    first = TF.poll_watches()
    msgs = TF.poll_watches()
    res = autoexport.wait()
    t.check(not first and msgs and np.array_equal(TF.get_rgb(img), ext),
            "the PNG saved outside: read back after it stood still a poll (%s)" % (msgs[0] if msgs else "nothing"))
    t.check(res is not None and res[0], "the .blend saved and exported: %s" % (res[1][0] if res else "no export"))
    if os.path.isfile(mdl_path):
        with open(mdl_path, "rb") as f:
            m = mdl.Model(f.read(), name)
        flat = [im for _, _, ims_ in m.skins for im in ims_]
        sk = np.frombuffer(bytes(flat[skin]), np.uint8).reshape(h, w)  # (top to bottom: rows 30..40 from the top)
        glow = sk[30:40, w // 2:w // 2 + 10]
        red = sk[30:40, w // 2 + 10:w // 2 + 20]
        t.check((glow == 251).all() and (red < qpal.FULLBRIGHT).all() and (red != sk[0, 0]).any(),
                "the game's %s: the painted block glows (index 251), the red one is ordinary" % name)
    # Painted in Blender, not saved, and the PNG changes outside: not read.
    px = TF.get_pixels(img)
    px[:4] = [1.0, 1.0, 1.0, 1.0]
    img.pixels.foreach_set(px)
    img.update()
    ext[h - 50:h - 45, :5] = (0, 0, 0)
    tmp = bpy.data.images.new("ext", w, h, alpha=False)
    rgba[:, :, :3] = ext / 255.0
    tmp.pixels.foreach_set(rgba.ravel())
    tmp.filepath_raw = path
    tmp.file_format = 'PNG'
    tmp.save()
    bpy.data.images.remove(tmp)
    TF.poll_watches()
    msgs = TF.poll_watches()
    w0 = next(iter(TF.watches.values()))
    t.check(img.is_dirty and w0["conflict"] and np.allclose(TF.get_pixels(img)[:4], 1.0),
            "painted in Blender meanwhile: the PNG not read (%s)" % (msgs[0][:60] if msgs else "no message"))
    r = bpy.ops.quakevr_models.load_png_anyway()
    t.check(np.array_equal(TF.get_rgb(img), ext) and not w0["conflict"], "Load PNG Anyway reads it")
    autoexport.wait()


tally = qvrtest.Tally("blend_texflow")
try:
    main(tally)
except Exception as e:  # noqa: BLE001
    import traceback
    traceback.print_exc()
    tally.check(False, "the test ran through (%s)" % e)
sys.exit(tally.verdict())
