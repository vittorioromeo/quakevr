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
from types import SimpleNamespace

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


def piece(slot, bone, t, face, span, rows=2, tile="plain", more=(), spikes=0, stand=0.0, thick=0.0, pivot=None, end=None, cap=False,
          pair=None):
    """`pair` (beastfit): the right side's bone of a left piece: measured there (its radii, its length) and laid on this
    bone mirrored, so a pair is one shape (the fiend's left pauldron and bracer came out a different size and stretched)."""
    return P(slot=slot, bone=bone, t=t, face=face, span=span, rows=rows, tile=tile, more=more, spikes=spikes,
             stand=stand, thick=thick, pivot=pivot, end=end, cap=cap, pair=pair)


def fpiece(slot, bone, ref, shapes, pair=None, mid=False, face="out", tile="plain", more=(), pct=None, cap_r=None, tilt=0.0,
           part=None):
    """A faceted kind's piece (beastplates: facet_plates): `ref` the left/right bones whose line is the body's side to side
    here, `pair` the mirrored piece's bone, `mid` a middle piece (on the body's middle plane, mirrored across it; neither:
    measured as it stands, a tail); `face` front, back, out (from the body), up, down: the angle 0 of its shapes (`band`,
    `dome`); `pct` the flesh's percentile covered, `cap_r` the radius's cut (times its median), `tilt` the facing raised. A
    second piece of a slot already made (another bone) is a part of it: drawn, hit and broken with it (beastarmour.txt's
    later line). `part` (beastfit): such a part's own Armour Studio part (armour.txt: "tail", "shin"), tweaked apart from
    its slot's first piece (beastarmour.txt's last word)."""
    return P(slot=slot, bone=bone, ref=ref, shapes=shapes, pair=pair, mid=mid, face=face, tile=tile, more=more, pct=pct,
             cap_r=cap_r, tilt=tilt, stand=0.0, thick=0.0, even=0.0, part=part)


def band(t, rows, span=360, cw=None, step=0.45, lap=0.5, gap=0.35, ridge=0.0, ridge_rows=(), flare=0.0, lift=0.0,
         tiles=None, spike=None, line=None, heights=None, turn=0.0):
    """Rows of plates along the bone from t[0] (the top: the row above laps over the next) to t[1], round it over `span`
    degrees: `rows` each row's plates, `cw` a row's plates' widths (shares), `step` how far a row's lower edge stands proud
    of its top, `lap` (units) how far it reaches over the row below, `gap` (units) between a row's plates, `ridge` (units)
    the line facing `face` raised on `ridge_rows`, `flare` the first row's top edge raised (a collar), `lift` the whole shape
    raised, `tiles` each row's tile kind, `spike` (where along 0..1, angle, height, width, way: "axis" or (fwd, out, up)),
    `line` (bone, t, bone, t): the line it lies round (t 0 and 1 its ends) instead of its bone's, `heights` the rows' shares,
    `turn` its span's middle this far round from `face` (degrees)."""
    return P(kind="band", u=t, rows=rows, span=span, cw=cw, step=step, lap=lap, gap=gap, ridge=ridge, ridge_rows=ridge_rows,
             flare=flare, lift=lift, tiles=tiles, spike=spike, line=line, heights=heights, turn=math.radians(turn))


def dome(mid, polar, rows, pole, span=360, step=0.45, lap=0.5, gap=0.35, lift=0.0, tiles=None, spike=None, heights=None,
         brow=None):
    """Rings of plates round a middle `mid` along the bone, from the pole `pole` (fwd, out, up) down to `polar` degrees: a
    row of 1 is a cap (one flat plate of `capn` sides over the pole), the rest as `band`'s. `brow` (a helmet, beastfit):
    its middle at this share of its bone's flesh's height (rest pose), over the middle of that flesh above it, and only the
    flesh above it measured: the cranium, the face below kept clear."""
    return P(kind="dome", mid=mid, u=(0.0, polar), rows=rows, pole=pole, span=span, cw=None, step=step, lap=lap, gap=gap,
             ridge=0.0, ridge_rows=(), flare=0.0, lift=lift, tiles=tiles, spike=spike, line=None, heights=heights, turn=0.0,
             brow=brow)


