#!/usr/bin/env python3
# make_adweapon_sounds.py -- two sounds for Arcane Dimensions' weapons in Quake VR (docs/vr-port/ROUND22.md, "Widowmaker
# Gatling"), synthesized here from nothing but noise and sines (no recording: nothing to credit), into quakevr/sound/vr/:
#
#   widowmaker_shot.wav  the Widowmaker's shot (QC AD_FireWidowmaker), an auto shotgun's: unlike the pump and the double
#                        barrel (weapons/guncock.wav, shotgn2.wav: a long boom), a short dry punch (a crack, a low
#                        thump, a boom that dies in ~60 ms), then the action cycling: a bright clack as the bolt runs
#                        back (75 ms) and a heavier one as it slams home (115 ms), a short room tail.
#   xbow_bolt_in.wav     the AD crossbow's next bolt morphing into its groove (vr_adcrossbow.cpp, its volume
#                        vr_adgun_crossbow_bolt_sound): a soft rising shimmer, subtle.
#
#   python Misc/quakevr/make_adweapon_sounds.py [out folder, default quakevr/sound/vr]
#
# 22050 Hz 16-bit mono; a fixed random seed (the same files every run). Pure Python (no numpy).

import math
import os
import random
import struct
import sys

RATE = 22050


def write_wav(path, samples):
    peak = max(1e-9, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(data)) + data)
    print("%s: %d samples (%.3f s), peak %.3f" % (path, len(samples), len(samples) / RATE, peak))


def low_pass(x, hz):
    a = 1.0 - math.exp(-2.0 * math.pi * hz / RATE)
    y, out = 0.0, []
    for v in x:
        y += a * (v - y)
        out.append(y)
    return out


def high_pass(x, hz):
    lp = low_pass(x, hz)
    return [v - l for v, l in zip(x, lp)]


def noise(n, rng):
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def normalize(x, peak):
    m = max(1e-9, max(abs(v) for v in x))
    return [v * peak / m for v in x]


def fade_out(x, seconds):
    n = int(seconds * RATE)
    for i in range(n):
        k = len(x) - n + i
        x[k] *= 0.5 + 0.5 * math.cos(math.pi * i / n)
    return x


def clack(n, at, partials, tau, amp, rng):
    """A metallic clack at `at` s: damped sines (hz) and a short click of noise."""
    out = [0.0] * n
    start = int(at * RATE)
    click = high_pass(noise(int(0.004 * RATE), rng), 2500.0)
    for i in range(start, n):
        t = (i - start) / RATE
        v = sum(math.sin(2.0 * math.pi * hz * t + ph) for hz, ph in partials) / len(partials) * math.exp(-t / tau)
        if i - start < len(click):
            v += click[i - start] * math.exp(-t / 0.0012) * 1.5
        out[i] = v * amp
    return out


def widowmaker_shot():
    rng = random.Random(3412)
    n = int(0.62 * RATE)
    crack = high_pass(noise(n, rng), 1500.0)
    boom = low_pass(low_pass(noise(n, rng), 900.0), 900.0)
    tail = low_pass(noise(n, rng), 350.0)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / RATE
        attack = 1.0 - math.exp(-t / 0.0007)
        f = 50.0 + 70.0 * math.exp(-t / 0.03) # the thump's pitch falling 120 -> 50 Hz
        phase += 2.0 * math.pi * f / RATE
        v = crack[i] * math.exp(-t / 0.0035) * 0.9
        v += boom[i] * 9.0 * math.exp(-t / 0.055) * attack
        v += math.sin(phase) * 0.9 * math.exp(-t / 0.05) * attack
        v += tail[i] * 3.0 * math.exp(-t / 0.16) * attack * 0.35
        out.append(v)
    back = clack(n, 0.075, [(2300.0, 0.0), (3650.0, 1.0), (5100.0, 2.0)], 0.005, 0.55, rng)
    home = clack(n, 0.115, [(1650.0, 0.5), (2850.0, 1.5), (420.0, 0.0)], 0.008, 0.85, rng)
    out = [a + b + c for a, b, c in zip(out, back, home)]
    out = [math.tanh(1.8 * v) / math.tanh(1.8) for v in normalize(out, 1.0)] # (a soft knee: punchy, never clipped)
    return fade_out(normalize(out, 0.95), 0.04)


def bolt_in():
    rng = random.Random(9077)
    n = int(0.45 * RATE)
    air = high_pass(noise(n, rng), 3000.0)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / RATE
        u = t / (n / RATE)
        env = math.sin(0.5 * math.pi * min(1.0, t / 0.12)) ** 2 * (0.5 + 0.5 * math.cos(math.pi * max(0.0, (t - 0.12) / (n / RATE - 0.12))))
        f = 900.0 * (1400.0 / 900.0) ** u # rising
        phase += 2.0 * math.pi * f / RATE
        tone = math.sin(phase) + 0.5 * math.sin(2.01 * phase + 0.7) + 0.25 * math.sin(3.03 * phase + 1.9)
        tremolo = 1.0 + 0.35 * math.sin(2.0 * math.pi * 28.0 * t)
        out.append(env * (tone * tremolo / 1.75 + air[i] * 0.3))
    return normalize(out, 0.5)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "sound", "vr")
    write_wav(os.path.join(out_dir, "widowmaker_shot.wav"), widowmaker_shot())
    write_wav(os.path.join(out_dir, "xbow_bolt_in.wav"), bolt_in())


if __name__ == "__main__":
    main()
