#!/usr/bin/env python3
# improve_weapons_alt.py -- the alternate view models kept in line with the normal ones (feedback
# round 18: "the changes only apply to the normal version ... otherwise it's inconsistent and the
# position also changes when you press the button").
#
# Five guns switch to another model when the button on them selects the secondary ammo (QC
# vr_weaponutil.qc WeaponIdToModel / SelectModelByFlags; each model has its own vr_weapons.inc slot):
#
#   nailgun            v_nail.mdl   (slot 3)  lava nails     v_lava.mdl   (slot 11)  improve_weapons2.py
#   super nailgun      v_nail2.mdl  (slot 4)  lava nails     v_lava2.mdl  (slot 12)  improve_weapons2.py
#   grenade launcher   v_rock.mdl   (slot 5)  multi grenades v_multi.mdl  (slot 13)  improve_weapons3.py
#   rocket launcher    v_rock2.mdl  (slot 6)  multi rockets  v_multi2.mdl (slot 14)  improve_weapons.py
#   lightning gun      v_light.mdl  (slot 7)  plasma         v_plasma.mdl (slot 15)  improve_weapons2.py
#
# (The knight's sword's v_hksword.mdl is the hell knight's sword, not an ammo toggle.)
#
# Measured (Misc/quakevr/src_models/ keeps the alternates as they were, byte for byte): each
# alternate is its normal model with another skin, re-exported:
# - v_lava.mdl, v_plasma.mdl: the same triangles, in the same order, at the same positions in every
#   frame; only the UVs and the skin differ (and so some vertices are split along more seams).
# - v_lava2.mdl: the same parts, re-quantised: the body and the hand's core 0.23 units further
#   forward (one quantisation step) and 0.03 higher and to the left; the barrels where they were.
#   Each barrel's outer face is cut in two: its back half carries a painted lava window (in the
#   barrels' shared texture), its front half a plain copy of the metal.
# - v_multi2.mdl: the same tube, re-quantised (0.04..0.09 units apart, 0.4 where a vertex rounded to
#   the next step), its underside cut in two; a hazard band painted round the tube.
# - v_multi.mdl was modelled apart (improve_weapons3.py builds it as its own launcher).
#
# So an alternate is built by the same builder as its normal model, from its own source: first moved
# by the offset between the two models' hand anchor vertices (so that its hand's model space is the
# normal one's: every part lands where it does on the normal gun, and the hand is at the same place),
# with the builder's vertex indices translated to the alternate's (VertexMap). Then its vr_weapons.inc
# slot is set up from the normal slot (align_slot): the same settings, but for the anchor indices
# (the engine's strip order of the alternate's own triangles), the hand offsets (from its own anchor
# vertex to the same hand) and the weapon offsets (compensating the header's origin, about which the
# weapon's Scale applies). Normal and alternate are then drawn in the same place, the hand on the same
# grip; switching the ammo only changes the paint.

import math
import os
import re

from improve_weapons import K, Mdl, fmt, strip_order

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "src_models")
INC = os.path.join(HERE, "..", "..", "Quake", "vr", "vr_weapons.inc")

ANCHORS = ["HandAnchorVertex", "MuzzleAnchorVertex", "TwoHHandAnchorVertex", "WpnButtonAnchorVertex",
           "WpnTextAnchorVertex"]


# ----------------------------------------------------------------------------
# Vertex correspondence (sources)

class VertexMap:
    """A normal model's vertex -> the alternate's vertices at the same place (more than one where the
    alternate splits a vertex along a seam). Calling it gives the first."""

    def __init__(self, groups):
        self.groups = groups

    def __call__(self, v):
        return self.groups[v][0]

    def all(self, v):
        return self.groups[v]


class Identity:
    def __call__(self, v):
        return v

    def all(self, v):
        return [v]


IDENTITY = Identity()


def _positions(model, f=0):
    return model.frames[f][1]


def _st(model, v):
    return tuple(model.st[v][1:])


