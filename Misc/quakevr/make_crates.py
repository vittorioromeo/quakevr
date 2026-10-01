#!/usr/bin/env python3
# make_crates.py -- generates the wooden crates and the pieces they break into (engine vr_crates.cpp, QC vr_crates.qc;
# docs/vr-port/ROUND21.md, "Wooden crates" and "Crates: monsters, damage, held crates, texture"):
#   quakevr/progs/vr_crate1.mdl    the small crate, 32 x 32 x 32 units (the small explosive box's size): planks inside a
#                                  frame of battens on every face, nailed; three skins (pine, brown, weathered)
#   quakevr/progs/vr_crate2.mdl    the large crate, 40 x 40 x 48: the same, and a diagonal brace across each side
#   quakevr/progs/vr_plank1..4.mdl what a crate breaks into: a whole board, a board broken off, a splinter, a batten
#                                  broken off; three skins each, matching the crates'
#   quakevr/progs/<model>_<skin>.png  each skin in full colour at four times the size (1024 x 1024 a crate's, 512 x 512 a
#                                  piece's): what the engine draws (an external skin); the model's own 8-bit skins are
#                                  the same, in Quake's palette, for a renderer without them
#
# Usage: python Misc/quakevr/make_crates.py [output game folder] [--preview out.png]
#   --preview: a contact sheet of every model's skins.
# Then bake the normal maps (python Misc/quakevr/bake_normals.py vr_crate1.mdl vr_crate2.mdl vr_plank1.mdl ...): their
# relief is this file's (relief(), normaltiles.py's "crate" recipe).
#
# Model space: Quake units, unscaled (the crates are the explosive boxes' size, which are brush models Quake VR doesn't
# scale with vr_world_scale); x along the longest side of a piece, z up, the origin in the middle of its box. Every
# shape is convex (a box with worn, chamfered edges; a board cut by planes), so Box3D's hull (the convex hull of the
# drawn model) is exactly what is drawn. The frame, the planks' gaps and the nails are painted, and their depth is the
# normal map's: the planks sunk under the battens and cupped, their edges rounded into the gaps, the growth rings, the
# fibres and pores, checks along the grain, scratches, dents, the nails' heads.
#
# Skins: our own, painted from 3D value noise (make_debris.py's): wood's figure (growth rings cut at a slant, fibres,
# pores), each board its own shade; worn edges, scratches, dirt, water stains, floor grime up the sides, dust on top,
# nails with rust run down from them; the weathered one grey and checked. The skin's atlas and the texel points are
# make_debris.py's (each face its own island, at one texel density).
#
# Deterministic: fixed seeds; the same files each run. genguard.py keeps it from overwriting models edited since.

import math
import os
import sys

import numpy as np

import genguard
import make_debris as md

CHAMFER = 1.0  # units: the crate's worn edges
HIRES = 4  # the full-colour skins' (and the relief's) texels per texel of the model's own skin


def v3(x):
    return np.asarray(x, dtype=np.float64)


def rgb(c):
    return np.asarray(c, dtype=np.float64)


# ---------------------------------------------------------------------------------------------------------------------
# Shapes


def crate_shape(hx, hy, hz):
    """A box of half sizes hx, hy, hz with its twelve edges chamfered and its corners knocked."""
    faces = md.box(hx, hy, hz)
    for f in faces:
        f.tag = "face"
    for a in range(3):
        b, e = [k for k in range(3) if k != a]
        for sb in (-1, 1):
            for se in (-1, 1):
                n = np.zeros(3)
                n[b], n[e] = sb, se
                n /= np.linalg.norm(n)
                faces = md.clip(faces, n, md.support(faces, n) - CHAMFER / math.sqrt(2), "chamfer")
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                n = v3((sx, sy, sz)) / math.sqrt(3)
                faces = md.clip(faces, n, md.support(faces, n) - CHAMFER * 0.3, "chamfer")
    return md.centred(faces)


