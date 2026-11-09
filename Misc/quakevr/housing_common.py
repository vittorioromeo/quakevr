#!/usr/bin/env python3
# housing_common.py -- what the ammo screen housing scripts share (make_screen_housing_shot.py, _ssg.py, _nailpistol.py;
# docs/vr-port/MODELS_IN_BLENDER.md, "Tweaking the ammo screen housings"). Each script builds its housing as a list of
# vertices and triangles (each corner's UV), then:
#
#   h = Run("shot2")                 # the arguments (-- [--dry] [--save other.blend]), the table entry's numbers
#   moved = h.apply(me, verts, tris)  # into the gun's mesh: moved in place or built anew (below)
#   h.finish(grow=..., summary)       # exports as blend_export.py does, saves the .blend, prints "housing_<stem>: ok, ..."
#
# In place: the housing's vertices carry the point attribute `qvr_housing` (1). With as many vertices as the build
# makes, they are moved (in the order made) and the triangles that are still the build's keep their place in the model
# file (re-mapped); otherwise the old housing is deleted and the new one added (its old vertices then stay in the
# .mdl unused: start from the .blend before the housing, in git history, to keep the file lean). A refusal ends the
# run with "housing_<stem>: REFUSED: ..." (exit code 1), writing nothing.

import os
import re
import struct
import sys

import bmesh
import bpy
from mathutils import kdtree

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
ADDONS = os.path.join(HERE, "blender", "addons")
TAG = "qvr_housing"


def style_entry(stem):
    """The table entry `stem` of Quake/vr/vr_screenstyle.cpp: {"radius", "size", "opening": (w, h), "offset": (r, u, o)}
    (fields left out: their defaults)."""
    src = open(os.path.join(ROOT, "Quake", "vr", "vr_screenstyle.cpp"), encoding="utf-8").read()
    m = re.search(r'\{"%s",(.*?)\n\s*(?:\{"|\};)' % re.escape(stem), src, re.S)  # (up to the next entry or the end)
    if not m:
        return None
    body = m.group(1)

    def num(name, default):
        n = re.search(r"\.%s\s*=\s*(-?[0-9.]+)f?\b" % name, body)
        return float(n.group(1)) if n else default

    def vec(name, n):
        v = re.search(r"\.%s\s*=\s*\{([^}]*)\}" % name, body)
        return tuple(float(x.strip().rstrip("f")) for x in v.group(1).split(",")) if v else (0.0,) * n

    return {"radius": num("radius", 4.0), "size": num("size", 1.0), "opening": vec("opening", 2),
            "offset": vec("offset", 3)}


def mdl_grid(model):
    """The model file's grid (quakevr/progs/<model>.mdl): (scale, origin), each (x, y, z)."""
    d = open(os.path.join(ROOT, "quakevr", "progs", model + ".mdl"), "rb").read(32)
    return struct.unpack("<3f", d[8:20]), struct.unpack("<3f", d[20:32])


def addon():
    """This checkout's add-on modules (source, mdl_blender), not an installed one's (as blend_export.py)."""
    if ADDONS not in sys.path:
        sys.path.insert(0, ADDONS)
    loaded = sys.modules.get("quakevr_models")
    if loaded is not None and not os.path.normcase(os.path.abspath(getattr(loaded, "__file__", "") or "")).startswith(
            os.path.normcase(os.path.abspath(ADDONS))):
        for k in [k for k in sys.modules if k == "quakevr_models" or k.startswith("quakevr_models.")]:
            del sys.modules[k]
    from quakevr_models import mdl_blender, source
    return source, mdl_blender


