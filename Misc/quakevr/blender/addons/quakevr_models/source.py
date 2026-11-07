# source.py -- a weapon's source .blend (docs/vr-port/MODELS_IN_BLENDER.md, "A weapon's source .blend"): the .blend
# is the source and the shipped files are made from it by a headless export (Misc/quakevr/blend_export.py), as the
# generators made them before.
#
# What a source .blend holds (made once from the shipped files by Misc/quakevr/blend_build.py, `build` below):
# - the model: one mesh object as the add-on imports it (mdl_blender.py: the frames as shape keys named as QuakeC's
#   $frame lines have them, in their order; the qvr_* attributes and the reference model that make an unedited export
#   byte for byte); its parts, drawn in its place by the engine (the shotgun's auto pump: the moving fore-end and the
#   gun without it), as a face attribute (qvr_part: an index into the object's qvr_part_files), so an edit of the
#   gun is an edit of its parts; the rivets as geometry (vertex group "rivets", to find them);
# - the skin as a PNG beside the .blend (<model>_skin0.png: Quake's palette colours; the export puts every changed
#   texel on the palette and keeps the fullbright ones where they were);
# - the relief the normal map is baked from (<model>_relief.png beside it: grey, 128 flat, a level a height step;
#   the gun's relief that gunlooks.py made), and the bake's recipe as before (normaltiles.py);
# - the anchors and hotspots the game hangs off the model as empties parented to it ("av <key>": weapons.txt's
#   *_av points; "hs <n>": the off hand's grip hotspots; "port shell": vr_shells.cpp's ejection port; "sight rear",
#   "sight front": the painted sight line, worked out from the skin: shown, not edited);
# - extra animations (optional): an armature driving the mesh (an Armature modifier), each action that has a custom
#   property qvr_frames (the frames' name stem, at most 12 characters) baked to frames appended after the model's own,
#   one a scene frame over the action's frame range.
#
# The export (`export` below) checks before it writes anything: the frames (count, names), the box, the anchors
# (every one on the surface, and the surface under an unmoved one where it was in every frame), the sight line, the
# parts. Then it writes the model, its parts, their normal maps, and the data the game keeps for the model
# (weapons.txt: an anchor or hotspot whose empty moved; the barrel mask, kept on its triangles).

import importlib
import importlib.util
import math
import os
import re
import shutil
import sys
import tempfile

import bpy
import numpy as np

from . import mdl
from . import mdl_blender as mb
from . import sights

ANCHOR_KEYS = ("hand_av", "2h_hand_av", "muzzle_av", "wpnbtn_av", "wpntxt_av")
A_PART = "qvr_part"
FRAME_STEM_MAX = 12
SIGHT_MAX_DEG = 0.25   # the sight line may turn this much (Weapon Studio's "lined up")
SURFACE_MAX = 0.1      # an anchor this far off the surface at most (anchors.py check)

# What each source .blend's model has besides itself (the parts the engine draws in its place, the generator whose
# relief the bake reads, and where the rivets start: gunlooks.py's input's triangle count).
SPECS = {
    "v_shot.mdl": {"parts": ("vr_pumpbody_on_v_shot.mdl", "vr_pump_on_v_shot.mdl"), "relief": "gunlooks",
                   "rivets_from": "Misc/quakevr/src_models/gunlooks/v_shot.mdl"},
    "v_nail.mdl": {"relief": "gunlooks", "rivets_from": "Misc/quakevr/src_models/gunlooks/v_nail.mdl"},
    "v_lava.mdl": {"relief": "gunlooks", "rivets_from": "Misc/quakevr/src_models/gunlooks/v_lava.mdl"},
    # The super shotgun broken open: its two halves cut from the exported gun by make_ssg_open.py's code (the cut, the
    # plates closing it), so they follow every edit of the gun.
    "v_shot2.mdl": {"derived": ("make_ssg_open", ("vr_ssg_barrels_on_v_shot2.mdl", "vr_ssg_frame_on_v_shot2.mdl"))},
}
# The models whose skins after 0 a generator makes from skin 0 (its code run on the exported model: "post"): the
# hand's and fingers' blood skins (make_bloody_hands.py), the grenades' unarmed skin (make_grenade_skins.py).
for _n in ("hand_base", "finger_thumb", "finger_index", "finger_middle", "finger_ring", "finger_pinky"):
    SPECS[_n + ".mdl"] = {"post": ("make_bloody_hands", "bloody_hands")}
for _n in ("grenade", "mervup", "proxbomb"):
    SPECS[_n + ".mdl"] = {"post": ("make_grenade_skins", "grenade_skins")}
# Models made from another's exported model by a generator's code ("derived", as the super shotgun's open halves): the
# armed monsters without their own weapon (make_vrgrunt.py), the explosion's molten debris (make_explosion_debris.py).
for _n, _d in (("soldier", "vrgrunt"), ("enforcer", "vrenforcer"), ("knight", "vrknight"), ("hknight", "vrhknight")):
    SPECS[_n + ".mdl"] = {"derived": ("make_vrgrunt", (_d + ".mdl",))}
