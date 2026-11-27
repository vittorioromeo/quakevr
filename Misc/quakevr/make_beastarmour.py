#!/usr/bin/env python3
# make_beastarmour.py -- the armour of the beasts (ROUND26.md, "beastarmour"; Quake/vr/vr_beastarmour.cpp): rigid plates
# for the rottweiler, the ogre, the scrag and the fiend (the shambler's path: one more table below), each piece a model of
# its own (quakevr/progs/vrbarmor_<kind>_<slot>.mdl) riding one bone of the monster's derived rig (vr_ragdoll.cpp's seed
# tables), and quakevr/data/beastarmour.txt, the engine's list of them (which bone, where its middle is in the rest pose).
#
# The pieces are the armed humanoids' slots (vr_agarmor.hpp Piece: 0 the cuirass, 1 the helmet, 2/3, 4/5, 6/7, 8/9 the
# pairs), so the same rules hold (vr_agarmor.qc: pools by the slot's share, the helmet's headshot rule, breaking, debris).
# A large body's cuirass is several pieces (`group`: the breastplate and the back, belly or croup plates on their own
# bones), each breaking on its own; the roll's cuirass gives the whole group.
#
# Each piece is a shell round its bone: rows of separate bevelled lames (the humanoids' look), each lame a patch of a tube
# round the bone's line whose radius follows the body (measured from the .mdl's rest pose, pose 0: the vertices nearest
# this bone's seed, a high percentile of their distance from the line by place along it and angle round it), a little
# off the skin. Only measurements are taken from the .mdl: every vertex here is ours. Textures: make_agarmor.py's atlas
# (its nine skins, tier x wear; halved to 128 x 64), the lames on its plate tiles, their sides on its rim, inside on its
# padding.
#
# Usage: py -3.13 Misc/quakevr/make_beastarmour.py   (deterministic; needs numpy and the monsters' .mdl in quakevr/progs)

import math
import os
import re
import sys
import warnings

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)

import genguard  # noqa: E402
import make_agarmor  # noqa: E402
import mdlgen  # noqa: E402
from check_mdl_art import load_mdl  # noqa: E402

import importlib.util  # noqa: E402

_spec = importlib.util.spec_from_file_location("ragdoll_mdl", os.path.join(HERE, "ragdoll", "mdl.py"))
ragdoll_mdl = importlib.util.module_from_spec(_spec)  # (every frame of a model: the symmetric kinds' envelopes)
_spec.loader.exec_module(ragdoll_mdl)

HALF = 2  # the atlas's texels per skin texel here (256 x 128 -> 128 x 64)
SW, SH = make_agarmor.SKIN_W // HALF, make_agarmor.SKIN_H // HALF
TILE_W = 160.0 / make_agarmor.TILES_X / HALF
TILE_H = 128.0 / make_agarmor.TILES_Y / HALF
RIM = tuple(c / HALF for c in make_agarmor.BLOCKS["rim"])
INNER = tuple(c / HALF for c in make_agarmor.BLOCKS["inner"])

# A piece: slot, bone, (t0, t1) along the bone (0 its pivot, 1 its end), the angle round it its middle faces (`face`: a
# direction in the rest pose, or "out" (away from the body's middle line), "up", "down"), its span (degrees; 360 a ring),
# its rows of lames, the tile kind (plain, panel, vent, emblem, rib), its bones measured (the seed's cluster and these),
# spikes (a collar's).
P = dict


def piece(slot, bone, t, face, span, rows=2, tile="plain", more=(), spikes=0, stand=0.0, thick=0.0, pivot=None, end=None, cap=False):
    return P(slot=slot, bone=bone, t=t, face=face, span=span, rows=rows, tile=tile, more=more, spikes=spikes,
             stand=stand, thick=thick, pivot=pivot, end=end, cap=cap)


def spiece(slot, bone, t, face, span, ref, pair=None, mid=False, rows=1, tile="plain", more=(), pct=95, cap_r=1.6,
           tilt=0.0, even=0.0, stand=0.0, thick=0.0, cap=False, dome=None, pole=(0.0, 0.0, 1.0)):
    """A symmetric kind's piece (shell_sym): `ref` the left/right bones whose line is the body's side to side here,
    `pair` the mirrored piece's bone, `mid` a middle piece; `face` front, back, out (from the body), up, down; `pct` the
    flesh's percentile covered, `cap_r` the radius's cut (times its median), `tilt` the facing raised, `even` how much
    of the largest radius round it each row takes (a band's straight sides); `dome` (its middle along the bone, its
    rim's angle from the pole): a dome round that middle, its pole `pole` (forward, out, up), `span` round it."""
    return P(slot=slot, bone=bone, t=t, face=face, span=span, rows=rows, tile=tile, more=more, spikes=0, stand=stand,
             thick=thick, pivot=None, end=None, cap=cap, ref=ref, pair=pair, mid=mid, pct=pct, cap_r=cap_r, tilt=tilt,
             even=even, dome=dome, pole=pole)


