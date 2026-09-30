# make_walltorch.py -- the wall torch, on its wall and taken off it (QC vr_walltorch.qc; docs/vr-port/ROUND21.md, "Wall
# torches you can take" and "Wall torches: one model, the old wood"): quakevr/progs/vrtorch.mdl, a wooden stick with a
# head round a shallow pit, the shape and size of the wall torch's stick (progs/flame.mdl, whose flame the engine draws
# over it: vr_walltorch.cpp), built here from scratch (no model is copied).
#
# Along the model's +x (as a weapon: held the same way every time, Held Object Offsets' grip angles are 0): the butt at
# x -14, the handle to -7, the head to the rim at +1.8, the pit (where the flame sits) at +0.9. The origin is where the
# wall torch's is (1.8 below the rim).
# Its skin is laid out as the wall torch's that Quake VR loads (quakevr/progs/flame.mdl, 256 x 128: its stick's front and
# back projected flat across it), so that the engine gives it that torch's wood and metal as it loads
# (vr_walltorch.cpp VR_DerivedModelFile: the game's own progs/flame.mdl's skin copied in, and charred for the burnt-out
# skin; nothing of it is stored here). The pit's embers are ours, in a corner that skin leaves empty (OWN). What this
# file paints shows only without that torch (another progs/flame.mdl).
# Skins: 0 lit (the pit's embers glowing: fullbright colours), 1 burnt out (charred, a few dull sparks).
# Frames: 18, all the same shape: the frame is the flame's size as the server sends it (0 out, 1..16 sixteenths; 17 on
# its wall, a full fire).
#
# Usage: python Misc/quakevr/make_walltorch.py [output progs folder]

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import genguard  # noqa: E402
import mdlgen  # noqa: E402

SIDES = 8
FRAMES = 18
SKIN_W, SKIN_H = 256, 128  # the wall torch's (quakevr/progs/flame.mdl; vr_walltorch.cpp checks it before copying it in)
OWN = (129, 2, 149, 30)     # s0 t0 s1 t1: ours (the pit's embers), empty in that skin (vr_walltorch.cpp ownRect)
REGIONS = {"ember": OWN, "rag": (0, 0, SKIN_W, 72), "wood": (0, 72, SKIN_W, SKIN_H)}
RAMPS_LIT = {"wood": [119, 120, 121, 122, 123], "rag": [98, 99, 100, 101, 102], "ember": [232, 233, 234, 235, 236]}
RAMPS_OUT = {"wood": [118, 119, 120, 121, 122], "rag": [0, 1, 16, 17, 2], "ember": [0, 1, 2, 16, 17]}

# The wall torch's skin's projection (its stick's texels, measured): flat across the stick, the front (its +x: our +y)
# at s 17 + 5.8 * side, the back at 51.5 + 5.8 * side (its y: our z); along it t 23.5 - 6.87 * height (the rim at 11,
# the butt at 120).
S_FRONT, S_BACK, S_PER_UNIT = 17.0, 51.5, 5.8
T_TOP, T_PER_UNIT = 23.5, 6.87

# The profile along x: (x, radius, region of the band from the ring before). Id's stick: 0.75..1 thick to -7, the
# head's bulge 2.26 at -4.7, 1.75..2 at -1.5, the rim 2.26 at +1.8 round a pit down to +1.3.
PROFILE = [
    (-14.0, 0.82, None),
    (-10.0, 0.92, "wood"),
    (-7.2, 1.02, "wood"),
    (-6.8, 1.45, "rag"),
    (-4.7, 2.3, "rag"),
    (-3.0, 2.05, "rag"),
    (-1.5, 1.95, "rag"),
    (0.4, 2.15, "rag"),
    (1.8, 2.35, "rag"),
]
RIM_INNER = 1.75
PIT_X = 0.9


def ring(x, r, phase=0.0):
    return [(x, r * math.cos(2 * math.pi * (i + phase) / SIDES), r * math.sin(2 * math.pi * (i + phase) / SIDES))
            for i in range(SIDES)]


def t_of(x):
    """The skin's row for a height along the stick."""
    return max(7.0, min(121.0, T_TOP - T_PER_UNIT * x))


def skin_st(p, back, t=None):
    """Where `p` (on the stick) is in the wall torch's skin: across by its side (the model's z), along by its height; the
    front (the model's +y) or the back."""
    s = (S_BACK if back else S_FRONT) + S_PER_UNIT * max(-2.26, min(2.26, p[2]))
    return (int(round(s)), int(round(t_of(p[0]) if t is None else t)))


def quad_st(m, p0, p1, p2, p3, sts):
    """A quad p0 p1 p2 p3, counter-clockwise seen from outside, with its texels (mdlgen.Mesh.quad's, laid out here)."""
    n = mdlgen.norm(mdlgen.cross(mdlgen.sub(p1, p0), mdlgen.sub(p3, p0)))
    base = len(m.verts)
    for p, st in zip((p0, p1, p2, p3), sts):
        m.verts.append((p, n, st))
    m.tris.append((base, base + 2, base + 1))
    m.tris.append((base, base + 3, base + 2))


