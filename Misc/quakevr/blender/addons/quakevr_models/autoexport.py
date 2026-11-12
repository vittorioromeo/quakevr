# autoexport.py -- export on save (docs/vr-port/MODELS_IN_BLENDER.md, "A weapon's source .blend"): saving a weapon's
# source .blend (one model object with its anchors' empties, inside the repository) runs the same export and checks
# as `blender -b <file.blend> --python Misc/quakevr/blend_export.py`, on the file just saved, in a background Blender:
# the export changes the object it exports (it triangulates it, leaves Edit Mode, sets its frame), so it can't run on
# the scene being edited, and in the background it doesn't hold up the editing. It writes the game's files into the
# repository's quakevr/progs (or the folder set in the add-on's preferences) and nothing when a check fails; the
# result shows in Blender's status bar, a refusal in a popup too. The game reads changed models again by itself
# (vr_model_watch).

import os
import subprocess
import tempfile
import time

import bpy
from bpy.app.handlers import persistent

from . import mdl_blender as MB

LOG_NAME = "quakevr_export_on_save.log"  # the last export's whole report, in the system's temp folder

# The export under way, the one asked for meanwhile (a save during an export: run after it), the last result.
state = {"proc": None, "blend": None, "out": "", "again": None, "result": None, "started": 0.0}


def prefs():
    a = bpy.context.preferences.addons.get(__package__)
    return a.preferences if a else None


def enabled():
    p = prefs()
    return p.export_on_save if p else True


def out_dir():
    """The folder set in the preferences ("": the repository's quakevr/progs, weapons.txt written too)."""
    p = prefs()
    d = p.progs_dir.strip() if p else ""
    return os.path.abspath(bpy.path.abspath(d)) if d else ""


def source_model():
    """The source .blend's model object: the one model object, with its anchors' empties (blend_build.py made them;
    an imported model has none)."""
    obs = [o for o in bpy.data.objects if o.type == 'MESH' and o.get("qvr_kind") == MB.KIND and o.get("qvr_mdl")]
    if len(obs) != 1 or not (obs[0].get("qvr_source_blend") or any(c.get("qvr_key") for c in obs[0].children)):
        return None  # (qvr_source_blend: every weapon's source made since 2026-11, anchors or not)
    return obs[0]


def is_source(blend):
    """A weapon's source .blend: one blend_build.py made (source_model), or a hand-made one (models_src/<gun>/<gun>.blend
    with its export_mdl.py beside it: blend_export.py runs that), or a .blend holding the jointed hand or the body
    (imported, then saved: md5source.py)."""
    from . import md5source, source
    return (source_model() is not None or source.handmade_script(blend) is not None or
            md5source.find()[0] is not None)


def export_script(blend):
    from . import source
    root = source.repo_root(blend)
    if root is None:
        return None
    script = os.path.join(root, "Misc", "quakevr", "blend_export.py")
    return script if os.path.isfile(script) else None


def log_path():
    return os.path.join(tempfile.gettempdir(), LOG_NAME)


def start(blend):
    script = export_script(blend)
    if script is None:
        return False
    out = out_dir()
    args = [bpy.app.binary_path, "-b", "--factory-startup", blend, "--python", script]
    if out:
        args += ["--", "--out", out]
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    with open(log_path(), "w") as log:
        state["proc"] = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                         creationflags=flags)
    state.update(blend=blend, out=out, started=time.time())
    print("Quake VR: exporting %s (export on save)" % blend)
    if not bpy.app.background:
        msg = "Quake VR: exporting %s..." % os.path.basename(blend)
        bpy.app.timers.register(lambda: show('INFO', msg), first_interval=0.0)  # (not from inside the save)
        bpy.app.timers.register(poll, first_interval=0.5)
    return True


def finish():
    try:
        with open(log_path()) as f:
            text = f.read()
    except OSError as e:
        text = "blend_export: REFUSED: the export's report couldn't be read (%s)" % e
    lines = text.splitlines()
    ok = any(l.startswith("blend_export: ok") for l in lines)
    secs = time.time() - state["started"]
    name = os.path.basename(state["blend"] or "")
    if ok:
        wrote = [l.strip()[len("wrote "):].split(" (")[0] for l in lines if l.startswith("  wrote ") and "qvr_src_" not in l]
        tail = next(l for l in lines if l.startswith("blend_export: ok"))[len("blend_export: "):]
        msg = "Quake VR: %s exported (%s) to %s in %.1f s; the game reads it again by itself" % (
            name, tail, os.path.dirname(wrote[0]) if wrote else "?", secs)
        box = [l[len("  box: "):] for l in lines if l.startswith("  box: ")]  # (Grow the Box grew it: the offsets)
        if box:
            msg += "; THE BOX GREW (see the popup)"
        state["result"] = (True, [msg] + box)
        print(msg + "".join("\n  " + l for l in box))
        show('WARNING' if box else 'INFO', msg)
        if box:
            popup("Quake VR: exported; the model's box grew", box + ["(the whole report: %s)" % log_path()],
                  icon='INFO')
    else:
        at = next((i for i, l in enumerate(lines) if "REFUSED" in l), None)
        why = [l.strip() for l in lines[at:]] if at is not None else lines[-12:]
        why = [l for l in why if l and not l.startswith("Blender quit")]
        msg = "Quake VR: %s NOT exported, nothing written: %s" % (name, " ".join(why[1:3]) or why[0] if why else "?")
        state["result"] = (False, why)
        print(msg + "\n  " + "\n  ".join(why) + "\n  (the whole report: %s)" % log_path())
        show('ERROR', msg)
        popup("Quake VR: not exported, nothing written", why + ["(the whole report: %s)" % log_path()])
    redraw()
    again, state["again"] = state["again"], None
    if again:
        start(again)


