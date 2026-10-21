#!/usr/bin/env python3
# make_gore_placeholders.py -- PLACEHOLDER gore sounds (vr_goresound.cpp; docs/vr-port/ROUND22.md, "Gore sounds";
# SOUNDS_OURS.md, Gore). Synthesised stand-ins, every file named ph_*: Vittorio replaces them with his own recordings
# (same names, or new names in vr_goresound.cpp's table). No recordings used.
#
#   quakevr/sound/vr/gore/ph_spurt1..3.wav   a stump's spurt: one wet pulse of blood (a low gush, bubbly)
#   quakevr/sound/vr/gore/ph_pour1..2.wav    a stream's splatter on the floor: 0.6 s grains played back to back
#                                            (faded in and out: their fades cross), a dense patter of drops
#   quakevr/sound/vr/gore/ph_choke1..2.wav   a wet choke / gargle (a voiced buzz through bubbling, a cough at the end)
#   quakevr/sound/vr/gore/ph_tear1..2.wav    a limb or head torn off: a wet rip, then a bone crack
#   quakevr/sound/vr/gore/ph_squelch1..2.wav a hand closing on a gib or a bloody body
#   quakevr/sound/vr/gore/ph_step1..3.wav    a wet footstep (a thud, a short splash)
#   quakevr/sound/vr/gore/ph_slap1..3.wav    a gib slapping into a wall
#
# 22050 Hz, mono, 16 bit; peaks at -1 dBFS. Deterministic (a fixed seed). Pure Python (no numpy).
#
# Usage: python Misc/quakevr/make_gore_placeholders.py [output folder, default quakevr/sound/vr/gore]

import math
import os
import random
import struct
import sys
import wave

RATE = 22050


def zeros(seconds):
    return [0.0] * int(seconds * RATE)


def lowpass(x, cutoff):
    a = math.exp(-2.0 * math.pi * cutoff / RATE)
    y, out = 0.0, []
    for v in x:
        y = (1.0 - a) * v + a * y
        out.append(y)
    return out


def highpass(x, cutoff):
    lo = lowpass(x, cutoff)
    return [a - b for a, b in zip(x, lo)]


