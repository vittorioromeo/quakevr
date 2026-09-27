#!/usr/bin/env python3
# make_flashlight.py -- generates the flashlight (vr_flashlight.cpp):
#   quakevr/progs/vrflashlight.mdl     a straight tactical torch (round 21; it was a right-angle army torch): a
#                                      knurled aluminium tube with three grip rings, a ribbed tail cap with a rubber
#                                      button, a rubber switch on the tube just below the head, and a wider head with
#                                      cooling fins, a steel bezel and the lens. Held in the fist like a real torch
#                                      (the tube through the curled fingers, the head out past the thumb and index
#                                      finger), it lights along its axis; clipped on a gun, it lies under the barrel,
#                                      parallel to it; on the chest, it points forward from its clip.
#   quakevr/sound/vr/flashlight_on.wav, flashlight_off.wav  its switch's clicks
#   quakevr/sound/vr/flashlight_attach.wav, flashlight_detach.wav  its clamp clipping onto a gun, and off
#   quakevr/sound/vr/flashlight_flip.wav  the hand turning it round in the fist (B/Y: the low and overhead grips)
#
# Usage: python Misc/quakevr/make_flashlight.py [output game folder]
#
# Model space: Quake units at vr_world_scale 1 (1 m = 1 / 0.0381 units), +x along the tube to the lens (the beam),
# +z the side the switch is on, +y left; the origin on the axis in the middle of the grip (where the fist holds it).
# The lens's centre is at LENS, its radius LENS_R, the tail's end at TAIL (vr_flashlight.cpp's lensPoint, lensRadius
# and capPoint must match; the visible beam starts at the lens, the cord goes into the tail), the switch at SWITCH
# (its clicks come from there). Skins: 0 off, 1 on (the lens fullbright).
# Palette indices from gfx/palette.lmp: greys 1..15, fullbright yellow-whites 252..254.

import math
import os
import random
import struct
import sys

import mdlgen
from mdlgen import add, sub, mul, dot, cross, norm

UNITS = 1.0 / 0.0381
SIDES = 12

SKIN_W, SKIN_H = 128, 64
# Round regions (the tube's, the head's...) run along the axis across s and round it down t.
REGIONS = {"body": (0, 0, 64, 32), "head": (64, 0, 96, 32), "cap": (96, 0, 128, 32), "ring": (0, 32, 32, 48),
           "bezel": (32, 32, 64, 48), "rubber": (64, 32, 96, 64), "lens": (96, 32, 128, 64), "face": (0, 48, 64, 64)}

# Along the axis (metres): the tail button's tip, the tail cap, the tube (grip rings on it), the head's flare, the
# head, the bezel and the lens inset in it. About 13 cm long, the tube 2.6 cm across, the head 3.7.
TAIL = -0.052
CAP_BACK, CAP_FRONT = -0.049, -0.036
TUBE_FRONT = 0.040
FLARE_END = 0.052
HEAD_FRONT = 0.073
BEZEL_FRONT = 0.077
LENS_X = 0.0755
R_TUBE, R_RING, R_CAP, R_BUTTON = 0.0130, 0.0138, 0.0142, 0.0065
R_HEAD, R_BEZEL, LENS_R = 0.0185, 0.0195, 0.0158
RINGS = ((-0.022, -0.018), (0.000, 0.004), (0.022, 0.026))
SWITCH = (0.032, 0.0, R_TUBE + 0.0012)
LENS = (LENS_X, 0.0, 0.0)


