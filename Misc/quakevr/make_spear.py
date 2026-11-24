#!/usr/bin/env python3
# make_spear.py -- the spear (QC VR_SWORDKIND_SPEAR: the knight's sword, WID_SWORD, of its kind; weapon_spear;
# docs/vr-port/ROUND25.md, "spear"; docs/vr-port/POLEWEAPONS.md): quakevr/progs/v_spear.mdl, built here from scratch (an
# original design: no model or texture is copied).
#
# A plain infantry spear, 1.31 m: an ash shaft (round, 3.9 cm), an iron butt cap (ferrule), a leather wrap where the main
# hand holds, an iron socket with a collar, and a forged leaf-shaped steel head (27 cm, its widest 7.5 cm at 40% of it, a
# raised midrib, ground edges). Its edges lie where the swords' edges do (the axe head's way), so a swing cuts with them.
#
# It is laid exactly where the Crystal Lance's pole is (Quake/vr/vr_adweapons.cpp, the progs/dw_lance.mdl#vr recipe:
# along the axe's handle (0.3419 0 0.9397) through the fist's point (1.72 0 0.63) in v_axe.mdl's space, the butt 24 units
# behind it, the tip 76.94 ahead: the Lance's muzzle), then moved past the axe's corner as the recipe's keepScale does
# (by (corner - lo) (1 - 0.34), written with that origin), so that at its Scale, 0.34, every point draws where the
# Lance's would: weapons.txt slot 57 keeps the Lance's hold (slot 55: the hand values, the two-handed hold, the
# holsters) and its anchors are the Lance's moved by this model's move less the Lance's (printed below).
#
# Lofts of rings, each facet its own strip of vertices (flat across, smooth along: the id models' facets): the shaft from
# the butt to the socket's top (8 sides), the head (8 points round a flattened diamond, its tip one point). The skin
# (512 x 128): the shaft's loft unrolled along rows 2..46, the head's along rows 64..124, the end faces from a patch of
# iron. What the engine draws is its full-colour skin, progs/v_spear.mdl_0.png (FULL, 4, times the size: 2048 x 512;
# paint_full()); the 8-bit skin is that one in Quake's palette. Nine identical frames (a weapon's frame numbers).
#
# Usage: python Misc/quakevr/make_spear.py [output progs folder] [--keep-edited | --force]

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import genguard  # noqa: E402
import mdlgen  # noqa: E402
from mdlgen import add, cross, dot, mul, norm, sub  # noqa: E402

FRAMES = 9
SKIN_W, SKIN_H = 512, 128

# The Lance's frame (v_axe.mdl's space, before the keepScale move) and the move's terms (vr_adweapons.cpp).
FIST = (1.72, 0.0, 0.63)
AXIS = (0.3419, 0.0, 0.9397)
EDGE = (0.9397, 0.0, -0.3419)  # across the pole, the axe head's way (the edges)
SIDE = (0.0, 1.0, 0.0)
CORNER = (-2.997, -1.6813, -3.9006)  # v_axe.mdl's packed corner (the recipe's)
SCALE = 0.34
LANCE_MOVE = (2.995, 5.18, 12.03)  # the Lance's own keepScale move (weapons.txt slot 55's comment)
BUTT, TIP = -24.0, 76.94

# The shaft's loft (units along the pole from the fist, radius): ferrule, wood, the leather wrap, wood, collar, socket.
SHAFT = [(BUTT, 1.05), (BUTT + 0.4, 1.62), (-21.2, 1.62), (-21.0, 1.5), (-7.0, 1.5), (-6.8, 1.66), (7.8, 1.66),
         (8.0, 1.5), (46.0, 1.42), (46.2, 1.78), (47.4, 1.78), (47.6, 1.6), (55.0, 1.1), (55.4, 0.9)]
FERRULE_END, WRAP, COLLAR = -21.1, (-6.9, 7.9), 46.1
ROUND = [(math.cos(math.radians(22.5 + 45 * k)), math.sin(math.radians(22.5 + 45 * k))) for k in range(8)]
# The head's loft (along, half width, half thickness): a leaf, widest at 40% of its length; the tip one point.
HEAD = [(54.8, 0.75, 0.75), (56.0, 1.45, 0.8), (60.0, 2.6, 0.72), (64.0, 2.9, 0.62), (69.0, 2.3, 0.5), (73.5, 1.2, 0.34),
        (TIP, 0.0, 0.0)]
