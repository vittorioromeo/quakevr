#!/usr/bin/env python3
# make_sounds.py -- generates Quake VR's own synthesised sounds (quakevr/sound/vr/):
#   headshot.wav  a headshot's confirmation (vr_headshot_sound): meaty -- a crack, a helmet dink, a low
#                 punch and a wet crunch of bone; short, so that it cuts through gunfire
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


class OnePole:
    """A one-pole low-pass at `cutoff` Hz (high-pass: the input minus it)."""

    def __init__(self, cutoff):
        self.a = 1.0 - math.exp(-2 * math.pi * cutoff / RATE)
        self.y = 0.0

    def __call__(self, x):
        self.y += self.a * (x - self.y)
        return self.y


def headshot():
    """Meaty, in layers: a sharp crack, a short helmet dink (inharmonic partials, dropping a
    little in pitch), a low punch sweeping down, a mid thwack, and a wet crunch of bone (grains of
    band-passed noise) tailing off; soft-clipped together for weight."""
    rng = random.Random(7)
    n = int(RATE * 0.42)
    crack_hp = OnePole(2500)
    thwack_lp, thwack_hp = OnePole(900), OnePole(250)
    crunch_lp, crunch_hp = OnePole(2200), OnePole(500)
    wet_lp = OnePole(700)
    grain = 0.0
    phase_punch = 0.0
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)

        # The crack: 3 ms of bright noise.
        crack = (noise - crack_hp(noise)) * math.exp(-t / 0.003)

        # The dink: a struck helmet, partials that die fast (the high ones faster).
        drop = 1.0 - 0.06 * min(1.0, t / 0.08)
        dink = 0.0
        for f, amp, decay in ((1870, 0.55, 0.050), (2960, 0.45, 0.040), (4630, 0.30, 0.028), (6310, 0.18, 0.018)):
            dink += amp * math.sin(2 * math.pi * f * drop * t) * math.exp(-t / decay)
        dink *= min(1.0, t / 0.0008)

        # The punch: a sine sweeping 130 -> 50 Hz.
        phase_punch += 2 * math.pi * (50 + 80 * math.exp(-t / 0.03)) / RATE
        punch = math.sin(phase_punch) * math.exp(-t / 0.09) * min(1.0, t / 0.002)

        # The thwack: mid-band noise, 60 ms.
        thwack = thwack_hp(thwack_lp(noise)) * math.exp(-t / 0.035)

        # The crunch: sparse grains of band-passed noise, thinning out.
        if rng.random() < 0.035 * math.exp(-t / 0.08):
            grain = rng.uniform(0.5, 1.0) * (1 if rng.random() < 0.5 else -1)
        grain *= 0.93
        crunch_in = grain + noise * 0.15
        crunch = (crunch_lp(crunch_in) - crunch_hp(crunch_lp.y)) * math.exp(-t / 0.11) * min(1.0, t / 0.006)

        # The wet tail: low noise, swelling slightly after the hit.
        wet = wet_lp(noise) * (t / 0.02) * math.exp(-t / 0.05) * 0.6 if t < 0.3 else 0.0

        s = crack * 0.9 + dink * 0.5 + punch * 1.1 + thwack * 1.6 + crunch * 1.4 + wet * 1.2
        out.append(math.tanh(s * 1.6))
    peak = max(abs(s) for s in out)
    fade = int(RATE * 0.02)
    return [s * 0.97 / peak * (min(1.0, (n - i) / fade)) for i, s in enumerate(out)]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "sound", "vr")
    os.makedirs(out, exist_ok=True)
    write_wav(os.path.join(out, "headshot.wav"), headshot())
    print("headshot.wav -> " + os.path.normpath(out))


if __name__ == "__main__":
    main()