class Builder:
    def __init__(self):
        self.mesh = mdlgen.Mesh(SKIN_W, SKIN_H, REGIONS)

    def vert(self, p, n, st):
        s0, t0, s1, t1 = (0, 0, SKIN_W, SKIN_H)
        s = min(max(int(round(st[0])), s0), s1 - 1)
        t = min(max(int(round(st[1])), t0), t1 - 1)
        self.mesh.verts.append((mul(p, UNITS), n, (s, t)))
        return len(self.mesh.verts) - 1

    def tri(self, a, b, c, n):
        """A triangle, turned clockwise seen from where `n` points."""
        v = self.mesh.verts
        p0, p1, p2 = v[a][0], v[b][0], v[c][0]
        if dot(cross(sub(p1, p0), sub(p2, p0)), n) > 0.0:
            b, c = c, b
        self.mesh.tris.append((a, b, c))

    @staticmethod
    def ring_point(x, r, k):
        a = 2 * math.pi * (k + 0.5) / SIDES
        return (x, r * math.cos(a), r * math.sin(a))

    def band(self, xa, ra, xb, rb, region, facing=None):
        """Quads round the axis between (xa, ra) and (xb, rb), facing away from the axis (or along `facing`: a flat
        step; "in": towards the axis). The region runs from xa to xb across s, and once round the axis down t."""
        s0, t0, s1, t1 = REGIONS[region]
        for k in range(SIDES):
            pts = [self.ring_point(xa, ra, k), self.ring_point(xa, ra, k + 1), self.ring_point(xb, rb, k + 1),
                   self.ring_point(xb, rb, k)]
            n = norm(cross(sub(pts[1], pts[0]), sub(pts[3], pts[0])))
            centre = mul(add(add(pts[0], pts[1]), add(pts[2], pts[3])), 0.25)
            out = (0.0, centre[1], centre[2])
            want = mul(out, -1.0) if facing == "in" else facing if facing else out
            if dot(n, want) < 0.0:
                n = mul(n, -1.0)
            ta = t0 + 0.5 + (t1 - t0 - 1) * k / SIDES
            tb = t0 + 0.5 + (t1 - t0 - 1) * (k + 1) / SIDES
            st = [(s0 + 0.5, ta), (s0 + 0.5, tb), (s1 - 0.5, tb), (s1 - 0.5, ta)]
            i = [self.vert(p, n, uv) for p, uv in zip(pts, st)]
            self.tri(i[0], i[1], i[2], n)
            self.tri(i[0], i[2], i[3], n)

    def disc(self, x, r, facing, region):
        """A flat disc across the axis at `x`, facing +x (1) or -x (-1), the region's picture on it."""
        n = (float(facing), 0.0, 0.0)
        s0, t0, s1, t1 = REGIONS[region]
        cs, ct = (s0 + s1) / 2, (t0 + t1) / 2
        centre = self.vert((x, 0.0, 0.0), n, (cs, ct))
        ring = []
        for k in range(SIDES):
            a = 2 * math.pi * (k + 0.5) / SIDES
            y, z = math.cos(a), math.sin(a)
            ring.append(self.vert((x, r * y, r * z), n, (cs + y * (s1 - s0 - 2) / 2, ct + z * (t1 - t0 - 2) / 2)))
        for k in range(SIDES):
            self.tri(centre, ring[k], ring[(k + 1) % SIDES], n)

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
    back, front = (-1.0, 0.0, 0.0), (1.0, 0.0, 0.0)
    # The tail: the rubber button's dome, the cap's end face round it, the ribbed cap, the step down to the tube.
    b.disc(TAIL, R_BUTTON * 0.7, -1, "rubber")
    b.band(TAIL, R_BUTTON * 0.7, CAP_BACK, R_BUTTON, "rubber")
    b.band(CAP_BACK, R_BUTTON, CAP_BACK, R_CAP, "face", back)
    b.band(CAP_BACK, R_CAP, CAP_FRONT, R_CAP, "cap")
    b.band(CAP_FRONT, R_CAP, CAP_FRONT, R_TUBE, "face", front)
    # The knurled tube, three smooth grip rings standing out of it.
    x = CAP_FRONT
    for ra, rb in RINGS:
        b.band(x, R_TUBE, ra, R_TUBE, "body")
        b.band(ra, R_TUBE, ra, R_RING, "ring", back)
        b.band(ra, R_RING, rb, R_RING, "ring")
        b.band(rb, R_RING, rb, R_TUBE, "ring", front)
        x = rb
    b.band(x, R_TUBE, TUBE_FRONT, R_TUBE, "body")
    # The switch: a rubber button on the tube's top, just below the head (under the thumb in the fist).
    b.box(SWITCH, (0.0045, 0.0035, 0.0018), "rubber", bevel=0.0012)
    # The head: flaring out of the tube, finned, a steel bezel standing a little proud, the lens set into it.
    b.band(TUBE_FRONT, R_TUBE, FLARE_END, R_HEAD, "head")
    b.band(FLARE_END, R_HEAD, HEAD_FRONT, R_HEAD, "head")
    b.band(HEAD_FRONT, R_HEAD, HEAD_FRONT, R_BEZEL, "bezel", back)
    b.band(HEAD_FRONT, R_BEZEL, BEZEL_FRONT, R_BEZEL, "bezel")
    b.band(BEZEL_FRONT, R_BEZEL, BEZEL_FRONT, LENS_R, "bezel", front)
    b.band(BEZEL_FRONT, LENS_R, LENS_X, LENS_R, "bezel", "in")
    b.disc(LENS_X, LENS_R, 1, "lens")
    return b.mesh