def vertex_map(normal, alt):
    """Measured, not assumed: if the triangle lists correspond (the same count, each triangle's corners
    at the same places in frame 0), corner by corner; else each vertex to the alternate's vertex with
    the same UV nearest to it over every frame (a re-quantised copy of the same mesh), failing that the
    nearest one."""
    P, Q = _positions(normal), _positions(alt)
    groups = {}
    if len(normal.tris) == len(alt.tris) and all(
            max(math.dist(P[a], Q[b]) for a, b in zip(tn[1:], ta[1:])) < 1e-4 for tn, ta in zip(normal.tris, alt.tris)):
        for tn, ta in zip(normal.tris, alt.tris):
            for a, b in zip(tn[1:], ta[1:]):
                if b not in groups.setdefault(a, []):
                    groups[a].append(b)
        return VertexMap(groups)
    frames = range(min(len(normal.frames), len(alt.frames)))

    def far(v, w):
        return sum(math.dist(_positions(normal, f)[v], _positions(alt, f)[w]) for f in frames)

    by_st = {}
    for w in range(len(alt.st)):
        by_st.setdefault(_st(alt, w), []).append(w)
    for v in range(len(normal.st)):
        w = min(by_st.get(_st(normal, v), []) or range(len(alt.st)), key=lambda w: far(v, w))
        if far(v, w) > 0.5 * len(frames):  # re-mapped there (a cut face): the nearest vertex
            w = min(range(len(alt.st)), key=lambda w: far(v, w))
        groups[v] = [w]
    return VertexMap(groups)


def translate(model, d):
    """Moves a model by -d in every frame (its header's origin too, so that an unchanged box keeps
    every vertex's bytes)."""
    for frame in model.frames:
        frame[1] = [(p[0] - d[0], p[1] - d[1], p[2] - d[2]) for p in frame[1]]
    origin = [model.origin[k] - d[k] for k in range(3)]
    model.origin = tuple(origin) if isinstance(model.origin, tuple) else origin


def prepare(normal, alt, hand_vertex):
    """The alternate moved onto the normal model (by the offset between their hand anchor vertices),
    and the vertex map. Prints what was measured."""
    vmap = vertex_map(normal, alt)
    vmap.order = strip_order(normal.tris)  # the normal model's anchor indices -> its vertices
    d = tuple(_positions(alt)[vmap(hand_vertex)][k] - _positions(normal)[hand_vertex][k] for k in range(3))
    translate(alt, d)
    P, Q = _positions(normal), _positions(alt)
    far = max(math.dist(P[v], Q[w]) for v in vmap.groups for w in vmap.all(v))
    print("  alternate: %d -> %d vertices mapped, moved by (%s), then at most %.3f units from the normal one"
          % (len(normal.st), len(alt.st), ", ".join("%.3f" % x for x in d), far))
    return vmap


# ----------------------------------------------------------------------------
# Slot settings (outputs)

def slot_defaults():
    slots = {}
    for slot, key, value in re.findall(r'QVR_WEAPON_DEFAULT\((\d+), (\w+), "([^"]*)"\)', open(INC).read()):
        slots.setdefault(int(slot), {})[key] = value
    return slots


def _num(x):
    return fmt(x) if abs(x) > 5e-7 else "0"


