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


KINDS = [
    # The rottweiler: barding over his chest and croup (the cuirass: two pieces), a chanfron down his face, a spiked collar,
    # plates over his shoulders and haunches.
    P(name="dog", model="progs/dog.mdl", seeds="dog", stand=0.5, thick=0.8, group=(0, 8), pieces=[
        piece(0, "chest", (0.0, 1.0), "up", 220, rows=3, tile="panel"),
        piece(8, "pelvis", (-0.25, 1.0), "up", 220, rows=2, tile="rib"),
        piece(1, "head", (0.2, 0.95), "up", 130, rows=2, tile="plain"),
        piece(9, "chest", (0.82, 1.08), "up", 360, rows=1, tile="rim", spikes=10, stand=0.7),
        piece(2, "upperleg_fl", (-0.15, 0.7), "out", 150, rows=2, tile="emblem"),
        piece(3, "upperleg_fr", (-0.15, 0.7), "out", 150, rows=2, tile="emblem"),
        piece(6, "thigh_bl", (-0.1, 0.75), "out", 150, rows=2, tile="plain"),
        piece(7, "thigh_br", (-0.1, 0.75), "out", 150, rows=2, tile="plain"),
    ]),
    # The ogre: a cuirass of three (breastplate, back plate, the faulds over his belly), a helmet, pauldrons, bracers and
    # tassets over his thighs.
    P(name="ogre", model="progs/ogre.mdl", seeds="ogre", stand=0.7, thick=1.1, group=(0, 8, 9), pieces=[
        piece(0, "chest", (0.0, 1.05), (1.0, 0.0, 0.0), 170, rows=3, tile="panel"),
        piece(8, "chest", (0.0, 1.05), (-1.0, 0.0, 0.0), 170, rows=3, tile="plain"),
        piece(9, "pelvis", (-0.2, 1.0), (1.0, 0.0, 0.0), 180, rows=2, tile="rib"),
        piece(1, "head", (0.25, 1.0), (-1.0, 0.0, 0.0), 250, rows=2, tile="plain", cap=True),
        piece(2, "upperarm_l", (-0.15, 0.55), "out", 200, rows=3, tile="emblem"),
        piece(3, "upperarm_r", (-0.15, 0.55), "out", 200, rows=3, tile="emblem"),
        piece(4, "forearm_l", (0.25, 0.9), "out", 360, rows=2, tile="vent"),
        piece(5, "forearm_r", (0.25, 0.9), "out", 360, rows=2, tile="vent"),
        piece(6, "thigh_l", (0.0, 0.75), (1.0, 0.0, 0.0), 160, rows=2, tile="plain"),
        piece(7, "thigh_r", (0.0, 0.75), (1.0, 0.0, 0.0), 160, rows=2, tile="plain"),
    ]),
    # The scrag: a breastplate and a back plate on his chest and a belly plate on his ribbed belly (the cuirass: three), a
    # mask over his face, plates on his shoulders, bracers at his claws' wrists.
    P(name="wizard", model="progs/wizard.mdl", seeds="wizard", stand=0.5, thick=0.8, group=(0, 8, 9), pieces=[
        piece(0, "chest", (0.0, 1.0), (1.0, 0.0, 0.0), 170, rows=3, tile="panel"),
        piece(9, "chest", (0.0, 1.0), (-1.0, 0.0, 0.0), 160, rows=2, tile="plain"),
        piece(8, "pelvis", (-0.6, 1.0), (1.0, 0.0, 0.0), 170, rows=3, tile="rib"),
        piece(1, "head", (0.15, 0.9), (1.0, 0.0, 0.0), 150, rows=2, tile="plain", cap=True),
        piece(2, "arm_l", (-0.1, 0.8), (0.0, 0.0, 1.0), 200, rows=2, tile="emblem"),
        piece(3, "arm_r", (-0.1, 0.8), (0.0, 0.0, 1.0), 200, rows=2, tile="emblem"),
        piece(4, "hand_l", (-0.05, 0.4), (0.0, 0.0, 1.0), 360, rows=1, tile="vent"),
        piece(5, "hand_r", (-0.05, 0.4), (0.0, 0.0, 1.0), 360, rows=1, tile="vent"),
    ]),
    # The fiend: plates over his hunched back and shoulders and over his haunches, a breastplate under his chest (the cuirass:
    # three), a helmet, pauldrons, bracers, plates over his thighs.
    P(name="demon", model="progs/demon.mdl", seeds="demon", stand=0.7, thick=1.1, group=(0, 8, 9), pieces=[
        piece(0, "chest", (0.0, 1.0), "up", 200, rows=3, tile="panel"),
        piece(9, "chest", (0.1, 0.9), "down", 120, rows=2, tile="rib"),
        piece(8, "pelvis", (-0.6, 1.0), "up", 200, rows=2, tile="plain"),
        piece(1, "head", (0.1, 0.9), "up", 160, rows=2, tile="plain", cap=True),
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


def shell(kind, spec, seeds, pos):
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
    stand = spec["stand"] or kind["stand"]
    thick = spec["thick"] or kind["thick"]

    def at(t, p, r):
        return pivot + a * (t * length) + (math.cos(p) * r0 + math.sin(p) * r1) * r

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
                bt = 0.0 if 0 < i < sub else (0.45 * thick / length) * (1 if i == 0 else -1)
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


def build_piece(kind, spec, seeds, pos, centre_out):
    lames, spikes, ring = shell(kind, spec, seeds, pos)
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
        group = sum(1 << s for s in kind["group"])
        lines.append("kind %s %d" % (kind["model"], group))
        for spec in kind["pieces"]:
            centre = []
            mesh = build_piece(kind, spec, seeds[kind["seeds"]], pos, centre)
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