SH_OGRE = ("upperarm_l", "upperarm_r")  # (the ogre's shoulders: his chest's side to side)
HIP_OGRE = ("thigh_l", "thigh_r")
SH_WIZ = ("arm_l", "arm_r")

KINDS = [
    # The rottweiler: barding over his chest and croup (the cuirass: two pieces), a chanfron down his face, a spiked collar,
    # plates over his shoulders and haunches.
    P(name="dog", model="progs/dog.mdl", seeds="dog", stand=0.5, thick=0.8, group=(0, 8), pieces=[
        piece(0, "chest", (0.0, 1.0), "up", 200, rows=2, tile="panel"),
        piece(8, "pelvis", (-0.25, 1.0), "up", 220, rows=2, tile="rib"),
        piece(1, "head", (0.2, 0.95), "up", 130, rows=2, tile="plain"),
        piece(9, "chest", (0.82, 1.08), "up", 360, rows=1, tile="rim", spikes=10, stand=0.7),
        piece(2, "upperleg_fl", (-0.15, 0.7), "out", 150, rows=2, tile="emblem"),
        piece(3, "upperleg_fr", (-0.15, 0.7), "out", 150, rows=2, tile="emblem"),
        piece(6, "thigh_bl", (-0.1, 0.75), "out", 150, rows=2, tile="plain"),
        piece(7, "thigh_br", (-0.1, 0.75), "out", 150, rows=2, tile="plain"),
    ]),
    # The ogre (beastlook: symmetric, shell_sym): a cuirass of three (breastplate, back plate, the faulds over his belly),
    # a helmet, pauldrons, bracers and tassets over his thighs; his twisted stance's middle from his shoulders and hips.
    P(name="ogre", model="progs/ogre.mdl", seeds="ogre", stand=0.6, thick=1.0, group=(0, 8, 9), sym=True, pieces=[
        spiece(0, "chest", (0.0, 1.0), "front", 170, SH_OGRE, mid=True, tile="panel", pct=90),
        spiece(8, "chest", (0.1, 0.95), "back", 130, SH_OGRE, mid=True, tile="plain", pct=90),
        spiece(9, "pelvis", (-0.15, 0.95), "front", 160, HIP_OGRE, mid=True, rows=2, tile="rib"),
        spiece(1, "head", (0, 0), "back", 360, SH_OGRE, mid=True, rows=2, dome=(0.45, 80), pole=(-0.15, 0.0, 1.0)),
        spiece(2, "upperarm_l", (0, 0), "out", 360, SH_OGRE, pair="upperarm_r", rows=2, tile="emblem", dome=(0.0, 75),
               pole=(0.0, 0.7, 1.0)),
        spiece(3, "upperarm_r", (0, 0), "out", 360, SH_OGRE, pair="upperarm_l", rows=2, tile="emblem", dome=(0.0, 75),
               pole=(0.0, 0.7, 1.0)),
        spiece(4, "forearm_l", (0.3, 0.9), "out", 360, SH_OGRE, pair="forearm_r", tile="vent", even=0.6),
        spiece(5, "forearm_r", (0.3, 0.9), "out", 360, SH_OGRE, pair="forearm_l", tile="vent", even=0.6),
        spiece(6, "thigh_l", (0.2, 0.75), "front", 150, HIP_OGRE, pair="thigh_r", rows=2, tile="plain"),
        spiece(7, "thigh_r", (0.2, 0.75), "front", 150, HIP_OGRE, pair="thigh_l", rows=2, tile="plain"),
    ]),
    # The scrag (beastlook: symmetric): a plastron and a back plate on his chest and a belly plate (the cuirass: three), a
    # mask over his face, plates over his shoulders, plates over his claws' roots; his middle from his shoulders.
    P(name="wizard", model="progs/wizard.mdl", seeds="wizard", stand=0.5, thick=0.8, group=(0, 8, 9), sym=True, pieces=[
        spiece(0, "chest", (0.0, 1.0), "front", 130, SH_WIZ, mid=True, tile="panel"),
        spiece(9, "chest", (0.05, 0.95), "back", 140, SH_WIZ, mid=True, tile="plain"),
        spiece(8, "pelvis", (-0.3, 0.9), "front", 150, SH_WIZ, mid=True, rows=2, tile="rib"),
        spiece(1, "head", (0, 0), "up", 360, SH_WIZ, mid=True, dome=(0.5, 80), pole=(1.0, 0.0, 0.6)),
        spiece(2, "arm_l", (0, 0), "out", 360, SH_WIZ, pair="arm_r", rows=2, tile="emblem", dome=(0.35, 60),
               pole=(0.0, 0.5, 1.0)),
        spiece(3, "arm_r", (0, 0), "out", 360, SH_WIZ, pair="arm_l", rows=2, tile="emblem", dome=(0.35, 60),
               pole=(0.0, 0.5, 1.0)),
        spiece(4, "hand_l", (0.0, 0.35), "up", 140, SH_WIZ, pair="hand_r", tile="vent", cap_r=3.0),
        spiece(5, "hand_r", (0.0, 0.35), "up", 140, SH_WIZ, pair="hand_l", tile="vent", cap_r=3.0),
    ]),
    # The fiend: plates over his hunched back and shoulders and over his haunches, a breastplate under his chest (the cuirass:
    # three), a helmet, pauldrons, bracers, plates over his thighs.
    P(name="demon", model="progs/demon.mdl", seeds="demon", stand=0.7, thick=1.1, group=(0, 8, 9), pieces=[
        piece(0, "chest", (0.0, 1.0), "up", 170, rows=2, tile="panel"),
        piece(9, "chest", (0.1, 0.9), "down", 120, rows=2, tile="rib"),
        piece(8, "pelvis", (-0.6, 1.0), "up", 200, rows=2, tile="plain"),
        piece(1, "head", (0.1, 0.9), "up", 160, rows=1, tile="plain", cap=True),
        piece(2, "upperarm_l", (-0.4, 0.8), "out", 180, rows=2, tile="emblem"),
        piece(3, "upperarm_r", (-0.4, 0.8), "out", 180, rows=2, tile="emblem"),
        piece(4, "forearm_l", (0.1, 0.9), "out", 360, rows=2, tile="vent"),
        piece(5, "forearm_r", (0.1, 0.9), "out", 360, rows=2, tile="vent"),
        piece(6, "thigh_l", (-0.2, 0.9), "out", 170, rows=2, tile="plain"),
        piece(7, "thigh_r", (-0.2, 0.9), "out", 170, rows=2, tile="plain"),
    ]),
]