DIAMOND = [(1.0, 0.0), (0.55, 0.62), (0.0, 1.0), (-0.55, 0.62), (-1.0, 0.0), (-0.55, -0.62), (0.0, -1.0), (0.55, -0.62)]

# The skin's layout (texels): s along each loft, t round it.
S0, SPU = 8, 5.0          # the shaft: s at the butt, texels a unit
T_SHAFT = (2, 46)
H0, HPU = 8, 10.0         # the head: s at its base, texels a unit
T_HEAD = (64, 124)
CAP_ST = (470, 52)        # the end faces: a patch of iron

# Palette ramps (Quake's; dark to light).
WOOD = [16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26]
LEATHER = [16, 175, 174, 17, 173, 18, 19, 172, 20, 171]
IRON = [0, 1, 2, 3, 4, 5, 6, 7, 8]
STEEL = [2, 3, 4, 34, 5, 35, 6, 36, 7, 37, 8, 9, 10, 11]


def ring_t(ring, k, t0, t1):
    """The skin's row of ring point k (its share of the ring's perimeter)."""
    n = len(ring)
    lengths = [math.dist(ring[i], ring[(i + 1) % n]) for i in range(n)]
    return t0 + (t1 - t0) * sum(lengths[:k]) / sum(lengths)


class Out:
    def __init__(self):
        self.mesh = mdlgen.Mesh(SKIN_W, SKIN_H, {})
        self.normals = []

    def vert(self, p, st):
        self.mesh.verts.append((p, (0.0, 0.0, 1.0), st))
        self.normals.append((0.0, 0.0, 0.0))
        return len(self.mesh.verts) - 1

    def tri(self, a, b, c, outward):
        """Clockwise seen from outside (Quake's front faces); degenerate ones dropped."""
        p0, p1, p2 = (self.mesh.verts[i][0] for i in (a, b, c))
        n = cross(sub(p1, p0), sub(p2, p0))
        if dot(n, n) < 1e-10:
            return
        if dot(n, outward) > 0:
            b, c = c, b
            n = mul(n, -1.0)
        self.mesh.tris.append((a, b, c))
        for i in (a, b, c):
            self.normals[i] = add(self.normals[i], mul(n, -1.0))

    def loft(self, rings, s_of, t_rows, caps):
        """Rings (lists of local (u, v, w) points, one list per station) joined facet by facet; `s_of(w)` the skin's s,
        `t_rows` the rows round; `caps`: close the first and last ring with a flat face."""
        n = len(rings[0])
        ring2 = [(p[0], p[1]) for p in rings[max(range(len(rings)), key=lambda i: abs(rings[i][0][0]))]]
        for f in range(n):
            g = (f + 1) % n
            ta = int(round(ring_t(ring2, f, *t_rows)))
            tb = int(round(ring_t(ring2, f + 1, *t_rows)))
            col = [(self.vert(r[f], (int(round(s_of(r[f][2]))), ta)), self.vert(r[g], (int(round(s_of(r[g][2]))), tb)))
                   for r in rings]
            for i in range(len(rings) - 1):
                a0, b0 = col[i]
                a1, b1 = col[i + 1]
                mid = mul(add(add(rings[i][f], rings[i][g]), add(rings[i + 1][f], rings[i + 1][g])), 0.25)
                outward = (mid[0], mid[1], 0.0)
                self.tri(a0, b0, b1, outward)
                self.tri(a0, b1, a1, outward)
        for r, sign in ((rings[0], -1.0), (rings[-1], 1.0)) if caps else ():
            ids = [self.vert(p, (CAP_ST[0] + k, CAP_ST[1] + (k % 3))) for k, p in enumerate(r)]
            for k in range(1, n - 1):
                self.tri(ids[0], ids[k], ids[k + 1], (0.0, 0.0, sign))


