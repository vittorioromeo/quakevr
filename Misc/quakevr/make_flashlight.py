#!/usr/bin/env python3
# make_flashlight.py -- generates the flashlight (vr_flashlight.cpp):
#   quakevr/progs/vrflashlight.mdl     a straight torch (round 21; it was a right-angle army torch), made over later
#                                      to fit Quake: an old army torch of pressed steel, painted a dark umber worn
#                                      through to the iron on every edge, rusted in its seams and pits, grimy. Its tube
#                                      has seven rolled grip ridges (modelled), a fluted iron tail cap round a cracked
#                                      rubber button, a slide switch on a riveted boss just below the head (the button
#                                      with three ridges across it, modelled), a head with four cooling fins and a
#                                      fluted tarnished-brass bezel round the lens. Held in the fist like a real torch
#                                      (the tube through the curled fingers, the head out past the thumb and index
#                                      finger), it lights along its axis; clipped on a gun, it lies under the barrel,
#                                      parallel to it; on the chest, it points forward from its clip.
#   quakevr/progs/vrflashlight.mdl_0.png  its full-colour skin (FULL times the 8-bit skin's size: what the engine
#                                      draws; paint_full()), painted with its relief (the normal map's: bake_normals.py,
#                                      normaltiles.py's "relief" recipe: relief()); the 8-bit skins are it in Quake's
#                                      palette, skin 1 with the lens fullbright
#   quakevr/sound/vr/flashlight_on.wav, flashlight_off.wav  its switch's clicks
#   quakevr/sound/vr/flashlight_attach.wav, flashlight_detach.wav  its clamp clipping onto a gun, and off
#   quakevr/sound/vr/flashlight_flip.wav  the hand turning it round in the fist (B/Y: the low and overhead grips)
#   quakevr/sound/vr/flashlight_grab.wav  a hand closing round it as it takes it (off the belt, or from the other hand)
#
# Usage: python Misc/quakevr/make_flashlight.py [output game folder] [--keep-edited | --force]
# Then: python Misc/quakevr/bake_normals.py vrflashlight.mdl (its normal map, from relief()).
# (genguard.py: it stops rather than overwrite a file edited since it wrote it: the model edited in Blender.)
#
# Model space: Quake units at vr_world_scale 1 (1 m = 1 / 0.0381 units), +x along the tube to the lens (the beam),
# +z the side the switch is on, +y left; the origin on the axis in the middle of the grip (where the fist holds it).
# The lens's centre is at LENS, its radius LENS_R, the tail's end at TAIL (vr_flashlight.cpp's lensPoint, lensRadius
# and capPoint must match; the visible beam starts at the lens, the cord goes into the tail), the switch at SWITCH
# (its clicks come from there). Skins: 0 off, 1 on (the lens fullbright). The grip ridges stand out to R_RING, the
# radius the hands were tuned on (checks.py FL_GRIP_R); the switch no higher over the tube than the round-21 one.

import math
import os
import random
import struct
import sys

import genguard
import mdlgen
from mdlgen import add, sub, mul, dot, cross, norm

UNITS = 1.0 / 0.0381
SIDES = 24  # round the axis (even: the flutes are every other vertex)

SKIN_W, SKIN_H = 256, 128
FULL = 4
ROUND_H = 2 * SIDES  # a round region's rows: two a side, the last vertex on its last row + 1 (the next region's bleed)
# Round regions (the tube's, the head's...) run along the axis across s (by x: AXIAL; a flat step across the axis by
# its radius: RADIAL) and once round it down t (vertex k on row t0 + 2 k). The lens's disc and the switch's boxes are
# flat.
REGIONS = {"body": (0, 0, 160, 50), "head": (160, 0, 224, 50), "cap": (224, 0, 256, 50),
           "bezel": (0, 64, 40, 114), "rubber": (40, 64, 72, 114), "face": (72, 64, 136, 114),
           "metal": (136, 64, 168, 96), "button": (136, 96, 168, 128), "lens": (192, 64, 256, 128)}
ROUND = ("body", "head", "cap", "bezel", "rubber", "face")