def band(m, a, b):
    """Quads between rings `a` (lower x) and `b`, facing out."""
    for i in range(SIDES):
        j = (i + 1) % SIDES
        back = a[i][1] + a[j][1] < 0
        # Counter-clockwise seen from outside: round the ring the way y turns towards z, then back along -x.
        quad_st(m, a[i], a[j], b[j], b[i], [skin_st(p, back) for p in (a[i], a[j], b[j], b[i])])


def fan(m, centre, rim, facing_plus_x, st):
    """A cone from `rim` to `centre`, facing +x (the pit) or -x (the butt's end); `st(p, back)` its texels."""
    for i in range(SIDES):
        j = (i + 1) % SIDES
        p0, p1 = rim[i], rim[j]
        back = p0[1] + p1[1] < 0
        n = mdlgen.norm(mdlgen.cross(mdlgen.sub(p1, p0), mdlgen.sub(centre, p0)))
        if (n[0] > 0) != facing_plus_x:
            p0, p1 = p1, p0
            n = mdlgen.norm(mdlgen.cross(mdlgen.sub(p1, p0), mdlgen.sub(centre, p0)))
        base = len(m.verts)
        m.verts.append((p0, n, st(p0, back, 0)))
        m.verts.append((p1, n, st(p1, back, 1)))
        m.verts.append((centre, n, st(centre, back, 2)))
        # Quake's triangles are clockwise seen from outside (mdlgen's quads: (0, 2, 1)).
        m.tris.append((base, base + 2, base + 1))


def build():
    m = mdlgen.Mesh(SKIN_W, SKIN_H, REGIONS)
    rings = [ring(x, r) for x, r, _ in PROFILE]
    for k in range(1, len(PROFILE)):
        band(m, rings[k - 1], rings[k])
    # The butt: a low cone, the skin's end cap.
    fan(m, (PROFILE[0][0] - 0.35, 0.0, 0.0), rings[0], False, lambda p, back, k: skin_st(p, back, 119 if k < 2 else 121))
    # The rim's top (a flat ring: the skin's top edge) and the pit inside it (our embers).
    top_x = PROFILE[-1][0]
    inner = ring(top_x, RIM_INNER)
    for i in range(SIDES):
        j = (i + 1) % SIDES
        back = rings[-1][i][1] + rings[-1][j][1] < 0
        quad_st(m, rings[-1][i], rings[-1][j], inner[j], inner[i],
                [skin_st(rings[-1][i], back, 10), skin_st(rings[-1][j], back, 10), skin_st(inner[j], back, 8),
                 skin_st(inner[i], back, 8)])
    s0, t0, s1, t1 = OWN
    fan(m, (PIT_X, 0.0, 0.0), inner, True,
        lambda p, back, k: ((s0 + 1, t1 - 1), (s1 - 1, t1 - 1), ((s0 + s1) // 2, t0 + 1))[k])
    return m


def outward_check(m):
    """Triangles facing into the stick (its sides and the rim's top face away from the axis or +x; the pit and the
    butt are fans made facing the right way)."""
    bad = 0
    for a, b, c in m.tris:
        p0, p1, p2 = (m.verts[k][0] for k in (a, c, b))  # (Quake's clockwise order undone)
        n = mdlgen.cross(mdlgen.sub(p1, p0), mdlgen.sub(p2, p0))
        cx, cy, cz = ((p0[i] + p1[i] + p2[i]) / 3 for i in range(3))
        radial = math.hypot(cy, cz)
        if radial > 1.0:  # a side or the rim's top: away from the axis, or towards +x
            bad += n[1] * cy + n[2] * cz < 0 and n[0] <= 0
    return bad


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "quakevr", "progs")
    path = os.path.join(out, "vrtorch.mdl")
    guard = genguard.Guard("make_walltorch.py", [path])
    m = build()
    bad = outward_check(m)
    if bad:
        raise SystemExit("%d triangles face inwards" % bad)
    skins = [mdlgen.dithered_skin(SKIN_W, SKIN_H, REGIONS, RAMPS_LIT, 4242),
             mdlgen.dithered_skin(SKIN_W, SKIN_H, REGIONS, RAMPS_OUT, 4243)]
    mdlgen.write_mdl(path, m, skins, "torch", frames=[m] * (FRAMES - 1))
    print("vrtorch.mdl: %d vertices, %d triangles, %d frames, 2 skins -> %s" % (len(m.verts), len(m.tris), FRAMES,
                                                                               os.path.normpath(path)))
    guard.finish()


if __name__ == "__main__":
    main()