def build():
    out = Out()
    shaft = [[(r * c, r * s, w) for c, s in ROUND] for w, r in SHAFT]
    out.loft(shaft, lambda w: S0 + (w - BUTT) * SPU, T_SHAFT, True)
    head = [[(hw * a, ht * b, w) for a, b in DIAMOND] for w, hw, ht in HEAD]
    out.loft(head, lambda w: H0 + (w - HEAD[0][0]) * HPU, T_HEAD, False)
    # (the head's base closed: it sits inside the socket, so a flat face)
    base = [out.vert(p, (CAP_ST[0] + k, CAP_ST[1])) for k, p in enumerate(head[0])]
    for k in range(1, len(base) - 1):
        out.tri(base[0], base[k], base[k + 1], (0.0, 0.0, -1.0))
    out.mesh.verts = [(p, norm(n) if dot(n, n) > 1e-12 else (0.0, 0.0, 1.0), uv)
                      for (p, _, uv), n in zip(out.mesh.verts, out.normals)]
    return out.mesh


def frame_point(p):
    """A local point (u across towards the edges, v to the side, w along the pole from the fist) in the Lance's frame."""
    u, v, w = p
    return add(FIST, add(mul(EDGE, u), add(mul(SIDE, v), mul(AXIS, w))))


def to_model(mesh):
    """Into the Lance's place, then moved past the axe's corner (the recipe's keepScale): returns the move and origin."""
    pts = [frame_point(p) for p, _, _ in mesh.verts]
    lo = tuple(min(min(p[k] for p in pts), CORNER[k]) for k in range(3))
    move = tuple((CORNER[k] - lo[k]) * (1.0 - SCALE) for k in range(3))
    mesh.verts = [(add(q, move), norm(add(mul(EDGE, n[0]), add(mul(SIDE, n[1]), mul(AXIS, n[2])))), uv)
                  for q, (_, n, uv) in zip(pts, mesh.verts)]
    return move, add(lo, move)


# ----------------------------------------------------------------------------
# The skin: full colour (FULL times the 8-bit skin) and its relief (the normal map's: normaltiles.py)

FULL = 4
DEPTH = 2.5


def paint_full(k=FULL):
    import numpy as np
    import make_debris as md
    ys, xs = np.mgrid[0:SKIN_H * k, 0:SKIN_W * k]
    s = (xs + 0.5) / k - 0.5
    t = (ys + 0.5) / k - 0.5
    shaft_rows = (t >= T_SHAFT[0] - 1) & (t < T_SHAFT[1] + 1)
    head_rows = (t >= T_HEAD[0] - 1) & (t < T_HEAD[1] + 1)
    w = (s - S0) / SPU + BUTT  # the shaft's place along the pole
    hw = (s - H0) / HPU + HEAD[0][0]

    def q(cell, stretch=1.0, rows=T_SHAFT):
        period = rows[1] - rows[0]
        th = 2 * math.pi * (t - rows[0]) / period
        r = period / (2 * math.pi)
        return np.stack([s / (cell * stretch), r * np.cos(th) / cell, r * np.sin(th) / cell], -1)

    def ramp(rr, x):
        return md.ramp_rgb(rr, np.clip(x, 0, 1))

    # The wood: ash, its grain long streaks along the shaft, darker by the hands (handled), a few knots.
    grain = md.fbm(q(0.5, 24.0), 3, 3)
    lines = np.abs(np.sin(2 * math.pi * (grain * 6.0 + md.vnoise(q(2.0, 8.0), 5))))
    knots = md.smoothstep(md.vnoise(q(3.0, 2.0), 9), 0.86, 0.95)
    handled = np.exp(-np.abs(w - 16.0) / 9.0) * 0.5  # where the other hand mostly holds
    lit_wood = 0.5 + 0.18 * (grain - 0.5) - 0.18 * md.smoothstep(lines, 0.85, 1.0) - 0.3 * knots - 0.15 * handled
    c_wood = ramp(WOOD, lit_wood)
    # The leather wrap: a strap wound on a slant, each turn's edge darker, worn shiny where the fist goes.
    phase = np.mod(s - S0 + (np.clip(t, T_SHAFT[0], T_SHAFT[1]) - T_SHAFT[0]) * 0.5, 7.0)
    lit_lea = 0.42 + 0.22 * (md.fbm(q(1.5), 13, 3) - 0.5) + 0.25 * md.smoothstep(md.vnoise(q(2.5), 15), 0.7, 0.9)
    lit_lea = np.where(phase < 0.9, lit_lea - 0.3, lit_lea)
    c_lea = ramp(LEATHER, lit_lea)
    # The iron (ferrule, collar, socket, end faces): dark, hammered, pitted, brighter on its edges.
    pits = md.smoothstep(md.vnoise(q(0.8), 21), 0.72, 0.9)
    lit_iron = 0.45 + 0.3 * (md.fbm(q(2.0), 23, 3) - 0.5) - 0.3 * pits
    collar = (np.abs(w - COLLAR - 0.6) < 0.7)
    lit_iron = lit_iron + 0.2 * collar
    c_iron = ramp(IRON, lit_iron)
    iron = ~shaft_rows | (w < FERRULE_END) | (w > COLLAR)
    leather = (w >= WRAP[0]) & (w <= WRAP[1])
    rgb_shaft = np.where(iron[..., None], c_iron, np.where(leather[..., None], c_lea, c_wood))
    # The head: forged steel, its edges ground bright (ring points 0 and 4), the midrib's ridge (2 and 6) darker, the
    # forging's scale left in the hollows, a few nicks.
    edges = [ring_t(DIAMOND, j, *T_HEAD) for j in (0, 4, 8)]
    ribs = [ring_t(DIAMOND, j, *T_HEAD) for j in (2, 6)]
    d_edge = np.min(np.abs(t[..., None] - np.array(edges)), -1)
    d_rib = np.min(np.abs(t[..., None] - np.array(ribs)), -1)
    ground = 1 - md.smoothstep(d_edge, 1.5, 4.0)
    scale = md.smoothstep(md.fbm(q(3.0, 1.0, T_HEAD), 31, 3), 0.55, 0.7) * (1 - ground)
    lit_steel = (0.5 + 0.22 * (md.fbm(q(1.2, 6.0, T_HEAD), 33, 3) - 0.5) + 0.35 * ground - 0.12 * (1 - md.smoothstep(d_rib, 0.5, 2.0))
                 - 0.25 * scale)
    c_head = ramp(STEEL, lit_steel)
    rgb = np.where(head_rows[..., None] & (hw >= HEAD[0][0] - 0.5)[..., None], c_head, rgb_shaft)
    h_shaft = np.where(iron, -0.5 * pits + 0.3 * collar, np.where(leather, 0.35 * (1 - phase / 7.0), -0.25 * md.smoothstep(lines, 0.85, 1.0) - 0.4 * knots))
    h = np.where(head_rows, 0.2 * (md.fbm(q(0.6, 1.0, T_HEAD), 37, 2) - 0.5) - 0.3 * scale, h_shaft) * DEPTH
    return np.clip(rgb, 0, 1), h