TILES = {"plain": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], "panel": [20, 21, 22], "vent": [23, 24, 25], "rib": [26, 27],
         "emblem": [28], "rim": [29]}


def read_seeds():
    """Every seed table of vr_ragdoll.cpp: name -> [(bone, parent, centre, pivot, end)]."""
    text = open(os.path.join(ROOT, "Quake", "vr", "vr_ragdoll.cpp")).read()
    num = r"(-?[\d.]+)f?"
    vec = r"\{\s*%s,\s*%s,\s*%s\s*\}" % (num, num, num)
    out = {}
    for m in re.finditer(r"constexpr Seed (\w+)Seeds\[\] = \{(.*?)\n\};", text, re.S):
        bones = []
        for b in re.finditer(r'\{"(\w+)",\s*(-?\d+),\s*Joint::\w+,\s*%s,\s*%s,\s*%s' % (vec, vec, vec), m.group(2)):
            g = [float(x) for x in b.groups()[2:]]
            bones.append((b.group(1), int(b.group(2)), np.array(g[0:3]), np.array(g[3:6]), np.array(g[6:9])))
        out[m.group(1)] = bones
    return out


def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v


def shell(kind, spec, seeds, pos, frames=None):
    """The piece's lames: a list of (outer grid, inner grid) arrays [rows+1 along, cols+1 round, 3], its frame."""
    if kind.get("sym"):
        return shell_sym(kind, spec, seeds, pos, frames)
    byname = {b[0]: b for b in seeds}
    name, _, _, pivot, end = byname[spec["bone"]]
    pivot = np.array(spec["pivot"]) if spec["pivot"] is not None else pivot
    end = np.array(spec["end"]) if spec["end"] is not None else end
    centres = np.array([b[2] for b in seeds])
    nearest = np.argmin(((pos[:, None, :] - centres[None]) ** 2).sum(-1), axis=1)
    want = [i for i, b in enumerate(seeds) if b[0] == name or b[0] in spec["more"]]
    verts = pos[np.isin(nearest, want)]
    axis = end - pivot
    length = float(np.linalg.norm(axis))
    a = unit(axis)
    face = spec["face"]
    mid = (pivot + end) * 0.5
    if face == "out":
        body = np.array([mid[0], 0.0, mid[2]])  # (the body's middle line: y 0)
        f = mid - body
        f = np.array([0.0, math.copysign(1.0, mid[1]), 0.0]) if abs(f[1]) < 1.0 else np.array([0.0, f[1], 0.0])
    elif face == "up":
        f = np.array([0.0, 0.0, 1.0])
    elif face == "down":
        f = np.array([0.0, 0.0, -1.0])
    else:
        f = np.array(face, dtype=float)
    r0 = f - a * float(np.dot(f, a))
    if np.linalg.norm(r0) < 1e-3:
        r0 = np.cross(a, [0.0, 1.0, 0.0])
    r0 = unit(r0)
    r1 = np.cross(a, r0)
    rel = verts - pivot
    tv = rel @ a / length
    radial = rel - np.outer(rel @ a, a)
    rv = np.linalg.norm(radial, axis=1)
    pv = np.arctan2(radial @ r1, radial @ r0)
    t0, t1 = spec["t"]
    span = math.radians(spec["span"])
    ring = spec["span"] >= 359.0
    rows = spec["rows"]
    cols = max(3, int(round(spec["span"] / 22.0)))
    sub = 2  # samples per lame along the bone
    nt = rows * sub + 1
    ts = np.linspace(t0, t1, nt)
    ps = np.linspace(-span / 2, span / 2, cols + 1) if not ring else np.linspace(-math.pi, math.pi, cols + 1)
    rad = np.full((nt, cols + 1), np.nan)
    tw = max(1.5 / length, 0.18)
    for i, t in enumerate(ts):
        for j, p in enumerate(ps):
            dp = np.abs((pv - p + math.pi) % (2 * math.pi) - math.pi)
            sel = rv[(np.abs(tv - t) < tw) & (dp < math.radians(32))]
            if len(sel) >= 2:
                rad[i, j] = np.percentile(sel, 85)
    fallback = np.nanmedian(rad) if np.isfinite(rad).any() else 3.0
    for _ in range(6):  # (holes filled from their neighbours)
        bad = ~np.isfinite(rad)
        if not bad.any():
            break
        pad = np.pad(rad, 1, mode="edge")
        nb = np.stack([pad[:-2, 1:-1], pad[2:, 1:-1], pad[1:-1, :-2], pad[1:-1, 2:]])
        fill = np.nanmean(np.where(np.isfinite(nb), nb, np.nan), axis=0)
        rad = np.where(bad & np.isfinite(fill), fill, rad)
    rad = np.where(np.isfinite(rad), rad, fallback)
    for _ in range(2):  # (smoothed: a shell, not the body's every bump)
        pad = np.pad(rad, 1, mode="wrap" if ring else "edge")
        rad = 0.4 * rad + 0.15 * (pad[:-2, 1:-1] + pad[2:, 1:-1] + pad[1:-1, :-2] + pad[1:-1, 2:])
    rad = np.maximum(rad, 1.0)
    return build_lames(kind, spec, pivot, a, length, r0, r1, rad, ts, ps, ring, rows, sub, cols, t0, t1, nt)