def board_shape(seed, length, width, thick, broken_end=0, broken_start=0, taper=0.0):
    """A board `length` x `width` x `thick` units along x: sawn ends a little off square, or broken (that many planes: a
    splintered end, fresh wood), its long edges barely worn; `taper` narrows it towards +x (a splinter)."""
    rng = np.random.default_rng(seed)
    faces = md.box(length * 0.5, width * 0.5, thick * 0.5)
    for f in faces:
        f.tag = "end" if abs(f.n[0]) > 0.9 else ("face" if abs(f.n[2]) > 0.9 else "edge")

    def cut(n, depth, tag):
        nonlocal faces
        n = v3(n)
        n /= np.linalg.norm(n)
        faces = md.clip(faces, n, md.support(faces, n) - depth, tag)

    for sign, count in ((1, broken_end), (-1, broken_start)):
        if count:
            for k in range(count):
                n = v3((sign * 1.0, rng.normal(0, 0.9), rng.normal(0, 0.25)))
                cut(n, rng.uniform(0.2, 2.2) + (0.6 if k else 0.0), "break")
        else:
            cut((sign * 1.0, rng.normal(0, 0.04), 0.0), 0.05, "end")
    if taper:
        for s in (-1, 1):
            cut((taper, s * 1.0, 0.0), rng.uniform(0.1, 0.5), "break")
    for sy in (-1, 1):
        for sz in (-1, 1):
            cut((0.0, sy, sz), rng.uniform(0.1, 0.25), "edge")
    return md.centred(faces)


# ---------------------------------------------------------------------------------------------------------------------
# Painting


def face_frames(faces):
    """Each face's in-plane axes (t1: along the planks, t2: across) and half extents along them from its middle."""
    T1, T2, A, B, C = [], [], [], [], []
    for f in faces:
        n = f.n
        if abs(n[2]) > 0.9:
            t1, t2 = v3((1, 0, 0)), v3((0, 1, 0))
        elif abs(n[2]) < 0.1 and (abs(n[0]) > 0.9 or abs(n[1]) > 0.9):
            t1 = np.cross(v3((0, 0, 1)), n)
            t1 /= np.linalg.norm(t1)
            t2 = v3((0, 0, 1))
        else:  # a chamfer: along the edge it wears
            axis = int(np.argmin(np.abs(n)))
            t1 = np.zeros(3)
            t1[axis] = 1.0
            t2 = np.cross(n, t1)
        c = sum(f.pts) / len(f.pts)
        a = [abs(np.dot(p - c, t1)) for p in f.pts]
        b = [abs(np.dot(p - c, t2)) for p in f.pts]
        T1.append(t1)
        T2.append(t2)
        A.append(max(a))
        B.append(max(b))
        C.append(c)
    return np.array(T1), np.array(T2), np.array(A), np.array(B), np.array(C)


def mix(a, b, t):
    """Colours a to b by t ((h, w) or a number)."""
    t = np.asarray(t, dtype=np.float64)
    return a + (b - a) * (t[..., None] if t.ndim else t)


def wood(along, across, layer, seed, res, rings=0.55):
    """Wood's figure at points `along` the grain and `across` it (units) on board `layer`, for a texture of `res` texels
    a unit (nothing finer than it shows): late wood 0..1 (the darker growth rings, cut through at a slant: long flames),
    fibres 0..1, pores 0..1 (short dark dashes along the grain)."""
    warp = md.fbm(np.stack([along * 0.022, across * 0.11, layer], -1), seed, 3)
    ring = across * rings + (warp - 0.5) * 6.0 + np.sin(along * 0.045 + layer * 1.7) * 0.7
    f = ring - np.floor(ring)
    late = md.smoothstep(f, 0.52, 0.78) * (1.0 - md.smoothstep(f, 0.88, 1.0))
    k = min(10.0, 0.42 * res)  # the finest across the grain the texture shows
    # Fibres: streaks along the grain at three scales, the finest at the texture's own (what makes it read as worn,
    # gritty wood up close, as the texture packs' wood does: NOTES.md vrfiringrange_2026-10-01_11-43-14).
    fib = 0.35 * md.vnoise(np.stack([along * 0.35, across * k * 0.5, layer + 3.0], -1), seed + 1) + \
        0.3 * md.vnoise(np.stack([along * 0.9, across * k, layer + 9.0], -1), seed + 5) + \
        0.35 * md.vnoise(np.stack([along * 0.5, across * k * 1.6, layer + 19.0], -1), seed + 7)
    # Crevices: the grain opened by age, long thin dark cracks between the fibres.
    crev = md.smoothstep(md.vnoise(np.stack([along * 0.12, across * k * 1.2, layer + 23.0], -1), seed + 8), 0.62, 0.86)
    crev = crev * md.smoothstep(md.vnoise(np.stack([along * 0.05, across * 0.4, layer + 29.0], -1), seed + 10), 0.25, 0.6)
    fib = np.clip((fib - 0.5) * 1.6 + 0.5 - 0.9 * crev, 0.0, 1.0)
    pores = 0.6 * md.smoothstep(md.vnoise(np.stack([along * 0.6, across * k * 0.8, layer + 6.0], -1), seed + 2), 0.84, 0.92)
    return late, fib, pores