SPECS["vr_rock1.mdl"] = {"derived": ("make_explosion_debris", ("vr_explosion_debris.mdl",))}


def _post_bloody_hands(gen, path, name):
    gen.process(path, os.path.splitext(name)[0])


def _post_grenade_skins(gen, path, name):
    gain, lift = {n: (g, l) for n, g, l in gen.MODELS}[name]
    gen.mute(path, gen.SATURATION, gen.BAND_LIGHT, gain, lift)


POST = {"bloody_hands": _post_bloody_hands, "grenade_skins": _post_grenade_skins}
# The hand-made sources (Misc/quakevr/models_src/<gun>/<gun>.blend): their own export_mdl.py beside them writes their
# files; blend_export.py and export on save run it.
HANDMADE_SCRIPT = "export_mdl.py"


def handmade_script(blend):
    """The hand-made source .blend's own export script (models_src/<gun>/export_mdl.py), or None."""
    if not blend:
        return None
    d = os.path.dirname(os.path.abspath(blend))
    s = os.path.join(d, HANDMADE_SCRIPT)
    if os.path.basename(os.path.dirname(d)) == "models_src" and os.path.isfile(s) and repo_root(blend):
        return s
    return None


HANDMADE_FILES = {  # (what each hand-made source writes into quakevr/progs: genguard's manifest records it as owned)
    "flaregun": ("v_flaregun.mdl", "vr_flare.mdl", "vr_flaregun_preview.mdl"),
    "revolver": ("v_revolver.mdl", "v_revolver_crane.mdl", "v_revolver_cyl.mdl", "v_revolver_frame.mdl",
                 "v_revolver_shell.mdl", "vr_revolver_preview.mdl"),
    "widowmaker": ("v_widowmaker.mdl", "vr_widowmaker_preview.mdl"),
}


def owned_files(ob):
    """The files (in quakevr/progs) the source .blend's export writes: the model, its parts, the files made from it,
    their normal maps, its full-colour skins."""
    from . import normalmaps
    name = ob["qvr_name"]
    spec = SPECS.get(name, {})
    files = [name] + [str(f) for f in ob.get("qvr_part_files", [])] + list(spec.get("derived", ("", ()))[1])
    if ob.get("qvr_source_blend"):
        files += [str(f) + "_0_norm.png" for f in ob.get("qvr_norms", {})]
    else:
        files += [os.path.basename(p) for p in normalmaps.outputs("", normalmaps.target_of(name))]
    return files + [str(f) for f in ob.get("qvr_extra_files", [])]


def sha(data):
    import hashlib
    return hashlib.sha256(data).hexdigest()


def norm_inputs(model_bytes, relief=None):
    """What a model's normal map is baked from (its file; the relief): the export bakes it again only when they change,
    so an unedited .blend gives the shipped map byte for byte (it was baked by an earlier bake's code, or numpy)."""
    return sha(model_bytes + (relief.astype(np.int64).tobytes() if relief is not None else b""))


class SourceError(Exception):
    pass


def fmt(x):
    s = "%.4f" % x
    s = s.rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def point_text(p):
    return " ".join(fmt(c) for c in p)


def weapons_data(root):
    path = os.path.join(root, "Misc", "quakevr", "weapons_data.py")
    spec = importlib.util.spec_from_file_location("qvr_weapons_data", path)
    wd = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(wd)
    return wd


def model_slots(root, name):
    """{slot: {key: value}} of every weapons.txt slot whose id is progs/<name>."""
    wd = weapons_data(root)
    return {s: v for s, v in wd.slot_values().items() if v.get("id", "").replace("\\", "/") == "progs/" + name}


def repo_root(path):
    d = os.path.dirname(os.path.abspath(path))
    for _ in range(8):
        if os.path.exists(os.path.join(d, "Quake", "vr", "vr_weapons.inc")):
            return d
        d = os.path.dirname(d)
    return None


# ----------------------------------------------------------------------------
# Building a source .blend from the shipped files (once)


def subset_tris(whole, part):
    """Which triangles of `whole` the part file has (in order, on the same corners' bytes and texels)."""
    wp, pp = whole.pose_bytes()[0], part.pose_bytes()[0]
    out, k = [], 0
    for t, tri in enumerate(whole.tris):
        if k >= len(part.tris):
            break
        q = part.tris[k]
        if tri[0] == q[0] and all(wp[tri[i]][:3] == pp[q[i]][:3] and whole.st[tri[i]][1:] == part.st[q[i]][1:]
                                  for i in (1, 2, 3)):
            out.append(t)
            k += 1
    if k != len(part.tris):
        raise SourceError("%s: %d of its %d triangles found in %s" % (part.name, k, len(part.tris), whole.name))
    return out


def make_empty(context, ob, name, loc, size, kind='PLAIN_AXES', props=None, locked=False):
    e = bpy.data.objects.new(name, None)
    e.empty_display_type = kind
    e.empty_display_size = size
    e.location = loc
    e.parent = ob
    for k, v in (props or {}).items():
        e[k] = v
    if locked:
        e.lock_location = (True, True, True)
        e.hide_select = True
    context.collection.objects.link(e)
    return e


