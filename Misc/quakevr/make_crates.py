#!/usr/bin/env python3
# make_crates.py -- generates the wooden crates and the pieces they break into (engine vr_crates.cpp, QC vr_crates.qc;
# docs/vr-port/ROUND21.md, "Wooden crates"):
#   quakevr/progs/vr_crate1.mdl    the small crate, 32 x 32 x 32 units (the small explosive box's size): planks inside a
#                                  frame of battens on every face, nailed; three skins (pine, brown, weathered)
#   quakevr/progs/vr_crate2.mdl    the large crate, 40 x 40 x 48: the same, and a diagonal brace across each side
#   quakevr/progs/vr_plank1..4.mdl what a crate breaks into: a whole board, a board broken off, a splinter, a batten
#                                  broken off; three skins each, matching the crates'
#
# Usage: python Misc/quakevr/make_crates.py [output game folder] [--preview out.png]
#   --preview: a contact sheet of every model's skins.
#
# Model space: Quake units, unscaled (the crates are the explosive boxes' size, which are brush models Quake VR doesn't
# scale with vr_world_scale); x along the longest side of a piece, z up, the origin in the middle of its box. Every
# shape is convex (a box with worn, chamfered edges; a board cut by planes), so Box3D's hull (the convex hull of the
# drawn model) is exactly what is drawn. The frame, the planks' gaps and the nails are painted; the normal map
# (bake_normals.py, normaltiles.py's "stone" recipe: relief from the skin's shading) gives them their depth.
#
# Skins: our own, painted from 3D value noise (make_debris.py's), in Quake's palette. The skin's atlas and the texel
# points are make_debris.py's (each face its own island, at one texel density).
#
# Deterministic: fixed seeds; the same files each run. genguard.py keeps it from overwriting models edited since.

import math
import os
import sys

import numpy as np

import genguard
import make_debris as md

# Palette ramps, dark to light (gfx/palette.lmp's rows).
BROWN = list(range(17, 32))                           # Quake's own yellow-brown wood
WEATHERED = list(range(175, 161, -1))                 # beige greys (light first in the palette)
SKINS = [("pine", BROWN, 0.0, 0.14), ("brown", BROWN, 0.0, -0.04), ("weathered", WEATHERED, 1.0, 0.0)]  # (name, ramp,
# stains, lighter: the pine a light, fresh yellow-brown, the brown darker, the weathered grey and stained)

CHAMFER = 1.0  # units: the crate's worn edges


def v3(x):
    return np.asarray(x, dtype=np.float64)


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


def grain(along, across, layer, seed):
    """Wood grain: streaks along `along` (units), fine across."""
    p = np.stack([along * 0.045, across * 1.1, layer], -1)
    g = md.fbm(p, seed, 3)
    fine = md.vnoise(np.stack([along * 0.3, across * 3.0, layer + 50.0], -1), seed + 3)
    return 0.16 * (g - 0.5) * 2 + 0.05 * (fine - 0.5)


def knots(along, across, layer, seed):
    p = np.stack([along * 0.16, across * 0.5, layer], -1)
    return md.smoothstep(md.vnoise(p, seed + 11), 0.9, 0.96)