def lines(u, v, seed, layer, spacing, width, keep):
    """Thin lines along u (scratches; checks along the grain): 0..1, about `spacing` units apart, `width` wide, broken
    into pieces (`keep`: the share of their length drawn)."""
    wob = md.fbm(np.stack([u * 0.04, v * 0.02, layer + 13.0], -1), seed + 41, 2)
    w = v / spacing + (wob - 0.5) * 3.0
    row = np.floor(w)
    d = np.abs(w - row - 0.5) * spacing
    on = md.vnoise(np.stack([u * 0.07, row * 3.1, layer + 17.0], -1), seed + 43) > 1.0 - keep
    return np.where(on, 1.0 - md.smoothstep(d, width * 0.3, width), 0.0)


# A skin's colours (sRGB 0..1): early wood (the light growth), late wood (its rings), fresh wood (worn edges, scratches,
# breaks), grime; how weathered (grey, its grain checked) and how dirty.
# Darker and duller than a fresh board's: old, handled, damp wood, the texture packs' (QRP's wood1_1 averages 0.32 0.22
# 0.12; NOTES.md vrfiringrange_2026-10-01_11-43-14).
SKINS = [
    ("pine", rgb((0.52, 0.38, 0.20)), rgb((0.31, 0.21, 0.10)), rgb((0.52, 0.41, 0.27)), rgb((0.08, 0.065, 0.045)), 0.15,
     1.2),
    ("brown", rgb((0.32, 0.20, 0.10)), rgb((0.18, 0.10, 0.045)), rgb((0.44, 0.33, 0.21)), rgb((0.06, 0.05, 0.035)), 0.2,
     1.2),
    ("weathered", rgb((0.38, 0.36, 0.32)), rgb((0.22, 0.21, 0.18)), rgb((0.44, 0.39, 0.31)), rgb((0.08, 0.075, 0.065)),
     1.0, 1.3),
]
RUST = rgb((0.35, 0.17, 0.07))
IRON = rgb((0.16, 0.12, 0.09))  # old iron, rusty brown