def redraw():
    """The panel's "Last:" line shown anew."""
    wm = bpy.context.window_manager
    for win in (wm.windows if wm and not bpy.app.background else ()):
        for area in win.screen.areas:
            if area.type == 'VIEW_3D':
                area.tag_redraw()


def poll():
    p = state["proc"]
    if p is None:
        return None
    if p.poll() is None:
        return 0.25
    state["proc"] = None
    finish()
    return None


def wait(timeout=600.0):
    """Waits for the export under way and the one asked for after it (a background Blender has no timers): the last
    result, (ok, lines), or None if none ran."""
    while state["proc"] is not None:
        p = state["proc"]
        p.wait(timeout)
        state["proc"] = None
        finish()
    return state["result"]


def window_override():
    wm = bpy.context.window_manager
    win = bpy.context.window or (wm.windows[0] if wm and wm.windows else None)
    return bpy.context.temp_override(window=win) if win else None


def show(level, msg):
    """The message in the status bar (an operator's report)."""
    if bpy.app.background:
        return
    o = window_override()
    if o is None:
        return
    with o:
        bpy.ops.quakevr_models.save_export_report(level=level, message=msg)


def popup(title, lines, icon='ERROR'):
    if bpy.app.background:
        return

    import textwrap
    rows = [r for line in lines for r in (textwrap.wrap(line, 120, subsequent_indent="    ") or [""])]

    def draw(self, _context):
        for row in rows[:30]:
            self.layout.label(text=row)
    o = window_override()
    if o is None:
        return
    with o:
        bpy.context.window_manager.popup_menu(draw, title=title, icon=icon)


@persistent
def on_save(*args):
    if not enabled():
        return
    blend = args[0] if args and isinstance(args[0], str) and args[0] else bpy.data.filepath
    if not blend or not blend.lower().endswith(".blend") or not is_source(blend) or export_script(blend) is None:
        return
    if state["proc"] is not None:
        state["again"] = blend  # (exported again when the one under way ends: the newest file)
        return
    start(blend)


class QVR_OT_save_export_report(bpy.types.Operator):
    """The export on save's result in the status bar"""
    bl_idname = "quakevr_models.save_export_report"
    bl_label = "Quake VR Export on Save Report"
    bl_options = {'INTERNAL'}

    level: bpy.props.StringProperty(default='INFO')
    message: bpy.props.StringProperty()

    def execute(self, context):
        self.report({self.level}, self.message)
        return {'FINISHED'}


def draw_panel(layout):
    p = prefs()
    box = layout.box()
    col = box.column(align=True)
    blend = bpy.data.filepath
    if p is None or p.export_on_save:
        exports = bool(blend) and is_source(blend) and export_script(blend) is not None
        col.label(text=("Ctrl+S exports it to the game" if exports else
                        "Saving this .blend exports nothing" if blend else "Not saved yet"),
                  icon='FILE_TICK' if exports else 'INFO')
    if p:
        col.prop(p, "export_on_save")
    else:
        col.label(text="Export on save: on")
    ob = source_model()
    if ob is not None:
        col.prop(ob, "qvr_grow_box")  # (kept in the .blend: the background export reads it, as blend_export.py --grow)
        col.prop(ob, "qvr_allow_moved")  # (same: source.export reads it, as --allow-moved)
        col.prop(ob, "qvr_allow_sights")  # (same, as --allow-sights)
    busy = state["proc"] is not None
    res = state["result"]
    if busy:
        col.label(text="Exporting...", icon='SORTTIME')
    elif res:
        col.label(text=("Last: exported" if res[0] else "Last: NOT exported (see the popup)"),
                  icon='CHECKMARK' if res[0] else 'ERROR')
    if p:
        col.prop(p, "progs_dir", text="Game folder")


def register():
    bpy.utils.register_class(QVR_OT_save_export_report)
    bpy.types.Object.qvr_grow_box = bpy.props.BoolProperty(
        name="Grow the Box", default=False, options=set(),
        description="Vertices past the model's box (its byte grid) make the export make the box bigger instead of "
                    "refusing: the grid gets coarser (every vertex moves a little); past the low side the origin "
                    "moves too, and the export writes the weapon's offsets that keep it in place into weapons.txt "
                    "(and shows them: your config keeps its own, so type them in the game's console too). Kept in "
                    "the .blend; later exports start from the grown box (the model file they replace)")
    bpy.types.Object.qvr_allow_moved = bpy.props.BoolProperty(
        name="Allow Moved Anchors", default=False, options=set(),
        description="Export even if the surface under an anchor moved (its empty not moved with it). Otherwise the "
                    "export refuses and names the anchor: move its empty onto the new surface, or tick this. Kept in "
                    "the .blend (as blend_export.py --allow-moved)")
    bpy.types.Object.qvr_allow_sights = bpy.props.BoolProperty(
        name="Allow Moved Sights", default=False, options=set(),
        description="Export even if the sight line turned (the gun no longer shoots where its sights look): line them "
                    "up again in the game afterwards (vr_weaponstudio_align main apply). Kept in the .blend (as "
                    "blend_export.py --allow-sights)")
    if on_save not in bpy.app.handlers.save_post:
        bpy.app.handlers.save_post.append(on_save)


def unregister():
    if on_save in bpy.app.handlers.save_post:
        bpy.app.handlers.save_post.remove(on_save)
    del bpy.types.Object.qvr_grow_box
    del bpy.types.Object.qvr_allow_moved
    del bpy.types.Object.qvr_allow_sights
    bpy.utils.unregister_class(QVR_OT_save_export_report)