def save_png(img, path):
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()
    if img.packed_file is not None:
        img.unpack(method='REMOVE')
    img.filepath = path
    img.reload()


def build(context, root, name, out_dir):
    """Makes the source .blend of progs/<name> in out_dir from the shipped files. Returns the .blend's path."""
    spec = SPECS.get(name, {})
    progs = os.path.join(root, "quakevr", "progs")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    context = bpy.context
    ob = mb.import_mdl(context, os.path.join(progs, name))
    stem = os.path.splitext(name)[0]
    model, group_of, degenerate = mb.reference(ob)
    me = ob.data

    # The parts: each file triangle's part, as a face attribute.
    parts = spec.get("parts", ())
    if parts:
        part_of = [-1] * len(model.tris)
        for k, pn in enumerate(parts):
            for t in subset_tris(model, mdl.Model.read(os.path.join(progs, pn))):
                if part_of[t] >= 0:
                    raise SourceError("triangle %d is in two parts" % t)
                part_of[t] = k
        face_tri = np.empty(len(me.polygons), np.int32)
        me.attributes[mb.A_TRI].data.foreach_get("value", face_tri)
        loose = [t for t in range(len(model.tris)) if part_of[t] < 0 and t not in degenerate]
        if loose:
            raise SourceError("%d triangles are in no part (the first %d)" % (len(loose), loose[0]))
        # The triangles that cover nothing (two corners welded: not faces in Blender, kept in the file in place).
        ob["qvr_degenerate_parts"] = {str(t): part_of[t] for t in degenerate if part_of[t] >= 0}
        a = me.attributes.new(A_PART, 'INT', 'FACE')
        a.data.foreach_set("value", [part_of[t] for t in face_tri])
        ob["qvr_part_files"] = list(parts)

    # The rivets: the triangles after the generator's input's.
    if spec.get("rivets_from"):
        first = len(mdl.Model.read(os.path.join(root, spec["rivets_from"])).tris)
        face_tri = np.empty(len(me.polygons), np.int32)
        me.attributes[mb.A_TRI].data.foreach_get("value", face_tri)
        verts = sorted({v for f in me.polygons if face_tri[f.index] >= first for v in f.vertices})
        vg = ob.vertex_groups.new(name="rivets")
        vg.add(verts, 1.0, 'REPLACE')

    # The skin and the relief, as PNGs beside the .blend.
    os.makedirs(out_dir, exist_ok=True)
    for img_name in ob["qvr_images"]:
        img = bpy.data.images[img_name]
        save_png(img, os.path.join(out_dir, img_name + ".png"))
    if spec.get("relief"):
        d = np.load(os.path.join(root, "Misc", "quakevr", "src_models", spec["relief"], name + ".relief.npz"))
        h = d["h"].astype(np.int64)
        if h.min() < -127 or h.max() > 127:
            raise SourceError("the relief's heights don't fit a grey PNG (%d..%d)" % (h.min(), h.max()))
        H, W = h.shape
        img = bpy.data.images.new(stem + "_relief", W, H, alpha=False)
        g = ((h + 128) / 255.0)[::-1].astype(np.float32)
        rgba = np.ones((H, W, 4), np.float32)
        rgba[:, :, 0] = rgba[:, :, 1] = rgba[:, :, 2] = g
        img.colorspace_settings.name = 'Non-Color'
        img.pixels.foreach_set(rgba.ravel())
        img.update()
        img.use_fake_user = True  # (no material uses it: kept in the .blend all the same)
        save_png(img, os.path.join(out_dir, stem + "_relief.png"))
        ob["qvr_relief"] = img.name
        ob["qvr_relief_density"] = float(d["density"])

    # Generic (every weapon, 2026-11): marked a source; its normal maps kept as shipped until what they're baked from
    # changes; its full-colour skins (<model>_<n>.png) kept beside the .blend and copied by the export.
    ob["qvr_source_blend"] = 1
    owned = [name] + list(parts) + list(spec.get("derived", ("", ()))[1])
    relief = image_heights(bpy.data.images[ob["qvr_relief"]]) if ob.get("qvr_relief") else None
    norms = {}
    for fn in owned:
        np_path = os.path.join(progs, fn + "_0_norm.png")
        if os.path.exists(np_path):
            with open(os.path.join(progs, fn), "rb") as f:
                mb_ = f.read()
            with open(np_path, "rb") as f:
                norms[fn] = {"in": norm_inputs(mb_, relief if fn == name else None), "out": sha(f.read())}
    ob["qvr_norms"] = norms
    extras = sorted(f for f in os.listdir(progs) if re.fullmatch(re.escape(name) + r"_\d+(_glow|_luma)?\.png", f))
    for fn in extras:
        shutil.copyfile(os.path.join(progs, fn), os.path.join(out_dir, fn))
    ob["qvr_extra_files"] = extras

    # The anchors, hotspots, the shell port and the sight line, as empties.
    coll = bpy.data.collections.new("anchors")
    context.scene.collection.children.link(coll)
    lc = context.view_layer.layer_collection.children[coll.name]
    context.view_layer.active_layer_collection = lc
    for slot, vals in sorted(model_slots(root, name).items()):
        for key in ANCHOR_KEYS:
            p = mdl.parse_anchor(vals.get(key, ""))
            if p is not None:
                # (how far off the surface the shipped anchor is: one already off it isn't refused unless moved)
                off = mdl.bind_anchor(model, vals.get(key, ""))[2]
                make_empty(context, ob, "av %s" % key, p, 0.6, props={"qvr_slot": slot, "qvr_key": key,
                                                                      "qvr_built_off": float(off)})
        for n in range(1, 5):
            if vals.get("hs%d_type" % n, "0") not in ("0", ""):
                p = tuple(float(vals.get("hs%d_%s" % (n, c), "0")) for c in "xyz")
                make_empty(context, ob, "hs %d" % n, p, 1.0, 'SPHERE',
                           props={"qvr_slot": slot, "qvr_key": "hs%d" % n, "qvr_type": vals["hs%d_type" % n]})
    for what, text in mdl.weapon_anchors(root, name):
        if "vr_shells.cpp" in what:
            make_empty(context, ob, "port shell", mdl.parse_anchor(text), 0.6, 'ARROWS', props={"qvr_cpp": what})
    line = sights.sight_line(model)
    if line:
        for nm, p in (("sight rear", line[0]), ("sight front", line[1])):
            make_empty(context, ob, nm, p, 0.4, 'SINGLE_ARROW', locked=True)
    context.view_layer.active_layer_collection = context.view_layer.layer_collection
    context.view_layer.objects.active = ob

    # Paths relative to the .blend; the model's own path is found from the .blend's place when exporting.
    for k in ("qvr_source",):
        if k in ob:
            del ob[k]
    path = os.path.join(out_dir, stem + ".blend")
    context.preferences.filepaths.save_version = 0  # (no .blend1 beside it)
    bpy.ops.wm.save_as_mainfile(filepath=path, compress=True, relative_remap=True)
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_mainfile(compress=True)
    return path