class Run:
    def __init__(self, stem, model):
        self.stem = stem
        self.model = model
        argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
        self.argv = argv
        self.dry = "--dry" in argv
        self.save = os.path.abspath(argv[argv.index("--save") + 1]) if "--save" in argv else bpy.data.filepath
        self.style = style_entry(stem)
        if self.style is None:
            self.fail("no \"%s\" entry in Quake/vr/vr_screenstyle.cpp" % stem)
        self.exported = 0
        self.grid = mdl_grid(model)

    def main(self, fn):
        """Runs the script's main: an error ends it as a refusal (Blender's own exit code is 0 after a traceback)."""
        try:
            fn(self)
        except SystemExit:
            raise
        except Exception:  # noqa: BLE001
            import traceback
            traceback.print_exc()
            self.fail("the script failed (the traceback above)")

    def say(self, msg):
        print("housing_%s: %s" % (self.stem, msg))

    def fail(self, msg):
        print("housing_%s: REFUSED: %s" % (self.stem, msg))
        sys.stdout.flush()
        sys.exit(1)

    def apply(self, me, verts, tris, smooth=False, new_vert=None, legacy=None):
        """Puts the housing into the mesh. `verts`: positions; `tris`: (i, j, k, (uv_i, uv_j, uv_k)), wound as the
        faces should be. `new_vert(bm, bmvert, i)`: sets the new vertex verts[i]'s shape keys (default: each at its
        place) and other layers; `legacy(bm)`: the vertices of a housing made before the tag, or None (matched to the
        build by their places). Returns whether it moved the housing there (else built it)."""
        bm = bmesh.new()
        bm.from_mesh(me)
        lay = bm.verts.layers.int.get(TAG)
        old = []
        matched = True
        if lay is None:
            ids = [v.index for v in ((legacy(bm) if legacy else None) or [])]
            lay = bm.verts.layers.int.new(TAG)  # (a new layer: the vertices found before are stale, found again)
            bm.verts.ensure_lookup_table()
            old = [bm.verts[i] for i in ids]
            # Their order is the memory's, not the order made: each matched to the build's vertex at its place (a
            # housing whose shape changed since can't be, and is built anew).
            matched = False
            if len(old) == len(verts):
                tree = kdtree.KDTree(len(old))
                for i, v in enumerate(old):
                    tree.insert(v.co, i)
                tree.balance()
                match = [tree.find(co) for co in verts]
                if all(d is not None and d < 1e-4 for _, _, d in match) and len({i for _, i, _ in match}) == len(old):
                    old = [old[i] for _, i, _ in match]
                    matched = True
            for v in old:
                v[lay] = 1
        else:
            # The tag is the vertex's place in the build, 1 up (1 for all of them: the order the mesh has).
            old = sorted((v for v in bm.verts if v[lay] > 0), key=lambda v: (v[lay], v.index))
        lay_vert = bm.verts.layers.int.get("qvr_vert")
        lay_tri = bm.faces.layers.int.get("qvr_tri")
        lay_part = bm.faces.layers.int.get("qvr_part")
        lay_vid = bm.loops.layers.int.get("qvr_vid")
        lay_uv = bm.loops.layers.uv.active
        shapes = list(bm.verts.layers.shape.values())
        moved = matched and len(old) == len(verts)
        if old and not moved:
            bmesh.ops.delete(bm, geom=old, context='VERTS')
        self.moved_verts = 0
        if moved:
            # A vertex already on the grid's place for where it should be stays: moved within its grid step, the
            # export would carry that move into the other frames, rounded again on every run. The rest move (the
            # frames follow frame 0: the export carries the move into them).
            sc, org = self.grid
            for v, co in zip(old, verts):
                on = [org[i] + round((co[i] - org[i]) / sc[i]) * sc[i] for i in range(3)]
                if max(abs(v.co[i] - on[i]) for i in range(3)) > 1e-4:
                    v.co = co
                    self.moved_verts += 1
            for i, v in enumerate(old):
                v[lay] = i + 1
            bv = old
        else:
            bv = []
            for co in verts:
                v = bm.verts.new(co)
                v[lay] = len(bv) + 1
                if lay_vert is not None:
                    v[lay_vert] = -1
                if new_vert is not None:
                    new_vert(bm, v, len(bv))
                else:
                    for sl in shapes:
                        v[sl] = co
                bv.append(v)
        # The faces: those still the build's kept (their triangle in the file) and mapped again, the rest made anew.
        index = {v: i for i, v in enumerate(bv)}
        want = {}
        for t in tris:
            want[frozenset(t[:3])] = t
        kept, drop = 0, []
        if moved:
            for f in [f for f in bm.faces if all(v in index for v in f.verts)]:
                ids = [index[v] for v in f.verts]
                t = want.get(frozenset(ids))
                if t is None or len(ids) != 3 or ids[(ids.index(t[0]) + 1) % 3] != t[1]:
                    drop.append(f)
                    continue
                for loop in f.loops:
                    loop[lay_uv].uv = t[3][t[:3].index(index[loop.vert])]
                del want[frozenset(ids)]
                kept += 1
            for f in drop:
                bm.faces.remove(f)
        for t in tris:
            if frozenset(t[:3]) not in want:
                continue
            f = bm.faces.new((bv[t[0]], bv[t[1]], bv[t[2]]))
            f.material_index = 0
            f.smooth = smooth
            if lay_tri is not None:
                f[lay_tri] = -1
            if lay_part is not None:
                f[lay_part] = 0  # (the gun, not a moving part)
            for loop, uv in zip(f.loops, t[3]):
                loop[lay_uv].uv = uv
                if lay_vid is not None:
                    loop[lay_vid] = -1
        for e in [e for e in bm.edges if not e.link_faces and all(v in index for v in e.verts)]:
            bm.edges.remove(e)
        bm.to_mesh(me)
        me.update()
        bm.free()
        self.kept, self.made = kept, len(tris) - kept
        return moved

    def export(self, grow=False):
        """Writes the model's files as blend_export.py does (quakevr/progs, weapons.txt). Returns how many."""
        if self.dry:
            return 0
        source, _ = addon()
        try:
            written, changes = source.export(bpy.context, ROOT, None, grow=grow)
        except source.SourceError as e:
            self.fail("the export refused, nothing written:\n  " + str(e).replace("\n", "\n  "))
        for p in written:
            print("  wrote %s (%d bytes)" % (p, os.path.getsize(p)))
        if changes:
            print("  weapons.txt changed")
        self.exported += len(written)
        return len(written)

    def finish(self, summary, grow=False, after_export=None):
        """The last export, `after_export()` (files of its own to write), the .blend saved (in place or as --save), and
        the verdict line."""
        summary += "; %d vertices moved, %d triangles kept, %d made" % (self.moved_verts, self.kept, self.made)
        if self.dry:
            self.say("ok (--dry: nothing exported or saved), %s" % summary)
            return
        self.export(grow)
        if after_export is not None:
            after_export()
        bpy.context.preferences.filepaths.save_version = 0  # (no .blend1 beside it)
        bpy.ops.wm.save_as_mainfile(filepath=self.save, compress=True)
        self.say("ok, %s; exported %d files, saved %s" % (summary, self.exported, self.save))