SH_OGRE = ("upperarm_l", "upperarm_r")  # (the ogre's shoulders: his chest's side to side)
HIP_OGRE = ("thigh_l", "thigh_r")
SH_WIZ = ("arm_l", "arm_r")
WIZ_BELLY = ("pelvis", 1.0, "tail1", 0.0)
PEC = (1.0, 1.7, 1.7, 1.0)  # (a breastplate's top row: two broad panels either side of its ridge)

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
    # The ogre (beastplates: faceted, his mockup of 2026-11-27): a cuirass of three (a breastplate of angled panels with a
    # ridge and a raised collar edge, a back plate, a belt band and faulds), a helmet, layered pauldrons over upper arms in
    # bands, forearms in bands, thighs in bands, and his shins with knee cops (parts of the thighs' slots); his twisted
    # stance's middle from his shoulders and hips (beastlook's symmetric measuring).
    P(name="ogre", model="progs/ogre.mdl", seeds="ogre", stand=0.6, thick=1.0, group=(0, 8, 9), facet=True, fit="rest", pct=88, cap_r=1.3,
      skip=("death", "bdeath", "base", "frame", "pull"), pieces=[  # (pull: frames he never plays)
        fpiece(0, "chest", SH_OGRE, [
            band((1.1, 0.0), (4, 4, 4), span=190, cw=(PEC, None, None), ridge=0.9, ridge_rows=(0, 1), flare=0.9,
                 tiles=("panel", "plain", "rib"))], mid=True, face="front"),
        fpiece(8, "chest", SH_OGRE, [band((1.05, 0.05), (4, 4, 4), span=160)], mid=True, face="back"),
        fpiece(9, "pelvis", HIP_OGRE, [
            band((0.98, 0.72), (10,), step=0.2, lap=0.3, tiles=("panel",)),
            band((0.72, 0.1), (4,), span=150, tiles=("rib",)),
            band((0.72, 0.0), (4,), span=150, turn=180)], mid=True, face="front"),
        fpiece(1, "head", SH_OGRE, [dome(0.45, 82, (1, 6, 8), (-0.15, 0.0, 1.0), heights=(0.6, 1, 1), brow=0.62)], mid=True,
               face="back"),
        fpiece(2, "upperarm_l", SH_OGRE, [
            dome(0.0, 72, (1, 5, 6), (0.0, 0.7, 1.0), heights=(0.7, 1, 1)),
            band((0.45, 0.95), (6, 6), lift=0.2)], pair="upperarm_r"),
        fpiece(3, "upperarm_r", SH_OGRE, [
            dome(0.0, 72, (1, 5, 6), (0.0, 0.7, 1.0), heights=(0.7, 1, 1)),
            band((0.45, 0.95), (6, 6), lift=0.2)], pair="upperarm_l"),
        fpiece(4, "forearm_l", SH_OGRE, [band((0.1, 0.92), (6, 6, 6), tiles=("plain", "plain", "vent"))], pair="forearm_r"),
        fpiece(5, "forearm_r", SH_OGRE, [band((0.1, 0.92), (6, 6, 6), tiles=("plain", "plain", "vent"))], pair="forearm_l"),
        # (beastfit: the thighs down to the knee; the shins (their own Studio part) with a knee cop from above the knee)
        fpiece(6, "thigh_l", HIP_OGRE, [band((0.05, 1.0), (6, 6), span=300)], pair="thigh_r", face="front"),
        fpiece(7, "thigh_r", HIP_OGRE, [band((0.05, 1.0), (6, 6), span=300)], pair="thigh_l", face="front"),
        fpiece(6, "shin_l", HIP_OGRE, [
            band((-0.3, 0.12), (3,), span=200, ridge=0.7, ridge_rows=(0,), flare=0.4, tiles=("rim",)),
            band((0.12, 0.95), (6, 6, 6), span=320, tiles=("plain", "plain", "vent"))], pair="shin_r", face="front",
            part="shin"),
        fpiece(7, "shin_r", HIP_OGRE, [
            band((-0.3, 0.12), (3,), span=200, ridge=0.7, ridge_rows=(0,), flare=0.4, tiles=("rim",)),
            band((0.12, 0.95), (6, 6, 6), span=320, tiles=("plain", "plain", "vent"))], pair="shin_l", face="front",
            part="shin"),
    ]),
    # The scrag (beastplates: faceted, his mockup): a cuirass of three (a plastron of two angled pec panels on a ridge over
    # stacked bands, a raised collar round his neck, a back plate; a riveted belt over stacked bands narrowing down, his tail
    # in overlapping segments down to a spike: parts of the belt's slot), a mask, spiked layered shoulder plates, plates over
    # his claws' roots; his middle from his shoulders.
    P(name="wizard", model="progs/wizard.mdl", seeds="wizard", stand=0.5, thick=0.8, group=(0, 8, 9), facet=True, fit="rest", pct=88, cap_r=1.3,
      pieces=[
        fpiece(0, "chest", SH_WIZ, [
            band((1.14, 0.97), (4,), span=190, step=0.15, lap=0.3, flare=1.3, lift=0.5),
            band((0.97, 0.55), (4,), span=180, cw=(PEC,), ridge=0.8, ridge_rows=(0,), tiles=("panel",)),
            band((0.55, 0.0), (4, 4), span=160, ridge=0.4, ridge_rows=(0, 1), tiles=("rib", "rib"))],
            mid=True, face="front"),
        fpiece(9, "chest", SH_WIZ, [
            band((1.14, 0.97), (4,), span=180, step=0.15, lap=0.3, flare=1.3, lift=0.5),
            band((0.97, 0.05), (4, 4, 4), span=170)], mid=True, face="back"),
        fpiece(8, "pelvis", SH_WIZ, [
            # (round the line down from his waist to his tail's root: his body bends back into it)
            band((-0.02, 0.14), (10,), step=0.2, lap=0.3, tiles=("panel",), line=WIZ_BELLY),
            band((0.14, 1.0), (8, 8, 7), line=WIZ_BELLY)], mid=True, face="front"),
        fpiece(8, "tail1", SH_WIZ, [band((0.0, 1.0), (6, 6))], face="up", part="tail"),
        fpiece(8, "tail2", SH_WIZ, [band((0.0, 1.0), (6, 6))], face="up", part="tail"),
        fpiece(8, "tail3", SH_WIZ, [band((0.0, 1.0), (6, 5))], face="up", part="tail"),
        fpiece(8, "tail4", SH_WIZ, [band((0.0, 0.9), (5, 5), lift=0.2, spike=(1.1, 0, 5.0, 0.9, "axis"))], face="up", part="tail"),
        fpiece(1, "head", SH_WIZ, [dome(0.5, 82, (1, 6, 8), (-0.1, 0.0, 1.0), heights=(0.6, 1, 1), brow=0.6)], mid=True,
               face="front"),
        fpiece(2, "arm_l", SH_WIZ, [dome(0.35, 65, (1, 5, 6), (0.0, 0.5, 1.0), heights=(0.6, 1, 1),
                                    spike=(0.8, 0, 5.5, 1.3, (0.0, 1.0, -0.3)))],
               pair="arm_r"),
        fpiece(3, "arm_r", SH_WIZ, [dome(0.35, 65, (1, 5, 6), (0.0, 0.5, 1.0), heights=(0.6, 1, 1),
                                    spike=(0.8, 0, 5.5, 1.3, (0.0, 1.0, -0.3)))],
               pair="arm_l"),
        fpiece(4, "hand_l", SH_WIZ, [band((0.0, 0.35), (3,), span=150, tiles=("vent",))], pair="hand_r", face="up", cap_r=3.0),
        fpiece(5, "hand_r", SH_WIZ, [band((0.0, 0.35), (3,), span=150, tiles=("vent",))], pair="hand_l", face="up", cap_r=3.0),
    ]),
    # The fiend: plates over his hunched back and shoulders and over his haunches, a breastplate under his chest (the cuirass:
    # three), a helmet, pauldrons, bracers, plates over his thighs.
    P(name="demon", model="progs/demon.mdl", seeds="demon", stand=0.7, thick=1.1, group=(0, 8, 9), pieces=[
        piece(0, "chest", (0.0, 1.0), "up", 170, rows=2, tile="panel"),
        piece(9, "chest", (0.1, 0.9), "down", 120, rows=2, tile="rib"),
        piece(8, "pelvis", (-0.6, 1.0), "up", 200, rows=2, tile="plain"),
        piece(1, "head", (0.1, 0.9), "up", 160, rows=1, tile="plain", cap=True),
        piece(2, "upperarm_l", (-0.4, 0.8), "out", 180, rows=2, tile="emblem", pair="upperarm_r"),
        piece(3, "upperarm_r", (-0.4, 0.8), "out", 180, rows=2, tile="emblem"),
        piece(4, "forearm_l", (0.1, 0.9), "out", 360, rows=2, tile="vent", pair="forearm_r"),
        piece(5, "forearm_r", (0.1, 0.9), "out", 360, rows=2, tile="vent"),
        piece(6, "thigh_l", (-0.2, 0.9), "out", 170, rows=2, tile="plain", pair="thigh_r"),
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


def shell(kind, spec, seeds, pos, frames=None, want_rad=False):
    """The piece's lames: a list of (outer grid, inner grid) arrays [rows+1 along, cols+1 round, 3], its frame."""
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
    if want_rad:
        return rad, length
    if spec.get("pair"):
        # (beastfit: the right side's shape on this bone, mirrored: its angles run the other way round a left bone)
        rad, length = shell(kind, dict(spec, bone=spec["pair"], pair=None), seeds, pos, frames, want_rad=True)
        rad = rad[:, ::-1]
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
    names, poses = frames[:2]
    centres = np.array([b[2] for b in seeds])
    label = np.argmin(((pos[:, None, :] - centres[None]) ** 2).sum(-1), axis=1)
    # (the engine's own labels when listed: the vertices its fit follows, not the nearest seeds')
    for line in open(os.path.join(HERE, "beastarmour_labels.txt")):
        f = line.split()
        if f and f[0] == kind["model"] and len(f) - 1 == len(pos):
            label = np.array([int(x) for x in f[1:]])
    keep = [f for f, n in enumerate(names) if not n.startswith(kind.get("skip", ("death", "bdeath", "base", "frame")))]
    return dict(seeds=seeds, byname={b[0]: (i, b) for i, b in enumerate(seeds)}, label=label, pos=pos,
                poses=poses[keep], fit=kind.get("fit", "frames"),
                pct=kind.get("pct", 100), cap_r=kind.get("cap_r", 1.6),
                tris=np.array(frames[2]) if len(frames) > 2 else None)


def region_tris(ctx, bones):
    """The rest pose's triangles of the bones' skin (any corner on one of them): the surface a beastfit piece lies on."""
    want = [ctx["byname"][b][0] for b in bones]
    t = ctx["tris"]
    t = t[np.isin(ctx["label"][t], want).any(axis=1)]
    p = ctx["pos"]
    return p[t[:, 0]], p[t[:, 1]], p[t[:, 2]]


def cast_out(O, D, tri, reach=60.0):
    """Each ray's (O + D s, 0 < s < reach) farthest crossing of the triangles: the skin's outer surface that way (nan
    none). The beastfit measure: the body's skin itself, not a percentile of its few vertices (the ogre's upper arm has 20)."""
    A, B, C = tri
    e1, e2 = B - A, C - A
    out = np.full(len(O), np.nan)
    for i0 in range(0, len(O), 512):
        o, d = O[i0:i0 + 512, None], D[i0:i0 + 512, None]
        p = np.cross(d, e2[None])
        det = (e1[None] * p).sum(-1)
        ok = np.abs(det) > 1e-9
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        tv = o - A[None]
        u = (tv * p).sum(-1) * inv
        q = np.cross(tv, e1[None])
        v = (d * q).sum(-1) * inv
        s = (e2[None] * q).sum(-1) * inv
        hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (s > 0) & (s < reach)
        far = np.where(hit, s, -1.0).max(axis=1)
        out[i0:i0 + 512] = np.where(far > 0, far, np.nan)
    return out


def sym_cloud(ctx, bones):
    """The bones' flesh in every kept frame carried back to the rest pose by the first bone's rigid fit."""
    want = [ctx["byname"][b][0] for b in bones]
    mine = np.where(np.isin(ctx["label"], want))[0]
    own = np.where(ctx["label"] == want[0])[0]
    rest = ctx["pos"]
    # (the engine's anchors: spread over the bone, the first nearest the piece's middle: `first`, else its flesh's)
    mid = ctx.get("first", {}).get(bones[0])
    mid = rest[own].mean(0) if mid is None else mid
    anc = [own[np.argmin(np.linalg.norm(rest[own] - mid, axis=1))]]
    while len(anc) < 10 and len(anc) < len(own):
        d = np.min(np.linalg.norm(rest[own][:, None] - rest[anc][None], axis=2), axis=1)
        k = int(np.argmax(d))
        if d[k] < 0.25:
            break
        anc.append(own[k])
    out = [rest[mine]]
    if ctx["fit"] == "rest":
        # (beastfit: the skin as it stands in the rest pose, where the engine lays the piece on; the played frames' flesh
        # carried back by a rigid fit swings at the joints, and its envelope made every plate stand units off the skin)
        return out[0]
    for p in ctx["poses"]:
        r, t = kabsch(rest[anc], p[anc])
        out.append((p[mine] - t) @ r)
    return np.concatenate(out)


def sym_frame(ctx, spec, bone, side, line=None):
    _, b = ctx["byname"][bone]
    pivot, end = np.array(b[3]), np.array(b[4])
    if line:  # (a band's own line: (bone, t, bone, t) of the rest pose)
        ends = []
        for nm, t in ((line[0], line[1]), (line[2], line[3])):
            bb = ctx["byname"][nm if side > 0 else nm.replace("_l", "_r")][1]
            ends.append(np.array(bb[3]) + (np.array(bb[4]) - np.array(bb[3])) * t)
        pivot, end = ends
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
    if ctx["fit"] == "rest":
        # (beastfit: rays from its line out to its bone's skin, the outer crossing)
        tri = region_tris(ctx, [bone] + [m if side > 0 else m.replace("_l", "_r") for m in spec["more"]])
        tt, pp = np.meshgrid(ts, ps, indexing="ij")
        o = pivot + a * (tt.reshape(-1, 1) * length)
        d = np.cos(pp).reshape(-1, 1) * r0 + np.sin(pp).reshape(-1, 1) * r1
        return cast_out(o, d, tri).reshape(tt.shape)
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
                rad[i, j] = np.percentile(sel, spec["pct"] or ctx["pct"])
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


# --- Faceted plates (beastplates, ROUND26.md: the ogre and the scrag) ---------------------------------------------------
# The humanoids' look (make_agarmor.py's atlas, vr_agarmor.cpp addPlate's slab): every plate a flat trapezoid on ONE
# tile of the atlas (its bevel, trim, corner rivets and the under-layer at its top painted there): a face inset by its
# bevel, the bevel round it, its sides on the rim (the dark gaps between plates), its inside on the padding. Rows are
# shingled: each row's top, thin, tucked under the row above, whose lower edge stands proud and laps over it; a gap
# between a row's plates. A piece is shapes of rows, a band along its bone or a dome round a pole, laid over the body's
# envelope as measured above (symmetric pairs and middles, every frame), each corner raised so its plate's flat face
# clears the flesh under it.


def body_axes(ctx, ref):
    """The body's forward, left and up where its `ref` pair (shoulders, hips) stands: level, square to the pair."""
    pl, pr = ctx["byname"][ref[0]][1][3], ctx["byname"][ref[1]][1][3]
    lat = unit(np.array(pl) - np.array(pr))
    lat = unit(lat - np.array([0.0, 0.0, lat[2]]))
    return np.cross(lat, [0.0, 0.0, 1.0]), lat, np.array([0.0, 0.0, 1.0])


def dome_frame(ctx, spec, shape, bone, side):
    pivot, a, length, _, _ = sym_frame(ctx, spec, bone, side)
    fwd, lat, up = body_axes(ctx, spec["ref"])
    pole = unit(shape["pole"][0] * fwd + shape["pole"][1] * lat * side + shape["pole"][2] * up)
    f = {"front": fwd, "back": -fwd, "out": lat * side, "up": up, "down": -up}[spec["face"]]
    r0 = f - pole * float(np.dot(f, pole))
    r0 = unit(r0 if np.linalg.norm(r0) > 1e-3 else np.cross(pole, lat))
    r1 = np.cross(pole, r0) * side
    if shape.get("brow") is not None:
        flesh = ctx["pos"][ctx["label"] == ctx["byname"][bone][0]]
        z0, z1 = flesh[:, 2].min(), flesh[:, 2].max()
        zb = z0 + shape["brow"] * (z1 - z0)
        top = flesh[flesh[:, 2] >= zb]
        c = np.array([*((top[:, :2].min(0) + top[:, :2].max(0)) * 0.5), zb])
        return c, pole, r0, r1
    return pivot + a * (shape["mid"] * length), pole, r0, r1


def dome_radius(ctx, spec, bone, side, fr, us, ps):
    """The flesh's radius from a dome's middle by polar angle (us) and angle round its pole (ps)."""
    c, pole, r0, r1 = fr
    if ctx["fit"] == "rest":
        # (beastfit: rays from its middle out to its bone's skin)
        tri = region_tris(ctx, [bone] + [m if side > 0 else m.replace("_l", "_r") for m in spec["more"]])
        uu, pp = np.meshgrid(us, ps, indexing="ij")
        d = (np.cos(uu)[..., None] * pole + np.sin(uu)[..., None] * (np.cos(pp)[..., None] * r0 + np.sin(pp)[..., None] * r1))
        return cast_out(np.repeat(c[None], uu.size, axis=0), d.reshape(-1, 3), tri).reshape(uu.shape)
    pts = sym_cloud(ctx, [bone] + [m if side > 0 else m.replace("_l", "_r") for m in spec["more"]]) - c
    if any(s.get("brow") is not None for s in spec["shapes"]):
        pts = pts[pts @ pole > -0.25]  # (a helmet: the cranium above its brow alone)
    d = np.linalg.norm(pts, axis=1)
    u = pts / np.maximum(d, 1e-6)[:, None]
    th = np.arccos(np.clip(u @ pole, -1.0, 1.0))
    ph = np.arctan2(u @ r1, u @ r0)
    rad = np.full((len(us), len(ps)), np.nan)
    for i, t in enumerate(us):
        near = np.abs(th - t) < math.radians(18)
        for j, p in enumerate(ps):
            # (round the pole every angle is near: there by the polar angle alone)
            dp = np.abs((ph[near] - p + math.pi) % (2 * math.pi) - math.pi) * max(math.sin(t), 0.3)
            sel = d[near][dp < math.radians(20)]
            if len(sel) >= 3:
                rad[i, j] = np.percentile(sel, spec["pct"] or ctx["pct"])
    return rad


def surface(ctx, spec, shape, side):
    """A shape's surface: at(u, p, r) (u along the bone, or the polar angle of a dome), the units a step of u is at radius
    r, its axis's point at u, and the flesh's radius over a fine grid (us, ps) as the symmetric kinds measure it."""
    ring = shape["span"] >= 359.0
    span = math.radians(shape["span"])
    u0, u1 = shape["u"]
    if shape["kind"] == "dome":
        u0, u1 = math.radians(u0), math.radians(u1)
    us = np.linspace(min(u0, u1), max(u0, u1), len(shape["rows"]) * 6 + 1)
    n = max(9, int(shape["span"] / 8.0))
    n += 1 - n % 2  # (odd: a sample on the middle line)
    ps = np.linspace(-math.pi, math.pi, n) if ring else np.linspace(-span / 2, span / 2, n) + shape["turn"]
    # (a pair: beastfit measures the right side alone and lays the same shape on the left, mirrored; before, the larger of
    # both sides' radii at their mean length)
    right = ctx["fit"] == "rest" and spec["pair"]
    src, sside = (spec["pair"], -side) if right and side > 0 else (spec["bone"], side)
    if shape["kind"] == "band":
        fr = sym_frame(ctx, spec, spec["bone"], side, shape["line"])
        fs = sym_frame(ctx, spec, src, sside, shape["line"])
        rad = sym_radius(ctx, spec, src, sside, us, ps, fs)
        length = fs[2]
        if spec["pair"] and not right:
            fo = sym_frame(ctx, spec, spec["pair"], -side, shape["line"])
            rad = np.fmax(rad, sym_radius(ctx, spec, spec["pair"], -side, us, ps, fo))
            length = (fr[2] + fo[2]) * 0.5
        pivot, a, _, r0, r1 = fr

        def at(u, p, r):
            return pivot + a * (u * length) + (math.cos(p) * r0 + math.sin(p) * r1) * r

        def axis(u):
            return pivot + a * (u * length)

        def ulen(r):
            return length
        way = a * math.copysign(1.0, u1 - u0)
    else:
        fr = dome_frame(ctx, spec, shape, spec["bone"], side)
        rad = dome_radius(ctx, spec, src, sside, dome_frame(ctx, spec, shape, src, sside), us, ps)
        if spec["pair"] and not right:
            rad = np.fmax(rad, dome_radius(ctx, spec, spec["pair"], -side, dome_frame(ctx, spec, shape, spec["pair"], -side),
                                           us, ps))
        c, pole, r0, r1 = fr

        def at(u, p, r):
            return c + (math.cos(u) * pole + math.sin(u) * (math.cos(p) * r0 + math.sin(p) * r1)) * r

        def axis(u):
            return c

        def ulen(r):
            return max(r, 1.0)
        way = pole
    if spec["mid"] and not spec["pair"]:
        # (mirrored across the middle plane: beastfit the mean of both sides, the twisted rest pose's larger side no more)
        rad = 0.5 * (rad + rad[:, ::-1]) if ctx["fit"] == "rest" else np.fmax(rad, rad[:, ::-1])
    rad = sym_smooth(dict(cap_r=spec["cap_r"] or ctx["cap_r"], even=0.0), rad, ring)
    return SimpleNamespace(at=at, axis=axis, ulen=ulen, us=us, ps=ps, rad=rad, ring=ring, span=span, u0=u0, u1=u1, way=way)


def wrapped(d):
    return np.abs((d + math.pi) % (2 * math.pi) - math.pi)


def node_radius(s, ua, ub, top, p, half):
    """The flesh's radius a plate's corner clears: the most of its row near it (round it: half its plates' widths), between
    the most of the row's half by the corner and of the whole row (a straight plate's edge over the row's bulge)."""
    lo, hi = min(ua, ub) - 1e-9, max(ua, ub) + 1e-9
    um = (ua + ub) * 0.5
    hl, hh = (min(ua, um) - 1e-9, max(ua, um) + 1e-9) if top else (min(um, ub) - 1e-9, max(um, ub) + 1e-9)
    rows = (s.us >= lo) & (s.us <= hi)
    halfrows = (s.us >= hl) & (s.us <= hh)
    if not rows.any():
        rows = halfrows = np.abs(s.us - um) == np.abs(s.us - um).min()
    dp = wrapped(s.ps - p) if s.ring else np.abs(s.ps - p)
    cols = dp <= half + 1e-9
    if not cols.any():
        cols = dp == dp.min()
    whole = float(s.rad[rows][:, cols].max())
    part = float(s.rad[halfrows][:, cols].max()) if halfrows.any() else whole
    return 0.5 * (whole + part)


def facet_plates(kind, spec, ctx):
    """A faceted piece's plates [(corners, thickness at each, outward, tile)] and spikes [(base, way, height, width)]."""
    side = -1.0 if spec["bone"].endswith("_r") else 1.0
    stand, thick = kind["stand"], kind["thick"]
    plates, spikes = [], []
    for si, shape in enumerate(spec["shapes"]):
        s = surface(ctx, spec, shape, side)
        rows = shape["rows"]
        dome_ = shape["kind"] == "dome"
        for k, ncol in enumerate(rows):
            share = np.concatenate([[0.0], np.cumsum(shape["heights"] or (1.0,) * len(rows))])
            share /= share[-1]
            ua = s.u0 + (s.u1 - s.u0) * share[k]
            ub = s.u0 + (s.u1 - s.u0) * share[k + 1]
            tiles = TILES[(shape["tiles"] or (spec["tile"],) * len(rows))[k]]
            # (every row's top under the row above, thin there; a piece's first edge whole)
            top_th = thick if (si == 0 and k == 0) else 0.35 * thick
            lift = stand + shape["lift"]
            if dome_ and ncol == 1:
                # A cap: one flat plate over the pole, its rim's corners clear of the flesh under its middle too.
                capn = 6
                ps = [-math.pi + 2 * math.pi * (i + 0.5) / capn for i in range(capn)]
                under = float(s.rad[s.us <= ub + 1e-9].max())
                chord = 1.0 / math.cos(math.pi / capn)
                r = [max(node_radius(s, 0.0, ub, False, p, math.pi / capn) * chord, under / max(math.cos(ub), 0.3)) for p in ps]
                lapu = shape["lap"] / s.ulen(float(np.mean(r)))
                c = [s.at(ub + lapu, p, rr + lift) for p, rr in zip(ps, r)]
                plates.append((np.array(c), [thick] * capn, s.axis(ub), tiles[(spec["slot"] + k) % len(tiles)]))
                continue
            ring = s.ring
            widths = np.array((shape["cw"] or (None,) * len(rows))[k] or (1.0,) * ncol, dtype=float)
            full = 2 * math.pi if ring else s.span
            start = -math.pi + (math.pi / ncol if dome_ and k % 2 == 0 else 0.0) if ring else shape["turn"] - s.span / 2
            bnd = start + np.concatenate([[0.0], np.cumsum(widths)]) / widths.sum() * full
            wide = np.diff(bnd)
            rt, rb = [], []
            for j, p in enumerate(bnd):
                near = max(wide[j - 1] if j > 0 else (wide[-1] if ring else 0.0), wide[j] if j < ncol else (wide[0] if ring else 0.0))
                chord = 1.0 / math.cos(min(near / 2, 1.1))
                if dome_:
                    chord /= math.cos(min(abs(ub - ua) / 2, 1.1))
                ridge = shape["ridge"] if k in shape["ridge_rows"] and abs(math.remainder(p, 2 * math.pi)) < 1e-6 else 0.0
                rt.append(node_radius(s, ua, ub, True, p, near / 2) * chord + ridge)
                rb.append(node_radius(s, ua, ub, False, p, near / 2) * chord + ridge)
            for j in range(ncol):
                pa, pb = bnd[j], bnd[j + 1]
                r_mean = 0.25 * (rt[j] + rt[j + 1] + rb[j] + rb[j + 1])
                g = shape["gap"] * 0.5 / max(r_mean, 1.0)
                pa, pb = pa + g, pb - g
                lapu = shape["lap"] / s.ulen(r_mean) * math.copysign(1.0, s.u1 - s.u0)
                flare = shape["flare"] if k == 0 else 0.0
                c = [s.at(ua, pa, rt[j] + lift + flare), s.at(ua, pb, rt[j + 1] + lift + flare),
                     s.at(ub + lapu, pb, rb[j + 1] + lift + shape["step"]), s.at(ub + lapu, pa, rb[j] + lift + shape["step"])]
                plates.append((np.array(c), [top_th, top_th, thick, thick], s.axis((ua + ub) * 0.5),
                               tiles[(spec["slot"] + 3 * k + j) % len(tiles)]))
        if shape["spike"]:
            f, pdeg, h, w, way = shape["spike"]
            u = s.u0 + (s.u1 - s.u0) * f
            if way == "axis":
                spikes.append((s.axis(u) + s.way * (shape["lap"] if not dome_ else 0.0), unit(s.way), h, w))
            else:
                fwd, lat, up = body_axes(ctx, spec["ref"])
                p = math.radians(pdeg)
                r = node_radius(s, u, u, True, p, 0.3)
                spikes.append((s.at(u, p, r + lift + thick * 0.6), unit(way[0] * fwd + way[1] * lat * side + way[2] * up), h, w))
    return plates, spikes


def emit_plate(b, c, th, inside, tile):
    """One plate as vr_agarmor.cpp addPlate's slab: its face (inset by its bevel) on the tile's face, the bevel round it
    from the tile's edge, its sides down on the rim, its inside on the padding. Corners c[0..n] (a quad's top edge first:
    the tile's top, its under-layer, is the plate's top), th the thickness at each, `inside` a point within the body."""
    n_ = len(c)
    cen = c.mean(axis=0)
    nrm = np.zeros(3)
    for i in range(n_):  # (Newell's normal)
        a, d = c[i], c[(i + 1) % n_]
        nrm += np.array([(a[1] - d[1]) * (a[2] + d[2]), (a[2] - d[2]) * (a[0] + d[0]), (a[0] - d[0]) * (a[1] + d[1])])
    nrm = unit(nrm)
    if np.dot(nrm, cen - inside) < 0.0:
        nrm = -nrm
    rect = tile_rect(tile)
    if n_ == 4:
        uv = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
    else:
        e1 = unit(c[0] - cen)
        e2 = np.cross(nrm, e1)
        q = np.array([[np.dot(x - cen, e1), np.dot(x - cen, e2)] for x in c])
        q = (q - q.min(0)) / np.maximum(q.max(0) - q.min(0), 1e-6)
        uv = [tuple(x) for x in q]
    uvf = [(0.5 + (u - 0.5) * (1 - 2 * make_agarmor.BEVEL), 0.5 + (v - 0.5) * (1 - 2 * make_agarmor.BEVEL)) for u, v in uv]
    edge = [c[i] + nrm * th[i] * 0.45 for i in range(n_)]
    face = []
    for i in range(n_):
        d = cen - c[i]
        dl = float(np.linalg.norm(d))
        bw = min(0.55 * max(th), 0.3 * dl)
        face.append(c[i] + (d / max(dl, 1e-6)) * bw + nrm * th[i])
    for i in range(1, n_ - 1):
        b.quad([face[0], face[i], face[i + 1], face[i + 1]],
               [st(rect, *uvf[0]), st(rect, *uvf[i]), st(rect, *uvf[i + 1]), st(rect, *uvf[i + 1])], nrm)
        b.quad([c[0], c[i], c[i + 1], c[i + 1]], [st(INNER, *uv[0]), st(INNER, *uv[i]), st(INNER, *uv[i + 1]),
                                                  st(INNER, *uv[i + 1])], -nrm)
    for i in range(n_):
        j = (i + 1) % n_
        out = unit((c[i] + c[j]) * 0.5 - cen)
        b.quad([edge[i], edge[j], face[j], face[i]], [st(rect, *uv[i]), st(rect, *uv[j]), st(rect, *uvf[j]), st(rect, *uvf[i])],
               nrm + out)
        b.quad([c[i], c[j], edge[j], edge[i]], [st(RIM, 0, 1), st(RIM, 1, 1), st(RIM, 1, 0), st(RIM, 0, 0)], out)


def emit_spike(b, base, way, h, w):
    s1 = unit(np.cross(way, [0.0, 0.0, 1.0] if abs(way[2]) < 0.9 else [1.0, 0.0, 0.0]))
    s2 = np.cross(way, s1)
    tip = base + way * h
    corners = [base + (s1 + s2) * w, base + (s2 - s1) * w, base - (s1 + s2) * w, base + (s1 - s2) * w]
    for q in range(4):
        p0, p1 = corners[q], corners[(q + 1) % 4]
        c = (p0 + p1 + tip) / 3.0
        b.quad([p0, p1, tip, tip], [st(RIM, 0, 1), st(RIM, 1, 1), st(RIM, 0.5, 0), st(RIM, 0.5, 0)], c - base + way * 0.1)


def piece_key(spec):
    return "%d %s" % (spec["slot"], spec["bone"])


def facet_context(kind, spec, seeds, pos, frames):
    """The kind's measuring context (made once), the engine's first anchors set for this piece and its pair: their
    middles as a first pass made them (a piece rides the fit of anchors from its middle out, so it is measured so)."""
    if "ctx" not in kind:
        kind["ctx"] = sym_context(kind, seeds, pos, frames)
        middles = {}
        for p in kind["pieces"]:
            kind["ctx"]["first"] = {}
            c = []
            build_facets(kind, p, kind["ctx"], c)
            middles[piece_key(p)] = np.array(c)
        kind["middles"] = middles
    ctx = kind["ctx"]
    ctx["first"] = {spec["bone"]: kind["middles"][piece_key(spec)]}
    if spec["pair"]:
        mate = next(p for p in kind["pieces"] if p["bone"] == spec["pair"] and p["pair"] == spec["bone"])
        ctx["first"][spec["pair"]] = kind["middles"][piece_key(mate)]
    return ctx


def build_facets(kind, spec, ctx, centre_out):
    plates, spikes = facet_plates(kind, spec, ctx)
    b = Builder()
    for c, th, inside, tile in plates:
        emit_plate(b, c, th, inside, tile)
    for base, way, h, w in spikes:
        emit_spike(b, base, way, h, w)
    return centred(b, centre_out)


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
    if kind.get("facet"):
        return build_facets(kind, spec, facet_context(kind, spec, seeds, pos, frames), centre_out)
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
    return centred(b, centre_out)


def centred(b, centre_out):
    """The mesh about its box's middle (written to centre_out: the piece's middle in the rest pose)."""
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
        "//         [a slot's part: its own armour.txt part (tail, shin), else its slot's]",
    ]
    for kind in KINDS:
        pos = load_mdl(os.path.join(progs, os.path.basename(kind["model"]))).pos
        frames = None
        if kind.get("facet"):
            names, poses, tris = ragdoll_mdl.load(os.path.join(progs, os.path.basename(kind["model"])))
            frames = (names, poses, tris)
        group = sum(1 << s for s in kind["group"])
        lines.append("kind %s %d" % (kind["model"], group))
        for spec in kind["pieces"]:
            centre = []
            mesh = build_piece(kind, spec, seeds[kind["seeds"]], pos, centre, frames)
            # (a slot's later piece, a part of it on another bone: named by its bone too)
            first = spec is next(p for p in kind["pieces"] if p["slot"] == spec["slot"])
            path = "progs/vrbarmor_%s_%d%s.mdl" % (kind["name"], spec["slot"], "" if first else "_" + spec["bone"])
            meshes.append((os.path.join(ROOT, "quakevr", path.replace("/", os.sep)), mesh, path))
            lines.append("piece %s %d %s %s %.2f %.2f %.2f%s" % (kind["model"], spec["slot"], spec["bone"], path, *centre,
                                                                 " " + spec["part"] if spec.get("part") else ""))
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