# ----------------------------------------------------------------------------
# Exporting


def source_object():
    obs = [o for o in bpy.data.objects if o.type == 'MESH' and o.get("qvr_kind") == mb.KIND and o.get("qvr_mdl")]
    if len(obs) != 1:
        raise SourceError("the .blend must hold one model object (qvr_kind mdl); it has %d" % len(obs))
    return obs[0]


def image_heights(img):
    w, h = img.size
    px = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(px)
    g = np.round(px.reshape(h, w, 4)[::-1, :, 0] * 255.0).astype(np.int64) - 128
    return g


def file_vertex_places(ob, nv, frame0):
    """Each file vertex's Blender vertex (from the corners), or -1 (unused: its frame-0 bytes stand)."""
    me = ob.data
    nl = len(me.loops)
    lv = np.empty(nl, np.int32)
    me.loops.foreach_get("vertex_index", lv)
    lid = np.empty(nl, np.int32)
    me.attributes[mb.A_VID].data.foreach_get("value", lid)
    bv = np.full(nv, -1, np.int64)
    ok = (lid >= 0) & (lid < nv)
    bv[lid[ok]] = lv[ok]
    return bv


def armature_frames(context, ob, model):
    """[(name, places (nv, 3))] for every action with qvr_frames, the mesh posed by its armature (basis only)."""
    acts = [a for a in bpy.data.actions if a.get("qvr_frames")]
    if not acts:
        return []
    arm = next((m.object for m in ob.modifiers if m.type == 'ARMATURE' and m.object is not None), None)
    if arm is None:
        raise SourceError("actions with qvr_frames (%s) but the model has no Armature modifier" %
                          ", ".join(a.name for a in acts))
    keys = ob.data.shape_keys
    muted = [kb.mute for kb in keys.key_blocks] if keys else []
    if keys:
        for kb in keys.key_blocks:
            kb.mute = True
    ad = arm.animation_data or arm.animation_data_create()
    old_action = ad.action
    scene = context.scene
    old_frame = scene.frame_current
    bv = file_vertex_places(ob, model.num_verts, None)
    rest = np.array([model.place(b) for b in model.pose_bytes()[0]])
    out = []
    try:
        for act in sorted(acts, key=lambda a: a.name):
            stem = str(act["qvr_frames"])
            if not re.fullmatch(r"[A-Za-z0-9_]{1,%d}" % FRAME_STEM_MAX, stem):
                raise SourceError("action %s: qvr_frames \"%s\" must be 1-%d letters, digits or _" %
                                  (act.name, stem, FRAME_STEM_MAX))
            ad.action = act
            if getattr(ad, "action_slot", None) is None and getattr(act, "slots", None):
                ad.action_slot = act.slots[0]
            f0, f1 = (int(round(x)) for x in act.frame_range)
            for k, f in enumerate(range(f0, f1 + 1)):
                scene.frame_set(f)
                dg = context.evaluated_depsgraph_get()
                ev = ob.evaluated_get(dg)
                me = ev.to_mesh()
                co = np.empty(len(me.vertices) * 3, np.float32)
                me.vertices.foreach_get("co", co)
                ev.to_mesh_clear()
                co = co.reshape(-1, 3).astype(np.float64)
                places = rest.copy()
                used = bv >= 0
                places[used] = co[bv[used]]
                out.append(("%s%d" % (stem, k + 1), places))
    finally:
        ad.action = old_action
        if keys:
            for kb, m in zip(keys.key_blocks, muted):
                kb.mute = m
        scene.frame_set(old_frame)
    return out