# Along the axis (metres): the tail button's tip, the tail cap, the tube (grip ridges on it), the head's flare, the
# head (its fins), the bezel and the lens inset in it. About 13 cm long, the tube 2.6 cm across, the head 3.7.
TAIL = -0.052
CAP_BACK, CAP_FRONT = -0.049, -0.036
TUBE_FRONT = 0.040
FLARE_END = 0.052
HEAD_FRONT = 0.073
BEZEL_FRONT = 0.077
LENS_X = 0.0755
R_TUBE, R_RING, R_CAP, R_BUTTON = 0.0130, 0.0138, 0.0142, 0.0065
R_HEAD, R_BEZEL, LENS_R = 0.0185, 0.0195, 0.0158
FLUTE_CAP, FLUTE_BEZEL, FIN_DEPTH = 0.0009, 0.0007, 0.0014
# The grip ridges: rolled beads round the tube (their feet, their crowns), from GRIP0 every RIDGE_PITCH.
GRIP0, RIDGES, RIDGE_PITCH = -0.030, 7, 0.008
RIDGE_FOOT, RIDGE_CROWN = 0.0030, 0.0016  # metres wide
# The head's fins: grooves cut round it between the flare and the bezel.
FINS = ((0.0555, 0.0575), (0.0605, 0.0625), (0.0655, 0.0675))
AXIAL = {"body": (CAP_FRONT, TUBE_FRONT), "head": (TUBE_FRONT, HEAD_FRONT), "cap": (CAP_BACK, CAP_FRONT),
         "bezel": (HEAD_FRONT, BEZEL_FRONT), "rubber": (TAIL, CAP_BACK)}
RADIAL = {"face": (R_BUTTON * 0.5, R_BEZEL), "bezel": (LENS_R, R_BEZEL), "rubber": (0.0, R_BUTTON)}
SWITCH = (0.032, 0.0, R_TUBE + 0.0012)
LENS = (LENS_X, 0.0, 0.0)


def ridges():
    """The grip ridges' (foot, crown start, crown end, foot) x positions, back to front."""
    out = []
    for i in range(RIDGES):
        a = GRIP0 + i * RIDGE_PITCH
        m = (RIDGE_FOOT - RIDGE_CROWN) / 2
        out.append((a, a + m, a + m + RIDGE_CROWN, a + RIDGE_FOOT))
    return out


