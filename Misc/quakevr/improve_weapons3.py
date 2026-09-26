#!/usr/bin/env python3
# improve_weapons3.py -- grips, trigger guards and triggers for the rest of Quake VR's guns held in
# one hand (round 16's voice notes: "the weapon models look a bit amateurish"; "a handle"; "the
# trigger guard or something, a little bit more detail, without changing the art style"):
#
#   quakevr/progs/v_rock.mdl    the grenade launcher: its belly (the drum under the tube) sat where
#   quakevr/progs/v_prox.mdl    a trigger guard goes, and the fist held the sloped back corner of
#   quakevr/progs/v_multi.mdl   the tube. Now a frame under the back of the tube carries a pistol
#                               grip behind the belly (ribbed, a butt plate), a trigger and a guard
#                               whose bar runs from the grip into the belly's back face (the belly
#                               is the guard's front). The gun sits a little higher and further
#                               forward over the hand. The proximity gun (hipnotic) is the same
#                               model with a red skin: the same parts, in its reds; rogue's
#                               multi-grenade launcher is the same model again (a few tenths of a
#                               unit apart): the same parts, in the launcher's browns.
#   quakevr/progs/v_shot.mdl    the shotgun: the fist held a thin stub of a grip that leaned back
#                               behind the fingers. A proper pistol grip (ribbed like the pump) is
#                               laid out in the fist, with a trigger and a trigger guard; the stub
#                               folds into the grip's back (a small tang under the receiver). The
#                               sights and the gun are untouched, the hand stays where it was.
#   quakevr/progs/v_laserg.mdl  the laser cannon (hipnotic): held by a thin kinked blade rising from
#                               the back of the body. Now a spade grip: a head over the fist (its
#                               front overhanging the index finger, the trigger under it), a thick
#                               knurled grip through the fist, a neck down into the body, and a
#                               trigger guard. The blade folds into the neck. The gun and the hand
#                               stay where they were.
#
# Mjolnir (v_hammer.mdl) is left alone: the hand holds its shaft.
#
# Usage: python Misc/quakevr/improve_weapons3.py [output progs folder]   (default: quakevr/progs)
#
# The inputs are Quake VR's models as they were before (Misc/quakevr/src_models/, byte for byte):
# running it again gives the same files. Pure Python; the machinery is improve_weapons.py's (the
# model reader and writer, the strip-order port of vr_anchor.cpp, the rigid carrier, the lofts, the
# grip laid out in the drawn fist's space, the palette painters).
#
# Rules the edits keep (as improve_weapons.py and improve_weapons2.py):
# - Every frame is kept; the new parts follow the gun rigidly through them (a frame from three
#   clusters of the body's vertices, per frame).
# - Old triangles and vertex indices are kept (new ones are appended, sharing no vertex with the old
#   ones), so every anchor index (vr_weapons.inc's *_av, vr_shells.cpp's v_shot entry) still names
#   the same vertex: checked against the engine's strip order. Folded-away parts only move their
#   vertices.
# - The skins grow downwards for the new parts' texels (every old UV and texel stays), painted in
#   each gun's own ramps; no fullbright index (224 and up) is used: the shotgun's sight texels
#   (vr_sights.cpp recolours the fullbright indices on its skin) stay the only ones.
# - The bounds grow (the grips hang lower), moving the origin the weapon Scale is applied about:
#   the weapon offsets are compensated so the old parts stay where they were drawn (and, where the
#   gun moves over the hand, move by exactly that). The script prints the new vr_weapons.inc values.

import math
import os
import sys

from improve_weapons import (CEILING, GRIP_LEAN, GRIP_PROFILE, GRIP_Y, GUARD_PATH, K, SHOTGUN_FLASH_FRAMES,
                             TRIGGER_PATH, Carrier, HandSpace, Mdl, Noise, Parts, assemble, centroid, check_anchors,
                             edge, flame, flame_paint, fmt, grip, grip_x, metal, paint_glow, pick, ribbed, section,
                             show_flash, strip_order, sweep)
from mdlgen import add, cross, dot, mul, norm, sub

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "src_models")

BLUE = [32, 33, 34, 35, 36, 37, 38, 39]                        # the guns' blue-black metal
STEEL = [0, 1, 2, 3, 4, 5, 6, 7, 8]
BROWN = [16, 174, 17, 173, 18, 19, 172, 20, 171, 170, 169]     # the launcher's and the pump's browns
GRIPBROWN = [16, 174, 17, 173, 18, 19, 172, 20]
PROXRED = [64, 65, 142, 66, 141, 67, 68, 69, 70]                # the proximity gun's dark reds
LASER = [16, 174, 17, 173, 18, 19, 172, 20]                    # the laser cannon's browns
LASER_DARK = [49, 16, 32, 174, 33, 17, 34]


def grow_skin(model, rows):
    """Adds `rows` rows under the skin; returns the first new row."""
    first = model.sh
    model.skin += bytes(model.sw * rows)
    model.sh += rows
    model.h[14] = model.sh
    return first


