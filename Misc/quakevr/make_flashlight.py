#!/usr/bin/env python3
# make_flashlight.py -- generates the chest flashlight (vr_flashlight.cpp):
#   quakevr/progs/vrflashlight.mdl     a right-angle army torch: an upright body with a clip on its
#                                      back, the head on top facing forward. Clipped to the chest,
#                                      the body lies against it; in the hand it is held like a
#                                      pistol's grip, the beam going where the hand points.
#   quakevr/sound/vr/flashlight_on.wav, flashlight_off.wav  its switch's clicks
#
# Usage: python Misc/quakevr/make_flashlight.py [output game folder]
#
# Model space: Quake units at vr_world_scale 1 (1 m = 1 / 0.0381 units), +x forward (the beam), +y
# left, +z up (along the body), the origin at the middle of the body. The lens's centre is at
# LENS (vr_flashlight.cpp's lensPoint must match). Skins: 0 off, 1 on (the lens fullbright).
# Palette indices from gfx/palette.lmp: greys 1..15, fullbright yellow-whites 252..254.

import math
import os
import random
import struct
import sys

import mdlgen
from mdlgen import add, sub, mul, dot, cross, norm

UNITS = 1.0 / 0.0381

SKIN_W, SKIN_H = 64, 32
REGIONS = {"body": (0, 0, 32, 16), "head": (32, 0, 64, 16), "lens": (0, 16, 16, 32), "bezel": (16, 16, 32, 32),
           "clip": (32, 16, 48, 32), "cap": (48, 16, 64, 32)}

# The body: a bevelled block; the head: an octagonal barrel along x over its top, flaring to the
# bezel; the lens inset in the bezel.
BODY_HALF = (0.016, 0.014, 0.036)
HEAD_Z = 0.047
HEAD_BACK, HEAD_FRONT = -0.017, 0.040
LENS = (HEAD_FRONT - 0.002, 0.0, HEAD_Z)


class Builder:
    def __init__(self):
        self.mesh = mdlgen.Mesh(SKIN_W, SKIN_H, REGIONS)

    def vert(self, p, n, st):
        self.mesh.verts.append((mul(p, UNITS), n, (int(round(st[0])), int(round(st[1])))))
        return len(self.mesh.verts) - 1

    def tri(self, a, b, c, n):
        """A triangle, turned clockwise seen from where `n` points."""
        v = self.mesh.verts
        p0, p1, p2 = v[a][0], v[b][0], v[c][0]
        if dot(cross(sub(p1, p0), sub(p2, p0)), n) > 0.0:
            b, c = c, b
        self.mesh.tris.append((a, b, c))

    def quad(self, pts, region, facing=None):
        """A flat quad (its outline in order around) facing away from the head's axis, or towards
        `facing` (a direction; "in": towards the axis)."""
        n = norm(cross(sub(pts[1], pts[0]), sub(pts[3], pts[0])))
        centre = mul(add(add(pts[0], pts[1]), add(pts[2], pts[3])), 0.25)
        out = sub(centre, (centre[0], 0.0, HEAD_Z))
        want = mul(out, -1.0) if facing == "in" else facing if facing else out
        if dot(n, want) < 0.0:
            n = mul(n, -1.0)
        s0, t0, s1, t1 = REGIONS[region]
        st = [(s0 + 1, t0 + 1), (s1 - 1, t0 + 1), (s1 - 1, t1 - 1), (s0 + 1, t1 - 1)]
        i = [self.vert(p, n, uv) for p, uv in zip(pts, st)]
        self.tri(i[0], i[1], i[2], n)
        self.tri(i[0], i[2], i[3], n)

    def disc(self, x, radius, facing, region, sides):
        """A flat octagon across the head's axis at `x`, facing +x (facing 1) or -x (-1)."""
        n = (float(facing), 0.0, 0.0)
        s0, t0, s1, t1 = REGIONS[region]
        cs, ct = (s0 + s1) / 2, (t0 + t1) / 2
        centre = self.vert((x, 0.0, HEAD_Z), n, (cs, ct))
        ring = []
        for k in range(sides):
            a = 2 * math.pi * (k + 0.5) / sides
            y, z = math.cos(a), math.sin(a)
            ring.append(self.vert((x, radius * y, HEAD_Z + radius * z), n,
                                  (cs + y * (s1 - s0 - 2) / 2, ct + z * (t1 - t0 - 2) / 2)))
        for k in range(sides):
            self.tri(centre, ring[k], ring[(k + 1) % sides], n)

    def barrel(self, rings, sides, region, facing=None):
        """Octagonal bands along the head's axis between successive (x, radius) rings (facing as
        quad's)."""
        for (xa, ra), (xb, rb) in zip(rings, rings[1:]):
            for k in range(sides):
                pts = []
                for x, r, kk in ((xa, ra, k), (xa, ra, k + 1), (xb, rb, k + 1), (xb, rb, k)):
                    a = 2 * math.pi * (kk + 0.5) / sides
                    pts.append((x, r * math.cos(a), HEAD_Z + r * math.sin(a)))
                self.quad(pts, region, facing)

    def box(self, centre, half, region, bevel=0.0):
        """mdlgen's bevelled box, in metres."""
        m = mdlgen.Mesh(SKIN_W, SKIN_H, REGIONS)
        m.box(centre, half, region, bevel)
        base = len(self.mesh.verts)
        for p, n, st in m.verts:
            self.mesh.verts.append((mul(p, UNITS), n, st))
        for a, b, c in m.tris:
            self.mesh.tris.append((base + a, base + b, base + c))


