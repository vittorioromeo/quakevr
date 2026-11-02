#!/usr/bin/env python3
# make_footstep_sounds.py -- generates the enemies' footsteps (quakevr/sound/vr/step_*.wav; vr_footsteps.cpp), synthesised:
#   step_boot1..5.wav   a leather boot (grunts, enforcers): a heel's dull knock and a gritty scuff, the sole's slap after
#   step_metal1..5.wav  an armoured boot (knights): the same knock, heavier, a plate's short clink and a rattle after
#   step_bare1..5.wav   a wet bare foot (zombies, gremlins): a flesh slap, a soft low thud and a small squelch
#   step_thud1..5.wav   a great bare foot (ogres, shamblers, fiends): a deep falling thump with a low rumble after
# Five takes of each, apart in timing, pitch and grit. The monsters' size is the engine's (a playback rate and a weight per
# kind: vr_footsteps.cpp, kinds). Everything is seeded: the same files every run.
#
# Usage: python Misc/quakevr/make_footstep_sounds.py [out_dir]   (default: quakevr/sound/vr)

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
    """A one-pole low-pass at `cutoff` Hz."""

    def __init__(self, cutoff):
        self.a = 1.0 - math.exp(-2 * math.pi * cutoff / RATE)
        self.y = 0.0

    def __call__(self, x):
        self.y += self.a * (x - self.y)
        return self.y


def buffer(seconds):
    return [0.0] * int(seconds * RATE)


def thump(out, at, f0, f1, decay, level, attack=0.002):
    """A falling sine: a body's weight arriving."""
    phase = 0.0
    start = int(at * RATE)
    for i in range(start, len(out)):
        t = (i - start) / RATE
        f = f1 + (f0 - f1) * math.exp(-t / (decay * 0.5))
        phase += 2 * math.pi * f / RATE
        out[i] += level * math.sin(phase) * min(t / attack, 1.0) * math.exp(-t / decay)


def noise(out, at, cut_lo, cut_hi, decay, level, rng, attack=0.001):
    """A burst of band-passed noise: grit, a slap."""
    lo, hi = OnePole(cut_lo), OnePole(cut_hi)
    start = int(at * RATE)
    for i in range(start, len(out)):
        t = (i - start) / RATE
        x = rng.uniform(-1, 1)
        s = hi(x) - lo(x)
        out[i] += level * s * min(t / attack, 1.0) * math.exp(-t / decay)


def ring(out, at, freqs, decay, level, rng):
    """A short metal ring: inharmonic partials, the higher dying first."""
    start = int(at * RATE)
    parts = [(f * rng.uniform(0.97, 1.03), rng.uniform(0, 6.28), a) for f, a in freqs]
    for i in range(start, len(out)):
        t = (i - start) / RATE
        s = sum(a * math.sin(2 * math.pi * f * t + p) * math.exp(-t * f / (decay * 2000.0)) for f, p, a in parts)
        out[i] += level * s * math.exp(-t / decay)


def chirp(out, at, f0, f1, length, level):
    """A rising bubble: the squelch."""
    start = int(at * RATE)
    phase = 0.0
    for i in range(start, min(len(out), start + int(length * RATE))):
        u = (i - start) / (length * RATE)
        phase += 2 * math.pi * (f0 + (f1 - f0) * u) / RATE
        out[i] += level * math.sin(phase) * math.sin(math.pi * u) ** 2


def boot(rng):
    out = buffer(0.2)
    k = rng.uniform(0.9, 1.12)
    thump(out, 0.0, 170 * k, 85 * k, 0.03, 0.8)
    noise(out, 0.0, 250, 1700 * k, 0.018, 1.0, rng)
    slap = rng.uniform(0.04, 0.065)
    noise(out, slap, 400, 2600 * k, 0.014, 0.55, rng)
    thump(out, slap, 140 * k, 90 * k, 0.018, 0.25)
    return out


def metal(rng):
    out = buffer(0.3)
    k = rng.uniform(0.9, 1.1)
    thump(out, 0.0, 150 * k, 70 * k, 0.04, 0.9)
    noise(out, 0.0, 200, 1500 * k, 0.02, 0.9, rng)
    ring(out, 0.004, [(1320 * k, 0.5), (2050 * k, 0.32), (3170 * k, 0.2), (4400 * k, 0.1)], 0.05, 0.42, rng)
    for _ in range(rng.randint(2, 3)):
        at = rng.uniform(0.04, 0.16)
        ring(out, at, [(2400 * rng.uniform(0.8, 1.2), 0.5), (3600 * rng.uniform(0.8, 1.2), 0.3)], 0.012, rng.uniform(0.1, 0.2), rng)
    return out


def bare(rng):
    out = buffer(0.24)
    k = rng.uniform(0.88, 1.15)
    thump(out, 0.0, 120 * k, 70 * k, 0.03, 0.55)
    noise(out, 0.0, 500 * k, 3600 * k, 0.012, 1.3, rng, attack=0.0005)
    at = rng.uniform(0.02, 0.05)
    noise(out, at, 300, 1500, 0.035, 0.32, rng, attack=0.006)
    chirp(out, at + rng.uniform(0.01, 0.03), 380 * k, 900 * k, rng.uniform(0.03, 0.05), 0.2)
    chirp(out, at + rng.uniform(0.05, 0.08), 500 * k, 1150 * k, 0.025, 0.1)
    return out


def thud(rng):
    out = buffer(0.42)
    k = rng.uniform(0.9, 1.1)
    thump(out, 0.0, 95 * k, 42 * k, 0.11, 1.0, attack=0.004)
    noise(out, 0.0, 60, 420 * k, 0.05, 0.9, rng, attack=0.003)
    noise(out, 0.002, 300, 1300, 0.01, 0.25, rng)
    noise(out, rng.uniform(0.05, 0.09), 40, 200, 0.12, 0.45, rng, attack=0.03)
    return out


def finish(out, level):
    """Peak to `level`, the tail faded (no click), the first sample at the attack."""
    fade = int(0.02 * RATE)
    n = len(out)
    for i in range(fade):
        out[n - 1 - i] *= i / fade
    peak = max(abs(s) for s in out) or 1.0
    return [s * level / peak for s in out]


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "quakevr", "sound", "vr")
    out_dir = sys.argv[1] if len(sys.argv) > 1 else root
    os.makedirs(out_dir, exist_ok=True)
    # (each a little under full scale: its level is the engine's; the thud carries in the lows, felt more than heard)
    for name, make, level, seed in (("boot", boot, 0.8, 101), ("metal", metal, 0.8, 202), ("bare", bare, 0.7, 303), ("thud", thud, 0.9, 404)):
        for take in range(5):
            rng = random.Random(seed + take)
            samples = finish(make(rng), level)
            path = os.path.join(out_dir, "step_%s%d.wav" % (name, take + 1))
            write_wav(path, samples)
            rms = math.sqrt(sum(s * s for s in samples) / len(samples))
            print("%s: %.2f s, rms %.3f" % (os.path.basename(path), len(samples) / RATE, rms))


if __name__ == "__main__":
    main()