def build_lames(kind, spec, pivot, a, length, r0, r1, rad, ts, ps, ring, rows, sub, cols, t0, t1, nt, at_fn=None):
    """The lames on radius grid `rad` [nt along (ts), cols+1 round (ps)] round the line pivot + a * t * length, angles
    from r0 towards r1 (`at_fn`: another surface's place of (t, angle, radius), `length` its units a t: a dome's)."""
    stand = spec["stand"] or kind["stand"]
    thick = spec["thick"] or kind["thick"]

    def at(t, p, r):
        return pivot + a * (t * length) + (math.cos(p) * r0 + math.sin(p) * r1) * r

    at = at_fn or at
    lames = []
    gap = min(0.6 / length, (t1 - t0) / rows * 0.12)
    for k in range(rows):
        lo, hi = k * sub, (k + 1) * sub
        outer = np.zeros((sub + 1, cols + 1, 3))
        inner = np.zeros((sub + 1, cols + 1, 3))
        for i in range(sub + 1):
            ii = lo + i
            t = ts[ii] + (gap if i == 0 and k > 0 else 0.0) - (gap if i == sub and k < rows - 1 else 0.0)
            # (each lame a little further out than the one before it: the rows overlap as lames do)
            lift = 0.25 * thick * k
            for j in range(cols + 1):
                r = rad[ii, j] + stand + lift
                inner[i, j] = at(t, ps[j], r)
                # (the outer face inset at the lame's edges: its bevel)
                bt = 0.0 if 0 < i < sub or (at_fn and k == 0 and i == 0) else (0.45 * thick / length) * (1 if i == 0 else -1)
                bp = 0.0 if ring or 0 < j < cols else (0.45 * thick / max(r, 1.0)) * (1 if j == 0 else -1)
                outer[i, j] = at(t + bt, ps[j] + bp, r + thick)
        lames.append((outer, inner))
    if spec["cap"]:
        # A dome over the far end (a helmet's crown): the last row's ring closing to the line in a quarter circle.
        steps = 3
        lift = 0.25 * thick * (rows - 1)
        outer = np.zeros((steps + 1, cols + 1, 3))
        inner = np.zeros((steps + 1, cols + 1, 3))
        for i in range(steps + 1):
            th = i / steps * math.pi / 2
            for j in range(cols + 1):
                r = rad[-1, j] + stand + lift
                inner[i, j] = at(t1 + r * math.sin(th) / length, ps[j], r * math.cos(th))
                outer[i, j] = at(t1 + (r + thick) * math.sin(th) / length, ps[j], (r + thick) * math.cos(th))
        lames.append((outer, inner))
    spikes = []
    if spec["spikes"]:
        tm = (t0 + t1) * 0.5
        im = int(round((tm - t0) / (t1 - t0) * (nt - 1)))
        for s in range(spec["spikes"]):
            p = -math.pi + (s + 0.5) * 2 * math.pi / spec["spikes"]
            j = int(round((p + math.pi) / (2 * math.pi) * cols)) % (cols + 1)
            r = rad[im, j] + stand + thick
            base = at(tm, p, r)
            out = unit(base - (pivot + a * (tm * length)))
            spikes.append((base, out, a))
    return lames, spikes, ring