def append_frames(model, frames):
    """The model with `frames` [(name, places)] appended as simple frames (bytes on its grid, normals from its
    triangles). Raises if a place is outside the box."""
    org, sc = np.array(model.origin), np.array(model.scale)
    table = np.array(mdl.anorms())
    tris = np.array([t[1:] for t in model.tris], np.int64)
    nv = model.num_verts
    radius = model.h[8]
    # Normals: frame 0's, turned with the piece each vertex is on (its rigid motion from frame 0: the shading the model
    # was made with, carried along); a piece that doesn't move keeps frame 0's bytes.
    pose0 = model.pose_bytes()[0]
    p0 = np.array([model.place(b) for b in pose0])
    n0 = table[[b[3] for b in pose0]]
    parent = list(range(nv))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for a, b, c in tris.tolist():
        parent[find(a)] = find(b)
        parent[find(b)] = find(c)
    first_at = {}
    for v in range(nv):  # (the file splits vertices at seams: those at one place are one piece)
        k = pose0[v][:3]
        if k in first_at:
            parent[find(v)] = find(first_at[k])
        else:
            first_at[k] = v
    piece = np.array([find(v) for v in range(nv)])
    pieces = [np.nonzero(piece == r)[0] for r in np.unique(piece)]
    for name, places in frames:
        if len(name) > 15:
            raise SourceError("frame name %s is longer than 15 characters" % name)
        q = np.round((places - org) / sc)
        if (q < 0).any() or (q > 255).any():
            raise SourceError("frame %s: %d vertices outside the model's box" % (name, int(((q < 0) | (q > 255)).any(1).sum())))
        q = q.astype(np.int64)
        g = org + q * sc
        ni = np.array([b[3] for b in pose0], np.int64)
        for vs in pieces:
            if np.abs(g[vs] - p0[vs]).max() <= 1e-6:
                continue
            R = mb.kabsch(p0[vs], g[vs]) if len(vs) >= 3 else np.eye(3)
            ni[vs] = np.argmax((n0[vs] @ R.T) @ table.T, axis=1)
        vb = bytes(np.concatenate([q, ni[:, None]], 1).astype(np.uint8).ravel())
        hdr = bytes(int(x) for x in q.min(0)) + b"\0" + bytes(int(x) for x in q.max(0)) + b"\0" + \
            name.encode("latin-1").ljust(16, b"\0")
        model.frames.append(["simple", hdr, vb])
        radius = max(radius, float(np.linalg.norm(g, axis=1).max()))
    model.h[8] = mb.f32(radius)
    return model


def part_bytes(whole_bytes, name, tris):
    """The part file: `whole` with only the triangles `tris` (in order) and the vertices they use, its poses' bounds
    over those (as gunlooks.py's write_subset)."""
    m = mdl.Model(whole_bytes, name)
    used = sorted({v for t in tris for v in m.tris[t][1:]})
    remap = {v: i for i, v in enumerate(used)}
    m.st = [m.st[v] for v in used]
    m.tris = [(m.tris[t][0],) + tuple(remap[v] for v in m.tris[t][1:]) for t in tris]

    def cut(hdr, vb):
        nvb = b"".join(vb[4 * v:4 * v + 4] for v in used)
        arr = np.frombuffer(nvb, np.uint8).reshape(-1, 4)
        h = bytearray(hdr)
        h[0:3] = bytes(int(x) for x in arr[:, :3].min(0))
        h[4:7] = bytes(int(x) for x in arr[:, :3].max(0))
        return bytes(h), nvb

    for fr in m.frames:
        if fr[0] == "simple":
            fr[1], fr[2] = cut(fr[1], fr[2])
        else:
            for sub in fr[3]:
                sub[0], sub[1] = cut(sub[0], sub[1])
            bb = bytearray(fr[1])
            bb[0:3] = bytes(min(s[0][k] for s in fr[3]) for k in range(3))
            bb[4:7] = bytes(max(s[0][4 + k] for s in fr[3]) for k in range(3))
            fr[1] = bytes(bb)
    return m.to_bytes()


MASK_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"


def mask_decode(text):
    n, bits = text.split(":")
    n = int(n)
    out = [False] * n
    for i, ch in enumerate(bits):
        v = MASK_ALPHABET.index(ch)
        for k in range(6):
            if i * 6 + k < n and v >> k & 1:
                out[i * 6 + k] = True
    return out


