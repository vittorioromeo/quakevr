#!/usr/bin/env python3
# make_slide_sounds.py -- generates flat-screen play's slide sounds (quakevr/sound/vr/slide_*.wav; vr_slidesound.cpp),
# synthesised (his words 2026-11-08: "The slide also needs a sound."):
#   slide_start.wav  dropping into the slide: the body's soft thump onto the floor and a short scuff of cloth and boots
#   slide_loop.wav   sliding: a gritty scrape of cloth and leather on stone, 1.2 s, looped (a cue point: Quake loops it
#                    until the engine stops it); the engine follows the slide's speed with its volume and pitch and fades
#                    it as the slide slows
# Seeded: the same files every run.
#
# Usage: python Misc/quakevr/make_slide_sounds.py [out_dir]   (default: quakevr/sound/vr)

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


def grit(n, rng, lo_cut, hi_cut, grain_rate):
    """Band-passed noise in uneven grains: cloth and leather dragging over grains of stone."""
    lo, hi = OnePole(lo_cut), OnePole(hi_cut)
    out = [0.0] * n
    grain = 0.5
    for i in range(n):
        if rng.random() < grain_rate:
            grain = rng.uniform(0.35, 1.0)
        grain = 0.5 + (grain - 0.5) * 0.997
        x = rng.uniform(-1, 1)
        out[i] = (hi(x) - lo(x)) * grain
    return out


def normalise(out, peak):
    m = max(abs(s) for s in out) or 1.0
    return [s * peak / m for s in out]


def start(rng):
    n = int(0.42 * RATE)
    out = [0.0] * n
    # the thump: a falling sine, the body meeting the floor
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = 55 + 70 * math.exp(-t / 0.02)
        phase += 2 * math.pi * f / RATE
        out[i] += 0.55 * math.sin(phase) * min(t / 0.004, 1.0) * math.exp(-t / 0.06)
    # the scuff: grit swelling in and dying away
    g = grit(n, rng, 250, 2600, 0.02)
    for i in range(n):
        t = i / RATE
        out[i] += 1.6 * g[i] * min(t / 0.015, 1.0) * math.exp(-t / 0.16)
    f = OnePole(3500)
    out = [f(s) for s in out]
    fade = int(0.04 * RATE)
    for i in range(fade):
        out[n - 1 - i] *= i / fade
    return normalise(out, 0.8)


def loop(rng):
    n = int(1.2 * RATE)
    x = int(0.15 * RATE)  # the crossfade: the tail runs on into the head
    g = grit(n + x, rng, 180, 2200, 0.01)
    rumble = grit(n + x, rng, 40, 260, 0.004)
    raw = [g[i] + 0.8 * rumble[i] for i in range(n + x)]
    # seamless: the head is the samples after the end fading out over the head's own fading in (equal power), so the
    # last sample runs on into the first
    out = raw[:n]
    for i in range(x):
        w = i / x
        out[i] = raw[n + i] * math.cos(w * math.pi / 2) + raw[i] * math.sin(w * math.pi / 2)
    return normalise(out, 0.7)


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "sound", "vr")
    rng = random.Random(1108)
    write_wav(os.path.join(out_dir, "slide_start.wav"), start(rng))
    write_wav(os.path.join(out_dir, "slide_loop.wav"), loop(rng), loop=True)
    print("slide sounds written to", os.path.normpath(out_dir))


if __name__ == "__main__":
    main()