# --- The symmetric kinds (beastlook, ROUND26.md: the ogre and the scrag) ------------------------------------------------
# A left/right pair is ONE definition: its shape measured on both bones (each in its own handed frame: angles from the
# same side of the body, turning the mirrored way) and the larger radius of the two taken, so the right piece is the
# left's mirror image wherever the rest pose stands its limbs. A middle piece (cuirass, helmet) lies on the body's middle
# plane (through the middle of its `ref` pair: the shoulders, the hips) facing the way that pair faces, not the rest
# pose's twist, and its shape is mirrored across that plane. The radius is the body's envelope round the bone in every
# frame of its main animations (each frame's flesh carried back to the rest pose by the bone's rigid fit, as the engine
# places the piece), raised and never lowered by the smoothing: the plates clear the flesh as it moves.


def kabsch(a, b):
    ca, cb = a.mean(0), b.mean(0)
    u, _, vt = np.linalg.svd((a - ca).T @ (b - cb))
    d = np.sign(np.linalg.det(vt.T @ u.T))
    r = vt.T @ np.diag([1.0, 1.0, d]) @ u.T
    return r, cb - r @ ca


def sym_context(kind, seeds, pos, frames):
    names, poses = frames
    centres = np.array([b[2] for b in seeds])
    label = np.argmin(((pos[:, None, :] - centres[None]) ** 2).sum(-1), axis=1)
    # (the engine's own labels when listed: the vertices its fit follows, not the nearest seeds')
    for line in open(os.path.join(HERE, "beastarmour_labels.txt")):
        f = line.split()
        if f and f[0] == kind["model"] and len(f) - 1 == len(pos):
            label = np.array([int(x) for x in f[1:]])
    keep = [f for f, n in enumerate(names) if not n.startswith(kind.get("skip", ("death", "bdeath", "base", "frame")))]
    return dict(seeds=seeds, byname={b[0]: (i, b) for i, b in enumerate(seeds)}, label=label, pos=pos,
                poses=poses[keep])


def sym_cloud(ctx, bones):
    """The bones' flesh in every kept frame carried back to the rest pose by the first bone's rigid fit."""
    want = [ctx["byname"][b][0] for b in bones]
    mine = np.where(np.isin(ctx["label"], want))[0]
    own = np.where(ctx["label"] == want[0])[0]
    rest = ctx["pos"]
    # (the engine's anchors: spread over the bone, the first nearest its middle)
    mid = rest[own].mean(0)
    anc = [own[np.argmin(np.linalg.norm(rest[own] - mid, axis=1))]]
    while len(anc) < 10 and len(anc) < len(own):
        d = np.min(np.linalg.norm(rest[own][:, None] - rest[anc][None], axis=2), axis=1)
        k = int(np.argmax(d))
        if d[k] < 0.25:
            break
        anc.append(own[k])
    out = [rest[mine]]
    for p in ctx["poses"]:
        r, t = kabsch(rest[anc], p[anc])
        out.append((p[mine] - t) @ r)
    return np.concatenate(out)


def sym_frame(ctx, spec, bone, side):
    _, b = ctx["byname"][bone]
    pivot, end = np.array(b[3]), np.array(b[4])
    refl, refr = spec["ref"]
    pl, pr = ctx["byname"][refl][1][3], ctx["byname"][refr][1][3]
    lat = unit(np.array(pl) - np.array(pr))
    lat = unit(lat - np.array([0.0, 0.0, lat[2]]))
    fwd = np.cross(lat, [0.0, 0.0, 1.0])
    if spec["mid"]:
        m = (np.array(pl) + np.array(pr)) * 0.5
        pivot = pivot - lat * float(np.dot(pivot - m, lat))
        end = end - lat * float(np.dot(end - m, lat))
    a = unit(end - pivot)
    f = {"front": fwd, "back": -fwd, "out": lat * side, "up": np.array([0.0, 0.0, 1.0]),
         "down": np.array([0.0, 0.0, -1.0])}[spec["face"]]
    if spec["tilt"]:
        f = unit(f + np.array([0.0, 0.0, spec["tilt"]]))
    r0 = f - a * float(np.dot(f, a))
    r0 = unit(r0 if np.linalg.norm(r0) > 1e-3 else np.cross(a, lat))
    r1 = np.cross(a, r0) * side
    return pivot, a, float(np.linalg.norm(end - pivot)), r0, r1


def sym_radius(ctx, spec, bone, side, ts, ps, frame):
    pivot, a, length, r0, r1 = frame
    pts = sym_cloud(ctx, [bone] + [m if side > 0 else m.replace("_l", "_r") for m in spec["more"]])
    rel = pts - pivot
    tv = rel @ a / length
    radial = rel - np.outer(rel @ a, a)
    rv = np.linalg.norm(radial, axis=1)
    pv = np.arctan2(radial @ r1, radial @ r0)
    rad = np.full((len(ts), len(ps)), np.nan)
    tw = max(1.5 / length, 0.16)
    for i, t in enumerate(ts):
        near = np.abs(tv - t) < tw
        for j, p in enumerate(ps):
            dp = np.abs((pv[near] - p + math.pi) % (2 * math.pi) - math.pi)
            sel = rv[near][dp < math.radians(26)]
            if len(sel) >= 3:
                rad[i, j] = np.percentile(sel, spec["pct"])
    return rad