def paint(model, region, fn):
    s0, t0, s1, t1 = region
    for t in range(t0, t1):
        for s in range(s0, s1):
            v = fn(s - s0, t - t0, s1 - s0, t1 - t0)
            assert v < 224, "no fullbright texels"
            model.skin[t * model.sw + s] = v


def knurled(noise, ramp):
    """A diamond knurl (the laser cannon's old checkered handle), darker round the edges."""
    def fn(s, t, w, h):
        a, b = (s + t) % 4, (s - t) % 4
        v = 0.45 + (noise.hash(s, t) - 0.5) * 0.12
        if a == 0 or b == 0:
            v = 0.12
        elif a == 1 and b == 1:
            v = 0.7
        v *= 0.7 + 0.3 * edge(t, h, 2.0)
        return pick(ramp, v, s, t)
    return fn


def rsec(x, yc, hw, zb, zt, ch):
    """A bevelled rectangle square to x (a box's section)."""
    return section((x, yc, (zb + zt) / 2), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0), (zt - zb) / 2, hw, ch)


def settings_for(slot_name, model, old_origin, origin, sw, offset, p0_old, p0_new, anchor_old, anchor_new=None,
                 hand_anchor=None):
    """The slot's new defaults: the hand at p0_new (anchor + HandOffset / (k Sw)), the weapon offsets
    keeping the gun where it was drawn relative to the hand's old place (the header's origin moved,
    and the gun moved by p0_old - p0_new under the hand)."""
    anchor = anchor_new if anchor_new is not None else anchor_old
    hand = mul(sub(p0_new, anchor), K * sw)
    new_offset = [offset[k] + (old_origin[k] - origin[k]) * (1 - sw) + (p0_old[k] - p0_new[k]) * sw
                  for k in range(3)]
    out = {}
    if hand_anchor is not None:
        out["HandAnchorVertex"] = str(hand_anchor)
    if p0_new != p0_old or anchor_new is not None:
        out.update({"HandOffsetX": fmt(hand[0]), "HandOffsetY": fmt(hand[1]), "HandOffsetZ": fmt(hand[2])})
    out.update({"OffsetX": fmt(new_offset[0]), "OffsetY": fmt(new_offset[1]), "OffsetZ": fmt(new_offset[2])})
    return slot_name, out


def pin(model, remap, carrier, targets):
    """Moves old vertices (frame 0 targets) and carries them through the frames."""
    for v, p in targets.items():
        for f, frame in enumerate(model.frames):
            frame[1][remap[v]] = carrier.place(f, p)


# ----------------------------------------------------------------------------
# The grenade launcher (and the proximity gun and the multi-grenade launcher: the same model)

GL_GUARD_FRONT = 1.3    # where the belly's back face goes in the hand's space (x)
GL_YC = -0.03           # the tube's middle across
# The guard in the hand's space: from the grip's front round under the index finger and forward
# into the belly's back face, near its bottom.
GL_GUARD_PATH = [(-0.55, -1.90), (0.05, -2.06), (0.7, -2.0), (1.18, -1.86), (1.6, -1.62)]


class Launcher:
    """One of the three launchers: its vr_weapons.inc settings before this and its geometry (frame 0).
    They were modelled apart and differ by a few tenths of a unit: rogue's multi-grenade launcher's
    belly's back face leans (x 3.49 at the tube, 3.79 at its bottom), its tube is 0.06 higher."""

    def __init__(self, name, slot, grip_ramp, seed, offset, kept, belly_back, belly_top, frame_bottom,
                 tube_back, belly_front):
        self.__dict__.update(locals())


LAUNCHERS = [
    Launcher("v_rock.mdl", 5, GRIPBROWN, 60, (11.100024, 2.25, -1.850028),
             {"hand, wpnbtn, wpntxt": 33, "2H hand": 15, "muzzle": 0},
             belly_back=3.67, belly_top=3.67, frame_bottom=2.0, tube_back=-1.52, belly_front=18.3),
    Launcher("v_prox.mdl", 10, PROXRED, 70, (11.099998, 2.25, -1.85),
             {"hand, wpnbtn": 33, "2H hand": 15, "muzzle": 0, "wpntxt": 50},
             belly_back=3.67, belly_top=3.67, frame_bottom=2.0, tube_back=-1.52, belly_front=18.3),
    Launcher("v_multi.mdl", 13, GRIPBROWN, 80, (11.000022, 2.25, -1.950028),
             {"hand, wpnbtn, wpntxt": 33, "2H hand": 15, "muzzle": 0},
             belly_back=3.75, belly_top=3.49, frame_bottom=2.06, tube_back=-1.38, belly_front=18.09),
]