def finish(fig, N, P, seed, res, regions):
    """The three skins ((h, w, 3) 0..1) of a wooden thing from its wood's figure `fig` (late, fibres, pores, each
    board's tint (h, w, 3)) and `regions` (h, w): ao (its own shadow, 1 none), wear, scratch, checks, rust, nail (its
    head), nailhi (the head's highlight), low (floor grime), fresh (a break's raw wood)."""
    late, fib, pores, tint = fig
    dirt = md.smoothstep(md.fbm(P * 0.11, seed + 51, 4), 0.48, 0.78)
    stain_n = md.fbm(P * 0.045 + 7.0, seed + 61, 3)
    stain = md.smoothstep(stain_n, 0.6, 0.63) * (0.55 + 0.45 * (1.0 - md.smoothstep(stain_n, 0.63, 0.66)))  # a tide line
    speck = md.smoothstep(md.vnoise(P * min(3.0, 0.4 * res), seed + 71), 0.86, 0.95)
    up = np.clip(N[..., 2], 0, 1)
    broad = 0.80 + 0.40 * md.fbm(P * 0.06 + 3.0, seed + 55, 3)  # the whole crate's light and dark: sun, damp, handling
    # Grime ground into it at every scale (blotches, smudges, specks), darkest where the grain is open.
    grunge = md.fbm(P * 0.25 + 11.0, seed + 57, 4)
    grit = md.vnoise(P * min(4.0, 0.45 * res) + 5.0, seed + 59)
    rough = (0.80 + 0.40 * grunge) * (0.82 + 0.36 * grit)
    out = []
    for _, early, latec, fresh, grime, weather, dirty in SKINS:
        c = mix(np.broadcast_to(early, late.shape + (3,)), latec, late * 0.6)
        c = c * (0.5 + 0.8 * fib)[..., None] * (1.0 - 0.5 * pores)[..., None] * tint * (broad * rough)[..., None]
        if weather:
            grey = c.mean(-1, keepdims=True) * rgb((1.0, 0.97, 0.92))
            c = mix(c, grey, 0.55 * weather)
            c = c * (1.0 - 0.6 * weather * regions["checks"])[..., None]
        raw = fresh * (0.82 + 0.26 * fib)[..., None]
        c = mix(c, raw, regions["fresh"])
        c = mix(c, raw, regions["wear"] * 0.5)
        c = mix(c, fresh, regions["scratch"] * 0.22)
        c = mix(c, grime, dirt * 0.42 * dirty)
        c = mix(c, grime, (1.0 - regions["ao"]) * 0.6 * dirty)  # grime packed into the gaps and against the battens
        c = mix(c, grime, stain * 0.4 * dirty)
        c = mix(c, grime, speck * 0.45 * dirty)
        c = mix(c, grime, regions["low"] * 0.5 * dirty)  # floor grime up its sides
        c = mix(c, rgb((0.50, 0.48, 0.44)), up * dirt * 0.18)  # dust on top
        c = mix(c, RUST, regions["rust"] * 0.65)
        c = c * regions["ao"][..., None]
        c = mix(c, IRON * (0.75 + 0.5 * regions["nailhi"])[..., None], regions["nail"])
        out.append(np.clip(c, 0, 1))
    return out


def rust_run(ox, oy):
    """Rust run down from a nail (ox across, oy up from it, units): 0..1."""
    w = np.maximum(-oy, 0.0)
    return np.where(oy < 0, np.exp(oy / 5.0) * (1 - md.smoothstep(np.abs(ox), 0.18 + 0.07 * w, 0.45 + 0.1 * w)), 0.0)