def sym_smooth(spec, rad, ring):
    """Holes filled, outliers cut, smoothed upwards only, evened, at least 1."""
    fallback = np.nanmedian(rad) if np.isfinite(rad).any() else 3.0
    for _ in range(8):
        bad = ~np.isfinite(rad)
        if not bad.any():
            break
        pad = np.pad(rad, 1, mode="edge")
        nb = np.stack([pad[:-2, 1:-1], pad[2:, 1:-1], pad[1:-1, :-2], pad[1:-1, 2:]])
        fill = np.nanmean(np.where(np.isfinite(nb), nb, np.nan), axis=0)
        rad = np.where(bad & np.isfinite(fill), fill, rad)
    rad = np.where(np.isfinite(rad), rad, fallback)
    # (an outlier cut: a weapon's or a spike's vertices are not flesh to cover)
    rad = np.minimum(rad, np.median(rad) * spec["cap_r"])
    for _ in range(3):  # (smoothed upwards only: a calm shell over the bumps, never into them)
        pad = np.pad(rad, 1, mode="wrap" if ring else "edge")
        rad = np.maximum(rad, 0.4 * rad + 0.15 * (pad[:-2, 1:-1] + pad[2:, 1:-1] + pad[1:-1, :-2] + pad[1:-1, 2:]))
    if spec["even"]:
        # (a plate's curve evened along the bone: each column one radius, the largest)
        rad = rad * (1 - spec["even"]) + np.repeat(rad.max(axis=0, keepdims=True), rad.shape[0], axis=0) * spec["even"]
    return np.maximum(rad, 1.0)


def shell_dome(kind, spec, ctx, side):
    """A dome (a helmet, a pauldron): rings of lames from its pole down to `dome`[1] degrees, round a middle on its bone
    (`dome`[0] along it); the pole `pole` (forward, out, up in the body's frame), the rings' angles from `face`."""
    t_mid, polar = spec["dome"]
    rows, sub = spec["rows"], 2
    span = math.radians(spec["span"])
    ring = spec["span"] >= 359.0
    cols = max(8, int(round(spec["span"] / 20.0)))
    cols += cols % 2
    nt = rows * sub + 1
    ts = np.linspace(0.0, math.radians(polar), nt)
    ps = np.linspace(-span / 2, span / 2, cols + 1) if not ring else np.linspace(-math.pi, math.pi, cols + 1)

    def frame(bone, sd):
        pivot, a, length, _, _ = sym_frame(ctx, spec, bone, sd)
        refl, refr = spec["ref"]
        pl, pr = ctx["byname"][refl][1][3], ctx["byname"][refr][1][3]
        lat = unit(np.array(pl) - np.array(pr))
        lat = unit(lat - np.array([0.0, 0.0, lat[2]]))
        fwd = np.cross(lat, [0.0, 0.0, 1.0])
        up = np.array([0.0, 0.0, 1.0])
        pole = unit(spec["pole"][0] * fwd + spec["pole"][1] * lat * sd + spec["pole"][2] * up)
        f = {"front": fwd, "back": -fwd, "out": lat * sd, "up": up, "down": -up}[spec["face"]]
        r0 = f - pole * float(np.dot(f, pole))
        r0 = unit(r0 if np.linalg.norm(r0) > 1e-3 else np.cross(pole, lat))
        r1 = np.cross(pole, r0) * sd
        return pivot + a * (t_mid * length), pole, r0, r1

    def radius(bone, sd, fr):
        c, pole, r0, r1 = fr
        pts = sym_cloud(ctx, [bone] + [m if sd > 0 else m.replace("_l", "_r") for m in spec["more"]]) - c
        d = np.linalg.norm(pts, axis=1)
        u = pts / np.maximum(d, 1e-6)[:, None]
        th = np.arccos(np.clip(u @ pole, -1.0, 1.0))
        ph = np.arctan2(u @ r1, u @ r0)
        rad = np.full((nt, cols + 1), np.nan)
        for i, t in enumerate(ts):
            near = np.abs(th - t) < math.radians(18)
            for j, p in enumerate(ps):
                # (round the pole every angle is near: there by the polar angle alone)
                dp = np.abs((ph[near] - p + math.pi) % (2 * math.pi) - math.pi) * max(math.sin(t), 0.3)
                sel = d[near][dp < math.radians(20)]
                if len(sel) >= 3:
                    rad[i, j] = np.percentile(sel, spec["pct"])
        return rad

    fr = frame(spec["bone"], side)
    if spec["pair"]:
        rad = np.fmax(radius(spec["bone"], side, fr), radius(spec["pair"], -side, frame(spec["pair"], -side)))
    else:
        rad = radius(spec["bone"], side, fr)
        rad = np.fmax(rad, rad[:, ::-1])
    return fr, ts, ps, ring, rows, sub, cols, nt, rad