class Builder:
    def __init__(self):
        self.mesh = mdlgen.Mesh(SKIN_W, SKIN_H, REGIONS)

    def vert(self, p, n, st):
        s = min(max(int(round(st[0])), 0), SKIN_W - 1)
        t = min(max(int(round(st[1])), 0), SKIN_H - 1)
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
    def ring_point(x, r, k, flute=0.0):
        a = 2 * math.pi * k / SIDES
        r = r - (flute if k % 2 else 0.0)
        return (x, r * math.cos(a), r * math.sin(a))

    @staticmethod
    def s_of(region, x, r, step):
        """s for a point at (x, r): by x in a region along the axis, by r in one across it (a flat step, where the
        region has a radial range)."""
        s0, t0, s1, t1 = REGIONS[region]
        if region in AXIAL and not (step and region in RADIAL):
            a, b = AXIAL[region]
            f = (x - a) / (b - a)
        else:
            a, b = RADIAL[region]
            f = (r - a) / (b - a)
        return s0 + 0.5 + max(0.0, min(1.0, f)) * (s1 - s0 - 1)

    def band(self, xa, ra, xb, rb, region, facing=None, fa=0.0, fb=0.0):
        """Quads round the axis between (xa, ra) and (xb, rb), facing away from the axis (or along `facing`: a flat
        step; "in": towards the axis); fluted by fa, fb at its ends (every other vertex that much further in). Down t
        once round the axis; across s by x (or, a flat step, by its radius)."""
        s0, t0, s1, t1 = REGIONS[region]
        step = xa == xb
        sa = self.s_of(region, xa, ra, step)
        sb = self.s_of(region, xb, rb, step)
        for k in range(SIDES):
            pts = [self.ring_point(xa, ra, k, fa), self.ring_point(xa, ra, k + 1, fa),
                   self.ring_point(xb, rb, k + 1, fb), self.ring_point(xb, rb, k, fb)]
            n = norm(cross(sub(pts[1], pts[0]), sub(pts[3], pts[0])))
            centre = mul(add(add(pts[0], pts[1]), add(pts[2], pts[3])), 0.25)
            out = (0.0, centre[1], centre[2])
            want = mul(out, -1.0) if facing == "in" else facing if facing else out
            if dot(n, want) < 0.0:
                n = mul(n, -1.0)
            ta, tb = t0 + 2 * k, t0 + 2 * (k + 1)
            st = [(sa, ta), (sa, tb), (sb, tb), (sb, ta)]
            i = [self.vert(p, n, uv) for p, uv in zip(pts, st)]
            self.tri(i[0], i[1], i[2], n)
            self.tri(i[0], i[2], i[3], n)

    def disc(self, x, r, facing, region):
        """A flat disc across the axis at `x`, facing +x (1) or -x (-1): a flat region's picture on it, or a round
        region's middle."""
        n = (float(facing), 0.0, 0.0)
        s0, t0, s1, t1 = REGIONS[region]
        if region in ROUND:
            centre = self.vert((x, 0.0, 0.0), n, (self.s_of(region, x, 0.0, True), t0 + SIDES))
            ring = [self.vert(self.ring_point(x, r, k), n, (self.s_of(region, x, r, True), t0 + 2 * k)) for k in range(SIDES)]
        else:
            cs, ct = (s0 + s1) / 2, (t0 + t1) / 2
            centre = self.vert((x, 0.0, 0.0), n, (cs, ct))
            ring = []
            for k in range(SIDES):
                p = self.ring_point(x, r, k)
                ring.append(self.vert(p, n, (cs + p[1] / r * (s1 - s0 - 2) / 2, ct + p[2] / r * (t1 - t0 - 2) / 2)))
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
    # The tail: the rubber button's dome, its lip, the fluted iron cap round it, the step down to the tube.
    b.disc(TAIL, R_BUTTON * 0.45, -1, "rubber")
    b.band(TAIL, R_BUTTON * 0.45, TAIL + 0.0012, R_BUTTON * 0.8, "rubber")
    b.band(TAIL + 0.0012, R_BUTTON * 0.8, CAP_BACK, R_BUTTON, "rubber")
    b.band(CAP_BACK, R_BUTTON, CAP_BACK, R_CAP - 0.0012, "face", back)
    b.band(CAP_BACK, R_CAP - 0.0012, CAP_BACK + 0.0012, R_CAP, "cap", fb=FLUTE_CAP)  # a chamfer
    b.band(CAP_BACK + 0.0012, R_CAP, CAP_FRONT - 0.001, R_CAP, "cap", fa=FLUTE_CAP, fb=FLUTE_CAP)
    b.band(CAP_FRONT - 0.001, R_CAP, CAP_FRONT, R_CAP - 0.0006, "cap", fa=FLUTE_CAP)
    b.band(CAP_FRONT, R_CAP - 0.0006, CAP_FRONT, R_TUBE, "face", front)
    # The tube, the grip's rolled ridges standing out of it.
    x = CAP_FRONT
    for a, c0, c1, d in ridges():
        b.band(x, R_TUBE, a, R_TUBE, "body")
        b.band(a, R_TUBE, c0, R_RING, "body")
        b.band(c0, R_RING, c1, R_RING, "body")
        b.band(c1, R_RING, d, R_TUBE, "body")
        x = d
    b.band(x, R_TUBE, TUBE_FRONT, R_TUBE, "body")
    # The switch: an iron boss riveted to the tube's top just below the head (under the thumb in the fist), a rubber
    # slide button on it with three ridges across it.
    sx, _, _ = SWITCH
    b.box((sx, 0.0, R_TUBE + 0.0002), (0.0068, 0.0044, 0.0008), "metal", bevel=0.0016)
    b.box((sx, 0.0, R_TUBE + 0.0018), (0.0042, 0.0031, 0.0008), "button", bevel=0.001)
    for dx in (-0.0024, 0.0, 0.0024):
        b.box((sx + dx, 0.0, R_TUBE + 0.0028), (0.00055, 0.0028, 0.0003), "button")
    # The head: flaring out of the tube, its fins cut round it, a fluted brass bezel standing a little proud, the lens
    # set into it.
    b.band(TUBE_FRONT, R_TUBE, FLARE_END, R_HEAD, "head")
    x = FLARE_END
    for a, c in FINS:
        b.band(x, R_HEAD, a, R_HEAD, "head")
        b.band(a, R_HEAD, a, R_HEAD - FIN_DEPTH, "head", front)
        b.band(a, R_HEAD - FIN_DEPTH, c, R_HEAD - FIN_DEPTH, "head")
        b.band(c, R_HEAD - FIN_DEPTH, c, R_HEAD, "head", back)
        x = c
    b.band(x, R_HEAD, HEAD_FRONT, R_HEAD, "head")
    b.band(HEAD_FRONT, R_HEAD, HEAD_FRONT, R_BEZEL - FLUTE_BEZEL, "face", back)
    b.band(HEAD_FRONT, R_BEZEL - FLUTE_BEZEL, HEAD_FRONT + 0.0008, R_BEZEL, "bezel", fb=FLUTE_BEZEL)
    b.band(HEAD_FRONT + 0.0008, R_BEZEL, BEZEL_FRONT, R_BEZEL, "bezel", fa=FLUTE_BEZEL, fb=FLUTE_BEZEL)
    b.band(BEZEL_FRONT, R_BEZEL, BEZEL_FRONT, LENS_R, "bezel", front, fa=FLUTE_BEZEL)
    b.band(BEZEL_FRONT, LENS_R, LENS_X, LENS_R, "bezel", "in")
    b.disc(LENS_X, LENS_R, 1, "lens")
    return b.mesh