def build_launcher(out_dir, gl):
    model = Mdl(open(os.path.join(SRC, gl.name), "rb").read())
    old_tris = list(model.tris)
    old_origin = model.origin
    frame0 = model.frames[0][1]

    # vr_weapons.inc (the same hand for the three guns before this)
    sw = 0.38
    hand_av, hand_ofs = 33, (0.4, -1.5, 2.0)

    order = strip_order(old_tris)
    anchor = frame0[order[hand_av]]
    p0_old = add(anchor, mul(hand_ofs, 1.0 / (K * sw)))
    # The fist goes under the frame, its guard's front at the belly's back face (where the guard
    # meets it).
    p0_new = (gl.belly_back - GL_GUARD_FRONT / sw, GL_YC - GRIP_Y / sw, gl.frame_bottom - CEILING / sw)
    hs = HandSpace(p0_new, sw, y_centre=GL_YC)

    row = grow_skin(model, 48)
    R = {"grip": (0, row, 96, row + 32), "butt": (96, row, 160, row + 10), "bottom": (96, row + 10, 128, row + 32),
         "guard": (160, row, 256, row + 12), "trigger": (160, row + 12, 208, row + 24),
         "frame": (256, row, 384, row + 40), "frameend": (384, row, 416, row + 24)}
    seed = gl.seed
    paint(model, R["grip"], ribbed(Noise(seed), gl.grip_ramp, 4))
    paint(model, R["butt"], metal(Noise(seed + 1), BLUE, 0.45, 0.12))
    paint(model, R["bottom"], metal(Noise(seed + 2), BLUE, 0.35, 0.12, lines=4))
    paint(model, R["guard"], metal(Noise(seed + 3), BLUE, 0.4, 0.1, grain=0.04))
    paint(model, R["trigger"], metal(Noise(seed + 4), STEEL, 0.5, 0.1))
    paint(model, R["frame"], metal(Noise(seed + 5), BLUE, 0.35, 0.1, grain=0.04))
    paint(model, R["frameend"], metal(Noise(seed + 6), BLUE, 0.28, 0.08))

    parts = Parts()
    grip(parts, hs, R["grip"], R["butt"], R["bottom"], GRIP_PROFILE)
    k = 1.0 / sw
    sweep(parts, [hs.m(x, z) for x, z in GL_GUARD_PATH], 0.09 * k, 0.26 * k, R["guard"])
    sweep(parts, [hs.m(x, z) for x, z in TRIGGER_PATH], 0.075 * k, 0.11 * k, R["trigger"])

    # The frame: under the back of the tube (its sloped underside rises to the pointed back), from
    # the belly's back face to over the grip's back strap; its back end slopes up into the tube.
    zb = gl.frame_bottom
    parts.loft([rsec(-3.3, GL_YC, 1.0, 4.1, 5.05, 0.35), rsec(-2.5, GL_YC, 1.75, zb, 5.05, 0.55),
                rsec(gl.belly_top + 0.2, GL_YC, 2.1, zb, 4.2, 0.6)], R["frame"], cap_start=R["frameend"])

    # Carried by the belly's back face, the belly's front end and the tube's back top.
    body = range(len(frame0))
    clusters = [[v for v in body if gl.belly_top - 0.05 < frame0[v][0] < gl.belly_back + 0.05 and frame0[v][2] < 2.5],
                [v for v in body if abs(frame0[v][0] - gl.belly_front) < 0.3 and frame0[v][2] < 2.5],
                [v for v in body if abs(frame0[v][0] - gl.tube_back) < 0.05 and frame0[v][2] > 4.5]]
    assert all(clusters), [len(c) for c in clusters]
    carrier = Carrier(model, clusters)
    remap = assemble(model, old_tris, parts, carrier)
    check_anchors(old_tris, model.tris, remap, gl.kept)

    origin = model.write(os.path.join(out_dir, gl.name))
    return settings_for("%s (slot %d)" % (gl.name, gl.slot), model, old_origin, origin, sw, gl.offset, p0_old,
                        p0_new, anchor), model


# ----------------------------------------------------------------------------
# The shotgun

SHOT_Y = 0.04   # the grip's middle across (where the hand already is: the receiver's middle)