def shell_sym(kind, spec, seeds, pos, frames):
    ctx = sym_context(kind, seeds, pos, frames)
    side = -1.0 if spec["bone"].endswith("_r") else 1.0
    if spec["dome"]:
        (c, pole, r0, r1), ts, ps, ring, rows, sub, cols, nt, rad = shell_dome(kind, spec, ctx, side)
        rad = sym_smooth(spec, rad, ring)
        length = float(np.mean(rad))

        def at(t, p, r):
            return c + (math.cos(t) * pole + math.sin(t) * (math.cos(p) * r0 + math.sin(p) * r1)) * r

        return build_lames(kind, spec, c, pole, length, r0, r1, rad, ts, ps, ring, rows, sub, cols, ts[0], ts[-1], nt, at)
    t0, t1 = spec["t"]
    span = math.radians(spec["span"])
    ring = spec["span"] >= 359.0
    rows, sub = spec["rows"], 2
    cols = max(4, int(round(spec["span"] / 20.0)))
    cols += cols % 2  # (an even count: a middle column, the mirror's line)
    nt = rows * sub + 1
    ts = np.linspace(t0, t1, nt)
    ps = np.linspace(-span / 2, span / 2, cols + 1) if not ring else np.linspace(-math.pi, math.pi, cols + 1)
    frame = sym_frame(ctx, spec, spec["bone"], side)
    if spec["pair"]:
        other = spec["pair"]
        fo = sym_frame(ctx, spec, other, -side)
        rad = np.fmax(sym_radius(ctx, spec, spec["bone"], side, ts, ps, frame),
                      sym_radius(ctx, spec, other, -side, ts, ps, fo))
        length = (frame[2] + fo[2]) * 0.5
        frame = (frame[0], frame[1], length, frame[3], frame[4])
    else:
        rad = sym_radius(ctx, spec, spec["bone"], side, ts, ps, frame)
        rad = np.fmax(rad, rad[:, ::-1])  # (mirrored across the middle plane)
    rad = sym_smooth(spec, rad, ring)
    rad = np.maximum(rad, 1.0)
    pivot, a, length, r0, r1 = frame
    return build_lames(kind, spec, pivot, a, length, r0, r1, rad, ts, ps, ring, rows, sub, cols, t0, t1, nt)


def tile_rect(tile):
    tx, ty = tile % make_agarmor.TILES_X, tile // make_agarmor.TILES_X
    return (tx * TILE_W, ty * TILE_H, (tx + 1) * TILE_W, (ty + 1) * TILE_H)


def st(rect, u, v):
    s0, t0, s1, t1 = rect
    return (int(round(s0 + 0.5 + u * (s1 - s0 - 1.0))), int(round(t0 + 0.5 + v * (t1 - t0 - 1.0))))


class Builder:
    def __init__(self):
        self.mesh = mdlgen.Mesh(SW, SH, {})

    def quad(self, p, uv, outward_hint):
        """Quad p[0..3] (any winding: each triangle turned to face `outward_hint`; a corner shared, a triangle alone),
        its corners' texels uv[0..3]."""
        for a, b, c in ((0, 1, 2), (0, 2, 3)):
            cr = np.cross(p[b] - p[a], p[c] - p[a])
            if np.linalg.norm(cr) < 1e-7:
                continue
            if np.dot(cr, outward_hint) > 0.0:
                b, c = c, b  # (Quake's: clockwise seen from outside)
                cr = -cr
            n = tuple(-unit(cr))
            base = len(self.mesh.verts)
            for k in (a, b, c):
                self.mesh.verts.append((tuple(float(x) for x in p[k]), n, uv[k]))
            self.mesh.tris.append((base, base + 1, base + 2))


