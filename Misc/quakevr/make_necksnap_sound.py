#!/usr/bin/env python3
# make_necksnap_sound.py -- the neck snap's bone crack (QC vr_foegrab_throw.qc VR_FoeGrab_NeckSnap; Combat > Holding
# Enemies > Neck Snap; docs/vr-port/ROUND22.md, "The neck snap"), synthesised (no recordings):
#
#   quakevr/sound/vr/necksnap.wav   a big dry bone crack: the snap itself (a hard broadband transient through two bony
#                                   resonances), the vertebrae's crunch right after it (a dozen short clicks, closer
#                                   together, then fewer), a cartilage pop, the neck's dull body thump under it all, and
#                                   a faint gristly crackle dying away
#
# 22050 Hz, mono, 16 bit (as the physics sounds), its peak at -0.5 dBFS after a soft clip (it is meant to be loud).
# Deterministic (a fixed seed). Pure Python (no numpy).
#
# Usage: python Misc/quakevr/make_necksnap_sound.py [output folder, default quakevr/sound/vr]

import math
import os
import random
import struct
import sys
import wave

RATE = 22050
LENGTH = 0.55  # s
SEED = 1337
DRIVE = 4.0  # the soft clip's drive (loudness: see make)


def biquad_bandpass(x, centre, q):
    """RBJ band-pass (0 dB peak gain) of the list x."""
    w = 2.0 * math.pi * centre / RATE
    alpha = math.sin(w) / (2.0 * q)
    b0, b1, b2 = alpha, 0.0, -alpha
    a0, a1, a2 = 1.0 + alpha, -2.0 * math.cos(w), 1.0 - alpha
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    y = [0.0] * len(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, v
        y2, y1 = y1, o
        y[i] = o
    return y


def highpass(x, cutoff):
    """One-pole high-pass."""
    rc = 1.0 / (2.0 * math.pi * cutoff)
    a = rc / (rc + 1.0 / RATE)
    y = [0.0] * len(x)
    prev_x = prev_y = 0.0
    for i, v in enumerate(x):
        prev_y = a * (prev_y + v - prev_x)
        prev_x = v
        y[i] = prev_y
    return y


def add(out, start, sig, gain):
    i0 = int(start * RATE)
    for i, v in enumerate(sig):
        if 0 <= i0 + i < len(out):
            out[i0 + i] += gain * v


def burst(rng, length, tau):
    """White noise length s long, decaying exponentially with time constant tau, a 0.3 ms attack."""
    n = max(1, int(length * RATE))
    attack = max(1, int(0.0003 * RATE))
    return [rng.uniform(-1.0, 1.0) * min(1.0, (i + 1) / attack) * math.exp(-i / (tau * RATE)) for i in range(n)]


def click(rng, centre, q, length, tau):
    """A short filtered noise click (a bony tick)."""
    raw = burst(rng, length, tau) + [0.0] * int(0.01 * RATE)
    y = biquad_bandpass(raw, centre, q)
    peak = max(abs(v) for v in y) or 1.0
    return [v / peak for v in y]


def thump(f0, f1, tau, length):
    """A damped sine gliding from f0 to f1 Hz: the neck's dull body under the crack."""
    n = int(length * RATE)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = f1 + (f0 - f1) * math.exp(-t / 0.03)
        phase += 2.0 * math.pi * f / RATE
        out.append(math.sin(phase) * math.exp(-t / tau) * min(1.0, i / (0.002 * RATE)))
    return out


def make():
    rng = random.Random(SEED)
    out = [0.0] * int(LENGTH * RATE)
    t0 = 0.015
    # The snap: a hard broadband transient through two bony resonances, and its raw edge.
    snap = burst(rng, 0.03, 0.0035) + [0.0] * int(0.02 * RATE)
    add(out, t0, biquad_bandpass(snap, 2300.0, 1.2), 2.4)
    add(out, t0, biquad_bandpass(snap, 4100.0, 2.0), 1.6)
    add(out, t0, biquad_bandpass(snap, 1100.0, 3.0), 1.4)
    add(out, t0, highpass(burst(rng, 0.004, 0.0008), 3000.0), 0.9)
    # A second, slightly lower crack hard on it (the joint giving way).
    add(out, t0 + 0.011, click(rng, 1700.0, 2.2, 0.02, 0.003), 0.95)
    # The vertebrae's crunch: clicks closer together then fewer, each a little softer, their pitch wandering.
    t = t0 + 0.022
    gain = 0.75
    for k in range(13):
        add(out, t, click(rng, rng.uniform(1300.0, 4200.0), rng.uniform(2.0, 5.0), rng.uniform(0.003, 0.008),
                          rng.uniform(0.0008, 0.0022)), gain * rng.uniform(0.6, 1.0))
        t += rng.uniform(0.004, 0.009) * (1.0 + 0.22 * k)
        gain *= 0.86
    # A cartilage pop: a short resonant ping.
    pop = burst(rng, 0.012, 0.002) + [0.0] * int(0.04 * RATE)
    add(out, t0 + 0.028, biquad_bandpass(pop, 820.0, 9.0), 1.1)
    # The neck's dull body thump under it all.
    add(out, t0, thump(150.0, 70.0, 0.06, 0.3), 1.1)
    # A faint gristly crackle dying away.
    t = t0 + 0.14
    gain = 0.22
    while t < LENGTH - 0.05:
        add(out, t, click(rng, rng.uniform(1800.0, 5000.0), 3.0, 0.002, 0.0006), gain * rng.uniform(0.4, 1.0))
        t += rng.uniform(0.012, 0.045)
        gain *= 0.85
    # Soft clip driven hard (it is meant to hit hard: its loudest 50 ms as loud as the blows', vr/headshot.wav's, about
    # -4 dBFS; it was -10 driven at 1.6, a background tick), peak at -0.5 dBFS, fades at both ends.
    peak = max(abs(v) for v in out) or 1.0
    out = [math.tanh(DRIVE * v / peak) for v in out]
    peak = max(abs(v) for v in out) or 1.0
    level = 10.0 ** (-0.5 / 20.0) / peak
    fade_out = int(0.06 * RATE)
    n = len(out)
    for i in range(n):
        f = min(1.0, (n - 1 - i) / fade_out)
        out[i] *= level * f
    return out


def write_wav(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", max(-32767, min(32767, int(round(v * 32767))))) for v in samples))


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                                                                 "quakevr", "sound", "vr")
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "necksnap.wav")
    samples = make()
    write_wav(path, samples)
    print(f"{os.path.normpath(path)}: {len(samples) / RATE:.2f} s, {RATE} Hz, peak {max(abs(v) for v in samples):.3f}")


if __name__ == "__main__":
    main()