def mask_encode(flags):
    chars = []
    for i in range(0, len(flags), 6):
        v = sum(1 << k for k in range(6) if i + k < len(flags) and flags[i + k])
        chars.append(MASK_ALPHABET[v])
    return "%d:%s" % (len(flags), "".join(chars))


def write_weapons_txt(root, changes):
    """Writes {(slot, key): value} into quakevr/data/weapons.txt (each in its slot's block: its line replaced, or added
    before the block's end)."""
    wd = weapons_data(root)
    lines, blocks = wd.parse_data()
    by_slot = {b["slot"]: b for b in blocks}
    adds = {}
    for (slot, key), value in changes.items():
        b = by_slot[slot]
        if key in b["keys"]:
            i = b["keys"][key]
            lines[i] = re.sub(r'"[^"]*"', '"%s"' % value, lines[i], count=1)
        else:
            adds.setdefault(b["close"], []).append('    %s "%s"' % (key, value))
    for i in sorted(adds, reverse=True):
        lines[i:i] = adds[i]
    with open(wd.DATA, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(lines))


def key_names(names):
    """The shape keys' names for the model's frame names, as the import gets them from Blender: an unnamed frame is
    frame<n>, a name already taken gets .001, .002... (id's player, dog and soldier have unnamed frames; the shambler's
    are all "From ASE"). The file keeps its own names: the export writes the model's."""
    seen, out = set(), []
    for p, n in enumerate(names):
        n = n or "frame%d" % p
        if n in seen:
            k = 1
            while "%s.%03d" % (n, k) in seen:
                k += 1
            n = "%s.%03d" % (n, k)
        seen.add(n)
        out.append(n)
    return out