def build_piece(kind, spec, seeds, pos, centre_out, frames=None):
    lames, spikes, ring = shell(kind, spec, seeds, pos, frames)
    b = Builder()
    tiles = TILES[spec["tile"]]
    rim = RIM
    for k, (outer, inner) in enumerate(lames):
        rect = tile_rect(tiles[(k + spec["slot"]) % len(tiles)])
        ni, nj = outer.shape[0], outer.shape[1]
        mid = outer.mean(axis=(0, 1))
        axis_mid = inner.mean(axis=(0, 1))
        for i in range(ni - 1):
            for j in range(nj - 1):
                po = [outer[i, j], outer[i, j + 1], outer[i + 1, j + 1], outer[i + 1, j]]
                pi = [inner[i, j], inner[i, j + 1], inner[i + 1, j + 1], inner[i + 1, j]]
                u0, u1 = j / (nj - 1), (j + 1) / (nj - 1)
                v0, v1 = i / (ni - 1), (i + 1) / (ni - 1)
                uv = [st(rect, u0, v0), st(rect, u1, v0), st(rect, u1, v1), st(rect, u0, v1)]
                c = sum(po) / 4.0
                ci = sum(pi) / 4.0
                b.quad(po, uv, c - ci)
                b.quad(pi, [st(INNER, u0, v0), st(INNER, u1, v0), st(INNER, u1, v1), st(INNER, u0, v1)], ci - c)
        # The lame's sides (its rim): along both ends and, unless a ring, both edges.
        edges = [(outer[0, :], inner[0, :]), (outer[-1, :], inner[-1, :])]
        if not ring:
            edges += [(outer[:, 0], inner[:, 0]), (outer[:, -1], inner[:, -1])]
        for eo, ei in edges:
            for j in range(len(eo) - 1):
                p = [eo[j], eo[j + 1], ei[j + 1], ei[j]]
                c = sum(p) / 4.0
                hint = c - mid if np.linalg.norm(c - mid) > 1e-6 else c - axis_mid
                b.quad(p, [st(rim, 0, 0), st(rim, 1, 0), st(rim, 1, 1), st(rim, 0, 1)], hint)
    for base, out, a in spikes:
        side = unit(np.cross(out, a))
        h, w = 2.6, 0.7
        tip = base + out * h
        corners = [base + a * w + side * w, base + side * w - a * w, base - a * w - side * w, base - side * w + a * w]
        for q in range(4):
            p0, p1 = corners[q], corners[(q + 1) % 4]
            # (a pyramid's face: a quad with two corners at its tip)
            c = (p0 + p1 + tip) / 3.0
            b.quad([p0, p1, tip, tip], [st(rim, 0, 1), st(rim, 1, 1), st(rim, 0.5, 0), st(rim, 0.5, 0)], c - base + out * 0.1)
    pts = np.array([v[0] for v in b.mesh.verts])
    centre = (pts.min(axis=0) + pts.max(axis=0)) * 0.5
    b.mesh.verts = [(tuple(float(x) for x in np.array(p) - centre), n, c) for p, n, c in b.mesh.verts]
    b.mesh.tris = [t for t in b.mesh.tris if len({b.mesh.verts[t[0]][0], b.mesh.verts[t[1]][0], b.mesh.verts[t[2]][0]}) == 3]
    centre_out.extend(float(x) for x in centre)
    return b.mesh


def skins():
    lay = make_agarmor.build()
    out = []
    for level in (0, 1, 2):
        for armor in (1, 2, 3):
            full = np.frombuffer(make_agarmor.to_palette(make_agarmor.paint(lay, armor, level), armor, lay.emblem), np.uint8)
            full = full.reshape(make_agarmor.SKIN_H, make_agarmor.SKIN_W)
            out.append(full[::HALF, ::HALF].copy().tobytes())
    return out


def main():
    warnings.simplefilter("ignore", RuntimeWarning)  # (nanmean of a hole with no neighbours yet: filled on a later pass)
    seeds = read_seeds()
    progs = os.path.join(ROOT, "quakevr", "progs")
    data = os.path.join(ROOT, "quakevr", "data", "beastarmour.txt")
    meshes = []
    lines = [
        "// quakevr/data/beastarmour.txt -- the beasts' armour (Quake/vr/vr_beastarmour.cpp; ROUND26.md, \"beastarmour\"). Made",
        "// by Misc/quakevr/make_beastarmour.py with the pieces' models: edit the script's tables and run it again.",
        "//   kind <monster model> <cuirass group: the slots' bits the roll's cuirass gives>",
        "//   piece <monster model> <slot 0-9> <its derived rig's bone> <the piece's model> <its middle in the rest pose: x y z>",
    ]
    for kind in KINDS:
        pos = load_mdl(os.path.join(progs, os.path.basename(kind["model"]))).pos
        frames = None
        if kind.get("sym"):
            names, poses, _ = ragdoll_mdl.load(os.path.join(progs, os.path.basename(kind["model"])))
            frames = (names, poses)
        group = sum(1 << s for s in kind["group"])
        lines.append("kind %s %d" % (kind["model"], group))
        for spec in kind["pieces"]:
            centre = []
            mesh = build_piece(kind, spec, seeds[kind["seeds"]], pos, centre, frames)
            path = "progs/vrbarmor_%s_%d.mdl" % (kind["name"], spec["slot"])
            meshes.append((os.path.join(ROOT, "quakevr", path.replace("/", os.sep)), mesh, path))
            lines.append("piece %s %d %s %s %.2f %.2f %.2f" % (kind["model"], spec["slot"], spec["bone"], path, *centre))
    guard = genguard.Guard("make_beastarmour.py", [m[0] for m in meshes] + [data])
    sk = skins()
    for full, mesh, path in meshes:
        mdlgen.write_mdl(full, mesh, sk, os.path.splitext(os.path.basename(path))[0])
    with open(data, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    guard.finish()
    print("beastarmour: %d pieces, %d kinds -> %s" % (len(meshes), len(KINDS), os.path.normpath(data)))


if __name__ == "__main__":
    main()