def crate_paint(faces, P, F, E, seed, batten, brace, res):
    """A crate's three skins ((h, w, 3) 0..1) and its relief ((h, w) units) at texels P, F, E (`res` texels a unit)."""
    rng = np.random.default_rng(seed)
    T1, T2, A, B, C = face_frames(faces)
    Fi = np.where(F < 0, 0, F)
    N = np.array([f.n for f in faces])[Fi]
    tags = np.array([f.tag for f in faces])[Fi]
    q = P - C[Fi]
    a = np.sum(q * T1[Fi], -1)
    b = np.sum(q * T2[Fi], -1)
    Aa, Bb = A[Fi], B[Fi]
    da, db = Aa - np.abs(a), Bb - np.abs(b)
    main = tags == "face"
    chamfer = tags == "chamfer"
    side = main & (np.abs(N[..., 2]) < 0.1)
    layer = Fi.astype(np.float64) * 37.0

    horiz = main & (db < batten)
    vert = main & ~horiz & (da < batten)
    frame = horiz | vert
    # The large crate's sides: a diagonal brace between the frame's corners (the other way round on the opposite side).
    diag = np.zeros_like(main)
    s_diag = np.zeros_like(a)
    p_diag = np.full_like(a, 99.0)
    nail_a = nail_b = np.zeros_like(a)
    if brace:
        flip = np.where((N[..., 0] + N[..., 1]) > 0, 1.0, -1.0)
        ia, ib = Aa - batten, Bb - batten
        dirn = np.stack([ia, ib * flip], -1)
        dirn /= np.linalg.norm(dirn, axis=-1, keepdims=True)
        p_diag = a * dirn[..., 1] - b * dirn[..., 0]
        s_diag = a * dirn[..., 0] + b * dirn[..., 1]
        diag = side & ~frame & (np.abs(p_diag) < batten * 0.5)
        s0 = np.sign(s_diag) * np.hypot(ia, ib) * 0.82
        nail_a, nail_b = s0 * dirn[..., 0], s0 * dirn[..., 1]
    planks = main & ~frame & ~diag

    # Planks across t2, about 7 units each.
    inner = np.maximum(Bb - batten, 1.0)
    count = np.maximum(np.round(2 * inner / 7.0), 1.0)
    pw = 2 * inner / count
    t = (b + inner) / pw
    idx = np.floor(t)
    fr = t - idx
    edge_d = np.minimum(fr, 1 - fr) * pw  # to the plank's own edge (its gap)
    gap = planks & (edge_d < 0.3)
    pid = (idx.astype(np.int64) * 7 + Fi * 13) % 64
    # Each board its own: a little lighter or darker, warmer or greyer.
    shade = rng.uniform(0.84, 1.12, 96)
    warm = rng.uniform(-0.05, 0.05, 96)
    bid = np.where(planks, pid, np.where(horiz, 64 + Fi % 16, np.where(vert, 80 + Fi % 8, 88 + Fi % 8)))
    tint = np.stack([shade[bid] * (1 + warm[bid]), shade[bid], shade[bid] * (1 - warm[bid])], -1)

    # The grain: along each board.
    along = np.where(horiz | planks, a, np.where(vert, b, np.where(diag, s_diag, a)))
    across = np.where(planks, b + idx * 13.7, np.where(vert, a, np.where(diag, p_diag, b)))
    lay = np.where(planks, layer + idx * 5.0, np.where(horiz, layer + 300.0, np.where(
        vert, layer + 400.0, np.where(diag, layer + 500.0, layer + 700.0))))
    late, fib, pores = wood(along, across, lay, seed, res)

    # Nails: in the vertical battens at each plank's middle, in the horizontal ones at the corners and between, along
    # the brace near its ends; (ca, cb) the nearest one's middle.
    pc = -inner + (idx + 0.5) * pw
    cand = np.stack([Aa - batten * 0.5, Aa * 0.33], -1)
    ha = np.sign(a) * np.take_along_axis(cand, np.argmin(np.abs(np.abs(a)[..., None] - cand), -1)[..., None], -1)[..., 0]
    hb = np.sign(b) * (Bb - batten * 0.5)
    ca = np.where(vert, np.sign(a) * (Aa - batten * 0.5), np.where(horiz, ha, nail_a))
    cb = np.where(vert, pc, np.where(horiz, hb, nail_b))
    nailed = frame | diag
    oa, ob = a - ca, b - cb
    nd = np.where(nailed, np.hypot(oa, ob), 99.0)
    r = 0.55
    # Rust: a halo round each nail and, on the sides, run down from it (from the top batten's onto the planks below).
    wob = md.vnoise(np.stack([a * 0.8, b * 0.15, layer + 21.0], -1), seed + 33)
    rust = np.where(nailed, (1 - md.smoothstep(nd, r, r * 2.6)) * 0.8, 0.0)
    rust = np.maximum(rust, np.where(side & nailed, rust_run(oa, ob), 0.0) * 0.7)
    top_b = Bb - batten * 0.5
    rust = np.maximum(rust, np.where(side & planks & (b < top_b), rust_run(np.abs(a) - np.abs(ha), b - top_b), 0.0) * 0.6)
    rust = rust * (0.55 + 0.45 * wob)

    # Worn edges (the crate's own and the battens'), scratches every way, checks along the grain.
    scratch = np.zeros_like(a)
    for k, ang in enumerate((0.3, 1.9, 2.6)):
        u = a * math.cos(ang) + b * math.sin(ang)
        v = -a * math.sin(ang) + b * math.cos(ang)
        scratch = np.maximum(scratch, lines(u, v, seed + k, layer, 9.0, 0.12 + 0.06 * k, 0.25))
    brk = md.vnoise(np.stack([a * 0.6, b * 0.6, layer + 31.0], -1), seed + 35)
    frame_edge = np.where(frame, np.minimum(np.abs(db - batten), np.where(vert, np.abs(da - batten), 99.0)), 99.0)
    brace_edge = np.where(diag, batten * 0.5 - np.abs(p_diag), 99.0)
    wear = np.clip((1 - md.smoothstep(E, 0.2, 0.9 + 0.8 * brk)) * (0.4 + 0.6 * brk), 0, 1)
    wear = np.where(chamfer, np.maximum(wear, 0.5 + 0.3 * brk), wear)
    wear = np.maximum(wear, (1 - md.smoothstep(np.minimum(frame_edge, brace_edge), 0.1, 0.45)) * brk * 0.6)
    checks = lines(along, across, seed + 9, lay, 2.3, 0.1, 0.3) * (planks | frame | diag)

    # Its own shadow: in the gaps, at the planks' edges beside the raised frame and brace and their own gaps; under it.
    to_raised = np.minimum(np.minimum(da, db) - batten, np.where(side & brace, np.abs(p_diag) - batten * 0.5, 99.0))
    ao = 1.0 - 0.38 * np.exp(-np.maximum(to_raised, 0) / 0.9) * planks
    ao = ao - 0.25 * np.exp(-np.maximum(edge_d, 0) / 0.35) * planks
    ao = np.where(gap, 0.22 + 0.25 * md.smoothstep(edge_d, 0.0, 0.3), ao)
    ao = ao * np.where(N[..., 2] < -0.9, 0.8, 1.0)
    zmin = min(p[2] for f in faces for p in f.pts)
    lowdirt = md.fbm(P * 0.3, seed + 91, 3)
    low = np.where(np.abs(N[..., 2]) < 0.9, (1 - md.smoothstep(P[..., 2] - zmin, 0.0, 5.0 + 4.0 * lowdirt)), 0.0)

    regions = {"ao": ao, "wear": wear, "scratch": scratch, "checks": checks, "rust": rust,
               "nail": np.clip((r - nd) / 0.12, 0, 1), "nailhi": np.clip(1 - nd / (r * 0.6), 0, 1), "low": low,
               "fresh": np.zeros_like(a)}
    skins = finish((late, fib, pores, tint), N, P, seed, res, regions)

    # Relief (units): the planks under the frame and the brace, cupped, their edges rounded into the gaps; the battens'
    # edges rounded; the grain (late wood standing), fibres, pores, checks, scratches, dents; the nails' heads.
    h = np.where(planks, -0.45 + 0.08 * (1 - (2 * fr - 1) ** 2), 0.0)
    h = h - np.where(planks, 0.3 * (1 - md.smoothstep(edge_d, 0.15, 0.55)), 0.0)
    h = np.where(gap, -1.1, h)
    h = h - 0.25 * (1 - md.smoothstep(np.minimum(frame_edge, brace_edge), 0.0, 0.45)) ** 2
    h = h + 0.06 * late + 0.045 * (fib - 0.5) - 0.05 * pores - 0.12 * checks - 0.05 * scratch
    h = h - 0.15 * md.smoothstep(md.vnoise(P * 0.45 + 3.0, seed + 81), 0.86, 0.97)  # dents
    h = h + np.where(nd < r, 0.14 * (1 - (nd / r) ** 2), np.where(nd < r * 1.8, -0.04 * (1 - (nd - r) / (r * 0.8)), 0.0))
    return skins, h


