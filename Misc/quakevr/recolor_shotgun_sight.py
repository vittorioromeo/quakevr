#!/usr/bin/env python3
# recolor_shotgun_sight.py -- gives the shotgun's glowing sights (quakevr/progs/v_shot.mdl) the
# double shotgun's orange gradient.
#
# Both weapons' skins keep their sights in a patch of fullbright "fire" palette indices (224..239,
# 252, 253: they glow, and the port's vr_weapon_glow and bloom make them brighter). The double
# shotgun's ring sight samples that patch across its whole range, so it goes from deep red at its
# rim through orange to pale yellow at its centre; the shotgun's three sights (the two rear posts and
# the front post) only sampled its dark red end (225..228), which the glow turned into a flat red.
# This rewrites the texels under the shotgun's sights with a radial gradient around each sight's hub
# (the texel its faces fan out from, its top): light orange on the top, orange down the sides, deep
# red at the bottom edge, lightly dithered as on the double shotgun. Only fullbright texels
# under the sights (and one texel around them, for filtered sampling) change: the rest of the skin,
# the geometry and the UVs stay as they are. Running it again gives the same skin.
#
# Usage: python Misc/quakevr/recolor_shotgun_sight.py [path to v_shot.mdl]

import math
import os
import struct
import sys

from mdlgen import HEADER

# Palette indices from the hub (hottest) to the rim, all fullbright: gfx/palette.lmp's fire ramp,
# 236 (227,151,79) light orange ... 228 (111,15,0) deep red.
RAMP = [236, 235, 234, 233, 232, 231, 230, 229, 228]
FULLBRIGHT = 224
BAYER = [[0.0, 0.5], [0.75, 0.25]]  # 2x2 ordered dither, a light touch of the double shotgun's grain


def recolor(data):
    data = bytearray(data)
    h = HEADER.unpack_from(data, 0)
    num_skins, sw, sh, num_verts, num_tris = h[12:17]
    assert num_skins == 1 and struct.unpack_from("<i", data, HEADER.size)[0] == 0, "one single skin expected"
    skin_off = HEADER.size + 4
    skin = data[skin_off : skin_off + sw * sh]
    off = skin_off + sw * sh
    stverts = [struct.unpack_from("<3i", data, off + 12 * i) for i in range(num_verts)]
    off += 12 * num_verts
    tris = [struct.unpack_from("<4i", data, off + 16 * i) for i in range(num_tris)]

    def uv(front, v):
        on_seam, s, t = stverts[v]
        return (s + sw // 2 if on_seam and not front else s, t)

    # Sight triangles: every texel in their UV bounds glows.
    sight = []
    for front, a, b, c in tris:
        uvs = [uv(front, v) for v in (a, b, c)]
        s0, s1 = min(p[0] for p in uvs), max(p[0] for p in uvs)
        t0, t1 = min(p[1] for p in uvs), max(p[1] for p in uvs)
        if all(skin[t * sw + s] >= FULLBRIGHT for s in range(s0, s1 + 1) for t in range(t0, t1 + 1)):
            sight.append(((a, b, c), uvs))
    assert sight, "no glowing sight triangles found"

    # Group them into sights (triangles sharing vertices); each sight's hub is the UV its faces
    # share most, its radius the furthest UV from it.
    groups = []
    for verts, uvs in sight:
        joined = [g for g in groups if g["verts"] & set(verts)]
        merged = {"verts": set(verts), "uvs": list(uvs)}
        for g in joined:
            merged["verts"] |= g["verts"]
            merged["uvs"] += g["uvs"]
            groups.remove(g)
        groups.append(merged)
    hubs = []
    for g in groups:
        hub = max(set(g["uvs"]), key=g["uvs"].count)
        radius = max(math.dist(hub, p) for p in g["uvs"]) or 1.0
        hubs.append((hub, radius))
        print("sight: %d UVs, hub %s, radius %.1f texels" % (len(g["uvs"]), hub, radius))

    # Their texels (UV bounds plus one texel around), if they glow.
    texels = set()
    for g in groups:
        s0, s1 = min(p[0] for p in g["uvs"]) - 1, max(p[0] for p in g["uvs"]) + 1
        t0, t1 = min(p[1] for p in g["uvs"]) - 1, max(p[1] for p in g["uvs"]) + 1
        texels |= {(s, t) for s in range(max(0, s0), min(sw, s1 + 1)) for t in range(max(0, t0), min(sh, t1 + 1))
                   if skin[t * sw + s] >= FULLBRIGHT}

    changed = 0
    for s, t in sorted(texels):
        heat = max(1.0 - math.dist((s, t), hub) / radius for hub, radius in hubs)
        heat = min(1.0, max(0.0, heat))
        k = int((1.0 - heat) * (len(RAMP) - 1) + BAYER[t % 2][s % 2])
        index = RAMP[min(len(RAMP) - 1, k)]
        if skin[t * sw + s] != index:
            changed += 1
        data[skin_off + t * sw + s] = index
    print("%d sight texels, %d changed" % (len(texels), changed))
    return bytes(data)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs", "v_shot.mdl")
    with open(path, "rb") as f:
        data = f.read()
    out = recolor(data)
    assert len(out) == len(data)
    with open(path, "wb") as f:
        f.write(out)
    print("wrote", os.path.normpath(path))


if __name__ == "__main__":
    main()