def build():
    b = Builder()
    sides = 8
    # The body, a rubber cap under it, and the belt clip down its back.
    b.box((0.0, 0.0, 0.0), BODY_HALF, "body", bevel=0.006)
    b.box((0.0, 0.0, -BODY_HALF[2] - 0.004), (0.013, 0.011, 0.004), "cap", bevel=0.004)
    b.box((-BODY_HALF[0] - 0.003, 0.0, -0.004), (0.0025, 0.011, 0.028), "clip", bevel=0.002)
    # The switch: a knurled button on the body's left side, below the head.
    b.box((0.004, BODY_HALF[1] + 0.002, 0.018), (0.006, 0.002, 0.006), "cap", bevel=0.002)

    # The head: a barrel over the body's top, flaring towards the front to the bezel.
    r = 0.016
    b.barrel([(HEAD_BACK, r), (0.010, r), (0.024, 0.021), (HEAD_FRONT, 0.021)], sides, "head")
    b.disc(HEAD_BACK, r, -1, "head", sides)
    # The bezel's face, and the lens set a little into it.
    b.barrel([(HEAD_FRONT, 0.021), (HEAD_FRONT, 0.0165)], sides, "bezel", (1.0, 0.0, 0.0))
    b.barrel([(HEAD_FRONT, 0.0165), (LENS[0], 0.0165)], sides, "bezel", "in")
    b.disc(LENS[0], 0.0165, 1, "lens", sides)
    return b.mesh


def paint(on):
    """Palette indices: black rubber body, dark gunmetal head and clip, a steel bezel, and the
    lens -- a silvery reflector behind glass when off, fullbright white-yellow when on."""
    rng = random.Random(1234)
    px = bytearray()
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            region = next(r for r, (s0, t0, s1, t1) in REGIONS.items() if s0 <= s < s1 and t0 <= t < t1)
            s0, t0, s1, t1 = REGIONS[region]
            u, v = (s - s0 + 0.5) / (s1 - s0), (t - t0 + 0.5) / (t1 - t0)
            k = rng.random()
            if region == "lens":
                d = math.hypot(u - 0.5, v - 0.5) * 2  # 0 at the middle, 1 at the rim
                if on:
                    px.append(254 if d < 0.35 else 253 if d < 0.7 else 252)
                else:
                    px.append(13 if d < 0.2 else 10 if d < 0.55 + 0.1 * k else 7)
                continue
            if region == "bezel":
                px.append(8 if k < 0.3 else 7 if k < 0.8 else 9)
                continue
            if region == "body":
                # Ribbed rubber grip: darker lines across.
                rib = int(v * 8) % 2 == 0 and 0.2 < v < 0.9
                px.append(1 if rib else (2 if k < 0.7 else 3))
                continue
            if region == "head":
                px.append(3 if k < 0.5 else 4 if k < 0.85 else 5)
                continue
            if region == "clip":
                px.append(5 if k < 0.6 else 6)
                continue
            px.append(1 if k < 0.6 else 2)  # rubber
    return bytes(px)


# ----------------------------------------------------------------------------
# The switch's clicks

RATE = 22050


def write_wav(path, samples):
    data = b"".join(struct.pack("<h", max(-32767, min(32767, int(s * 32767)))) for s in samples)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(data)) + data)


def click(pitch, seed):
    """A tactile switch: the press's snap (a bright tick and a short plastic knock), then the
    softer release 35 ms later."""
    rng = random.Random(seed)
    n = int(RATE * 0.09)
    out = [0.0] * n
    for at, level in ((0.0, 1.0), (0.035, 0.45)):
        lp = 0.0
        for i in range(int(RATE * 0.03)):
            t = i / RATE
            j = int((at + t) * RATE)
            if j >= n:
                break
            noise = rng.uniform(-1, 1)
            lp += 0.35 * (noise - lp)
            tick = (noise - lp) * math.exp(-t / 0.0012)
            knock = (math.sin(2 * math.pi * 2300 * pitch * t) * 0.6 + math.sin(2 * math.pi * 3900 * pitch * t) * 0.3) * \
                math.exp(-t / 0.006)
            body = math.sin(2 * math.pi * 620 * pitch * t) * math.exp(-t / 0.01) * 0.35
            out[j] += level * (tick * 0.8 + knock + body) * 0.5
    return out


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr")

    mesh = build()
    path = os.path.join(game, "progs", "vrflashlight.mdl")
    mdlgen.write_mdl(path, mesh, [paint(False), paint(True)], "flashlight")
    print("vrflashlight.mdl: %d vertices, %d triangles -> %s" % (len(mesh.verts), len(mesh.tris), os.path.normpath(path)))
    print("  lens at (%.3f %.3f %.3f) units" % tuple(c * UNITS for c in LENS))

    sounds = os.path.join(game, "sound", "vr")
    os.makedirs(sounds, exist_ok=True)
    for name, pitch, seed in (("flashlight_on", 1.1, 11), ("flashlight_off", 0.9, 12)):
        wav = os.path.join(sounds, name + ".wav")
        write_wav(wav, click(pitch, seed))
        print("%s.wav -> %s" % (name, os.path.normpath(wav)))


if __name__ == "__main__":
    main()