def crate_light(faces, P, F, E, seed, batten, brace):
    rng = np.random.default_rng(seed)
    T1, T2, A, B, C = face_frames(faces)
    nf = len(faces)
    Fi = np.where(F < 0, 0, F)
    N = np.array([f.n for f in faces])[Fi]
    tags = np.array([f.tag for f in faces])[Fi]
    q = P - C[Fi]
    a = np.sum(q * T1[Fi], -1)
    b = np.sum(q * T2[Fi], -1)
    Aa, Bb = A[Fi], B[Fi]
    da, db = Aa - np.abs(a), Bb - np.abs(b)
    main = tags == "face"
    layer = Fi.astype(np.float64) * 37.0

    horiz = main & (db < batten)
    vert = main & ~horiz & (da < batten)
    frame = horiz | vert
    # The large crate's sides: a diagonal brace between the frame's corners (the other way round on the opposite side).
    diag = np.zeros_like(main)
    s_diag = np.zeros_like(a)
    diag_nail = np.full_like(a, 99.0)
    if brace:
        side = main & (np.abs(N[..., 2]) < 0.1)
        flip = np.where((N[..., 0] + N[..., 1]) > 0, 1.0, -1.0)
        ia, ib = Aa - batten, Bb - batten
        dirn = np.stack([ia, ib * flip], -1)
        dirn /= np.linalg.norm(dirn, axis=-1, keepdims=True)
        dist = np.abs(a * dirn[..., 1] - b * dirn[..., 0])
        s_diag = a * dirn[..., 0] + b * dirn[..., 1]
        diag = side & ~frame & (dist < batten * 0.5)
        diag_nail = np.hypot(np.abs(s_diag) - np.hypot(ia, ib) * 0.82, dist)
    planks = main & ~frame & ~diag

    # Planks across t2, about 7 units each.
    inner = np.maximum(Bb - batten, 1.0)
    count = np.maximum(np.round(2 * inner / 7.0), 1.0)
    pw = 2 * inner / count
    t = (b + inner) / pw
    idx = np.floor(t)
    fr = t - idx
    gap = planks & ((fr * pw < 0.3) | ((1 - fr) * pw < 0.3))
    plank_base = rng.uniform(-0.07, 0.07, 64)
    pid = (idx.astype(np.int64) * 7 + Fi * 13) % 64

    L = np.full(P.shape[:2], 0.5)
    L = np.where(planks, 0.47 + plank_base[pid] + grain(a, b + idx * 13.7, layer + idx * 5.0, seed), L)
    L = np.where(planks, L - 0.22 * knots(a, b + idx * 13.7, layer + idx * 5.0, seed), L)
    # The planks' ends and edges beside the raised frame and brace: in their shadow.
    L = np.where(planks & ((da < batten + 0.7) | (db < batten + 0.7)), L - 0.12, L)
    L = np.where(horiz, 0.58 + grain(a, b, layer + 300.0, seed + 1), L)
    L = np.where(vert, 0.56 + grain(b, a, layer + 400.0, seed + 2), L)
    L = np.where(diag, 0.6 + grain(s_diag, a - b, layer + 500.0, seed + 4), L)
    # Where two battens meet: a joint's line.
    L = np.where(vert & (np.abs(db - batten) < 0.3), 0.18, L)
    L = np.where(gap, 0.07, L)
    # Worn edges catch the light; the chamfers are the frame's rounded edges.
    L = np.where(frame & (E < 0.6), L + 0.08, L)
    chamfer = tags == "chamfer"
    L = np.where(chamfer, 0.64 + grain(a, b, layer + 700.0, seed + 5), L)

    # Nails: in the vertical battens at each plank's middle, in the horizontal ones at the corners and between.
    r = 0.55
    pc = -inner + (idx + 0.5) * pw
    near_v = np.hypot(np.abs(a) - (Aa - batten * 0.5), b - pc)
    near_h = np.hypot(np.abs(b) - (Bb - batten * 0.5),
                      np.minimum(np.abs(np.abs(a) - (Aa - batten * 0.5)), np.abs(np.abs(a) - Aa * 0.33)))
    nail = np.where(vert, near_v, np.where(horiz, near_h, 99.0))
    nail = np.where(diag, diag_nail, nail)
    L = np.where(nail < r, 0.12, L)
    L = np.where(nail < r * 0.4, 0.42, L)
    # Its underside lay on the floor; stains and weathering come per skin.
    L = np.where(N[..., 2] < -0.9, L - 0.1, L)
    stain = md.smoothstep(md.fbm(P * 0.09, seed + 21, 3), 0.56, 0.7)
    return np.clip(L, 0, 1), stain


