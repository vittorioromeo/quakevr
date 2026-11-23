#!/usr/bin/env python3
# make_lance_sounds.py -- generates Dwell's Crystal Lance's charge hum (quakevr/sound/vr/lance_charge.wav;
# vr_lancecharge.cpp), synthesised (his note 2026-11-23: the trigger held charges the shot, with a charging sound):
#   lance_charge.wav  a crystalline electric hum, 1 s, looped (a cue point: Quake loops it until the engine stops it): a
#                     low buzzing body, glassy high partials shimmering, a fine crackle of sparks; the engine raises its
#                     pitch and volume as the charge grows and fades it when the charge is let go
# Our own sound: nothing from Dwell's files. Seeded: the same file every run.
#
# Usage: python Misc/quakevr/make_lance_sounds.py [out_dir]   (default: quakevr/sound/vr)

import math
import os
import random
import struct
import sys

RATE = 22050


def write_wav(path, samples, loop=False):
    data = b"".join(struct.pack("<h", max(-32767, min(32767, int(s * 32767)))) for s in samples)
    chunks = [b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16), b"data" + struct.pack("<I", len(data)) + data]
    if loop:
        # one cue point at the start (Quake's snd_mem.c GetWavinfo: the loop's start; it runs to the end)
        chunks.append(b"cue " + struct.pack("<II", 28, 1) + struct.pack("<II4sIII", 1, 0, b"data", 0, 0, 0))
    body = b"WAVE" + b"".join(chunks)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


class OnePole:
    def __init__(self, cutoff):
        self.a = 1.0 - math.exp(-2 * math.pi * cutoff / RATE)
        self.y = 0.0

    def __call__(self, x):
        self.y += self.a * (x - self.y)
        return self.y


def hum(rng):
    # Every tone and wobble a whole number of cycles in the second: the loop's end runs on into its start.
    n = RATE
    out = [0.0] * n
    body = [(110, 0.42), (220, 0.30), (330, 0.16), (440, 0.08), (550, 0.05)]     # the buzzing body
    glass = [(1320, 0.10, 7), (1757, 0.08, 11), (2213, 0.06, 13), (2949, 0.04, 17)]  # glassy partials, shimmering
    for i in range(n):
        t = i / RATE
        s = 0.0
        for f, a in body:
            s += a * math.sin(2 * math.pi * f * t)
        s = math.tanh(1.8 * s)  # (a little grit: the electric buzz)
        s *= 0.8 + 0.2 * math.sin(2 * math.pi * 6 * t)
        for f, a, w in glass:
            s += a * (0.55 + 0.45 * math.sin(2 * math.pi * w * t)) * math.sin(2 * math.pi * f * t)
        out[i] = s
    # The crackle: sparse sparks of high noise, crossfaded over the loop's seam.
    x = int(0.1 * RATE)
    hi, lo = OnePole(6000), OnePole(1800)
    crackle = [0.0] * (n + x)
    env = 0.0
    for i in range(n + x):
        if rng.random() < 0.0016:
            env = rng.uniform(0.4, 1.0)
        env *= 0.992
        v = rng.uniform(-1, 1)
        crackle[i] = (hi(v) - lo(v)) * env
    c = crackle[:n]
    for i in range(x):
        w = i / x
        c[i] = crackle[n + i] * math.cos(w * math.pi / 2) + crackle[i] * math.sin(w * math.pi / 2)
    out = [out[i] + 0.9 * c[i] for i in range(n)]
    m = max(abs(s) for s in out) or 1.0
    return [s * 0.75 / m for s in out]


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "sound", "vr")
    rng = random.Random(1123)
    write_wav(os.path.join(out_dir, "lance_charge.wav"), hum(rng), loop=True)
    print("lance sounds written to", os.path.normpath(out_dir))


if __name__ == "__main__":
    main()