# ----------------------------------------------------------------------------
# The skin. Palette ramps (dark to light) the full-colour skin is made in and the 8-bit one is matched to.

PAINT = [175, 174, 173, 172, 171, 170, 169, 168]           # a dark umber army paint, dulled
PAINT_EDGE = [0, 16, 175, 17, 174]                          # its broken rim, dark
IRON = [1, 2, 3, 4, 5, 6, 7, 8, 9]                          # the pressed steel under it, worn bright only on the edges
RUST = [16, 17, 18, 97, 98, 99, 100, 101, 102]
BRASS = [16, 17, 18, 19, 20, 21, 22, 23, 24, 25]            # the bezel, tarnished
RUBBER = [1, 16, 2, 17, 3, 18, 4]
LENS_OFF = [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]
GRIME = (0.05, 0.035, 0.025)
DEPTH = 3.0  # skin texels (0.25-0.4 mm each, round the tube) a relief unit: deep, as the crowbar's


def paint_full(k=FULL):
    """The skin in full colour ((SKIN_H k, SKIN_W k, 3) sRGB 0..1) and its relief ((SKIN_H k, SKIN_W k) skin texels).
    Noise is taken on the torch's own surface (round it, seamless at the skin's seam), in millimetres: the paint
    chipped along every edge (the ridges' crowns, the fins, the ends), worn off where the fist holds it, its rim dark,
    scratched, blistered, grime in the hollows; the iron under it rusty in the seams, pitted; the cap bare iron, fluted
    and knurled, rusted between its flutes; the bezel tarnished brass; the rubber cracked and stippled; the lens a
    dirty reflector behind scratched glass."""
    import numpy as np
    import make_debris as md
    H, W = SKIN_H * k, SKIN_W * k
    ys, xs = np.mgrid[0:H, 0:W]
    s = (xs + 0.5) / k - 0.5
    t = (ys + 0.5) / k - 0.5
    rgb = np.zeros((H, W, 3))
    h = np.zeros((H, W))

    def ramp(r, x):
        return md.ramp_rgb(r, np.clip(x, 0, 1))

    def where(name):
        s0, t0, s1, t1 = REGIONS[name]
        return (s >= s0 - 0.5) & (s < s1 - 0.5) & (t >= t0 - 0.5) & (t < t1 - 0.5)

    def surface(name, m, radius):
        """Millimetre coordinates on the torch's surface for a round region's texels: along x (or out along the
        radius, a step) and round it."""
        s0, t0, s1, t1 = REGIONS[name]
        f = np.clip((s[m] - s0 - 0.5) / (s1 - s0 - 1), 0, 1)
        if name in AXIAL:
            a, b = AXIAL[name]
            x = a + f * (b - a)
            r = np.full_like(x, radius)
        else:
            a, b = RADIAL[name]
            r = a + f * (b - a)
            x = np.full_like(r, radius)  # (a step: `radius` is its x)
        ang = 2 * math.pi * (t[m] - t0) / ROUND_H
        return x * 1000, r * 1000, ang, np.stack([x * 1000, r * 1000 * np.cos(ang), r * 1000 * np.sin(ang)], -1)

    def scratches(p, seed, density=1.0):
        """Fine scratches (0..1) at three angles, as the crowbar's: in mm on the surface (x, round it)."""
        u0, v0 = p[..., 0], np.arctan2(p[..., 2], p[..., 1]) * 14.0
        sc = np.zeros(p.shape[:-1])
        for j, (ang, spacing, keep) in enumerate(((0.3, 2.2, 0.22), (-0.5, 3.1, 0.16), (1.3, 3.7, 0.12))):
            u = u0 * math.cos(ang) + v0 * math.sin(ang)
            v = -u0 * math.sin(ang) + v0 * math.cos(ang)
            w = v / spacing + (md.fbm(np.stack([u * 0.08, v * 0.15, np.full_like(u, j * 7.0)], -1), seed + j, 2) - 0.5) * 2
            row = np.floor(w)
            d = np.abs(w - row - 0.5) * spacing
            on = md.vnoise(np.stack([u * 0.25, row * 3.1, np.full_like(u, j * 5.0)], -1), seed + 7 + j) > 1 - keep * density
            sc = np.maximum(sc, np.where(on, 1 - md.smoothstep(d, 0.03, 0.12), 0.0))
        return sc

    def painted_iron(m, p, crest, low, seed, worn=0.0):
        """Paint over iron: colour and relief for the texels `m` at surface points `p` (mm); `crest` 0..1 where the
        shape stands out (the paint wears through there first), `low` where it is sunk (grime, rust); `worn` more
        wear everywhere (where the hand holds it)."""
        patchy = md.fbm(p / 7.0, seed + 1, 3)
        wear = 0.6 * md.fbm(p / 4.0, seed + 2, 3) + 0.4 * md.fbm(p / 1.2, seed + 3, 2)
        wear = wear + crest * 0.24 * (0.25 + md.smoothstep(patchy, 0.3, 0.6)) + worn
        wear = wear + 0.25 * md.smoothstep(md.vnoise(p / 1.6, seed + 4), 0.8, 0.95) * md.smoothstep(patchy, 0.3, 0.6)
        chipped = md.smoothstep(wear, 0.735, 0.75)
        rim = md.smoothstep(wear, 0.71, 0.735) * (1 - chipped)
        pits = md.smoothstep(md.vnoise(p / 0.35, seed + 5), 0.72, 0.9) * (0.35 + 0.65 * md.smoothstep(md.fbm(p / 4.0, seed + 6, 2), 0.4, 0.7))
        rust = np.clip(md.smoothstep(md.fbm(p / 2.0, seed + 7, 3), 0.55, 0.72) * 0.7 + 0.6 * pits + 0.5 * low, 0, 1)
        lit_iron = 0.3 + 0.3 * (md.fbm(p / 2.5, seed + 8, 3) - 0.5) + 0.28 * crest - 0.25 * pits
        c_iron = md.mix(ramp(IRON, lit_iron) * np.array((1.0, 0.95, 0.88)), ramp(RUST, 0.2 + 0.5 * md.vnoise(p / 0.5, seed + 9) - 0.15 * pits), 0.8 * rust)
        sc = scratches(p, seed + 20)
        grime = np.clip(low * (0.5 + 0.5 * md.fbm(p / 1.5, seed + 10, 3)) + 0.35 * md.smoothstep(md.fbm(p / 3.0, seed + 11, 3), 0.55, 0.75), 0, 1)
        lit_paint = 0.42 + 0.34 * (md.fbm(p / 4.0, seed + 12, 3) - 0.5) + 0.1 * (md.vnoise(p / 0.4, seed + 13) - 0.5) + 0.1 * crest
        c = ramp(PAINT, lit_paint)
        c = md.mix(c, ramp(PAINT_EDGE, 0.3 + 0.4 * md.vnoise(p / 0.3, seed + 14)), rim)
        c = md.mix(c, ramp(IRON, 0.5 + 0.2 * md.vnoise(p / 0.3, seed + 15)), sc * (1 - chipped) * 0.75)
        blister = md.smoothstep(md.vnoise(p / 0.9, seed + 16), 0.8, 0.92)
        c = md.mix(c, ramp(RUST, 0.35), 0.5 * blister)  # rust coming up under the paint
        stain = md.smoothstep(md.fbm(p * np.array((0.15, 0.4, 0.4)), seed + 30, 3), 0.56, 0.72)  # rust bled over it
        c = md.mix(c, ramp(RUST, 0.25 + 0.35 * md.vnoise(p / 0.6, seed + 31)), 0.4 * stain)
        c = md.mix(c, np.array(GRIME), 0.65 * grime)
        c = md.mix(c, c_iron, chipped)
        c = md.mix(c, np.array(GRIME), 0.35 * grime * chipped)
        hh = (0.35 * (1 - chipped) + 0.1 * (md.fbm(p / 0.4, seed + 17, 2) - 0.5) - 0.18 * sc * (1 - chipped)
              + 0.2 * blister * (1 - chipped) - 0.1 * rim
              + chipped * (-0.5 * pits + 0.15 * rust * (md.fbm(p / 0.5, seed + 18, 2) - 0.35)))
        dents = md.smoothstep(md.vnoise(p / 3.5, seed + 19), 0.82, 0.95)
        rgb[m] = c
        h[m] = (hh - 0.3 * dents) * DEPTH

    # The tube: painted, worn through on the ridges' crowns and where the fist holds it (the grip's middle).
    m = where("body")
    x, r, ang, p = surface("body", m, R_TUBE)
    crest = np.zeros_like(x)
    low = np.zeros_like(x)
    for a, c0, c1, d in ridges():
        a, c0, c1, d = (v * 1000 for v in (a, c0, c1, d))
        crest = np.maximum(crest, 1 - md.smoothstep(np.abs(x - (c0 + c1) / 2), (c1 - c0) / 2 - 0.1, (c1 - c0) / 2 + 0.4))
        low = np.maximum(low, np.exp(-np.minimum(np.abs(x - a), np.abs(x - d)) / 0.5))  # the ridges' feet
    ends = np.minimum(x - CAP_FRONT * 1000, TUBE_FRONT * 1000 - x)
    low = np.maximum(low, np.exp(-np.maximum(ends, 0) / 1.2))
    sw = np.exp(-(((x - SWITCH[0] * 1000) / 8.0) ** 2 + ((ang - math.pi / 2) / 0.45) ** 2))  # round the switch's boss
    low = np.maximum(low, 0.8 * sw)
    held = np.exp(-((x - 0.0) / 22.0) ** 2) * 0.12 * md.smoothstep(md.fbm(p / 6.0, 77, 2), 0.35, 0.6)
    painted_iron(m, p, crest, low, 100, held)
    # A rolled seam along the tube (underneath, away from the switch), and stamped letters' dents near the cap.
    seam = np.exp(-((ang - 1.5 * math.pi) / 0.04) ** 2)
    rgb[m] = md.mix(rgb[m], ramp(RUST, 0.25), 0.6 * seam)
    h[m] = h[m] - 0.6 * DEPTH * seam

    # The head: painted, the fins' edges and the flare's shoulder worn to the iron, grime and rust in the grooves.
    m = where("head")
    x, r, ang, p = surface("head", m, R_HEAD)
    crest = np.zeros_like(x)
    low = np.zeros_like(x)
    for a, c in FINS:
        a, c = a * 1000, c * 1000
        crest = np.maximum(crest, np.exp(-np.minimum(np.abs(x - a), np.abs(x - c)) / 0.35))
        low = np.maximum(low, (x > a) & (x < c))
    crest = np.maximum(crest, np.exp(-np.abs(x - FLARE_END * 1000) / 0.6))
    crest = np.maximum(crest, np.exp(-np.abs(x - HEAD_FRONT * 1000) / 0.5))
    low = np.maximum(low, np.exp(-np.abs(x - TUBE_FRONT * 1000) / 1.0))
    painted_iron(m, p, crest, low.astype(float), 200)

    # The cap: bare iron, fluted (rust between the flutes, the crests polished by hands) and knurled.
    m = where("cap")
    x, r, ang, p = surface("cap", m, R_CAP)
    flute = 0.5 + 0.5 * np.cos(ang * SIDES / 2)  # 1 on a crest (even vertices), 0 in a flute
    edge = np.exp(-np.minimum(x - CAP_BACK * 1000, CAP_FRONT * 1000 - x) / 0.8)
    xr = x * 1.4
    ar = ang * R_CAP * 1000 * 1.4
    knurl = np.maximum(np.abs(np.sin((xr + ar) * 1.6)), np.abs(np.sin((xr - ar) * 1.6)))
    pits = md.smoothstep(md.vnoise(p / 0.3, 301), 0.7, 0.9)
    rust = np.clip((1 - flute) * 0.7 * md.smoothstep(md.fbm(p / 1.5, 302, 3), 0.3, 0.6) + 0.6 * pits
                   + 0.4 * md.smoothstep(md.fbm(p / 2.5, 303, 3), 0.6, 0.75), 0, 1)
    lit = 0.25 + 0.3 * flute + 0.2 * edge + 0.25 * (md.fbm(p / 2.0, 304, 3) - 0.5) - 0.15 * (1 - knurl) - 0.2 * pits
    c = md.mix(ramp(IRON, lit), ramp(RUST, 0.15 + 0.5 * md.vnoise(p / 0.4, 305)), 0.85 * rust)
    rgb[m] = md.mix(c, np.array(GRIME), 0.5 * (1 - flute) * (1 - knurl))
    h[m] = (0.3 * knurl - 0.45 * pits + 0.12 * rust) * DEPTH

    # The steps across the axis: iron, grimy and rusty (dirt collects against them).
    m = where("face")
    x, r, ang, p = surface("face", m, 0.0)
    pits = md.smoothstep(md.vnoise(p / 0.3, 401), 0.7, 0.9)
    rust = np.clip(md.smoothstep(md.fbm(p / 1.5, 402, 3), 0.45, 0.65) + 0.5 * pits, 0, 1)
    c = md.mix(ramp(IRON, 0.22 + 0.25 * (md.fbm(p / 2.0, 403, 3) - 0.5)), ramp(RUST, 0.15 + 0.4 * md.vnoise(p / 0.5, 404)), 0.75 * rust)
    rgb[m] = md.mix(c, np.array(GRIME), 0.45 * md.fbm(p / 1.0, 405, 2))
    h[m] = (-0.4 * pits + 0.15 * rust) * DEPTH

    # The bezel: tarnished brass, fluted (the crests rubbed bright, verdigris-dark grime in the flutes), dented, its
    # front face scratched.
    m = where("bezel")
    x, r, ang, p = surface("bezel", m, R_BEZEL)
    flute = 0.5 + 0.5 * np.cos(ang * SIDES / 2)
    sc = scratches(p * 1.5, 501)
    lit = 0.3 + 0.35 * flute + 0.25 * (md.fbm(p / 1.5, 502, 3) - 0.5) + 0.25 * sc
    c = ramp(BRASS, lit)
    tarnish = md.smoothstep(md.fbm(p / 1.2, 503, 3), 0.45, 0.7) * (1 - 0.6 * flute)
    c = md.mix(c, np.array((0.09, 0.08, 0.05)), 0.7 * tarnish)
    rgb[m] = c
    h[m] = (0.15 * flute - 0.15 * sc - 0.3 * md.smoothstep(md.vnoise(p / 1.2, 504), 0.8, 0.95)) * DEPTH

    # The rubber: the tail button, cracked and stippled; the switch's button the same.
    for name in ("rubber", "button"):
        m = where(name)
        if name in ROUND:
            _, _, _, p = surface(name, m, TAIL)
        else:
            p = np.stack([s[m] * 0.25, t[m] * 0.25, np.full(s[m].shape, 9.0)], -1)
        stip = 0.5 + 0.5 * np.sin(p[..., 0] * 9.0) * np.sin(p[..., 1] * 9.0 + p[..., 2] * 3.0)
        crack = md.smoothstep(1 - np.abs(2 * md.fbm(p / 1.2, 601, 3) - 1), 0.93, 0.985)
        lit = 0.3 + 0.25 * (md.fbm(p / 1.5, 602, 3) - 0.5) + 0.12 * stip - 0.3 * crack
        c = ramp(RUBBER, lit)
        rgb[m] = md.mix(c, np.array(GRIME), 0.3 * md.fbm(p / 2.0, 603, 2))
        h[m] = (0.12 * stip - 0.5 * crack) * DEPTH

    # The switch's boss: iron, two rivets, rusty round them.
    m = where("metal")
    s0, t0, s1, t1 = REGIONS["metal"]
    u, v = (s[m] - s0) / (s1 - s0), (t[m] - t0) / (t1 - t0)
    p = np.stack([s[m] * 0.3, t[m] * 0.3, np.full(u.shape, 3.0)], -1)
    rivet = np.zeros_like(u)
    for ru in (0.12, 0.88):
        d = np.hypot((u - ru) * 2.0, v - 0.5)
        rivet = np.maximum(rivet, np.sqrt(np.clip(1 - (d / 0.11) ** 2, 0, 1)))
    pits = md.smoothstep(md.vnoise(p / 0.3, 701), 0.7, 0.9)
    rust = np.clip(md.smoothstep(md.fbm(p / 1.2, 702, 3), 0.45, 0.65) + 0.5 * pits, 0, 1) * (1 - 0.6 * rivet)
    c = md.mix(ramp(IRON, 0.28 + 0.3 * rivet + 0.2 * (md.fbm(p / 2.0, 703, 3) - 0.5)), ramp(RUST, 0.2 + 0.4 * md.vnoise(p / 0.5, 704)), 0.8 * rust)
    rgb[m] = c
    h[m] = (0.6 * rivet - 0.4 * pits) * DEPTH

    # The lens (off): a dented reflector behind dirty, scratched glass.
    m = where("lens")
    s0, t0, s1, t1 = REGIONS["lens"]
    u, v = (s[m] - s0) / (s1 - s0) - 0.5, (t[m] - t0) / (t1 - t0) - 0.5
    d = np.hypot(u, v) * 2
    p = np.stack([s[m] * 0.4, t[m] * 0.4, np.full(u.shape, 5.0)], -1)
    lit = 0.85 - 0.6 * d + 0.12 * np.cos(d * 22.0) + 0.15 * (md.fbm(p / 1.0, 801, 3) - 0.5)
    c = ramp(LENS_OFF, lit)
    dirt = md.smoothstep(md.fbm(p / 3.0, 802, 3), 0.45, 0.8) * 0.35 + 0.5 * md.smoothstep(d, 0.7, 1.0)
    c = md.mix(c, np.array((0.16, 0.12, 0.08)), dirt)
    rgb[m] = md.mix(c, np.array((0.7, 0.68, 0.6)), 0.5 * scratches(p, 803, 0.6))
    h[m] = 0.0

    return np.clip(rgb, 0, 1), h


