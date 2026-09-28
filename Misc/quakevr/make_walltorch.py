# make_walltorch.py -- the wall torch taken off its wall (QC vr_walltorch.qc; docs/vr-port/ROUND21.md, "Wall torches you
# can take"): quakevr/progs/vrtorch.mdl, a wooden stick with a head of wrapped, tarred rag round a shallow pit, the shape
# and size of id's wall torch's stick (progs/flame.mdl, whose flame the engine draws over it: vr_walltorch.cpp), built
# here from scratch (id's model is not copied).
#
# Along the model's +x (as a weapon: held the same way every time, Held Object Offsets' grip angles are 0): the butt at
# x -14, the handle to -7, the head to the rim at +1.8, the pit (where the flame sits) at +0.9. The origin is where id's
# torch's is (1.8 below the rim).
# Skins: 0 lit (the pit's embers glowing: fullbright colours), 1 burnt out (charred, a few dull sparks).
# Frames: 17, all the same shape: the frame is the flame's size as the server sends it (0 out, 1..16 sixteenths).
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
FRAMES = 17
REGIONS = {"wood": (0, 0, 32, 16), "rag": (32, 0, 64, 16), "ember": (0, 16, 32, 32), "grain": (32, 16, 64, 32)}
RAMPS_LIT = {"wood": [119, 120, 121, 122, 123], "rag": [98, 99, 100, 101, 102], "ember": [232, 233, 234, 235, 236],
             "grain": [122, 123, 124, 125, 126]}
RAMPS_OUT = {"wood": [118, 119, 120, 121, 122], "rag": [0, 1, 16, 17, 2], "ember": [0, 1, 2, 16, 17],
             "grain": [121, 122, 123, 124, 125]}

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


def band(m, a, b, region):
    """Quads between rings `a` (lower x) and `b`, facing out."""
    for i in range(SIDES):
        j = (i + 1) % SIDES
        # Counter-clockwise seen from outside: round the ring the way y turns towards z, then back along -x.
        m.quad(a[i], a[j], b[j], b[i], region)


def fan(m, centre, rim, region, facing_plus_x):
    """A cone from `rim` to `centre`, facing +x (the pit, the rim's top) or -x (the butt's end)."""
    s0, t0, s1, t1 = m.regions[region]
    for i in range(SIDES):
        j = (i + 1) % SIDES
        p0, p1 = rim[i], rim[j]
        n = mdlgen.norm(mdlgen.cross(mdlgen.sub(p1, p0), mdlgen.sub(centre, p0)))
        if (n[0] > 0) != facing_plus_x:
            p0, p1 = p1, p0
            n = mdlgen.norm(mdlgen.cross(mdlgen.sub(p1, p0), mdlgen.sub(centre, p0)))
        base = len(m.verts)
        m.verts.append((p0, n, (s0 + 1, t1 - 1)))
        m.verts.append((p1, n, (s1 - 1, t1 - 1)))
        m.verts.append((centre, n, ((s0 + s1) // 2, t0 + 1)))
        # Quake's triangles are clockwise seen from outside (mdlgen's quads: (0, 2, 1)).
        m.tris.append((base, base + 2, base + 1))


def build():
    m = mdlgen.Mesh(64, 32, REGIONS)
    rings = [ring(x, r) for x, r, _ in PROFILE]
    for k in range(1, len(PROFILE)):
        band(m, rings[k - 1], rings[k], PROFILE[k][2])
    # The butt: a low cone of end grain.
    fan(m, (PROFILE[0][0] - 0.35, 0.0, 0.0), rings[0], "grain", False)
    # The rim's top (a flat ring, the rag's) and the pit inside it (the embers).
    top_x = PROFILE[-1][0]
    inner = ring(top_x, RIM_INNER)
    for i in range(SIDES):
        j = (i + 1) % SIDES
        m.quad(rings[-1][i], rings[-1][j], inner[j], inner[i], "rag")
    fan(m, (PIT_X, 0.0, 0.0), inner, "ember", True)
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
    skins = [mdlgen.dithered_skin(64, 32, REGIONS, RAMPS_LIT, 4242),
             mdlgen.dithered_skin(64, 32, REGIONS, RAMPS_OUT, 4243)]
    mdlgen.write_mdl(path, m, skins, "torch", frames=[m] * (FRAMES - 1))
    print("vrtorch.mdl: %d vertices, %d triangles, %d frames, 2 skins -> %s" % (len(m.verts), len(m.tris), FRAMES,
                                                                               os.path.normpath(path)))
    guard.finish()


if __name__ == "__main__":
    main()
