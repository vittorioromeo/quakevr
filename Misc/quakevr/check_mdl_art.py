#!/usr/bin/env python3
# check_mdl_art.py -- an art lint for Quake VR's own models (the polishing pass of 2026-10-10: "see if there's any
# model/texture that has issues ... fixing UVs or slightly improving the looks"). It finds, without rendering:
#
#   uv-oob       a vertex's skin coordinate outside the skin (the onseam half included): it reads the far edge
#   degenerate   a triangle with a repeated corner, or no area in frame 0 (harmless, but dead weight)
#   uv-streak    a triangle of real size whose UVs are a line or a point over texels that are not one colour: the
#                texture smears across it
#   uv-stretch   a triangle whose texels are stretched more than 4:1 (streaks), or its texel density a quarter of the
#                model's (blurry next to its neighbours), on a triangle of real size
#   overlap      texels shared by triangles facing different ways that are not a mirrored pair (two parts painted
#                with one picture); mirrored overlap (id's halves) is counted, not flagged
#   fullbright   lone fullbright texels (224 and up: they glow in the dark) inside a painted area: stray specks
#   bleed        texels next to an island (the bilinear and mip ring) that nothing covers, with a colour far from the
#                island's: a dark or bright line along the seams when filtered (normal maps: always filtered)
#   normal       a vertex whose normal (Quake's table) points against the faces around it: lit from the wrong side
#   zfight       two triangles in one plane facing the same way and overlapping, with different texels
#   normalmap    a <model>_0_norm.png whose size does not match the skin's shape, or covered texels with z < 0
#
# Usage: python Misc/quakevr/check_mdl_art.py [-v] [model.mdl | model.md5mesh ...]   (default: the VR gear)
# Prints one line per model and finding ("HIGH", "MED", "LOW"); -v prints where (texel, position).

import glob
import math
import os
import re
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import quakeimage  # noqa: E402
from mdlpolish import HEADER, FULLBRIGHT, anorms, palette  # noqa: E402

PAL = palette()
LUMA = PAL @ np.array([0.299, 0.587, 0.114])

DEFAULT = ["v_*.mdl", "vr_mag*.mdl", "vr_shell*.mdl", "vr_round_*.mdl", "vr_pump*.mdl", "vr_ssg_*.mdl",
           "vrpouch*.mdl", "vrgadget*.mdl", "vrflashlight.mdl", "legholster.mdl", "vrpauldron*.mdl",
           "hand_base.mdl", "finger_*.mdl", "vr_muzzleflash.mdl", "hand_rig.md5mesh", "vrbody*.md5mesh"]


# ----------------------------------------------------------------------------
# Loading: a common form (positions frame 0, triangles, per-corner texel coordinates, skin, normals)


class Art:
    pass