def build_shotgun(out_dir):
    model = Mdl(open(os.path.join(SRC, "v_shot.mdl"), "rb").read())
    old_tris = list(model.tris)
    old_origin = model.origin
    frame0 = model.frames[0][1]

    sw = 0.44  # vr_weapons.inc slot 1
    hand_av, hand_ofs = 165, (2.399994, 0.2, 0.7)
    offset = (0.950001, 1.350002, -0.050021)
    kept = {"hand": 165, "2H hand": 48, "muzzle": 1, "wpnbtn": 0, "wpntxt": 159, "shells (vr_shells.cpp)": 70}

    order = strip_order(old_tris)
    anchor = frame0[order[hand_av]]
    p0 = add(anchor, mul(hand_ofs, 1.0 / (K * sw)))
    hs = HandSpace(p0, sw, y_centre=p0[1] + GRIP_Y / sw)
    assert abs(hs.p0[1] - p0[1]) < 1e-9

    # The old stub of a grip: the receiver's vertices below z 0 (only its triangles use them).
    stub = {v for v in range(len(frame0)) if frame0[v][2] < 0.0}
    assert len(stub) == 26, len(stub)

    row = grow_skin(model, 40)
    R = {"grip": (0, row, 64, row + 32), "butt": (64, row, 112, row + 8), "bottom": (64, row + 8, 88, row + 32),
         "guard": (88, row + 8, 128, row + 20), "trigger": (88, row + 20, 128, row + 32),
         "tail": (0, row + 32, 64, row + 40)}
    paint(model, R["grip"], ribbed(Noise(40), BROWN[:8], 4))
    paint(model, R["butt"], metal(Noise(41), BLUE, 0.45, 0.12))
    paint(model, R["bottom"], metal(Noise(42), BLUE, 0.35, 0.12, lines=4))
    paint(model, R["guard"], metal(Noise(43), BLUE, 0.4, 0.1, grain=0.04))
    paint(model, R["trigger"], metal(Noise(44), STEEL, 0.5, 0.1))

    parts = Parts()
    grip(parts, hs, R["grip"], R["butt"], R["bottom"], GRIP_PROFILE)
    k = 1.0 / sw
    sweep(parts, [hs.m(x, z) for x, z in GUARD_PATH], 0.09 * k, 0.26 * k, R["guard"])
    sweep(parts, [hs.m(x, z) for x, z in TRIGGER_PATH], 0.075 * k, 0.11 * k, R["trigger"])
    round18 = shotgun_round18_parts(model, parts, frame0)

    # Carried by the receiver's back end, the barrel's muzzle and the receiver's top.
    rec = range(len(frame0))
    clusters = [[v for v in rec if frame0[v][0] < 1.5 and frame0[v][2] > 1.0],
                [v for v in rec if frame0[v][0] > 32.0],
                [v for v in rec if 3.0 < frame0[v][0] < 10.0 and frame0[v][2] > 6.0]]
    carrier = Carrier(model, clusters)
    remap = assemble(model, old_tris, parts, carrier)
    check_anchors(old_tris, model.tris, remap, kept)

    # The stub folds into the grip: its lower end goes inside the grip's back, leaving a small tang
    # from the receiver's underside down into the back strap.
    inside = hs.m(grip_x(-1.5) - 0.35, -1.5)
    pin(model, remap, carrier, {v: (inside[0], inside[1] + (frame0[v][1] - SHOT_Y) * 0.3, inside[2]) for v in stub})
    shotgun_round18_frames(model, len(remap), parts, carrier, round18)

    origin = model.write(os.path.join(out_dir, "v_shot.mdl"))
    return settings_for("v_shot.mdl (slot 1)", model, old_origin, origin, sw, offset, p0, p0, anchor), model


# ----------------------------------------------------------------------------
# The shotgun, round 18 (voice notes 14-30-05 and 14-32-01): an ejection port on the receiver's right
# side, where vr_shells.cpp throws the casings out; the pump's painted grooves made real; a muzzle
# flash. All new triangles, after the old ones and the grip (no anchor moves).

# The pump (frame 0): a faceted tube under the barrel from x 19 to 30.2, painted with three dark
# grooves round its sides and bottom (skin t 32..37, 49..54, 66..70; t runs back along x). Its
# lengthwise edges, back vertex to front vertex, from the right side's top corner round the bottom
# to the left side's: the upper corner (it runs on to the barrel's mouth ring), the side, the lower
# bevel, the bottom.
PUMP_EDGES = [(20, 0), (16, 17), (12, 15), (14, 13), (18, 19), (22, 24), (30, 31), (28, 7)]
PUMP_S = [87, 104, 110, 116, 119, 116, 110, 104, 87]          # the skin's s along that section
PUMP_GROOVES = [(21.38, 0.9), (24.41, 0.9), (27.52, 0.9)]     # the grooves: middle x, width
PUMP_X = (19.1, 30.1)                                          # the ribs' ends


def pump_t(x):
    """The pump's skin t at x (t 20 at x 30.17, t 81 at x 19.0)."""
    return 20.0 + (30.17 - x) / (11.17 / 61.0)


def pump_point(pos, k, x, x0):
    """Section point k of the pump at frame-0 x (`x0`: frame 0's positions, for the fractions) in the
    frame whose positions are `pos`. Point 4 is the middle of the bottom."""
    def on_edge(e):
        a, b = PUMP_EDGES[e]
        f = (x - x0[a][0]) / (x0[b][0] - x0[a][0])
        return add(pos[a], mul(sub(pos[b], pos[a]), f))
    if k == 4:
        return mul(add(on_edge(3), on_edge(4)), 0.5)
    return on_edge(k if k < 4 else k - 1)


