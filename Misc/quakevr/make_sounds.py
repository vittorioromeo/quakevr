#!/usr/bin/env python3
# make_sounds.py -- generates Quake VR's own synthesised sounds (quakevr/sound/vr/):
#   headshot.wav  a headshot's confirmation (vr_headshot_sound): a sharp metallic crack over a
#                 low thump, short, so that it cuts through gunfire without being mistaken for it
#
# Usage: python Misc/quakevr/make_sounds.py [output sound folder]

import math
import os
import random
import struct
import sys

RATE = 22050


def write_wav(path, samples):
    data = b"".join(struct.pack("<h", max(-32767, min(32767, int(s * 32767)))) for s in samples)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(data)) + data)


def headshot():
    rng = random.Random(7)
    n = int(RATE * 0.32)
    out = []
    for i in range(n):
        t = i / RATE
        click = rng.uniform(-1, 1) * math.exp(-t / 0.004)                       # the impact
        ping = (math.sin(2 * math.pi * 2350 * t) * 0.6 + math.sin(2 * math.pi * 3710 * t) * 0.35 +
                math.sin(2 * math.pi * 5230 * t) * 0.2) * math.exp(-t / 0.055)  # metal ringing
        thump = math.sin(2 * math.pi * (150 - 250 * t) * t) * math.exp(-t / 0.045)  # body
        out.append(click * 0.5 + ping * 0.55 + thump * 0.6)
    peak = max(abs(s) for s in out)
    return [s * 0.95 / peak for s in out]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "sound", "vr")
    os.makedirs(out, exist_ok=True)
    write_wav(os.path.join(out, "headshot.wav"), headshot())
    print("headshot.wav -> " + os.path.normpath(out))


if __name__ == "__main__":
    main()