def load_mdl(path):
    d = open(path, "rb").read()
    h = HEADER.unpack_from(d, 0)
    scale, origin = np.array(h[2:5]), np.array(h[5:8])
    ns, sw, sh, nv, nt, nf = h[12:18]
    off = HEADER.size
    skins = []
    for _ in range(ns):
        g, = struct.unpack_from("<i", d, off)
        off += 4
        n = 1
        if g:
            n, = struct.unpack_from("<i", d, off)
            off += 4 + 4 * n
        for k in range(n):
            if k == 0:
                skins.append(np.frombuffer(d[off:off + sw * sh], np.uint8).reshape(sh, sw))
            off += sw * sh
    st = np.array(struct.unpack_from("<%di" % (3 * nv), d, off), np.int64).reshape(nv, 3)
    off += 12 * nv
    tr = np.array(struct.unpack_from("<%di" % (4 * nt), d, off), np.int64).reshape(nt, 4)
    off += 16 * nt
    t, = struct.unpack_from("<i", d, off)
    off += 4
    if t:
        n, = struct.unpack_from("<i", d, off)
        off += 12 + 4 * n
    vb = np.frombuffer(d[off + 24:off + 24 + 4 * nv], np.uint8).reshape(nv, 4)
    a = Art()
    a.path, a.kind, a.w, a.h = path, "mdl", sw, sh
    a.skins = skins
    a.pos = vb[:, :3].astype(np.float64) * scale + origin
    a.tris = tr[:, 1:4]
    a.unit = float(np.max(scale))  # one byte step
    # Texel coordinates of each corner, as the engine reads them: (s + 0.5, t + 0.5), the back's onseam half over.
    uv = np.zeros((nt, 3, 2))
    for c in range(3):
        v = a.tris[:, c]
        s = st[v, 1] + np.where((tr[:, 0] == 0) & (st[v, 0] != 0), sw // 2, 0)
        uv[:, c, 0] = s + 0.5
        uv[:, c, 1] = st[v, 2] + 0.5
    a.uv = uv
    a.vnorm = anorms()[vb[:, 3]]
    a.rgb = lambda k=0: PAL[a.skins[k]]
    a.indexed = True
    return a


def quat_w(x, y, z):
    t = 1.0 - x * x - y * y - z * z
    return -math.sqrt(t) if t > 0 else 0.0


def qrot(q, v):
    w, x, y, z = q
    u = np.array([x, y, z])
    return v + 2.0 * np.cross(u, np.cross(u, v) + w * v)


def load_md5(path):
    text = open(path).read()
    num = r"(-?[\d.eE+-]+)"
    joints = []
    jt = re.search(r"joints \{(.*?)\}", text, re.S).group(1)
    for m in re.finditer(r'"[^"]*"\s+-?\d+\s+\(\s*%s\s+%s\s+%s\s*\)\s+\(\s*%s\s+%s\s+%s\s*\)' % ((num,) * 6), jt):
        p = np.array([float(m.group(i)) for i in (1, 2, 3)])
        x, y, z = (float(m.group(i)) for i in (4, 5, 6))
        joints.append((p, (quat_w(x, y, z), x, y, z)))
    mesh = re.search(r"mesh \{(.*)\}", text, re.S).group(1)
    shader = re.search(r'shader "([^"]*)"', mesh).group(1)
    verts = [(float(m.group(1)), float(m.group(2)), int(m.group(3)), int(m.group(4))) for m in
             re.finditer(r"\bvert \d+ \(\s*%s\s+%s\s*\)\s+(\d+)\s+(\d+)" % (num, num), mesh)]
    tris = [(int(m.group(1)), int(m.group(2)), int(m.group(3))) for m in
            re.finditer(r"\btri \d+ (\d+) (\d+) (\d+)", mesh)]
    weights = [(int(m.group(1)), float(m.group(2)), np.array([float(m.group(i)) for i in (3, 4, 5)])) for m in
               re.finditer(r"\bweight \d+ (\d+) %s \(\s*%s\s+%s\s+%s\s*\)" % ((num,) * 4), mesh)]
    pos = np.zeros((len(verts), 3))
    for i, (_, _, ws, wc) in enumerate(verts):
        for j in range(ws, ws + wc):
            ji, bias, wp = weights[j]
            jp, jq = joints[ji]
            pos[i] += bias * (jp + qrot(jq, wp))
    base = os.path.splitext(path)[0]
    skinfile = None
    for cand in (base + "_00_00.tga", base + "_00_00.lmp", os.path.join(os.path.dirname(path), shader + "_00_00.tga"),
                 os.path.join(os.path.dirname(path), shader + "_00_00.lmp")):
        if os.path.exists(cand):
            skinfile = cand
            break
    a = Art()
    a.path, a.kind = path, "md5"
    raw = open(skinfile, "rb").read()
    if skinfile.endswith(".lmp"):
        w, h = struct.unpack_from("<2i", raw)
        a.skins = [np.frombuffer(raw[8:8 + w * h], np.uint8).reshape(h, w)]
        a.indexed = True
        a.rgb = lambda k=0: PAL[a.skins[k]]
    else:
        w, h, px = quakeimage.read_rgb(raw, "tga")[:3]
        img = np.frombuffer(bytes(px), np.uint8).reshape(h, w, -1)[:, :, :3].astype(np.float64)
        a.skins = [img]
        a.indexed = False
        a.rgb = lambda k=0: a.skins[k]
    a.w, a.h = w, h
    a.pos = pos
    a.tris = np.array(tris, np.int64)
    uvv = np.array([(u, v) for u, v, _, _ in verts])
    a.uv = np.stack([uvv[a.tris[:, c]] for c in range(3)], 1) * np.array([w, h])
    a.vnorm = None
    a.unit = 0.01
    return a


# ----------------------------------------------------------------------------
# Texel coverage


def raster(uv, w, h, step=0.25):
    """Texels (y, x) a triangle samples with nearest filtering: the texels its area (and corners) touch."""
    lo = np.floor(uv.min(0)).astype(int)
    hi = np.ceil(uv.max(0)).astype(int)
    xs = np.arange(lo[0] + step / 2, hi[0], step)
    ys = np.arange(lo[1] + step / 2, hi[1], step)
    out = set((int(math.floor(v)) % h, int(math.floor(u)) % w) for u, v in uv)
    if len(xs) == 0 or len(ys) == 0:
        return out
    X, Y = np.meshgrid(xs, ys)
    P = np.stack([X.ravel(), Y.ravel()], 1)
    a, b, c = uv
    v0, v1 = b - a, c - a
    den = v0[0] * v1[1] - v1[0] * v0[1]
    if abs(den) < 1e-9:
        # A line or a point: sample along the edges.
        for p, q in ((a, b), (b, c), (c, a)):
            n = int(np.linalg.norm(q - p) / step) + 1
            for k in range(n + 1):
                r = p + (q - p) * k / n
                out.add((int(math.floor(r[1])) % h, int(math.floor(r[0])) % w))
        return out
    d = P - a
    l1 = (d[:, 0] * v1[1] - v1[0] * d[:, 1]) / den
    l2 = (v0[0] * d[:, 1] - d[:, 0] * v0[1]) / den
    m = (l1 >= -1e-6) & (l2 >= -1e-6) & (l1 + l2 <= 1 + 1e-6)
    for u, v in P[m]:
        out.add((int(math.floor(v)) % h, int(math.floor(u)) % w))
    return out


# ----------------------------------------------------------------------------
# Checks


def tri_geo(a):
    p = a.pos[a.tris]
    n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    area = 0.5 * np.linalg.norm(n, axis=1)
    if a.kind == "mdl":
        n = -n  # Quake: clockwise seen from outside
    nn = n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    e1, e2 = a.uv[:, 1] - a.uv[:, 0], a.uv[:, 2] - a.uv[:, 0]
    uvarea = 0.5 * np.abs(e1[:, 0] * e2[:, 1] - e1[:, 1] * e2[:, 0])
    uvsign = np.sign(e1[:, 0] * e2[:, 1] - e1[:, 1] * e2[:, 0])
    return p, nn, area, uvarea, uvsign


def stretch(p, uv):
    """Singular values of the map from the triangle's plane to texels (texels per unit along its two axes)."""
    e1, e2 = p[1] - p[0], p[2] - p[0]
    x = e1 / max(np.linalg.norm(e1), 1e-12)
    nrm = np.cross(e1, e2)
    y = np.cross(nrm, x)
    y /= max(np.linalg.norm(y), 1e-12)
    P = np.array([[e1 @ x, e2 @ x], [e1 @ y, e2 @ y]])
    U = np.array([uv[1] - uv[0], uv[2] - uv[0]]).T
    if abs(np.linalg.det(P)) < 1e-12:
        return 0.0, 0.0
    J = U @ np.linalg.inv(P)
    s = np.linalg.svd(J, compute_uv=False)
    return s[0], s[1]


def check(a, verbose):
    out = []

    def add(sev, kind, n, msg, where=None):
        out.append((sev, kind, n, msg, where or []))

    w, h = a.w, a.h
    p, nn, area, uvarea, uvsign = tri_geo(a)
    total = area.sum()
    big = area > max(0.002 * total, (6 * a.unit) ** 2 if a.kind == "mdl" else 0.0)
    rgb = a.rgb(0)

    # uv-oob
    raw = a.uv - 0.5 if a.kind == "mdl" else a.uv
    bad = (raw[..., 0] < -1e-6) | (raw[..., 0] > w - (1 if a.kind == "mdl" else 0) + 1e-6) | \
          (raw[..., 1] < -1e-6) | (raw[..., 1] > h - (1 if a.kind == "mdl" else 0) + 1e-6)
    if bad.any():
        ti = np.nonzero(bad.any(1))[0]
        add("MED", "uv-oob", len(ti), "triangles reading past the skin's edge",
            ["tri %d uv %s" % (t, a.uv[t].round(1).tolist()) for t in ti[:8]])

    # degenerate
    rep = (a.tris[:, 0] == a.tris[:, 1]) | (a.tris[:, 1] == a.tris[:, 2]) | (a.tris[:, 0] == a.tris[:, 2])
    flat = area < 1e-7
    if (rep | flat).any():
        add("LOW", "degenerate", int((rep | flat).sum()), "triangles with no area (%d repeated corners)" % rep.sum())

    # coverage per triangle
    cov = [raster(a.uv[t], w, h) for t in range(len(a.tris))]
    used = np.zeros((h, w), bool)
    owners = {}
    for t, c in enumerate(cov):
        for yx in c:
            used[yx] = True
            owners.setdefault(yx, []).append(t)

    # uv-streak and uv-stretch
    streak, stretched, blurry = [], [], []
    dens = np.sqrt(uvarea / np.maximum(area, 1e-12))
    ok = (area > 1e-7) & (uvarea > 0.05)
    med = float(np.median(np.repeat(dens[ok], np.maximum(1, (area[ok] / area[ok].min()).astype(int)).clip(1, 50)))) \
        if ok.any() else 0.0
    for t in np.nonzero(big)[0]:
        if uvarea[t] < 0.5:
            cols = np.array([rgb[yx] for yx in cov[t]])
            spread = float(np.ptp(cols @ np.array([0.299, 0.587, 0.114]))) if len(cols) > 1 else 0.0
            if spread > 24:
                streak.append((t, spread))
            continue
        s1, s2 = stretch(p[t], a.uv[t])
        if s2 > 0 and s1 / s2 > 4.0 and s1 * math.sqrt(area[t]) > 3:
            stretched.append((t, s1 / s2))
        if dens[t] < med / 4.0:
            blurry.append((t, dens[t]))
    if streak:
        add("MED", "uv-streak", len(streak), "triangles of real size with line/point UVs over varied texels",
            ["tri %d at %s area %.2f luma spread %.0f" % (t, p[t].mean(0).round(1).tolist(), area[t], s)
             for t, s in streak[:10]])
    if stretched:
        add("LOW", "uv-stretch", len(stretched), "triangles stretched over 4:1",
            ["tri %d at %s ratio %.1f" % (t, p[t].mean(0).round(1).tolist(), r) for t, r in stretched[:10]])
    if blurry:
        add("LOW", "uv-density", len(blurry), "triangles at under a quarter of the model's texel density (%.2f/unit)" % med,
            ["tri %d at %s %.2f/unit area %.2f" % (t, p[t].mean(0).round(1).tolist(), d, area[t]) for t, d in blurry[:10]])

    # overlap
    mir = nonmir = 0
    nonmir_where = []
    weld = {}
    wid = np.array([weld.setdefault(tuple(q), len(weld)) for q in np.round(a.pos, 3)])
    tw = wid[a.tris]
    for yx, ts in owners.items():
        if len(ts) < 2:
            continue
        ts = np.array(ts)
        n = nn[ts]
        cosm = n @ n.T
        sg = uvsign[ts]
        share = (tw[ts][:, None, :, None] == tw[ts][None, :, None, :]).any((2, 3))
        bad_pair = (cosm < 0.0) & (sg[:, None] == sg[None, :]) & ~share
        mirror = (sg[:, None] != sg[None, :])
        if bad_pair.any():
            nonmir += 1
            if len(nonmir_where) < 8:
                i, j = np.argwhere(bad_pair)[0]
                nonmir_where.append("texel %s tris %d %d at %s / %s" % (yx, ts[i], ts[j], p[ts[i]].mean(0).round(1).tolist(),
                                                                       p[ts[j]].mean(0).round(1).tolist()))
        elif mirror.any():
            mir += 1
    if nonmir:
        add("LOW", "overlap", nonmir, "texels shared by triangles facing opposite ways, not mirrored (%d mirrored)" % mir,
            nonmir_where)

    # fullbright specks
    if a.indexed:
        sk = a.skins[0]
        fb = (sk >= FULLBRIGHT) & used
        lone = []
        for y, x in np.argwhere(fb):
            nb = sk[max(0, y - 1):y + 2, max(0, x - 1):x + 2]
            ub = used[max(0, y - 1):y + 2, max(0, x - 1):x + 2]
            if ((nb >= FULLBRIGHT) & ub).sum() <= 1 and ub.sum() >= 6:
                lone.append((y, x))
        if lone:
            add("MED", "fullbright", len(lone), "lone fullbright texels inside painted areas (glow specks)",
                ["texel (%d,%d) index %d" % (y, x, sk[y, x]) for y, x in lone[:10]])

    # bleed: the uncovered ring (8 neighbours) of the covered texels
    lum = rgb @ np.array([0.299, 0.587, 0.114])
    ring_bad = []
    ring_n = 0
    for y, x in np.argwhere(~used):
        y0, y1, x0, x1 = max(0, y - 1), min(h, y + 2), max(0, x - 1), min(w, x + 2)
        u = used[y0:y1, x0:x1]
        if not u.any():
            continue
        ring_n += 1
        m = lum[y0:y1, x0:x1][u].mean()
        if abs(lum[y, x] - m) > 48:
            ring_bad.append((y, x, lum[y, x] - m))
    if ring_bad:
        sev = "MED" if len(ring_bad) > 0.15 * max(ring_n, 1) else "LOW"
        add(sev, "bleed", len(ring_bad), "of %d seam-ring texels far from their island's colour (filtered/mips)" % ring_n,
            ["texel (%d,%d) luma %+.0f" % (y, x, d) for y, x, d in ring_bad[:10]])

    # vertex normals against their faces
    if a.vnorm is not None:
        acc = np.zeros_like(a.pos)
        for c in range(3):
            np.add.at(acc, a.tris[:, c], nn * area[:, None])
        # Each vertex against every triangle using it (mdlpolish.fix_inverted_normals' test): a vertex shared by the
        # two sides of a thin sheet agrees with one of them and is not counted.
        best = np.full(len(a.pos), -2.0)
        for c in range(3):
            d = np.where(area > 1e-9, (a.vnorm[a.tris[:, c]] * nn).sum(1), -2.0)
            np.maximum.at(best, a.tris[:, c], d)
        inv = [(int(i), float(best[i])) for i in np.nonzero((best > -2.0) & (best < 0.0))[0]]
        if inv:
            add("MED" if len(inv) > 3 else "LOW", "normal", len(inv), "vertex normals pointing against all their faces",
                ["vert %d at %s best dot %.2f" % (i, a.pos[i].round(1).tolist(), d) for i, d in inv[:10]])

    # z-fighting: same plane, same facing, overlapping, different texels
    zf = []
    planes = {}
    for t in np.nonzero(area > 1e-5)[0]:
        k = tuple(np.round(nn[t] * 20).astype(int)) + (int(round(nn[t] @ p[t][0] / 0.05)),)
        planes.setdefault(k, []).append(t)
    rng = np.random.default_rng(1)
    bary = rng.dirichlet((1, 1, 1), 12)
    for k, ts in planes.items():
        if len(ts) < 2 or len(ts) > 400:
            continue
        for i in range(len(ts)):
            for j in range(i + 1, len(ts)):
                t1, t2 = ts[i], ts[j]
                if len(set(a.tris[t1]) & set(a.tris[t2])) or abs(nn[t1] @ p[t1][0] - nn[t2] @ p[t2][0]) > 0.002:
                    continue
                if nn[t1] @ nn[t2] < 0.995:
                    continue
                pts = bary @ p[t1]
                inside = 0
                a2, b2, c2 = p[t2]
                for q in pts:
                    s1 = np.cross(b2 - a2, q - a2) @ nn[t2]
                    s2 = np.cross(c2 - b2, q - b2) @ nn[t2]
                    s3 = np.cross(a2 - c2, q - c2) @ nn[t2]
                    if (s1 > 1e-6 and s2 > 1e-6 and s3 > 1e-6) or (s1 < -1e-6 and s2 < -1e-6 and s3 < -1e-6):
                        inside += 1
                if inside >= 3:
                    c1 = np.mean([lum[yx] for yx in cov[t1]])
                    c2 = np.mean([lum[yx] for yx in cov[t2]])
                    if abs(c1 - c2) > 10:
                        zf.append((t1, t2, abs(c1 - c2), abs(nn[t1] @ p[t1][0] - nn[t2] @ p[t2][0])))
    if zf:
        add("MED", "zfight", len(zf), "coplanar overlapping triangle pairs with different colours",
            ["tris %d %d at %s luma %.0f apart, planes %.4f apart" % (t1, t2, p[t1].mean(0).round(1).tolist(), d, g)
             for t1, t2, d, g in zf[:10]])

    # normal map
    nm = a.path + "_0_norm.png" if a.kind == "mdl" else os.path.splitext(a.path)[0] + "_00_00_norm.png"
    if os.path.exists(nm):
        nw, nh, px = quakeimage.read_rgb(open(nm, "rb").read(), "png")[:3]
        img = np.frombuffer(bytes(px), np.uint8).reshape(nh, nw, -1)
        if abs(nw / nh - w / h) > 0.02 * (w / h):
            add("HIGH", "normalmap", 1, "%s is %dx%d, the skin %dx%d: misaligned" % (os.path.basename(nm), nw, nh, w, h))
        else:
            ys, xs = np.nonzero(used)
            sy, sx = nh / h, nw / w
            vals = img[(ys * sy + sy / 2).astype(int), (xs * sx + sx / 2).astype(int), :3].astype(np.float64) / 127.5 - 1
            neg = (vals[:, 2] < -0.05).sum()
            if neg:
                add("HIGH" if neg > 0.01 * len(vals) else "LOW", "normalmap", int(neg),
                    "covered texels with a normal facing into the surface (z < 0), of %d" % len(vals))
            # The ring outside the islands: flat or stray normals there bleed in with linear filtering.
            ring = []
            for y, x in np.argwhere(~used):
                y0, y1, x0, x1 = max(0, y - 1), min(h, y + 2), max(0, x - 1), min(w, x + 2)
                if used[y0:y1, x0:x1].any():
                    v = img[int(y * sy + sy / 2), int(x * sx + sx / 2), :3].astype(np.float64) / 127.5 - 1
                    if v[2] < 0.3:
                        ring.append((y, x))
            if ring:
                add("LOW", "normalmap", len(ring), "seam-ring texels of the normal map steep or unfilled (z < 0.3)",
                    ["texel (%d,%d)" % yx for yx in ring[:8]])
    return out


def stretch_summary(a, verbose):
    """--stretch: the share of the model's surface (area, frame 0) whose texels are stretched over 3:1 and 4:1 (a
    triangle folded onto a line or a point counts as stretched), the median texel density, the share under half
    of it; -v lists the worst triangles. The before/after measure of the re-maps (reuv_*.py)."""
    p, nn, area, uvarea, uvsign = tri_geo(a)
    ok = area > 1e-7
    tot = area[ok].sum()
    ratio = np.zeros(len(area))
    dens = np.sqrt(uvarea / np.maximum(area, 1e-12))
    for t in np.nonzero(ok)[0]:
        s1, s2 = stretch(p[t], a.uv[t])
        ratio[t] = 99.0 if s2 <= 1e-6 or uvarea[t] < 0.05 else min(99.0, s1 / s2)
    w = np.where(ok, area, 0.0)
    order = np.argsort(dens[ok])
    cum = np.cumsum(w[ok][order])
    med = float(dens[ok][order][np.searchsorted(cum, cum[-1] / 2)])
    line = "%-28s stretch>3:1 %5.1f%%  >4:1 %5.1f%%  density median %5.2f/unit, under half %5.1f%%" % (
        os.path.basename(a.path), 100 * w[ratio > 3].sum() / tot, 100 * w[ratio > 4].sum() / tot, med,
        100 * w[dens < med / 2].sum() / tot)
    print(line)
    if verbose:
        for t in sorted(np.nonzero(ok & (ratio > 3))[0], key=lambda t: -area[t])[:40]:
            print("    tri %4d at %s area %6.2f ratio %5.1f density %5.2f" % (
                t, p[t].mean(0).round(1).tolist(), area[t], ratio[t], dens[t]))


def main():
    args = [x for x in sys.argv[1:] if not x.startswith("-")]
    verbose = "-v" in sys.argv
    if "--stretch" in sys.argv:
        for path in args:
            stretch_summary(load_md5(path) if path.endswith(".md5mesh") else load_mdl(path), verbose)
        return
    if not args:
        progs = os.path.join(ROOT, "quakevr", "progs")
        for g in DEFAULT:
            args += sorted(glob.glob(os.path.join(progs, g)))
    for path in args:
        try:
            a = load_md5(path) if path.endswith(".md5mesh") else load_mdl(path)
            res = check(a, verbose)
        except Exception as e:  # report and go on
            print("%-28s ERROR %s" % (os.path.basename(path), e))
            continue
        if not res:
            print("%-28s ok" % os.path.basename(path))
        for sev, kind, n, msg, where in res:
            print("%-28s %-4s %-10s %5d %s" % (os.path.basename(path), sev, kind, n, msg))
            if verbose:
                for w_ in where:
                    print("    " + w_)


if __name__ == "__main__":
    main()