def pump_section(pos, x, x0, d):
    """The pump's section at x pushed out by d (the ribs' outer face), its axis and its faces'
    outward normals."""
    pts = [pump_point(pos, k, x, x0) for k in range(9)]
    a0 = centroid([pos[a] for a, _ in PUMP_EDGES])
    a1 = centroid([pos[b] for _, b in PUMP_EDGES])
    axis = norm(sub(a1, a0))
    centre = centroid(pts)
    faces = []
    for k in range(8):
        n = norm(cross(axis, sub(pts[k + 1], pts[k])))
        if dot(n, sub(mul(add(pts[k], pts[k + 1]), 0.5), centre)) < 0:
            n = mul(n, -1.0)
        faces.append(n)
    out = []
    for k in range(9):
        adj = [faces[j] for j in (k - 1, k) if 0 <= j < 8]
        n = norm(tuple(map(sum, zip(*adj))))
        miter = 1.0 / max(0.5, dot(n, adj[0]))
        out.append(add(pts[k], mul(n, d * miter)))
    return out, axis, faces


RIB_HEIGHT = 0.4   # how far the ribs stand out of the pump (the grooves' depth)
RIB_SINK = 0.05    # how far their walls reach into it (no gap at the foot)


def pump_ribs(parts, x0):
    """Ribs round the pump between its painted grooves (and to its ends), so that the grooves are
    cuts between them. Their outsides take the pump's own texels at the same place, their walls the
    grooves' dark edge rows. Returns [(parts vertex, (k, x, offset))] for `follow_pump`."""
    track = []
    edges = [PUMP_X[0]] + [g for x, w in PUMP_GROOVES for g in (x - w / 2, x + w / 2)] + [PUMP_X[1]]
    ribs = [(edges[i], edges[i + 1]) for i in range(0, len(edges), 2)]

    def v(k, x, off, s, t):
        i = parts.vert(pump_section(x0, x, x0, off)[0][k], s, t)
        track.append((i, (k, x, off)))
        return i

    for xa, xb in ribs:
        mid, axis, faces = pump_section(x0, (xa + xb) / 2, x0, 0.0)
        ta, tb = pump_t(xa), pump_t(xb)
        for k in range(8):
            # The outside, textured as the pump there.
            a, b = v(k, xa, RIB_HEIGHT, PUMP_S[k], ta), v(k + 1, xa, RIB_HEIGHT, PUMP_S[k + 1], ta)
            c, d = v(k + 1, xb, RIB_HEIGHT, PUMP_S[k + 1], tb), v(k, xb, RIB_HEIGHT, PUMP_S[k], tb)
            parts.tri(a, b, c, faces[k])
            parts.tri(a, c, d, faces[k])
            # The walls, back and front: the grooves' edge rows.
            for x, t, out in ((xa, ta + 0.8, mul(axis, -1.0)), (xb, tb - 0.8, axis)):
                a, b = v(k, x, -RIB_SINK, PUMP_S[k], t), v(k + 1, x, -RIB_SINK, PUMP_S[k + 1], t)
                c, d = v(k + 1, x, RIB_HEIGHT, PUMP_S[k + 1], t), v(k, x, RIB_HEIGHT, PUMP_S[k], t)
                parts.tri(a, b, c, out)
                parts.tri(a, c, d, out)
        # The ends at the pump's top corners.
        for k, j in ((0, 1), (8, 7)):
            out = norm(sub(mid[k], mid[j]))
            a, b = v(k, xa, -RIB_SINK, PUMP_S[k], ta), v(k, xa, RIB_HEIGHT, PUMP_S[k], ta)
            c, d = v(k, xb, RIB_HEIGHT, PUMP_S[k], tb), v(k, xb, -RIB_SINK, PUMP_S[k], tb)
            parts.tri(a, b, c, out)
            parts.tri(a, c, d, out)
    return track


def follow_pump(model, base, track):
    """The ribs in every frame: on the pump's surface as that frame has it."""
    x0 = model.frames[0][1][:]
    for frame in model.frames:
        pos = frame[1]
        cache = {}
        for i, (k, x, off) in track:
            if (x, off) not in cache:
                cache[(x, off)] = pump_section(pos, x, x0, off)[0]
            pos[base + i] = cache[(x, off)][k]


# The ejection port: on the receiver's upper right face (the plane of its vertices 52, 53, 54),
# round the point vr_shells.cpp's port is over, a raised, chamfered bezel round a dark opening with
# the bolt's steel face at its back.
PORT_CENTRE = (14.5, 4.55)      # x, z
PORT_HALF = (1.6, 0.55)         # the opening's half length and height, in the face
PORT_BEZEL = (0.28, 0.3, 0.3)   # the bezel's width at its top, its chamfer, its height