def board_paint(faces, P, F, E, seed, nails, res):
    """A piece's three skins and its relief, as crate_paint's: a board's grain along x, sawn end grain, raw wood where
    it broke, the nails it was held by."""
    Fi = np.where(F < 0, 0, F)
    tags = np.array([f.tag for f in faces])[Fi]
    N = np.array([f.n for f in faces])[Fi]
    x, y, z = P[..., 0], P[..., 1], P[..., 2]
    layer = np.full_like(x, 11.0)
    late, fib, pores = wood(x, y + z * 0.5, layer, seed, res)
    fresh = tags == "break"
    end = tags == "end"
    fibres = md.vnoise(np.stack([x * 0.5, y * min(4.0, 0.4 * res), z * min(4.0, 0.4 * res)], -1), seed + 7)
    # End grain: the rings round the pith (off to one side, under the board).
    ring = np.hypot(y + 9.0, z - 6.0) * 0.55
    late = np.where(end, md.smoothstep(ring - np.floor(ring), 0.6, 0.85), late)
    brk = md.vnoise(np.stack([x * 0.6, y * 0.6, z * 0.6 + 31.0], -1), seed + 35)
    wear = np.where(fresh, 0.0, np.clip((1 - md.smoothstep(E, 0.1, 0.5 + 0.5 * brk)) * (0.3 + 0.5 * brk), 0, 1))
    wide = np.abs(N[..., 2]) > 0.9
    nd = np.full_like(x, 99.0)
    for nx in nails:
        nd = np.minimum(nd, np.where(wide, np.hypot(x - nx, y), 99.0))
    r = 0.55
    rust = (1 - md.smoothstep(nd, r, r * 2.6)) * 0.8 * (0.55 + 0.45 * md.vnoise(P * 0.8, seed + 33))
    scratch = lines(x, y, seed, layer, 9.0, 0.12, 0.25) * ~fresh
    checks = lines(x, y + z * 0.5, seed + 9, layer, 2.3, 0.1, 0.3) * ~fresh
    ao = np.where(end, 0.85, 1.0)
    regions = {"ao": ao, "wear": wear, "scratch": scratch, "checks": checks, "rust": rust,
               "nail": np.clip((r - nd) / 0.12, 0, 1), "nailhi": np.clip(1 - nd / (r * 0.6), 0, 1),
               "low": np.zeros_like(x), "fresh": np.where(fresh, 0.85, 0.0)}
    tint = np.ones(x.shape + (3,))
    skins = finish((late, fib, pores, tint), N, P, seed, res, regions)
    h = 0.06 * late + 0.045 * (fib - 0.5) - 0.05 * pores - 0.12 * checks - 0.05 * scratch
    h = np.where(fresh, 0.18 * (fibres - 0.5), h)  # torn fibres
    h = np.where(end, 0.05 * late - 0.04 * pores, h)
    h = h + np.where(nd < r, 0.14 * (1 - (nd / r) ** 2), 0.0)
    return skins, h


