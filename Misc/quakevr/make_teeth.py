#!/usr/bin/env python3
# make_teeth.py -- generates the tooth a hard punch knocks out of a humanoid's face (QC vr_smallgibs.qc, "Teeth";
# docs/vr-port/ROUND22.md, "More gore"):
#   quakevr/progs/vr_tooth.mdl   a molar-like tooth: a rounded crown, a narrower neck and a tapering root, ivory, the root
#                                bloody towards its tip; one frame, 8-bit skin (16 x 16)
#
# Usage: python Misc/quakevr/make_teeth.py [output game folder]
# Plain Python (mdlgen.py, qpal.py): no numpy. Model space: one unit long (the root's tip at z 0, the crown's top at
# z 1); QC scales it to vr_teeth_size (cm). The mesh is a lathe of the profile below round z, a little flattened across
# (x 1, y 0.8), 10 sides: 99 vertices, 140 triangles (the hull Box3D takes of the drawn model is the same shape).
# Deterministic: the same file each run. genguard.py keeps it from overwriting a model edited since.

import math
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, "blender", "addons", "quakevr_models"))

import genguard
import mdlgen
import qpal

SIDES = 10
SKIN = 16
# (z, radius) from the root's tip to the crown's top.
PROFILE = [(0.0, 0.0), (0.06, 0.07), (0.25, 0.15), (0.45, 0.19), (0.55, 0.22), (0.7, 0.28), (0.86, 0.29), (0.96, 0.22),
           (1.0, 0.0)]
FLAT_Y = 0.8
IVORY = (226, 214, 186)
ENAMEL = (240, 234, 214)
ROOT = (196, 160, 130)
BLOOD = (110, 12, 10)


def nearest(rgb):
    """The palette's index nearest `rgb` (no fullbrights)."""
    best, bi = 1e9, 0
    for i in range(224):
        r, g, b = qpal.PALETTE[i]
        d = (r - rgb[0]) ** 2 + (g - rgb[1]) ** 2 + (b - rgb[2]) ** 2
        if d < best:
            best, bi = d, i
    return bi


def skin():
    """16 x 16: t (rows) from the crown's top (0) to the root's tip (15); a speckled ivory crown, the root darker,
    bloody at its end."""
    px = bytearray()
    seed = 12345
    for t in range(SKIN):
        for s in range(SKIN):
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
            k = (seed >> 16) % 10
            if t < 7:
                c = ENAMEL if k < 4 else IVORY
            elif t < 12:
                c = ROOT if k < 6 else IVORY
            else:
                c = BLOOD if k < 3 + (t - 12) * 2 else ROOT
            px.append(nearest(c))
    return bytes(px)


def build():
    mesh = mdlgen.Mesh(SKIN, SKIN, {})
    n = len(PROFILE)
    # Profile normals: perpendicular to the profile's slope (in (r, z)), outwards.
    pn = []
    for i, (z, r) in enumerate(PROFILE):
        z0, r0 = PROFILE[max(0, i - 1)]
        z1, r1 = PROFILE[min(n - 1, i + 1)]
        dz, dr = z1 - z0, r1 - r0
        l = math.hypot(dz, dr) or 1.0
        pn.append((dz / l, -dr / l))  # (radial, z)
    index = {}
    for i, (z, r) in enumerate(PROFILE):
        for j in range(SIDES + 1):  # (the seam's column twice: its own texels)
            a = 2 * math.pi * j / SIDES
            ca, sa = math.cos(a), math.sin(a)
            p = (r * ca, r * sa * FLAT_Y, z)
            if r == 0.0:
                nrm = (0.0, 0.0, -1.0 if z < 0.5 else 1.0)
            else:
                nrm = mdlgen.norm((pn[i][0] * ca, pn[i][0] * sa / FLAT_Y, pn[i][1]))
            st = (min(SKIN - 1, int(j * (SKIN - 1) / SIDES)), min(SKIN - 1, int((1.0 - z) * (SKIN - 1) + 0.5)))
            index[(i, j)] = len(mesh.verts)
            mesh.verts.append((p, nrm, st))
    for i in range(n - 1):
        for j in range(SIDES):
            a, b, c, d = index[(i, j)], index[(i, j + 1)], index[(i + 1, j + 1)], index[(i + 1, j)]
            for tri in ((a, b, c), (a, c, d)):
                p0, p1, p2 = (mesh.verts[k][0] for k in tri)
                cr = mdlgen.cross(mdlgen.sub(p1, p0), mdlgen.sub(p2, p0))
                if mdlgen.dot(cr, cr) < 1e-12:
                    continue  # (a cap's sliver at the tip)
                if mdlgen.dot(cr, mesh.verts[tri[0]][1]) > 0.0:
                    tri = (tri[0], tri[2], tri[1])  # clockwise seen from outside
                mesh.tris.append(tri)
    return mesh


def main():
    game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr")
    path = os.path.join(game, "progs", "vr_tooth.mdl")
    guard = genguard.Guard("make_teeth.py", [path])
    mesh = build()
    if path not in guard.kept:
        mdlgen.write_mdl(path, mesh, [skin()], "tooth")
    guard.finish()
    print("vr_tooth.mdl: %d vertices, %d triangles" % (len(mesh.verts), len(mesh.tris)))


if __name__ == "__main__":
    main()
