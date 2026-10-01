#!/usr/bin/env python3
# make_muzzleflash.py -- the programmatic muzzle flash's model (vr_weaponfx.cpp): quakevr/progs/vr_muzzleflash.mdl.
#
# The shotgun's own flash, taken out of quakevr/progs/v_shot.mdl: the vertices its fire frame ("shot2") moves out of the
# barrel (in its other frames they sit together at the bore), the triangles made of them only, their texels (the
# fullbright yellows and oranges of the skin's flash strip) on a small skin of their own. The weapons that draw it
# (Weapon Offsets > Effects > Muzzle Flash) show it for a moment at their muzzle anchor, turned as the gun is.
#
# Usage: python Misc/quakevr/make_muzzleflash.py [output game folder]
#
# Model space: the shotgun's model units at a weapon Scale of 1 (its own units times its shipped Scale, 0.44), +x out of
# the barrel, the origin where the flash starts, on its middle line. vr_weaponfx.cpp draws it at the weapon models' scale
# (ModelTransform::k, worked out from the drawn weapon), so it is the shotgun's size on any gun (times the weapon's Flash
# Size). Every triangle is drawn both ways round (a flash seen from the side or the front in VR).

import math
import os
import struct
import sys

import genguard
import mdlgen
from mdlgen import sub, cross, norm, mul

SOURCE_SCALE = 0.44  # v_shot.mdl's Scale (vr_weapons.inc, slot 1): the flash at the shotgun's drawn size
FLASH_FRAME = 1      # "shot2": the flash out
MOVED = 2.0          # model units a vertex moves between frame 0 and the fire frame: the flash's


def load(path):
    data = open(path, "rb").read()
    h = mdlgen.HEADER.unpack_from(data, 0)
    scale, origin = h[2:5], h[5:8]
    num_skins, w, hgt, num_verts, num_tris, num_frames = h[12], h[13], h[14], h[15], h[16], h[17]
    skins, off = mdlgen.read_skins(data, mdlgen.HEADER.size, num_skins, w, hgt)
    stverts = [struct.unpack_from("<3i", data, off + 12 * i) for i in range(num_verts)]
    off += 12 * num_verts
    tris = [struct.unpack_from("<4i", data, off + 16 * i) for i in range(num_tris)]
    off += 16 * num_tris
    frames = []
    for _ in range(num_frames):
        (kind,) = struct.unpack_from("<i", data, off)
        assert kind == 0, "a frame group"
        off += 4 + 8 + 16
        verts = []
        for _ in range(num_verts):
            x, y, z, _n = data[off:off + 4]
            off += 4
            verts.append((x * scale[0] + origin[0], y * scale[1] + origin[1], z * scale[2] + origin[2]))
        frames.append(verts)
    skin = skins[0][4:]  # (a single skin: its group flag dropped)
    return stverts, tris, frames, skin, w, hgt


def build(source):
    stverts, tris, frames, skin, w, hgt = load(source)
    rest, fired = frames[0], frames[FLASH_FRAME]
    moved = {i for i in range(len(stverts)) if math.dist(rest[i], fired[i]) > MOVED}
    flash = [t for t in tris if all(v in moved for v in t[1:])]
    assert flash, "no flash in " + source
    # The bore: where the flash's vertices sit when it is in (all together in frame 0), across; the flash's middle
    # up and down and sideways (it rises above v_shot.mdl's bore: centred, it sits on any gun's muzzle line).
    bore = [sum(rest[i][k] for i in moved) / len(moved) for k in range(3)]
    for k in (1, 2):
        bore[k] = 0.5 * (min(fired[i][k] for i in moved) + max(fired[i][k] for i in moved))
    bore = tuple(bore)

    # Each corner's texel (a back face's seam vertex takes the skin's right half, as Quake draws it).
    def texel(front, v):
        onseam, s, t = stverts[v]
        return (s + w // 2 if onseam and not front else s), t

    corners = [texel(front, v) for front, a, b, c in flash for v in (a, b, c)]
    s0, t0 = min(c[0] for c in corners), min(c[1] for c in corners)
    s1, t1 = max(c[0] for c in corners) + 1, max(c[1] for c in corners) + 1
    sw = (s1 - s0 + 3) // 4 * 4  # a width of whole words
    sh = t1 - t0
    pixels = bytearray(sw * sh)
    for y in range(sh):
        for x in range(sw):
            sx, sy = min(s0 + x, w - 1), min(t0 + y, hgt - 1)
            pixels[y * sw + x] = skin[sy * w + sx]

    mesh = mdlgen.Mesh(sw, sh, {})
    for front, a, b, c in flash:
        pts = [mul(sub(fired[v], bore), SOURCE_SCALE) for v in (a, b, c)]
        uvs = [(texel(front, v)[0] - s0, texel(front, v)[1] - t0) for v in (a, b, c)]
        face = cross(sub(pts[1], pts[0]), sub(pts[2], pts[0]))
        if math.sqrt(face[0] ** 2 + face[1] ** 2 + face[2] ** 2) < 1e-9:
            continue
        n = norm(face)
        for order, normal in (((0, 1, 2), mul(n, -1.0)), ((0, 2, 1), n)):  # as it was, and the other way round
            base = len(mesh.verts)
            for k in order:
                mesh.verts.append((pts[k], normal, uvs[k]))
            mesh.tris.append((base, base + 1, base + 2))
    return mesh, bytes(pixels), bore, len(flash)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr")
    source = os.path.join(game, "progs", "v_shot.mdl")
    mesh, skin, bore, count = build(source)
    path = os.path.join(game, "progs", "vr_muzzleflash.mdl")
    # The files edited by hand since this wrote them are not overwritten (genguard.py: --keep-edited, --force).
    guard = genguard.Guard("make_muzzleflash.py", [path])
    mdlgen.write_mdl(path, mesh, [skin], "flash")
    lo = [min(v[0][k] for v in mesh.verts) for k in range(3)]
    hi = [max(v[0][k] for v in mesh.verts) for k in range(3)]
    print("vr_muzzleflash.mdl: %d triangles of v_shot.mdl's flash (both ways: %d), skin %dx%d -> %s" % (
        count, len(mesh.tris), mesh.skin_w, mesh.skin_h, os.path.normpath(path)))
    print("  origin at %.2f %.2f %.2f in v_shot.mdl; bounds %s .. %s" % (bore + (
        " ".join("%.2f" % x for x in lo), " ".join("%.2f" % x for x in hi))))
    guard.finish()


if __name__ == "__main__":
    main()