def board_light(faces, P, F, E, seed, nails):
    Fi = np.where(F < 0, 0, F)
    tags = np.array([f.tag for f in faces])[Fi]
    N = np.array([f.n for f in faces])[Fi]
    x, y, z = P[..., 0], P[..., 1], P[..., 2]
    L = 0.52 + grain(x, y + z * 0.5, np.full_like(x, 11.0), seed)
    L -= 0.22 * knots(x, y, np.full_like(x, 11.0), seed)
    fresh = tags == "break"
    fibres = md.vnoise(np.stack([x * 0.5, y * 4.0, z * 4.0], -1), seed + 7)
    L = np.where(fresh, 0.7 + 0.14 * (fibres - 0.5) * 2, L)  # broken: fresh, lighter wood, fibrous
    L = np.where(tags == "end", L - 0.1, L)  # sawn end grain
    L = np.where(tags == "edge", L - 0.04, L)
    L = np.where((E < 0.35) & ~fresh, L + 0.07, L)
    for nx in nails:
        d = np.hypot(x - nx, y)
        wide = np.abs(N[..., 2]) > 0.9
        L = np.where(wide & (d < 0.55), 0.12, L)
        L = np.where(wide & (d < 0.22), 0.42, L)
    stain = md.smoothstep(md.fbm(P * 0.09, seed + 21, 3), 0.56, 0.7)
    return np.clip(L, 0, 1), stain


def skins_of(L, stain):
    out = []
    for _, ramp, stains, lighter in SKINS:
        l = L - 0.13 * stain * stains + lighter
        out.append(md.quantize(np.clip(l, 0, 1), ramp).astype(np.uint8).tobytes())
    return out


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


def build_crate(spec):
    name, seed, half, batten, brace = spec
    md.SKIN = 256
    faces = crate_shape(*half)
    uvs, density = md.atlas(faces)
    mesh = md.mesh_of(faces, uvs)
    P, F, E = md.texel_points(faces, uvs)
    L, stain = crate_light(faces, P, F, E, seed, batten, brace)
    return name, faces, mesh, skins_of(L, stain), density, 256


def build_board(spec):
    name, seed, length, width, thick, be, bs, taper, nails = spec
    md.SKIN = 128
    faces = board_shape(seed, length, width, thick, be, bs, taper)
    uvs, density = md.atlas(faces)
    mesh = md.mesh_of(faces, uvs)
    P, F, E = md.texel_points(faces, uvs)
    L, stain = board_light(faces, P, F, E, seed, nails)
    return name, faces, mesh, skins_of(L, stain), density, 128


def preview(models, path):
    from PIL import Image, ImageDraw
    pal = np.array(md.palette(), dtype=np.uint8)
    W = 3 * (256 + 4)
    img = Image.new("RGB", (W, len(models) * (256 + 18)), (32, 32, 32))
    d = ImageDraw.Draw(img)
    for r, (name, _, _, skins, _, size) in enumerate(models):
        y = r * (256 + 18)
        d.text((2, y + 2), name, fill=(255, 255, 0))
        for k, s in enumerate(skins):
            im = Image.fromarray(pal[np.frombuffer(s, np.uint8).reshape(size, size)])
            if size != 256:
                im = im.resize((256, 256), Image.NEAREST)
            img.paste(im, (k * (256 + 4), y + 14))
    img.save(path)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    args = sys.argv[1:]
    prev = None
    if "--preview" in args:
        i = args.index("--preview")
        prev = args[i + 1]
        del args[i:i + 2]
    game = args[0] if args else os.path.join(here, "..", "..", "quakevr")
    models = [build_crate(s) for s in CRATES] + [build_board(s) for s in BOARDS]
    paths = [os.path.join(game, "progs", m[0]) for m in models]
    guard = genguard.Guard("make_crates.py", paths)
    for (name, faces, mesh, skins, density, size), path in zip(models, paths):
        if path not in guard.kept:
            import mdlgen
            mdlgen.write_mdl(path, mesh, skins, name.split(".")[0])
        pts = np.array([p for f in faces for p in f.pts])
        ext = pts.max(axis=0) - pts.min(axis=0)
        vol = md.volume(faces)
        print("%s: %d faces, %d vertices, %d triangles; %.1f x %.1f x %.1f units, %.0f cubic units; %.1f texels/unit "
              "(%d x %d)" % (name, len(faces), len(mesh.verts), len(mesh.tris), ext[0], ext[1], ext[2], vol, density,
                             size, size))
    guard.finish()
    if prev:
        preview(models, prev)
        print("preview -> " + prev)


if __name__ == "__main__":
    main()