def export(context, root, out_dir=None, allow_moved=False, allow_sights=False, report=print):
    """Exports the source .blend's model. Returns (written paths, data changes {(slot, key): value}). Raises
    SourceError (nothing written) if a check fails."""
    ob = source_object()
    name = ob["qvr_name"]
    spec = SPECS.get(name, {})
    progs = os.path.join(root, "quakevr", "progs")
    out_dir = out_dir or progs
    in_repo = os.path.normcase(os.path.abspath(out_dir)) == os.path.normcase(os.path.abspath(progs))
    ob["qvr_source"] = os.path.join(progs, name)  # (the anchors' and holes' checks: the repository's)
    old, _, _ = mb.reference(ob)
    errors, warns = [], []

    # Frames: one shape key per frame, named and ordered as the model's (QuakeC's $frame lines).
    keys = ob.data.shape_keys
    got = [k.name for k in keys.key_blocks] if keys else []
    want = key_names(old.pose_names())
    if got != want and not (not got and len(want) == 1):  # (a one-frame model imports without shape keys)
        errors.append("frames: the shape keys are %s; the model's frames are %s (QuakeC numbers them: keep their "
                      "names and order, add frames only as armature actions)" % (got, want))
    if errors:
        raise SourceError("\n".join(errors))

    # The model, written to a scratch folder first; nothing reaches out_dir unless every check passes.
    tmp = tempfile.mkdtemp(prefix="qvr_src_")
    path = os.path.join(tmp, name)
    msgs = []
    try:
        lines, moved = mb.export_mdl(context, ob, path, lambda lvl, m: msgs.append(m))
    except mb.ExportError as e:
        raise SourceError("export: %s" % e)
    for m in msgs:
        report("  " + m)
    tri_old = list(mb.LAST_EXPORT["tri_old_index"])
    new = mdl.Model.read(path)

    # Anchors and hotspots: the empties' places, against weapons.txt's.
    slots = model_slots(root, name)
    changes = {}
    moved_keys = set()
    rest = [new.place(b) for b in new.pose_bytes()[0]]
    for e in [o for o in bpy.data.objects if o.parent == ob and o.get("qvr_key")]:
        slot, key = int(e["qvr_slot"]), e["qvr_key"]
        loc = tuple(e.location)
        if key.endswith("_av"):
            text = point_text(loc)
            cur = slots.get(slot, {}).get(key, "")
            p_cur = mdl.parse_anchor(cur)
            if p_cur is None or max(abs(a - b) for a, b in zip(p_cur, loc)) > 5e-5:
                changes[(slot, key)] = text
                moved_keys.add(key)
            w, t, d = mdl.bind_anchor(new, text, rest)
            report("  anchor %-10s slot %d \"%s\": triangle %d, %.4f off the surface%s" % (
                key, slot, text, t, d, " (moved: written to weapons.txt)" if key in moved_keys else ""))
            if d > SURFACE_MAX and (key in moved_keys or d > float(e.get("qvr_built_off", 0.0)) + 1e-3):
                errors.append("anchor %s (empty \"%s\") is %.3f units off the model's surface: put it on the surface "
                              "(snap: Vertex/Face) or it binds to the nearest triangle somewhere else" % (key, e.name, d))
        else:
            vals = slots.get(slot, {})
            for c, x in zip("xyz", loc):
                k = "%s_%s" % (key, c)
                if abs(float(vals.get(k, "0")) - x) > 5e-5:
                    changes[(slot, k)] = fmt(x)
    for what, text, dist in moved:
        key = next((k for k in ANCHOR_KEYS if "vr_wofs_%s_" % k in what), None)
        if key in moved_keys:
            continue
        errors.append("anchor %s \"%s\": the surface under it moved %.3f units in some frame (%s). Move the anchor's "
                      "empty onto the new surface, or export with --allow-moved" % (what, text, dist,
                                                                                     "MOVED in the report"))
    if allow_moved:
        errors = [e for e in errors if "surface under it moved" not in e]
    for e in [o for o in bpy.data.objects if o.parent == ob and o.get("qvr_cpp")]:
        text = point_text(e.location)
        _, _, d = mdl.bind_anchor(new, text, rest)
        found = [t for w_, t in mdl.weapon_anchors(root, name) if w_ == e["qvr_cpp"]]
        if found and mdl.parse_anchor(found[0]) and max(abs(a - b) for a, b in zip(mdl.parse_anchor(found[0]),
                                                                                    e.location)) > 5e-5:
            warns.append("%s moved: paste \"%s\" into %s by hand (%.3f off the surface)" % (e.name, text, e["qvr_cpp"], d))

    # The sight line (Weapon Studio's align and Sight Align measure against it).
    if name.lower() in sights.PAINTED:
        a, b = sights.sight_line(old), sights.sight_line(new)
        if a and not b:
            errors.append("sights: the painted sights no longer make a sight line (the sight texels, palette 224-239, "
                          "252, 253, on the rear notch and the front post): repaint them")
        elif b and not a:
            # Sights painted where there were none (the nail pistol, 2026-11): nothing to keep, so nothing refused.
            (u1, l1) = sights.angle_deg(b)
            report("  sights: new painted sights, rear %s front %s, %.3f deg up, %.3f deg left" % (
                point_text(b[0]), point_text(b[1]), u1, l1))
            warns.append("sights: new painted sights: line them up in the game (vr_weaponstudio_align main apply)")
        elif a and b:
            (u0, l0), (u1, l1) = sights.angle_deg(a), sights.angle_deg(b)
            turn = math.hypot(u1 - u0, l1 - l0)
            shift = max(math.dist(a[0], b[0]), math.dist(a[1], b[1]))
            report("  sights: rear %s front %s, %.3f deg up, %.3f deg left (was %.3f, %.3f): turned %.3f deg, moved %.3f" % (
                point_text(b[0]), point_text(b[1]), u1, l1, u0, l0, turn, shift))
            if turn > SIGHT_MAX_DEG and not allow_sights:
                errors.append("sights: the sight line turned %.2f degrees (at most %.2f): the gun no longer shoots where "
                              "its sights look. Put the sights back, or line them up again in the game "
                              "(vr_weaponstudio_align main apply) after exporting with --allow-sights" % (turn, SIGHT_MAX_DEG))
            elif shift > 0.05:
                warns.append("sights: the sight line moved %.3f units: check vr_weaponstudio_align in the game" % shift)

    if errors:
        raise SourceError("\n".join(errors))

    # Extra animations: armature actions baked to frames after the model's own.
    extra = armature_frames(context, ob, new)
    if extra:
        new = append_frames(new, extra)
        report("  %d frames appended from actions: %s .. %s (frames %d-%d)" % (
            len(extra), extra[0][0], extra[-1][0], len(want), len(want) + len(extra) - 1))
    data = new.to_bytes()
    # Skins made from skin 0 by a generator's code (the hands' blood, the grenades' unarmed skin): made again from the
    # exported skin 0, so they follow its edits (the .blend's own skins after 0 are only shown).
    if spec.get("post"):
        gen_name, how = spec["post"]
        misc_dir = os.path.join(root, "Misc", "quakevr")
        if misc_dir not in sys.path:
            sys.path.insert(0, misc_dir)
        ptmp = os.path.join(tempfile.mkdtemp(prefix="qvr_src_post_"), name)
        with open(ptmp, "wb") as f:
            f.write(data)
        POST[how](importlib.import_module(gen_name), ptmp, name)
        with open(ptmp, "rb") as f:
            data = f.read()
        report("  skins after 0: made from skin 0 by %s.py" % gen_name)
    outs = {name: data}

    # The parts.
    part_files = list(ob.get("qvr_part_files", []))
    if part_files:
        me = ob.data
        face_tri = np.empty(len(me.polygons), np.int32)
        me.attributes[mb.A_TRI].data.foreach_get("value", face_tri)
        pa = me.attributes.get(A_PART)
        face_part = np.zeros(len(me.polygons), np.int32)
        if pa is not None:
            pa.data.foreach_get("value", face_part)
        old_deg = {int(t): int(k) for t, k in dict(ob.get("qvr_degenerate_parts", {})).items()}
        part_of = {t: old_deg[o] for t, o in enumerate(tri_old) if o in old_deg}
        ob["qvr_degenerate_parts"] = {str(t): k for t, k in part_of.items()}  # (the object stands for the file now)
        for t, k in zip(face_tri.tolist(), face_part.tolist()):
            if not 0 <= k < len(part_files):
                raise SourceError("a face's %s is %d: 0..%d (%s)" % (A_PART, k, len(part_files) - 1, part_files))
            part_of[t] = k
        for k, pn in enumerate(part_files):
            tris = [t for t in range(len(new.tris)) if part_of.get(t) == k]
            outs[pn] = part_bytes(data, pn, tris)

    # The data the game keeps for the model: the barrel mask follows its triangles.
    wd = weapons_data(root)
    for slot, vals in slots.items():
        mask = vals.get("fx_gfx_mask", "0")
        if ":" in mask:
            bits = mask_decode(mask)
            if len(bits) == old.h[16]:
                nm = mask_encode([bits[o] if 0 <= o < len(bits) else False for o in tri_old])
                if nm != mask:
                    changes[(slot, "fx_gfx_mask")] = nm

    misc = os.path.join(root, "Misc", "quakevr")
    if misc not in sys.path:
        sys.path.insert(0, misc)

    # The files made from the model by a generator's code (the super shotgun's open halves).
    if spec.get("derived"):
        gen_name, files = spec["derived"]
        dtmp = tempfile.mkdtemp(prefix="qvr_src_derived_")
        src = os.path.join(dtmp, name)
        with open(src, "wb") as f:
            f.write(data)
        gen = importlib.import_module(gen_name)
        try:
            gen.build(src=src, out_dir=dtmp, guarded=False)
        except (AssertionError, SystemExit, ValueError, IndexError, KeyError) as e:
            raise SourceError("%s (made from the model by %s.py) failed: %s (its code needs the model's vertex numbers "
                              "and triangle counts as they were: see that script)" % (", ".join(files), gen_name, e))
        for fn in files:
            with open(os.path.join(dtmp, fn), "rb") as f:
                outs[fn] = f.read()
        report("  %s: %s, made from the model by %s.py" % (" and ".join(files), "written", gen_name))

    # Write: the models, then their normal maps baked from them (the relief from the .blend's PNG).
    os.makedirs(out_dir, exist_ok=True)
    written = []
    for fn, b in outs.items():
        p = os.path.join(out_dir, fn)
        with open(p, "wb") as f:
            f.write(b)
        written.append(p)
    relief = None
    if spec.get("relief") and ob.get("qvr_relief"):
        img = bpy.data.images[ob["qvr_relief"]]
        img.reload()
        relief = image_heights(img)
        h = relief.astype(np.float64)
        density = float(ob["qvr_relief_density"])
        gen = importlib.import_module(spec["relief"])
        gen.relief = lambda _name, h=h, density=density: (h * 0.002, density, 1)
    from . import normalmaps
    from . import normalbake as nb
    if not ob.get("qvr_source_blend"):  # (the shotgun's .blend, made before: its maps always baked)
        for p, rgb in normalmaps.bake(out_dir, name).items():
            nb.write_png(p, rgb)
            written.append(p)
    else:
        # The maps the shipped model has: kept while what they're baked from is unchanged, else baked again.
        for fn, rec in dict(ob.get("qvr_norms", {})).items():
            fn = str(fn)
            npng = fn + "_0_norm.png"
            shipped = os.path.join(progs, npng)
            same_in = norm_inputs(outs[fn], relief if fn == name else None) == rec["in"]
            if same_in and os.path.exists(shipped) and sha(open(shipped, "rb").read()) == rec["out"]:
                if not in_repo:
                    shutil.copyfile(shipped, os.path.join(out_dir, npng))
                    written.append(os.path.join(out_dir, npng))
                report("  %s: kept (the model and its relief as it was baked from)" % npng)
                continue
            p = os.path.join(out_dir, npng)
            rgb = normalmaps.bake(out_dir, normalmaps.target_of(fn))[p]
            nb.write_png(p, rgb)
            written.append(p)
            report("  %s: baked again" % npng)
        # The full-colour skins beside the .blend, as they are.
        here = os.path.dirname(bpy.data.filepath)
        for fn in ob.get("qvr_extra_files", []):
            s, d = os.path.join(here, str(fn)), os.path.join(out_dir, str(fn))
            if os.path.normcase(os.path.abspath(s)) != os.path.normcase(os.path.abspath(d)):
                shutil.copyfile(s, d)
            written.append(d)
    if changes:
        if in_repo:
            write_weapons_txt(root, changes)
            report("  weapons.txt: %s" % ", ".join("slot %d %s \"%s\"" % (s, k, v if len(v) < 40 else v[:20] + "...")
                                                  for (s, k), v in sorted(changes.items())))
        else:
            report("  weapons.txt not written (the export isn't to the repository's quakevr/progs); it would get: %s" %
                   ", ".join("slot %d %s" % (s, k) for (s, k) in sorted(changes)))
    for w in warns:
        report("  WARNING " + w)
    return written, changes  # (the scratch copy stays in the system's temp folder)