def ejection_port(parts, x0, regions):
    a, b, c = x0[52], x0[53], x0[54]
    n = norm(cross(sub(c, a), sub(b, a)))
    if n[1] > 0:
        n = mul(n, -1.0)              # out of the right side
    ex = norm(sub((1.0, 0.0, 0.0), mul(n, n[0])))
    ez = cross(ex, n)
    if ez[2] < 0:
        ez = mul(ez, -1.0)
    # The plane's point at the port's x and z.
    x, z = PORT_CENTRE
    guess = (x, a[1], z)
    centre = sub(guess, mul(n, dot(sub(guess, a), n)))
    centre = add(centre, mul(ez, (z - centre[2]) / ez[2]))

    def rect(hx, hz, h):
        c = add(centre, mul(n, h))
        return [add(c, add(mul(ex, u * hx), mul(ez, w * hz))) for u, w in ((1, -1), (1, 1), (-1, 1), (-1, -1))]

    ox, oz = PORT_HALF
    wtop, chamfer, height = PORT_BEZEL
    foot = rect(ox + wtop + chamfer, oz + wtop + chamfer, -0.05)
    top_out = rect(ox + wtop, oz + wtop, height)
    top_in = rect(ox, oz, height)
    floor = rect(ox, oz, 0.03)
    s0, t0, s1, t1 = regions["port"]
    for ring_a, ring_b, kind in ((foot, top_out, "chamfer"), (top_out, top_in, "top"), (top_in, floor, "wall")):
        for k in range(4):
            p = [ring_a[k], ring_a[(k + 1) % 4], ring_b[(k + 1) % 4], ring_b[k]]
            radial = sub(centroid(p), centre)
            radial = sub(radial, mul(n, dot(radial, n)))
            out = {"chamfer": add(norm(radial), n), "top": n, "wall": mul(radial, -1.0)}[kind]
            ids = [parts.vert(q, s0 + 1 + (s1 - s0 - 2) * (j in (1, 2)), t0 + 1 + (t1 - t0 - 2) * (j in (2, 3)))
                   for j, q in enumerate(p)]
            parts.tri(ids[0], ids[1], ids[2], out)
            parts.tri(ids[0], ids[2], ids[3], out)
    s0, t0, s1, t1 = regions["portin"]
    ids = [parts.vert(q, s0 + 0.5 + (s1 - s0 - 1) * (u > 0), t0 + 0.5 + (t1 - t0 - 1) * (w > 0))
           for q, (u, w) in zip(floor, ((1, -1), (1, 1), (-1, 1), (-1, -1)))]
    parts.tri(ids[0], ids[1], ids[2], n)
    parts.tri(ids[0], ids[2], ids[3], n)
    return centre, n