_full = {}


def full():
    if not _full:
        _full["rgb"], _full["h"] = paint_full()
    return _full["rgb"], _full["h"]


def relief(name):
    """The torch's relief for its normal map (normaltiles.py): (heights in skin texels, 1, FULL)."""
    return full()[1], 1.0, FULL


def paint(rgb, on):
    """The 8-bit skin (palette indices): the full-colour one in Quake's palette; `on`: the lens fullbright
    white-yellow (vr_flashlight.cpp finds the lens by it)."""
    import make_debris as md
    skin = bytearray(md.to_palette(rgb, SKIN_W, PAINT + PAINT_EDGE + IRON + RUST + BRASS + RUBBER + LENS_OFF).tobytes())
    if on:
        s0, t0, s1, t1 = REGIONS["lens"]
        for t in range(t0, t1):
            for s in range(s0, s1):
                d = math.hypot((s - s0 + 0.5) / (s1 - s0) - 0.5, (t - t0 + 0.5) / (t1 - t0) - 0.5) * 2
                skin[t * SKIN_W + s] = 254 if d < 0.35 else 253 if d < 0.7 else 252
    return bytes(skin)


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


def grab(seed):
    """A hand closing round the torch: a short soft scuff of the palm on the tube (low-passed noise over 25 ms), then
    the fingers seating: a muted low knock and a faint tick of the metal. Quieter than the clamp's snap: a cue, not a
    clatter."""
    rng = random.Random(seed)
    n = int(RATE * 0.1)
    out = [0.0] * n
    lp = hp = 0.0
    for i in range(int(RATE * 0.025)):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        lp += 0.2 * (noise - lp)
        hp += 0.04 * (lp - hp)
        out[i] += (lp - hp) * 0.6 * math.sin(math.pi * t / 0.025)
    at = int(RATE * 0.018)
    for i in range(n - at):
        t = i / RATE
        knock = math.sin(2 * math.pi * 140 * t) * math.exp(-t / 0.014) * 0.8
        body = math.sin(2 * math.pi * 410 * t) * math.exp(-t / 0.006) * 0.25
        tick = math.sin(2 * math.pi * 3100 * t) * math.exp(-t / 0.002) * 0.12
        out[at + i] += (knock + body + tick) * min(1.0, t / 0.0015)
    peak = max(abs(s) for s in out) or 1.0
    return [s * 0.5 / peak for s in out]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    game = args[0] if args else os.path.join(here, "..", "..", "quakevr")

    mesh = build()
    path = os.path.join(game, "progs", "vrflashlight.mdl")
    sounds = os.path.join(game, "sound", "vr")
    # The files edited by hand since this wrote them are not overwritten (genguard.py: --keep-edited, --force).
    guard = genguard.Guard("make_flashlight.py", [path, path + "_0.png"] + [os.path.join(sounds, n + ".wav") for n in (
        "flashlight_on", "flashlight_off", "flashlight_attach", "flashlight_detach", "flashlight_flip", "flashlight_grab")])
    import numpy as np
    from PIL import Image
    rgb, _ = full()
    if path not in guard.kept:
        mdlgen.write_mdl(path, mesh, [paint(rgb, False), paint(rgb, True)], "flashlight")
    if path + "_0.png" not in guard.kept:
        Image.fromarray(np.round(rgb * 255).astype(np.uint8)).save(path + "_0.png", optimize=True)
    print("vrflashlight.mdl: %d vertices, %d triangles -> %s" % (len(mesh.verts), len(mesh.tris), os.path.normpath(path)))
    print("  lens at (%.3f %.3f %.3f) units, radius %.4f; tail at %.3f; switch at (%.3f %.3f %.3f)" %
          (tuple(c * UNITS for c in LENS) + (LENS_R * UNITS, TAIL * UNITS) + tuple(c * UNITS for c in SWITCH)))

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
    wav = os.path.join(sounds, "flashlight_grab.wav")
    write_wav(wav, grab(16))
    print("flashlight_grab.wav -> %s" % os.path.normpath(wav))
    guard.finish()


if __name__ == "__main__":
    main()