def hires_texels(faces, uvs, size):
    """make_debris.texel_points at HIRES times the skin's size (the same atlas)."""
    skin, bleed = md.SKIN, md.BLEED
    md.SKIN, md.BLEED = size * HIRES, bleed * HIRES
    try:
        return md.texel_points(faces, [[(s * HIRES, t * HIRES) for s, t in st] for st in uvs])
    finally:
        md.SKIN, md.BLEED = skin, bleed


def to_palette(img, size):
    """A full-colour skin as the model's own 8-bit one: its texels averaged down to `size`, then the nearest of the
    palette's greys, browns, rusts, tans and beiges (no fullbrights), with ordered dithering."""
    k = img.shape[0] // size
    small = img.reshape(size, k, size, k, 3).mean((1, 3))
    pal = np.array(md.palette(), dtype=np.float64) / 255.0
    idx = np.array(list(range(1, 32)) + list(range(96, 128)) + list(range(160, 176)))
    ys, xs = np.mgrid[0:size, 0:size]
    c = small + md.BAYER[ys % 4, xs % 4][..., None] * 0.035
    d = ((c[..., None, :] - pal[idx][None, None]) ** 2).sum(-1)
    return idx[np.argmin(d, -1)].astype(np.uint8)


# ---------------------------------------------------------------------------------------------------------------------

CRATES = [  # (file, seed, half size x y z, batten width, brace)
    ("vr_crate1.mdl", 71, (16.0, 16.0, 16.0), 4.0, False),
    ("vr_crate2.mdl", 72, (20.0, 20.0, 24.0), 4.5, True),
]
BOARDS = [  # (file, seed, length, width, thickness, broken end, broken start, taper, nails along x)
    ("vr_plank1.mdl", 81, 30.0, 6.5, 1.4, 0, 0, 0.0, (-12.5, 12.5)),
    ("vr_plank2.mdl", 82, 19.0, 6.5, 1.4, 3, 0, 0.0, (-7.0,)),
    ("vr_plank3.mdl", 83, 13.0, 4.0, 1.3, 2, 2, 0.12, ()),
    ("vr_plank4.mdl", 84, 26.0, 3.8, 2.2, 3, 0, 0.0, (-11.0,)),
]


