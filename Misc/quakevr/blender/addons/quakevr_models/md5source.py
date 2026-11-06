# md5source.py -- a .blend holding the jointed hand or the body (imported with the add-on: File > Import > Quake VR Hand
# / Quake VR Body, then saved) exports on save as a weapon's source .blend does (autoexport.py, blend_export.py): the
# hand through hand_blender.export_hand, the body through body_blender.export_body, then their normal maps baked again,
# all with the export dialog's defaults. Where it writes: the --out folder (the add-on's Game Folder), else the
# repository's quakevr/progs when the .blend is inside one, else the file it was imported from (qvr_source). Only the
# file's name is kept from qvr_source: the hand_rig.md5mesh of one checkout imported into a .blend of another writes the
# other's.

import os

import bpy

HAND = "hand"
BODY = "body"


class Md5SourceError(Exception):
    pass


def find(context=None):
    """(kind, mesh object) of the .blend's hand or body (the one the import made: qvr_hand / qvr_kind "body", with the
    file it was read from), or (None, None)."""
    hands = [o for o in bpy.data.objects if o.type == 'MESH' and o.get("qvr_hand") and o.parent is not None
             and o.parent.get("qvr_source")]
    bodies = [o for o in bpy.data.objects if o.type == 'MESH' and o.get("qvr_kind") == "body" and o.get("qvr_source")]
    if len(hands) == 1 and not bodies:
        return HAND, hands[0]
    if len(bodies) == 1 and not hands:
        return BODY, bodies[0]
    return None, None


def source_path(kind, ob):
    return (ob.parent if kind == HAND else ob)["qvr_source"]


def model_name(kind, ob):
    """The game's model name (vr_model_reload's): hand_rig, hand_rig_f, vrbody, vrbody_lean..."""
    return os.path.splitext(os.path.basename(source_path(kind, ob)))[0]


def target(kind, ob, blend, out):
    from . import source
    name = os.path.basename(source_path(kind, ob))
    if out:
        return os.path.join(out, name)
    root = source.repo_root(blend) if blend else None
    if root is not None:
        return os.path.join(root, "quakevr", "progs", name)
    return source_path(kind, ob)


def contents(folder, model):
    """A digest of the model's mesh (but its commandline line, which names the exporter) and skins in `folder`: the
    normal maps are baked again only when it changes."""
    import hashlib
    h = hashlib.sha256()
    for f in sorted(os.listdir(folder)):
        lo = f.lower()
        if not lo.startswith(model.lower() + ("." if lo.endswith(".md5mesh") else "_")) or "_norm" in lo:
            continue
        if not lo.endswith((".md5mesh", ".lmp", ".tga")) or lo.endswith("_skinmask.tga"):
            continue
        with open(os.path.join(folder, f), "rb") as fh:
            data = fh.read()
        if lo.endswith(".md5mesh"):
            data = b"\n".join(l for l in data.split(b"\n") if not l.startswith(b"commandline"))
        h.update(f.encode() + b"\0" + data)
    return h.hexdigest()


def export(context, blend, out, report):
    """Exports the .blend's hand or body: the files written (absolute paths). Raises Md5SourceError (nothing written
    when a check fails before the first file)."""
    kind, ob = find(context)
    if kind is None:
        raise Md5SourceError("no hand or body in this .blend (import one: File > Import > Quake VR Hand / Body)")
    path = target(kind, ob, blend, out)
    folder = os.path.dirname(path)
    if not os.path.isdir(folder):
        raise Md5SourceError("%s: no such folder" % folder)
    before = {f: os.path.getmtime(os.path.join(folder, f)) for f in os.listdir(folder)}
    model = os.path.splitext(os.path.basename(path))[0]
    unchanged = contents(folder, model)
    for o in context.view_layer.objects:
        o.select_set(False)
    context.view_layer.objects.active = ob
    ob.select_set(True)
    if kind == HAND:
        from . import hand_blender as H
        from . import md5hand
        try:
            H.export_hand(context, path, True, report)
        except (H.HandError, md5hand.Md5Error, OSError, ValueError) as e:
            raise Md5SourceError(str(e))
    else:
        from . import body_blender as BB
        from . import md5body
        from . import qpal
        try:
            BB.export_body(context, ob, path, report)
        except (BB.BodyError, md5body.Md5Error, qpal.TgaError, OSError, ValueError) as e:
            raise Md5SourceError(str(e))
    if contents(folder, model) == unchanged:
        report({'INFO'}, "the mesh and the skins are as they were: the normal maps kept")
    else:
        from . import normal_blender as NB
        try:
            NB.bake(context, ob, path, report=lambda m: report({'WARNING'}, m))
        except (OSError, ValueError, RuntimeError) as e:
            report({'WARNING'}, "normal map not baked: %s" % e)
    return [os.path.join(folder, f) for f in sorted(os.listdir(folder))
            if os.path.getmtime(os.path.join(folder, f)) != before.get(f)]