_relief = {}


def relief(name):
    """The spear's relief for its normal map (normaltiles.py): (heights in skin texels, 1, FULL)."""
    if name not in _relief:
        _relief[name] = (paint_full()[1], 1.0, FULL)
    return _relief[name]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out_dir = args[0] if args else os.path.join(HERE, "..", "..", "quakevr", "progs")
    path = os.path.join(out_dir, "v_spear.mdl")
    guard = genguard.Guard("make_spear.py", [path, path + "_0.png"])
    mesh = build()
    bad = mesh.check_winding()
    if bad:
        raise SystemExit("%d triangles wound the wrong way" % bad)
    move, origin = to_model(mesh)
    import numpy as np
    import make_debris as md
    from PIL import Image
    rgb, _ = paint_full()
    skin = md.to_palette(rgb, SKIN_W, WOOD + LEATHER + IRON + STEEL).tobytes()
    if path not in guard.kept:
        mdlgen.write_mdl(path, mesh, [skin], "spear", frames=[mesh] * (FRAMES - 1), origin=origin)
    if path + "_0.png" not in guard.kept:
        Image.fromarray(np.round(rgb * 255).astype(np.uint8)).save(path + "_0.png", optimize=True)
    shift = sub(move, LANCE_MOVE)
    tip = add(frame_point((0.0, 0.0, TIP)), move)
    print("v_spear.mdl: %d vertices, %d triangles, %d frames -> %s" % (len(mesh.verts), len(mesh.tris), FRAMES,
                                                                       os.path.normpath(path)))
    print("  move %.4f %.4f %.4f, origin %.4f %.4f %.4f" % (move + origin))
    print("  the Lance's anchors (_av) plus %.4f %.4f %.4f; the tip (muzzle_av) %.4f %.4f %.4f" % (shift + tip))
    head_from = (HEAD[1][0]) / TIP
    print("  the head from %.3f of the fist-to-tip line (bl_from)" % head_from)
    guard.finish()


if __name__ == "__main__":
    main()