def build(name):
    """A model: (name, faces, mesh, its 8-bit skins (bytes), the full-colour skins ((S, S, 3) uint8), the relief
    ((S, S) units), texels a unit of its own skin, its own skin's size); S = HIRES times that."""
    crate = next((s for s in CRATES if s[0] == name), None)
    if crate:
        _, seed, half, batten, brace = crate
        size = 256
        md.SKIN = size
        faces = crate_shape(*half)
    else:
        _, seed, length, width, thick, be, bs, taper, nails = next(s for s in BOARDS if s[0] == name)
        size = 128
        md.SKIN = size
        faces = board_shape(seed, length, width, thick, be, bs, taper)
    uvs, density = md.atlas(faces)
    mesh = md.mesh_of(faces, uvs)
    P, F, E = hires_texels(faces, uvs, size)
    if crate:
        skins, h = crate_paint(faces, P, F, E, seed, batten, brace, density * HIRES)
    else:
        skins, h = board_paint(faces, P, F, E, seed, nails, density * HIRES)
    full = [np.round(s * 255).astype(np.uint8) for s in skins]
    own = [to_palette(s, size).tobytes() for s in skins]
    return name, faces, mesh, own, full, h, density, size


_relief = {}


def relief(name):
    """The relief of model `name` (vr_crate1.mdl...) for its normal map (normaltiles.py): (heights (S, S) in units, its
    own skin's texels a unit, HIRES)."""
    if name not in _relief:
        m = build(name)
        _relief[name] = (m[5], m[6], HIRES)
    return _relief[name]


def preview(models, path):
    from PIL import Image, ImageDraw
    img = Image.new("RGB", (3 * (256 + 4), len(models) * (256 + 18)), (32, 32, 32))
    d = ImageDraw.Draw(img)
    for r, m in enumerate(models):
        y = r * (256 + 18)
        d.text((2, y + 2), m[0], fill=(255, 255, 0))
        for k, s in enumerate(m[4]):
            img.paste(Image.fromarray(s).resize((256, 256), Image.LANCZOS), (k * (256 + 4), y + 14))
    img.save(path)


def main():
    from PIL import Image
    here = os.path.dirname(os.path.abspath(__file__))
    args = sys.argv[1:]
    prev = None
    if "--preview" in args:
        i = args.index("--preview")
        prev = args[i + 1]
        del args[i:i + 2]
    game = args[0] if args else os.path.join(here, "..", "..", "quakevr")
    models = [build(s[0]) for s in CRATES + BOARDS]
    paths = [os.path.join(game, "progs", m[0]) for m in models]
    extra = [os.path.join(game, "progs", "%s_%d.png" % (m[0], k)) for m in models for k in range(len(m[4]))]
    guard = genguard.Guard("make_crates.py", paths + extra)
    for (name, faces, mesh, skins, full, h, density, size), path in zip(models, paths):
        if path not in guard.kept:
            import mdlgen
            mdlgen.write_mdl(path, mesh, skins, name.split(".")[0])
        for k, s in enumerate(full):
            out = "%s_%d.png" % (path, k)
            if out not in guard.kept:
                Image.fromarray(s).save(out, optimize=True)
        pts = np.array([p for f in faces for p in f.pts])
        ext = pts.max(axis=0) - pts.min(axis=0)
        vol = md.volume(faces)
        print("%s: %d faces, %d vertices, %d triangles; %.1f x %.1f x %.1f units, %.0f cubic units; %.1f texels/unit "
              "(%d x %d; full colour %d x %d)" % (name, len(faces), len(mesh.verts), len(mesh.tris), ext[0], ext[1],
                                                  ext[2], vol, density, size, size, size * HIRES, size * HIRES))
    guard.finish()
    if prev:
        preview(models, prev)
        print("preview -> " + prev)


if __name__ == "__main__":
    main()