def paint(on):
    """Palette indices: black anodised aluminium (the tube knurled in a diamond pattern, the head finned, the cap
    ribbed), smooth darker grip rings, a steel bezel, black rubber, and the lens -- a silvery reflector behind glass
    when off, fullbright white-yellow when on. Round regions run along the axis across s, round it down t."""
    rng = random.Random(1234)
    px = bytearray()
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            region = next(r for r, (s0, t0, s1, t1) in REGIONS.items() if s0 <= s < s1 and t0 <= t < t1)
            s0, t0, s1, t1 = REGIONS[region]
            u, v = (s - s0 + 0.5) / (s1 - s0), (t - t0 + 0.5) / (t1 - t0)
            i, j = s - s0, t - t0
            k = rng.random()
            if region == "lens":
                d = math.hypot(u - 0.5, v - 0.5) * 2  # 0 at the middle, 1 at the rim
                if on:
                    px.append(254 if d < 0.35 else 253 if d < 0.7 else 252)
                else:
                    px.append(13 if d < 0.2 else 10 if d < 0.55 + 0.1 * k else 7)
                continue
            if region == "body":
                # Knurling: a diamond grid of cut lines (dark), the flats between them catching a little light.
                cut = (i + j) % 4 == 0 or (i - j) % 4 == 0
                px.append(1 if cut else (2 if k < 0.55 else 3 if k < 0.9 else 4))
                continue
            if region == "head":
                # Cooling fins round the head: dark grooves across the axis, the flare left smooth.
                groove = i % 5 == 0 and u > 0.3
                px.append(1 if groove else (2 if k < 0.5 else 3 if k < 0.9 else 5))
                continue
            if region == "cap":
                # Ribs round the cap for a grip on it.
                px.append(1 if i % 4 < 2 else (3 if k < 0.7 else 4))
                continue
            if region == "ring":
                # Smooth, a thin highlight along the middle.
                px.append(6 if abs(u - 0.5) < 0.15 and k < 0.8 else (3 if k < 0.7 else 4))
                continue
            if region == "bezel":
                px.append(9 if k < 0.35 else 8 if k < 0.8 else 10)
                continue
            if region == "face":
                px.append(2 if k < 0.7 else 3)
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