def align_slot(out_dir, normal_name, alt_name, normal_slot, alt_slot, normal_changes, vmap, counts):
    """The alternate's vr_weapons.inc slot from the normal one (its defaults, with `normal_changes`,
    the settings its builder printed this run): the same, but the anchor indices, the hand offsets and
    the weapon offsets, so that the two models are drawn in the same place with the hand at the same
    place. `vmap` and `counts` (the two sources' vertex counts): the builders keep the old vertices'
    indices and append the same new parts in the same order. Prints the lines that change."""
    slots = slot_defaults()
    n = dict(slots[normal_slot])
    n.update(normal_changes or {})
    old = slots[alt_slot]
    N = Mdl(open(os.path.join(out_dir, normal_name), "rb").read())
    A = Mdl(open(os.path.join(out_dir, alt_name), "rb").read())
    assert len(A.st) - counts[1] >= len(N.st) - counts[0], "not the same parts"
    on, oa = strip_order(N.tris), strip_order(A.tris)
    frames = range(min(len(N.frames), len(A.frames)))

    def match(v):
        """The alternate's vertex that is the normal one's: the same old vertex, or the same new one."""
        cands = vmap.all(v) if v < counts[0] else [v - counts[0] + counts[1]]
        return min(cands, key=lambda w: sum(math.dist(N.frames[f][1][v], A.frames[f][1][w]) for f in frames))

    new = {k: v for k, v in n.items() if k != "ID"}
    report = []
    for key in ANCHORS:
        v = on[int(float(n[key]))]
        w = match(v)
        new[key] = str(oa.index(w))
        report.append("%s %s -> %s (%.2f, %.2f)" % (key[:-12] or "Hand", n[key], new[key], math.dist(N.frames[0][1][v], A.frames[0][1][w]),
                                                   max(math.dist(N.frames[f][1][v], A.frames[f][1][w]) for f in frames)))

    sw = float(n["Scale"])
    hand_n = N.frames[0][1][on[int(float(n["HandAnchorVertex"]))]]
    hand_a = A.frames[0][1][oa[int(new["HandAnchorVertex"])]]
    offs_n = [float(n["HandOffset" + c]) for c in "XYZ"]
    p0 = [hand_n[k] + offs_n[k] / (K * sw) for k in range(3)]
    for k, c in enumerate("XYZ"):
        new["HandOffset" + c] = _num((p0[k] - hand_a[k]) * K * sw)
        # The weapon's Scale applies about the header's origin: drawn at Offset + Scale p + (1 - Scale) origin.
        new["Offset" + c] = _num(float(n["Offset" + c]) + (1 - sw) * (N.origin[k] - A.origin[k]))
    print("  %s (slot %d) aligned on %s (slot %d); anchors (units apart in frame 0, in any frame):"
          % (alt_name, alt_slot, normal_name, normal_slot))
    print("    " + "; ".join(report))
    changed = {}
    for key, value in new.items():
        if key in old and value != old[key]:
            try:
                if abs(float(value) - float(old[key])) < 5e-7:
                    continue
            except ValueError:
                pass
            changed[key] = value
            print('    QVR_WEAPON_DEFAULT(%d, %s, "%s") // was %s' % (alt_slot, key, value, old[key]))
    return changed


# ----------------------------------------------------------------------------
# The rocket launcher's alternate (improve_weapons.py builds the others' in its main)

RL_HAND_ANCHOR = 12  # v_rock2.mdl's hand anchor index (improve_weapons.py's build_rocket_launcher)


def rocket_launcher(out_dir, normal_settings):
    """v_multi2.mdl: improve_weapons.py's rocket launcher parts (the grip, the guard, the trigger, the
    back-blast nozzle and its open mouth) on rogue's multi-rocket launcher, placed as v_rock2.mdl."""
    import improve_weapons

    normal = Mdl(open(os.path.join(SRC, "v_rock2.mdl"), "rb").read())
    alt = Mdl(open(os.path.join(SRC, "v_multi2.mdl"), "rb").read())
    print("v_multi2.mdl (the alternate of v_rock2.mdl):")
    counts = (len(normal.st), len(alt.st))
    vmap = prepare(normal, alt, strip_order(normal.tris)[RL_HAND_ANCHOR])
    _, _, model = improve_weapons.build_rocket_launcher(out_dir, alt=(alt, "v_multi2.mdl"))
    print("  %d vertices, %d triangles, %d frames" % (len(model.st), len(model.tris), len(model.frames)))
    return align_slot(out_dir, "v_rock2.mdl", "v_multi2.mdl", 6, 14, normal_settings, vmap, counts)