def bandpass(x, centre, q):
    w = 2.0 * math.pi * centre / RATE
    alpha = math.sin(w) / (2.0 * q)
    b0, b2 = alpha, -alpha
    a0, a1, a2 = 1.0 + alpha, -2.0 * math.cos(w), 1.0 - alpha
    x1 = x2 = y1 = y2 = 0.0
    out = []
    for v in x:
        y = (b0 * v + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1 = x1, v
        y2, y1 = y1, y
        out.append(y)
    return out


def noise(rng, seconds):
    return [rng.uniform(-1.0, 1.0) for _ in range(int(seconds * RATE))]


def env(n, attack, decay):
    # attack and decay in seconds: a fast rise, an exponential fall
    out = []
    for i in range(n):
        t = i / RATE
        a = min(1.0, t / attack) if attack > 0 else 1.0
        out.append(a * math.exp(-t / decay))
    return out


def mul(a, b):
    return [x * y for x, y in zip(a, b)]


def add(out, start, sig, gain=1.0):
    s = int(start * RATE)
    for i, v in enumerate(sig):
        if 0 <= s + i < len(out):
            out[s + i] += v * gain


def bubbles(rng, seconds, rate, f_lo, f_hi, tau):
    # little rising chirps (a bubble's ring) at random times: the wet part of everything here
    out = zeros(seconds)
    t = 0.0
    while t < seconds:
        f0 = rng.uniform(f_lo, f_hi)
        n = int(tau * 5 * RATE)
        ph = 0.0
        amp = rng.uniform(0.3, 1.0)
        sig = []
        for i in range(n):
            tt = i / RATE
            f = f0 * (1.0 + 2.5 * tt / (tau * 5))
            ph += 2 * math.pi * f / RATE
            sig.append(amp * math.sin(ph) * math.exp(-tt / tau))
        add(out, t, sig)
        t += rng.expovariate(rate)
    return out


def normalise(x, peak=0.89):
    m = max(1e-9, max(abs(v) for v in x))
    return [v * peak / m for v in x]


def fade(x, fin, fout):
    n = len(x)
    a, b = int(fin * RATE), int(fout * RATE)
    for i in range(min(a, n)):
        x[i] *= i / a
    for i in range(min(b, n)):
        x[n - 1 - i] *= i / b
    return x


def spurt(rng):
    n = 0.32
    gush = lowpass(noise(rng, n), 700)
    gush = mul(gush, env(len(gush), 0.015, 0.09))
    b = bubbles(rng, n, 60, 250, 700, 0.012)
    b = mul(b, env(len(b), 0.01, 0.12))
    out = zeros(n)
    add(out, 0, gush, 1.0)
    add(out, 0.01, b, 0.35)
    return fade(normalise(out), 0.002, 0.04)


def pour(rng):
    n = 0.6
    drops = bubbles(rng, n, 140, 600, 1800, 0.004)
    hiss = bandpass(noise(rng, n), 1200, 0.7)
    out = zeros(n)
    add(out, 0, drops, 0.8)
    add(out, 0, hiss, 0.25)
    return fade(normalise(out), 0.08, 0.08)


def choke(rng, f0):
    n = 1.1
    out = zeros(n)
    ph = 0.0
    voice = []
    for i in range(int(0.8 * RATE)):
        t = i / RATE
        f = f0 * (1.0 - 0.25 * t) * (1.0 + 0.04 * math.sin(2 * math.pi * 5 * t))
        ph += 2 * math.pi * f / RATE
        buzz = sum(math.sin(k * ph) / k for k in range(1, 9))
        gate = 0.5 + 0.5 * math.sin(2 * math.pi * 17 * t + 2 * math.sin(2 * math.pi * 3 * t))  # the gargle's bubbling
        voice.append(buzz * gate * min(1.0, t / 0.04) * math.exp(-t / 0.5))
    voice = lowpass(bandpass(voice, 450, 0.9), 1600)
    add(out, 0, voice, 1.0)
    b = bubbles(rng, 0.8, 45, 200, 600, 0.015)
    add(out, 0.02, b, 0.4)
    cough = lowpass(noise(rng, 0.18), 900)
    add(out, 0.82, mul(cough, env(len(cough), 0.01, 0.05)), 0.8)
    return fade(normalise(out), 0.005, 0.08)


def tear(rng):
    n = 0.55
    out = zeros(n)
    rip = []
    src = noise(rng, 0.32)
    for i, v in enumerate(src):
        t = i / RATE
        crackle = 1.0 if rng.random() < 0.08 else 0.25  # the fibres giving one by one
        rip.append(v * crackle * min(1.0, t / 0.02) * math.exp(-t / 0.14))
    rip = bandpass(rip, 1400, 0.6)
    add(out, 0, rip, 1.0)
    add(out, 0, mul(lowpass(noise(rng, 0.3), 500), env(int(0.3 * RATE), 0.01, 0.1)), 0.7)
    # the bone crack: a hard click through a bony resonance
    crack = mul(noise(rng, 0.08), env(int(0.08 * RATE), 0.0005, 0.008))
    crack = [a + b for a, b in zip(bandpass(crack, 2200, 3.0), bandpass(crack, 900, 2.0))]
    add(out, 0.2 + rng.uniform(-0.03, 0.03), crack, 2.2)
    return fade(normalise(out), 0.002, 0.06)


def squelch(rng):
    n = 0.3
    out = zeros(n)
    s = lowpass(noise(rng, n), 900)
    s = mul(s, env(len(s), 0.02, 0.07))
    add(out, 0, s, 1.0)
    add(out, 0.01, bubbles(rng, 0.25, 70, 300, 900, 0.008), 0.5)
    return fade(normalise(out), 0.003, 0.05)


def step(rng):
    n = 0.28
    out = zeros(n)
    thud = []
    for i in range(int(0.12 * RATE)):
        t = i / RATE
        thud.append(math.sin(2 * math.pi * (90 - 200 * t) * t) * math.exp(-t / 0.03))
    add(out, 0, thud, 0.8)
    add(out, 0.005, mul(highpass(noise(rng, 0.2), 500), env(int(0.2 * RATE), 0.005, 0.04)), 0.5)
    add(out, 0.01, bubbles(rng, 0.2, 90, 500, 1400, 0.005), 0.4)
    return fade(normalise(out, 0.7), 0.002, 0.05)


def slap(rng):
    n = 0.24
    out = zeros(n)
    hit = mul(noise(rng, 0.05), env(int(0.05 * RATE), 0.0005, 0.006))
    add(out, 0, bandpass(hit, 1500, 0.8), 1.0)
    add(out, 0.002, mul(lowpass(noise(rng, 0.2), 1100), env(int(0.2 * RATE), 0.003, 0.05)), 0.8)
    add(out, 0.01, bubbles(rng, 0.2, 80, 400, 1200, 0.006), 0.35)
    return fade(normalise(out), 0.001, 0.05)


def write_wav(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767)) for v in samples))


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join("quakevr", "sound", "vr", "gore")
    os.makedirs(folder, exist_ok=True)
    rng = random.Random(2210)
    files = {}
    for k in range(3):
        files["ph_spurt%d.wav" % (k + 1)] = spurt(rng)
    for k in range(2):
        files["ph_pour%d.wav" % (k + 1)] = pour(rng)
    files["ph_choke1.wav"] = choke(rng, 115)
    files["ph_choke2.wav"] = choke(rng, 92)
    for k in range(2):
        files["ph_tear%d.wav" % (k + 1)] = tear(rng)
    for k in range(2):
        files["ph_squelch%d.wav" % (k + 1)] = squelch(rng)
    for k in range(3):
        files["ph_step%d.wav" % (k + 1)] = step(rng)
    for k in range(3):
        files["ph_slap%d.wav" % (k + 1)] = slap(rng)
    for name, s in files.items():
        write_wav(os.path.join(folder, name), s)
        print("%s %.2f s" % (name, len(s) / RATE))


if __name__ == "__main__":
    main()