def latch(attach, seed):
    """The clamp on a gun's rail: clipping on, a metal snap (a noise crack, steel ringing at a few
    inharmonic partials, a low knock of the gun's body) and the latch catching 28 ms later;
    taking off, a short scrape of the clamp sliding off, then a lighter release click."""
    rng = random.Random(seed)
    n = int(RATE * 0.16)
    out = [0.0] * n

    def snap(at, level, pitch):
        lp = 0.0
        for i in range(int(RATE * 0.06)):
            t = i / RATE
            j = int((at + t) * RATE)
            if j >= n:
                break
            noise = rng.uniform(-1, 1)
            lp += 0.3 * (noise - lp)
            crack = (noise - lp) * math.exp(-t / 0.0009)
            ring = sum(a * math.sin(2 * math.pi * f * pitch * t) * math.exp(-t / tau)
                       for f, a, tau in ((1870, 0.5, 0.018), (3120, 0.35, 0.012), (4630, 0.25, 0.007)))
            knock = math.sin(2 * math.pi * 210 * pitch * t) * math.exp(-t / 0.012) * 0.6
            out[j] += level * (crack * 0.9 + ring + knock) * 0.45

    if attach:
        snap(0.0, 1.0, 1.0)
        snap(0.028, 0.6, 1.12)
    else:
        lp = 0.0
        for i in range(int(RATE * 0.05)):  # the scrape: filtered noise swelling and cut
            t = i / RATE
            noise = rng.uniform(-1, 1)
            lp += 0.5 * (noise - lp)
            out[i] += (noise - lp) * 0.25 * math.sin(math.pi * t / 0.05)
        snap(0.045, 0.7, 0.92)
    peak = max(abs(s) for s in out) or 1.0
    return [s * 0.85 / peak for s in out]


def regrip(seed):
    """The torch turned round in the fist: a short scuff of the knurled tube in the palm (band-passed noise swelling
    and dying over 70 ms), then the tube seating in the fingers: a muted low knock and a small tick of the metal."""
    rng = random.Random(seed)
    n = int(RATE * 0.14)
    out = [0.0] * n
    lp = hp = 0.0
    for i in range(int(RATE * 0.07)):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        lp += 0.25 * (noise - lp)
        hp += 0.05 * (lp - hp)
        out[i] += (lp - hp) * 0.5 * math.sin(math.pi * t / 0.07) ** 2
    at = int(RATE * 0.075)
    for i in range(n - at):
        t = i / RATE
        knock = math.sin(2 * math.pi * 170 * t) * math.exp(-t / 0.018) * 0.7
        tick = math.sin(2 * math.pi * 2600 * t) * math.exp(-t / 0.003) * 0.25
        out[at + i] += (knock + tick) * min(1.0, t / 0.001)
    peak = max(abs(s) for s in out) or 1.0
    return [s * 0.6 / peak for s in out]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr")

    mesh = build()
    path = os.path.join(game, "progs", "vrflashlight.mdl")
    mdlgen.write_mdl(path, mesh, [paint(False), paint(True)], "flashlight")
    print("vrflashlight.mdl: %d vertices, %d triangles -> %s" % (len(mesh.verts), len(mesh.tris), os.path.normpath(path)))
    print("  lens at (%.3f %.3f %.3f) units, radius %.4f; tail at %.3f; switch at (%.3f %.3f %.3f)" %
          (tuple(c * UNITS for c in LENS) + (LENS_R * UNITS, TAIL * UNITS) + tuple(c * UNITS for c in SWITCH)))

    sounds = os.path.join(game, "sound", "vr")
    os.makedirs(sounds, exist_ok=True)
    for name, pitch, seed in (("flashlight_on", 1.1, 11), ("flashlight_off", 0.9, 12)):
        wav = os.path.join(sounds, name + ".wav")
        write_wav(wav, click(pitch, seed))
        print("%s.wav -> %s" % (name, os.path.normpath(wav)))
    for name, attach, seed in (("flashlight_attach", True, 13), ("flashlight_detach", False, 14)):
        wav = os.path.join(sounds, name + ".wav")
        write_wav(wav, latch(attach, seed))
        print("%s.wav -> %s" % (name, os.path.normpath(wav)))
    wav = os.path.join(sounds, "flashlight_flip.wav")
    write_wav(wav, regrip(15))
    print("flashlight_flip.wav -> %s" % os.path.normpath(wav))


if __name__ == "__main__":
    main()