def port_inside(noise):
    """The opening: black inside, the bolt's steel face across its back (small s: its rear end)."""
    def fn(s, t, w, h):
        u = (s + 0.5) / w
        if u < 0.3:
            v = 0.45 + 0.15 * (noise.hash(s, t) - 0.5) + (0.2 if t == h // 2 else 0.0)
            return pick(STEEL, v * (0.7 + 0.3 * edge(t, h, 1.5)), s, t)
        return pick([0, 32, 1, 33], 0.2 + 0.2 * (noise.hash(s, t) - 0.5), s, t)
    return fn


SHOT_MUZZLE = (32.9, 0.02, 4.23)   # the bore's middle at the muzzle (frame 0)


def shotgun_round18_parts(model, parts, x0):
    row = grow_skin(model, 24)
    regions = {"port": (0, row, 48, row + 8), "portin": (48, row, 80, row + 8), "flash": (0, row + 8, 64, row + 24)}
    paint(model, regions["port"], metal(Noise(45), BLUE, 0.3, 0.08, grain=0.03))
    paint(model, regions["portin"], port_inside(Noise(46)))
    paint_glow(model.skin, model.sw, regions["flash"], flame_paint(Noise(47)))
    ribs = pump_ribs(parts, x0)
    port = ejection_port(parts, x0, regions)
    flash = flame(parts, SHOT_MUZZLE, (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), 17.0, 4.0, regions["flash"])
    return ribs, port, flash


def shotgun_round18_frames(model, base, parts, carrier, parts18):
    ribs, _, flash = parts18
    follow_pump(model, base, ribs)
    show_flash(model, base, flash, parts, SHOT_MUZZLE, carrier, SHOTGUN_FLASH_FRAMES)


# ----------------------------------------------------------------------------
# The laser cannon

LASER_Y = 0.42  # where the hand is across (the blade's middle)
# The blade the hand held: the body's back top rising into a kinked post (its vertices above z 0).
# The spade grip in the hand's space: the head over the fist (x, z bottom, z top, half width), the
# grip down the fist's axis (GRIP_PROFILE's rings without the flared butt), then the neck down and
# forward into the body's back (hand space x, z, half depth, half width).
LASER_HEAD = [(-2.45, -0.70, -0.30, 0.42), (-1.85, -0.82, 0.12, 0.62), (1.05, -0.82, 0.12, 0.62),
              (1.65, -0.66, -0.06, 0.48)]
LASER_NECK = [(-1.62, -3.35, 0.78, 0.55), (-1.1, -4.05, 0.72, 0.52), (0.3, -4.45, 0.66, 0.48),
              (1.3, -4.75, 0.62, 0.45)]


def build_laser(out_dir):
    model = Mdl(open(os.path.join(SRC, "v_laserg.mdl"), "rb").read())
    old_tris = list(model.tris)
    old_origin = model.origin
    frame0 = model.frames[0][1]

    sw = 0.34  # vr_weapons.inc slot 9
    hand_av, hand_ofs = 4, (-16.200041, 1.9, 10.3)
    offset = (-1.4, 5.9, 12.299991)
    kept = {"hand, 2H hand": 4, "muzzle": 22, "wpnbtn": 0, "wpntxt": 226}

    order = strip_order(old_tris)
    anchor = frame0[order[hand_av]]
    p0 = add(anchor, mul(hand_ofs, 1.0 / (K * sw)))
    hs = HandSpace(p0, sw, y_centre=p0[1] + GRIP_Y / sw)

    blade = {v for v in range(len(frame0)) if frame0[v][2] > 0.0 and frame0[v][0] < 6.0}
    assert len(blade) == 13, sorted(blade)

    row = grow_skin(model, 48)
    R = {"grip": (0, row, 96, row + 48), "head": (96, row, 224, row + 32), "headend": (224, row, 256, row + 24),
         "neck": (256, row, 352, row + 32), "guard": (352, row, 448, row + 12), "trigger": (352, row + 12, 400, row + 24),
         "neckend": (400, row + 12, 432, row + 36)}
    paint(model, R["grip"], knurled(Noise(50), [16, 174, 173, 172, 171, 170]))
    paint(model, R["head"], metal(Noise(51), LASER, 0.45, 0.12, lines=8))
    paint(model, R["headend"], metal(Noise(52), LASER, 0.35, 0.1))
    paint(model, R["neck"], metal(Noise(53), LASER, 0.4, 0.12))
    paint(model, R["guard"], metal(Noise(54), LASER_DARK, 0.5, 0.12, grain=0.04))
    paint(model, R["trigger"], metal(Noise(55), STEEL, 0.5, 0.1))
    paint(model, R["neckend"], metal(Noise(56), LASER, 0.3, 0.1))

    parts = Parts()
    k = 1.0 / sw
    ex = (math.cos(GRIP_LEAN), 0.0, -math.sin(GRIP_LEAN))
    ey = (0.0, 1.0, 0.0)

    # The grip and the neck: one loft, rings square to the path (the grip's lean, then the neck's
    # direction), from inside the head down to inside the body.
    rings = [section(hs.m(grip_x(z), z), ex, ey, hd * k, hw * k, ch * k) for z, hd, hw, ch in GRIP_PROFILE[:4]]
    parts.loft(rings, R["grip"])
    last = GRIP_PROFILE[3]
    path = [(grip_x(last[0]), last[0])] + [(x, z) for x, z, _, _ in LASER_NECK]
    sizes = [(last[1], last[2])] + [(hd, hw) for _, _, hd, hw in LASER_NECK]
    neck = []
    for i, ((x, z), (hd, hw)) in enumerate(zip(path, sizes)):
        a, b = path[max(0, i - 1)], path[min(len(path) - 1, i + 1)]
        d = norm((b[0] - a[0], 0.0, b[1] - a[1]))
        fwd = (-d[2], 0.0, d[0]) if i else ex  # square to the path, pointing forward/up
        if dot(fwd, ex) < 0 and i == 0:
            fwd = mul(fwd, -1.0)
        neck.append(section(hs.m(x, z), fwd, ey, hd * k, hw * k, 0.24 * k))
    parts.loft(neck, R["neck"], cap_end=R["neckend"])

    # The head: a bevelled block over the fist, its tail over the web of the hand, its front over
    # the index finger.
    head = [section(hs.m(x, (zb + zt) / 2), (0.0, 0.0, 1.0), ey, (zt - zb) / 2 * k, hw * k, 0.22 * k)
            for x, zb, zt, hw in LASER_HEAD]
    parts.loft(head, R["head"], cap_start=R["headend"], cap_end=R["headend"])

    sweep(parts, [hs.m(x, z) for x, z in GUARD_PATH], 0.09 * k, 0.26 * k, R["guard"])
    sweep(parts, [hs.m(x, z) for x, z in TRIGGER_PATH], 0.075 * k, 0.11 * k, R["trigger"])

    # The body does not move (only the barrels and the beams do): carried by its back.
    body = range(len(frame0))
    clusters = [[v for v in body if frame0[v][0] < 8.0 and frame0[v][2] < -4.0],
                [v for v in body if 40.0 < frame0[v][0] < 52.0 and frame0[v][2] < -10.0],
                [v for v in body if 35.0 < frame0[v][0] < 52.0 and frame0[v][2] > 3.0]]
    carrier = Carrier(model, clusters)
    remap = assemble(model, old_tris, parts, carrier)
    check_anchors(old_tris, model.tris, remap, kept)

    # The blade folds into the neck.
    inside = hs.m(-1.2, -3.9)
    pin(model, remap, carrier, {v: (inside[0], inside[1] + (frame0[v][1] - LASER_Y) * 0.2, inside[2]) for v in blade})

    origin = model.write(os.path.join(out_dir, "v_laserg.mdl"))
    return settings_for("v_laserg.mdl (slot 9)", model, old_origin, origin, sw, offset, p0, p0, anchor), model


# ----------------------------------------------------------------------------
# The grappling hook, round 18 (voice note 14-33-16: "improve the handle and add a trigger guard and
# trigger"): the fist held a thin stick leaning back under the back of the body. Now the grip the
# other guns have (GRIP_PROFILE, ribbed, a butt plate) goes through the fist, with a trigger guard
# and a trigger; the stick folds into the grip. The hand and the gun stay where they were.

def build_grapple(out_dir):
    model = Mdl(open(os.path.join(SRC, "v_grpple.mdl"), "rb").read())
    old_tris = list(model.tris)
    old_origin = model.origin
    frame0 = model.frames[0][1]

    sw = 0.33  # vr_weapons.inc slot 17
    hand_av, hand_ofs = 68, (1.7, -0.2, 1.1)
    offset = (-2.15, 5.0, -0.9)
    kept = {"hand": 68, "muzzle": 90, "2H hand, wpnbtn, wpntxt": 0}

    order = strip_order(old_tris)
    anchor = frame0[order[hand_av]]
    p0 = add(anchor, mul(hand_ofs, 1.0 / (K * sw)))
    hs = HandSpace(p0, sw, y_centre=p0[1] + GRIP_Y / sw)

    comps = model.components()
    # The body (a long diamond-section tube; the hook's claws, which vanish when it is fired, and
    # the stick are apart) and the stick: three pieces leaning back under the body's back end.
    body = [v for c in comps if len(c) in (19, 13) and min(frame0[v][0] for v in c) < 3.0 for v in c]
    stick = {v for c in comps if max(frame0[v][2] for v in c) < 3.1 and min(frame0[v][2] for v in c) < -2.0
             and max(frame0[v][0] for v in c) < 6.0 for v in c}
    assert len(body) == 32 and len(stick) == 16, (len(body), len(stick))

    row = grow_skin(model, 40)
    R = {"grip": (0, row, 64, row + 32), "butt": (64, row, 112, row + 8), "bottom": (64, row + 8, 88, row + 32),
         "guard": (88, row + 8, 136, row + 20), "trigger": (88, row + 20, 136, row + 32)}
    paint(model, R["grip"], ribbed(Noise(90), GRIPBROWN, 4))
    paint(model, R["butt"], metal(Noise(91), BLUE, 0.4, 0.12))
    paint(model, R["bottom"], metal(Noise(92), BLUE, 0.3, 0.12, lines=4))
    paint(model, R["guard"], metal(Noise(93), BLUE, 0.4, 0.1, grain=0.04))
    paint(model, R["trigger"], metal(Noise(94), STEEL, 0.5, 0.1))

    parts = Parts()
    grip(parts, hs, R["grip"], R["butt"], R["bottom"], GRIP_PROFILE)
    k = 1.0 / sw
    sweep(parts, [hs.m(x, z) for x, z in GUARD_PATH], 0.09 * k, 0.26 * k, R["guard"])
    sweep(parts, [hs.m(x, z) for x, z in TRIGGER_PATH], 0.075 * k, 0.11 * k, R["trigger"])

    # Carried by the body's back end, its front end and its top (it slides back when the hook
    # flies).
    clusters = [[v for v in body if frame0[v][0] < 3.5], [v for v in body if frame0[v][0] > 21.0],
                [v for v in body if frame0[v][2] > 7.0]]
    carrier = Carrier(model, clusters)
    remap = assemble(model, old_tris, parts, carrier)
    check_anchors(old_tris, model.tris, remap, kept)

    # The stick folds into the grip.
    inside = hs.m(grip_x(-1.2) - 0.2, -1.2)
    pin(model, remap, carrier, {v: (inside[0], inside[1] + frame0[v][1] * 0.2, inside[2]) for v in stick})

    origin = model.write(os.path.join(out_dir, "v_grpple.mdl"))
    return settings_for("v_grpple.mdl (slot 17)", model, old_origin, origin, sw, offset, p0, p0, anchor), model


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "quakevr", "progs")
    builds = [lambda gl=gl: build_launcher(out_dir, gl) for gl in LAUNCHERS]
    builds += [lambda: build_shotgun(out_dir),
              lambda: build_laser(out_dir),
              lambda: build_grapple(out_dir)]
    for build in builds:
        (name, settings), model = build()
        print("%s: %d vertices, %d triangles, %d frames, skin %dx%d" % (name, len(model.st), len(model.tris),
                                                                       len(model.frames), model.sw, model.sh))
        for key, value in settings.items():
            print("    %s = %s" % (key, value))


if __name__ == "__main__":
    main()
